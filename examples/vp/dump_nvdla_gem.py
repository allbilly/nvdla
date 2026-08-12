#!/usr/bin/env python3
import ctypes
import fcntl
import mmap
import os
import struct
import sys

DRM_IOCTL_PRIME_FD_TO_HANDLE = 0xC00C642E
DRM_IOCTL_NVDLA_GEM_MMAP = 0xC0106442


def ioctl_mut(fd, req, data):
    buf = bytearray(data)
    fcntl.ioctl(fd, req, buf, True)
    return bytes(buf)


def find_pid():
    matches = []
    for name in os.listdir("/proc"):
        if not name.isdigit():
            continue
        try:
            with open(f"/proc/{name}/comm") as f:
                if f.read().strip() == "nvdla_runtime":
                    matches.append(int(name))
        except OSError:
            pass
    if not matches:
        raise RuntimeError("nvdla_runtime pid not found")
    return max(matches)


def dump_one(drm_fd, target_pid, prime_fd, size, out_path):
    dup_fd = os.open(f"/proc/{target_pid}/fd/{prime_fd}", os.O_RDONLY)
    try:
        # struct drm_prime_handle { __u32 handle; __u32 flags; __s32 fd; }
        data = ioctl_mut(drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, struct.pack("<IIi", 0, 0, dup_fd))
        handle, _flags, _fd = struct.unpack("<IIi", data)
        # struct nvdla_gem_map_offset_args { __u32 handle; __u32 flags; __u64 offset; }
        data = ioctl_mut(drm_fd, DRM_IOCTL_NVDLA_GEM_MMAP, struct.pack("<IIQ", handle, 0, 0))
        _handle, _flags, offset = struct.unpack("<IIQ", data)
        with mmap.mmap(drm_fd, size, flags=mmap.MAP_SHARED, prot=mmap.PROT_READ | mmap.PROT_WRITE, offset=offset) as mm:
            blob = mm[:]
        with open(out_path, "wb") as f:
            f.write(blob)
        print(f"dumped prime_fd={prime_fd} handle={handle} size={len(blob)} offset=0x{offset:x} to {out_path}")
    finally:
        os.close(dup_fd)


def main():
    if len(sys.argv) < 4 or len(sys.argv[2:]) % 3 != 0:
        print("usage: dump_nvdla_gem.py DRM_DEVICE prime_fd size out [prime_fd size out ...]", file=sys.stderr)
        return 2
    target_pid = find_pid()
    with open(sys.argv[1], "r+b", buffering=0) as drm:
        drm_fd = drm.fileno()
        args = sys.argv[2:]
        for i in range(0, len(args), 3):
            dump_one(drm_fd, target_pid, int(args[i]), int(args[i + 1]), args[i + 2])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
