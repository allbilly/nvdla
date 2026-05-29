#!/usr/bin/env python3
"""Self-contained CONV+ReLU (2x2 kernel, 4→6 ch, FP16) on NVDLA via DRM ioctl.

Pipeline: CONV(2x2, 4→6) → SDP(ReLU) → SDP(Identity)
Input: 5x5x4 FP16 NCxHWx, Output: 4x4x6 FP16 NCxHWx
Weights: all 1.0 FP16
"""
from fcntl import ioctl
import ctypes, mmap, os, struct

# ---- Tensor parameters ----
IN_W, IN_H, IN_C = 5, 5, 4
OUT_W, OUT_H, OUT_C = 4, 4, 6
KH, KW = 2, 2
BPE = 2
X = 16

IN_LINE_STRIDE = IN_W * X * BPE          # 5*32 = 160
IN_SURF_STRIDE = IN_H * IN_LINE_STRIDE   # 800
OUT_LINE_STRIDE = OUT_W * X * BPE        # 128
OUT_SURF_STRIDE = OUT_H * OUT_LINE_STRIDE # 512
TOTAL_WEIGHT_BYTES = 256

# ---- Decoded descriptor data ----

def pack_net_desc(operation_desc_index, surface_desc_index, dependency_graph_index,
                  op_head, num_operations, num_addresses, lut_data_index=-1,
                  roi_array_index=-1, surface_index=-1, stat_list_index=-1,
                  num_rois=1, num_luts=0, input_layer=0, dynamic_roi=0):
    return struct.pack(
        '<8h6h4HhBB',
        operation_desc_index, surface_desc_index, dependency_graph_index,
        lut_data_index, roi_array_index, surface_index, stat_list_index, 0,
        *op_head, num_rois, num_operations, num_luts, num_addresses,
        input_layer, dynamic_roi, 0,
    )


def pack_consumer(index=-1, event=1):
    return struct.pack('<hBB', index, event, 0)


def pack_common_op(index, op_type, dependency_count, consumers, fused_parent=(-1, 1)):
    return (
        struct.pack('<hbBB3x', index, 0, op_type, dependency_count) +
        b''.join(pack_consumer(*c) for c in consumers) +
        pack_consumer(*fused_parent)
    )


def pack_cvt(scale=0, truncate=0, enable=0, offset=0):
    return struct.pack('<hBBi', scale, truncate, enable, offset)


def pack_sdp_unit(enable=0, alu_type=0, op_type=0, mode=0, act=0,
                  shift=0, truncate=0, precision=0, alu_operand=0,
                  mul_operand=0, alu_cvt=(0, 0, 0, 0), mul_cvt=(0, 0, 0, 0)):
    return (
        struct.pack('<BBBBBBBBii', enable, alu_type, op_type, mode, act,
                    shift, truncate, precision, alu_operand, mul_operand) +
        pack_cvt(*alu_cvt) + pack_cvt(*mul_cvt)
    )


def pack_sdp_op(src_precision, dst_precision, lut_index=-1, out_cvt=(0, 0, 0, 0),
                conv_mode=0, batch_num=1, batch_stride=0,
                x1_op=(), x2_op=(), y_op=()):
    return (
        struct.pack('<BBh', src_precision, dst_precision, lut_index) +
        pack_cvt(*out_cvt) +
        struct.pack('<BBHI', conv_mode, batch_num, 0, batch_stride) +
        pack_sdp_unit(*x1_op) + pack_sdp_unit(*x2_op) + pack_sdp_unit(*y_op)
    )


def pack_conv_op():
    op = (
        struct.pack('<BBBBBBH', 0, 0, 0, 0, 0, 0, 2) +
        struct.pack('<BBH', 36, 0, 1) +
        b'\x00' * 8 +
        struct.pack('<BBBB', 1, 0, 1, 1) +
        struct.pack('<I', 0) +
        struct.pack('<BBH', 0, 0, 5) +
        struct.pack('<HHHHHHHH', IN_W, IN_H, IN_C, KW, KH, IN_C, OUT_W, OUT_H) +
        struct.pack('<I', 32) +
        struct.pack('<hhhh', 0, 0, 0, 0) +
        struct.pack('<BBBBBBBBB', 0, 1, 1, 0, 0, 0, 0, 1, 1) +
        b'\x00' * 3 +
        struct.pack('<BBB', 2, 2, 0) +
        struct.pack('<h', 0) +
        pack_cvt(0, 0, 0, 0x01000000) +
        pack_cvt(0, 1, 0, 0)
    )
    return op + b'\x00' * (116 - len(op))


def pack_cube(mem_type=0, address=0, size=0, width=0, height=0, channel=0,
              line_stride=0, surf_stride=0, plane_stride=0):
    # Direct KMD submit expects the full 32-byte dla_data_cube layout.
    return struct.pack('<HhIIHHHHIII', mem_type, address, 0, size, width, height,
                       channel, 0, line_stride, surf_stride, plane_stride)


def pack_surface_container(*cubes):
    data = b''.join(pack_cube(*cube) for cube in cubes)
    return data + b'\x00' * (644 - len(data))


NET_DESC = pack_net_desc(7, 8, 6, [-1, 0, 1, -1, -1, -1], 3, 10)
DEP_GRAPH = b''.join([
    pack_common_op(0, 1, 1, [(-1, 1), (-1, 1), (1, 2), (-1, 1), (-1, 1), (-1, 1)]),
    pack_common_op(1, 2, 1, [(-1, 1), (-1, 1), (2, 1), (-1, 1), (-1, 1), (-1, 1)], fused_parent=(0, 3)),
    pack_common_op(2, 2, 1, [(-1, 1)] * 6),
])
OP_LIST = b''.join([
    pack_conv_op(),
    pack_sdp_op(2, 2, out_cvt=(1, 0, 1, 0)),
    pack_sdp_op(2, 2, out_cvt=(1, 0, 1, 0),
                x1_op=(1, 2, 0, 0, 1, 0, 0, 2, 0, 1)),
])
SURF_LIST = b''.join([
    pack_surface_container(
        (0, 1, TOTAL_WEIGHT_BYTES, KW, KH, IN_C, 0, 0, 0),
        (2, -1, 0, 0, 0, 0, 0, 0, 0),
        (2, -1, 0, 0, 0, 0, 0, 0, 0),
        (0, 2, IN_SURF_STRIDE, IN_W, IN_H, IN_C, IN_LINE_STRIDE, IN_SURF_STRIDE, 0),
        (2, -1, OUT_SURF_STRIDE, OUT_W, OUT_H, OUT_C, OUT_LINE_STRIDE, OUT_SURF_STRIDE, 0),
    ),
    pack_surface_container(
        (2, -1, OUT_SURF_STRIDE, OUT_W, OUT_H, OUT_C, 0, 0, 0),
        (), (), (),
        (0, 3, OUT_SURF_STRIDE, OUT_W, OUT_H, OUT_C, OUT_LINE_STRIDE, OUT_SURF_STRIDE, 0),
    ),
    pack_surface_container(
        (0, 3, OUT_SURF_STRIDE, OUT_W, OUT_H, OUT_C, OUT_LINE_STRIDE, OUT_SURF_STRIDE, 0),
        (), (), (),
        (0, 4, OUT_SURF_STRIDE, OUT_W, OUT_H, OUT_C, OUT_LINE_STRIDE, OUT_SURF_STRIDE, 0),
    ),
])
assert (len(NET_DESC), len(DEP_GRAPH), len(OP_LIST), len(SURF_LIST)) == (40, 108, 348, 1932)


# ---- Build tensor data ----

def build_weight_blob():
    """2x2x4x6 FP16, all 1.0, in NVDLA weight format.
    96 values * 2B = 192B, padded to 256B.
    """
    data = struct.pack('<96e', *([1.0] * 96))
    return data + b'\x00' * (TOTAL_WEIGHT_BYTES - len(data))

def build_input_fp16_ncxhwx():
    """5x5x4 FP16 NCxHWx with pixel values = f(atom, h, w) for verification."""
    buf = bytearray(IN_SURF_STRIDE)
    for h in range(IN_H):
        for w in range(IN_W):
            for c in range(IN_C):
                off = h * IN_LINE_STRIDE + w * X * BPE + c * BPE
                val = float(h * IN_W + w + 1 + c * 100)
                buf[off:off + BPE] = struct.pack('<e', val)
    return bytes(buf)


# ---- DRM ioctl infrastructure ----

DRM_IOCTL_BASE = ord("d")

def _IOC(d, t, nr, s): return (d << 30) | (s << 16) | (t << 8) | nr
def _IOWR(t, nr, s): return _IOC(3, t, nr, s)

class nvdla_gem_create(ctypes.Structure):
    _fields_ = [("handle", ctypes.c_uint32), ("flags", ctypes.c_uint32), ("size", ctypes.c_uint64)]
class nvdla_gem_map_offset(ctypes.Structure):
    _fields_ = [("handle", ctypes.c_uint32), ("reserved", ctypes.c_uint32), ("offset", ctypes.c_uint64)]
class nvdla_gem_destroy(ctypes.Structure):
    _fields_ = [("handle", ctypes.c_uint32)]
class drm_prime_handle(ctypes.Structure):
    _fields_ = [("handle", ctypes.c_uint32), ("flags", ctypes.c_uint32), ("fd", ctypes.c_int32)]
class nvdla_mem_handle(ctypes.Structure):
    _fields_ = [("handle", ctypes.c_uint32), ("reserved", ctypes.c_uint32), ("offset", ctypes.c_uint64)]
class nvdla_ioctl_submit_task(ctypes.Structure):
    _fields_ = [("num_addresses", ctypes.c_uint32), ("timeout", ctypes.c_uint32), ("address_list", ctypes.c_uint64)]
class nvdla_submit_args(ctypes.Structure):
    _fields_ = [("tasks", ctypes.c_uint64), ("num_tasks", ctypes.c_uint16), ("flags", ctypes.c_uint16), ("version", ctypes.c_uint32)]

GEM_CREATE   = _IOWR(DRM_IOCTL_BASE, 0x41, ctypes.sizeof(nvdla_gem_create))
GEM_MMAP     = _IOWR(DRM_IOCTL_BASE, 0x42, ctypes.sizeof(nvdla_gem_map_offset))
GEM_DESTROY  = _IOWR(DRM_IOCTL_BASE, 0x43, ctypes.sizeof(nvdla_gem_destroy))
PRIME_H2F    = _IOWR(DRM_IOCTL_BASE, 0x2d, ctypes.sizeof(drm_prime_handle))
NVDLA_SUBMIT = _IOWR(DRM_IOCTL_BASE, 0x40, ctypes.sizeof(nvdla_submit_args))


def main():
    sizes = [4096, 256, 800, 512, 512, 40, 108, 348, 1932, 4096]
    contents = {
        1: b'\x00' * 4096,               # scratch
        2: build_weight_blob(),           # weights (tb-0)
        3: build_input_fp16_ncxhwx(),     # input
        4: b'\x00' * 512,                 # intermediate
        5: b'\x00' * 512,                 # output
        6: NET_DESC,                      # net_desc
        7: DEP_GRAPH,                     # dep_graph
        8: OP_LIST,                       # op_list
        9: SURF_LIST,                     # surf_list
        10: b'\x00' * 4096,               # scratch2
    }

    fd = os.open("/dev/dri/renderD128", os.O_RDWR)
    handles = []
    maps = []
    prime_fds = []

    try:
        for i, size in enumerate(sizes):
            c = nvdla_gem_create(size=size)
            ioctl(fd, GEM_CREATE, c)
            mo = nvdla_gem_map_offset(handle=c.handle)
            ioctl(fd, GEM_MMAP, mo)
            m = mmap.mmap(fd, (size + 4095) // 4096 * 4096, mmap.MAP_SHARED,
                          mmap.PROT_READ | mmap.PROT_WRITE, offset=mo.offset)
            m[:len(contents[i + 1])] = contents[i + 1]
            p_ = drm_prime_handle(handle=c.handle, flags=0x80000)
            ioctl(fd, PRIME_H2F, p_)
            handles.append(c.handle)
            maps.append(m)
            prime_fds.append(p_.fd)

        # Address slots → UMD handle (1-based):
        # slot 0: net_desc(6)   slot 5: net_desc(dup=6)
        # slot 1: weights(2)    slot 6: dep_graph(7)
        # slot 2: input(3)      slot 7: op_list(8)
        # slot 3: interm(4)     slot 8: surf_list(9)
        # slot 4: output(5)     slot 9: scratch2(10)
        umd_handles = [6, 2, 3, 4, 5, 6, 7, 8, 9, 10]
        n = len(umd_handles)
        addr_arr = (nvdla_mem_handle * n)()
        for i, uh in enumerate(umd_handles):
            addr_arr[i].handle = prime_fds[uh - 1]

        task = nvdla_ioctl_submit_task(
            num_addresses=n, timeout=0,
            address_list=ctypes.addressof(addr_arr),
        )
        submit = nvdla_submit_args(
            tasks=ctypes.addressof(task),
            num_tasks=1, flags=0, version=0,
        )
        ioctl(fd, NVDLA_SUBMIT, submit)

        # Read output
        out_idx = 4  # handle index for output buffer
        mo2 = nvdla_gem_map_offset(handle=handles[out_idx])
        ioctl(fd, GEM_MMAP, mo2)
        m2 = mmap.mmap(fd, (OUT_SURF_STRIDE + 4095) // 4096 * 4096, mmap.MAP_SHARED,
                       mmap.PROT_READ, offset=mo2.offset)
        out = bytes(m2[:OUT_SURF_STRIDE])
        m2.close()

        # Decode FP16 NCxHWx output
        halfs = struct.unpack_from(f'<{len(out)//2}H', out)
        nz = [(i, v) for i, v in enumerate(halfs) if v]
        print(f"CONV+ReLU: {len(nz)} non-zero FP16 values")
        for pos, val in nz[:12]:
            h = pos // (OUT_W * X)
            w = (pos % (OUT_W * X)) // X
            c = pos % X
            f = struct.unpack_from('<e', out, pos * BPE)[0]
            print(f"  [{h},{w},c{c}] = {f}")
        if len(nz) > 12:
            print(f"  ... ({len(nz) - 12} more)")

        # Check: kernel=2x2 all ones, stride=1, no padding. Every output
        # channel receives the same sum across all four input channels.
        print()
        print("Expected (for verification):")
        expected00 = sum((h * IN_W + w + 1 + c * 100)
                         for h in range(KH) for w in range(KW) for c in range(IN_C))
        expected10 = sum(((h + 1) * IN_W + w + 1 + c * 100)
                         for h in range(KH) for w in range(KW) for c in range(IN_C))
        for c in range(OUT_C):
            print(f"  output[0,0,c{c}] = {expected00} (FP16)")
            print(f"  output[1,0,c{c}] = {expected10} (FP16)")

        return 0 if nz else 1
    finally:
        for pf in prime_fds:
            try: os.close(pf)
            except: pass
        for m in maps:
            m.close()
        for h in handles:
            try: ioctl(fd, GEM_DESTROY, nvdla_gem_destroy(handle=h))
            except: pass
        os.close(fd)


if __name__ == "__main__":
    raise SystemExit(main())
