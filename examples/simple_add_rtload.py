#!/usr/bin/env python3
"""Replay test_Add via DRM ioctl with blobs extracted from the .nvdla loadable."""
from fcntl import ioctl
import ctypes, mmap, os, struct, sys

sys.path.insert(0, '/mnt/vp')
import parse as p

DRM_IOCTL_BASE = ord("d")
DRM_COMMAND_BASE = 0x40

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

def loadable_blob(l, name):
    for b in l['blob_list']:
        if b['name'] == name:
            return b['data']
    return None

def build_input_fp16_ncxhwx():
    """Build FP16 NCxHWx input buffer for 7x5 PGM with values 0..34.
    
    NCxHWx layout (D_F16_CxHWx_x16_F):
      x=16 channels per atom, bpe=2
      xStride = 32 bytes, lineStride = 224 bytes
      offset(h,w,c) = h * 224 + w * 32 + c * 2
    """
    buf = bytearray(1120)
    for h in range(5):
        for w in range(7):
            pixel_val = h * 7 + w
            fp16 = struct.pack('<e', float(pixel_val))
            off = h * 224 + w * 32  # c=0
            buf[off:off+2] = fp16
    return bytes(buf)

def main():
    with open('/mnt/vp/test_Add.nvdla', 'rb') as f:
        l = p.parse_loadable(f.read())

    net_blob = loadable_blob(l, 'task-0-addr0')
    dep_blob = loadable_blob(l, 'task-0-dep_graph')
    op_blob  = loadable_blob(l, 'task-0-op_list')
    surf_blob = loadable_blob(l, 'task-0-surf_list')
    tb0_blob = loadable_blob(l, 'tb-0')

    input_pixels = build_input_fp16_ncxhwx()

    # UMD creates GEMs in this order (1-based):
    # 1:4096 (scratch)  2:1120 (x1_data=tb0)  3:40 (net desc)
    # 4:36 (dep graph)  5:116 (op list)  6:644 (surf list)
    # 7:4096 (scratch2)  8:1120 (input)  9:1120 (output)
    sizes = [4096, 1120, 40, 36, 116, 644, 4096, 1120, 1120]
    contents = {
        1: b'\x00' * 4096,
        2: tb0_blob,
        3: net_blob,
        4: dep_blob,
        5: op_blob,
        6: surf_blob,
        7: b'\x00' * 4096,
        8: input_pixels,
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
            data = contents[i + 1]
            m[:len(data)] = data
            p_ = drm_prime_handle(handle=h, flags=0x80000)
            ioctl(fd, PRIME_H2F, p_)
            handles.append(h)
            maps.append(m)
            prime_fds.append(p_.fd)
            print(f"gem h={h} size={size} prime_fd={p_.fd}")

        # Address list: task address_list [4,1,2,3,4,5,6,7,8]
        # maps to UMD handles: [3, 8, 2, 9, 3, 4, 5, 6, 7]
        umd_handles = [3, 8, 2, 9, 3, 4, 5, 6, 7]
        n = len(umd_handles)
        addr_arr = (nvdla_mem_handle * n)()
        for i, uh in enumerate(umd_handles):
            addr_arr[i].handle = prime_fds[uh - 1]
            addr_arr[i].offset = 0
            print(f"addr[{i}] fd={prime_fds[uh - 1]}")

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

        out_idx = 8  # UMD handle 9 = output tensor = index 8
        out_sz = sizes[out_idx]
        mo2 = nvdla_gem_map_offset(handle=handles[out_idx])
        ioctl(fd, GEM_MMAP, mo2)
        m2 = mmap.mmap(fd, (out_sz + 4095) // 4096 * 4096, mmap.MAP_SHARED,
                       mmap.PROT_READ, offset=mo2.offset)
        out = bytes(m2[:min(out_sz, 160)])
        m2.close()
        nz = [i for i, b in enumerate(out) if b]
        print(f"output non-zero at: {nz}")
        hexout = ' '.join(f'{b:02x}' for b in out[:80])
        print(f"output hex: {hexout}")
        if nz:
            print("Test pass (output non-zero)")
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
