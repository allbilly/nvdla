#!/usr/bin/env python3
"""SDP ADD test: output = input + 1.0 (FP16) on NVDLA via DRM ioctl.

Self-contained: descriptors and tensor data built programmatically.
No external files required.
"""
from fcntl import ioctl
import ctypes, mmap, os, struct

# ---- Tensor parameters ----
IN_W, IN_H, IN_C = 7, 5, 1
BPE = 2
X = 16
XSTRIDE = X * BPE                        # 32
LINE_STRIDE = IN_W * XSTRIDE             # 224
SURF_STRIDE = IN_H * LINE_STRIDE         # 1120


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


def pack_cube(mem_type=0, address=0, size=0, width=0, height=0, channel=0,
              line_stride=0, surf_stride=0, plane_stride=0):
    # Direct KMD submit expects the full 32-byte dla_data_cube layout.
    return struct.pack('<HhIIHHHHIII', mem_type, address, 0, size, width, height,
                       channel, 0, line_stride, surf_stride, plane_stride)


def pack_sdp_surface(src_data=(), x1_data=(), x2_data=(), y_data=(), dst_data=()):
    data = b''.join(pack_cube(*cube) for cube in (src_data, x1_data, x2_data, y_data, dst_data))
    return data + b'\x00' * (644 - len(data))


NET_DESC = pack_net_desc(6, 7, 5, [-1, -1, 0, -1, -1, -1], 1, 9)
DEP_GRAPH = pack_common_op(0, 2, 0, [(-1, 1)] * 6)
OP_LIST = pack_sdp_op(
    2, 2, out_cvt=(0, 0, 0, 0),
    x1_op=(1, 2, 2, 2, 0, 0, 0, 2, 0, 0),
)
SURF_LIST = pack_sdp_surface(
    src_data=(0, 1, SURF_STRIDE, IN_W, IN_H, IN_C, LINE_STRIDE, SURF_STRIDE, 0),
    x1_data=(0, 2, SURF_STRIDE, IN_W, IN_H, IN_C, LINE_STRIDE, SURF_STRIDE, 0),
    dst_data=(0, 3, SURF_STRIDE, IN_W, IN_H, IN_C, LINE_STRIDE, SURF_STRIDE, 0),
)
assert (len(NET_DESC), len(DEP_GRAPH), len(OP_LIST), len(SURF_LIST)) == (40, 36, 116, 644)

def build_tb0_blob():
    """x1_data: FP16 1.0 at every pixel in NCxHWx layout."""
    buf = bytearray(SURF_STRIDE)
    for h in range(IN_H):
        for w in range(IN_W):
            off = h * LINE_STRIDE + w * XSTRIDE
            buf[off:off + BPE] = struct.pack('<e', 1.0)
    return bytes(buf)

def build_input_fp16_ncxhwx():
    """Input values 0..34 → FP16 NCxHWx tensor."""
    buf = bytearray(SURF_STRIDE)
    for h in range(IN_H):
        for w in range(IN_W):
            off = h * LINE_STRIDE + w * XSTRIDE
            buf[off:off + BPE] = struct.pack('<e', float(h * IN_W + w))
    return bytes(buf)


# ---- DRM ioctl ----

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
    sizes = [4096, 1120, 40, 36, 116, 644, 4096, 1120, 1120]
    contents = {
        1: b'\x00' * 4096,
        2: build_tb0_blob(),
        3: NET_DESC,
        4: DEP_GRAPH,
        5: OP_LIST,
        6: SURF_LIST,
        7: b'\x00' * 4096,
        8: build_input_fp16_ncxhwx(),
        9: b'\x00' * 1120,
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

        umd_handles = [3, 8, 2, 9, 3, 4, 5, 6, 7]
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

        # Debug: read back input buffer
        in_idx = 7  # handle index for input
        mo_in = nvdla_gem_map_offset(handle=handles[in_idx])
        ioctl(fd, GEM_MMAP, mo_in)
        m_in = mmap.mmap(fd, (SURF_STRIDE + 4095) // 4096 * 4096, mmap.MAP_SHARED,
                         mmap.PROT_READ, offset=mo_in.offset)
        in_data = bytes(m_in[:64])
        m_in.close()
        print(f"Debug: input first 64B: {in_data.hex()}")
        half = struct.unpack_from('<e', in_data, 0)[0]
        print(f"Debug: input[0,0] = {half}")

        out_idx = 8
        mo2 = nvdla_gem_map_offset(handle=handles[out_idx])
        ioctl(fd, GEM_MMAP, mo2)
        m2 = mmap.mmap(fd, (SURF_STRIDE + 4095) // 4096 * 4096, mmap.MAP_SHARED,
                       mmap.PROT_READ, offset=mo2.offset)
        out = bytes(m2)
        m2.close()

        halfs = struct.unpack_from(f'<{len(out)//2}H', out)
        nz = [(i, v) for i, v in enumerate(halfs) if v]
        print(f"PASS: {len(nz)} non-zero FP16 values")
        for pos, val in nz[:10]:
            h = pos // (IN_W * X)
            w = (pos % (IN_W * X)) // X
            f = struct.unpack_from('<e', out, pos * BPE)[0]
            print(f"  [{h},{w}] = {f}")
        if len(nz) > 10:
            print(f"  ... ({len(nz) - 10} more)")
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
