#!/usr/bin/env python3
"""Standalone SDP ADD test: replay test_Add on NVDLA via DRM ioctl.

No external files required — all blob data and input generation are self-contained.
Computes: output_pixel = input_pixel + 1.0 (FP16)
"""
from fcntl import ioctl
import ctypes, mmap, os, struct

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

NET_DESC = bytes.fromhex('060007000500ffffffffffffffff0000ffffffff0000ffffffffffff010001000000090000000000')

DEP_GRAPH = bytes.fromhex('0000000200000000ffff0100ffff0100ffff0100ffff0100ffff0100ffff0100ffff0100')

OP_LIST = bytes.fromhex('0202ffff00000000000000000001000000000000010202020000000200000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')

SURF_LIST = bytes.fromhex('00000100600400000700050001000000e0000000600400000000000000000200600400000700050001000000e00000006004000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000300600400000700050001000000e00000006004000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')

TB0_BLOB = bytes.fromhex('003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000003c000000000000000000000000000000000000000000000000000000000000')

def build_input_fp16_ncxhwx():
    """Build FP16 NCxHWx input for 7x5 image with values 0..34.

    NCxHWx layout (D_F16_CxHWx_x16_F):
      x=16, bpe=2, xStride=32, lineStride=224
      offset(h,w,c) = h * 224 + w * 32 + c * 2
    """
    buf = bytearray(1120)
    for h in range(5):
        for w in range(7):
            buf[h * 224 + w * 32 : h * 224 + w * 32 + 2] = struct.pack('<e', float(h * 7 + w))
    return bytes(buf)


def main():
    sizes = [4096, 1120, 40, 36, 116, 644, 4096, 1120, 1120]
    contents = {
        1: b'\x00' * 4096,
        2: TB0_BLOB,
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
            h = c.handle
            mo = nvdla_gem_map_offset(handle=h)
            ioctl(fd, GEM_MMAP, mo)
            m = mmap.mmap(fd, (size + 4095) // 4096 * 4096, mmap.MAP_SHARED,
                          mmap.PROT_READ | mmap.PROT_WRITE, offset=mo.offset)
            m[:len(contents[i + 1])] = contents[i + 1]
            p_ = drm_prime_handle(handle=h, flags=0x80000)
            ioctl(fd, PRIME_H2F, p_)
            handles.append(h)
            maps.append(m)
            prime_fds.append(p_.fd)
            print(f"gem h={h} size={size} prime_fd={p_.fd}")

        umd_handles = [3, 8, 2, 9, 3, 4, 5, 6, 7]
        n = len(umd_handles)
        addr_arr = (nvdla_mem_handle * n)()
        for i, uh in enumerate(umd_handles):
            addr_arr[i].handle = prime_fds[uh - 1]
            addr_arr[i].offset = 0

        task = nvdla_ioctl_submit_task(
            num_addresses=n, timeout=0,
            address_list=ctypes.addressof(addr_arr),
        )
        submit = nvdla_submit_args(
            tasks=ctypes.addressof(task),
            num_tasks=1, flags=0, version=0,
        )

        print("SUBMIT...")
        ioctl(fd, NVDLA_SUBMIT, submit)
        print("SUBMIT done")

        out_idx = 8
        out_sz = sizes[out_idx]
        mo2 = nvdla_gem_map_offset(handle=handles[out_idx])
        ioctl(fd, GEM_MMAP, mo2)
        m2 = mmap.mmap(fd, (out_sz + 4095) // 4096 * 4096, mmap.MAP_SHARED,
                       mmap.PROT_READ, offset=mo2.offset)
        out = bytes(m2[:min(out_sz, 160)])
        m2.close()

        nz = [i for i, b in enumerate(out) if b]
        print(f"output non-zero at: {nz}")
        if nz:
            halfs = struct.unpack_from('<80H', out[:160])
            print(f"first 40 FP16: {' '.join(f'0x{v:04x}' for v in halfs[:40])}")
            print("PASS: output non-zero")
            return 0
        else:
            print("FAIL: output all zero")
            return 1
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
