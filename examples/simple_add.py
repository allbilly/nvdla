#!/usr/bin/env python3
"""Experimental NVDLA test_Add ioctl replay scaffold.

This mirrors the Linux UMD ioctl sequence used by nvdla_runtime:
GEM_CREATE -> PRIME_HANDLE_TO_FD -> GEM_MMAP for each runtime allocation,
then NVDLA_SUBMIT.  It is intentionally small and RK3588-simple_add.py-like,
but it still needs captured loadable-populated buffer bytes to be a complete
byte-for-byte replay.
"""
from __future__ import annotations

from fcntl import ioctl
import ctypes
import mmap
import os
import struct


DRM_IOCTL_BASE = ord("d")
DRM_COMMAND_BASE = 0x40
DRM_CLOEXEC = 0x80000


def _IOC(direction: int, type_: int, nr: int, size: int) -> int:
    return (direction << 30) | (size << 16) | (type_ << 8) | nr


def _IOWR(type_: int, nr: int, size: int) -> int:
    return _IOC(3, type_, nr, size)


class nvdla_gem_create_args(ctypes.Structure):
    _fields_ = [("handle", ctypes.c_uint32), ("flags", ctypes.c_uint32), ("size", ctypes.c_uint64)]


class nvdla_gem_map_offset_args(ctypes.Structure):
    _fields_ = [("handle", ctypes.c_uint32), ("reserved", ctypes.c_uint32), ("offset", ctypes.c_uint64)]


class nvdla_gem_destroy_args(ctypes.Structure):
    _fields_ = [("handle", ctypes.c_uint32)]


class drm_prime_handle(ctypes.Structure):
    _fields_ = [("handle", ctypes.c_uint32), ("flags", ctypes.c_uint32), ("fd", ctypes.c_int32)]


class nvdla_mem_handle(ctypes.Structure):
    _fields_ = [("handle", ctypes.c_uint32), ("reserved", ctypes.c_uint32), ("offset", ctypes.c_uint64)]


class nvdla_ioctl_submit_task(ctypes.Structure):
    _fields_ = [("num_addresses", ctypes.c_uint32), ("timeout", ctypes.c_uint32), ("address_list", ctypes.c_uint64)]


class nvdla_submit_args(ctypes.Structure):
    _fields_ = [("tasks", ctypes.c_uint64), ("num_tasks", ctypes.c_uint16), ("flags", ctypes.c_uint16), ("version", ctypes.c_uint32)]


DRM_IOCTL_NVDLA_SUBMIT = _IOWR(DRM_IOCTL_BASE, DRM_COMMAND_BASE + 0x00, ctypes.sizeof(nvdla_submit_args))
DRM_IOCTL_NVDLA_GEM_CREATE = _IOWR(DRM_IOCTL_BASE, DRM_COMMAND_BASE + 0x01, ctypes.sizeof(nvdla_gem_create_args))
DRM_IOCTL_NVDLA_GEM_MMAP = _IOWR(DRM_IOCTL_BASE, DRM_COMMAND_BASE + 0x02, ctypes.sizeof(nvdla_gem_map_offset_args))
DRM_IOCTL_NVDLA_GEM_DESTROY = _IOWR(DRM_IOCTL_BASE, DRM_COMMAND_BASE + 0x03, ctypes.sizeof(nvdla_gem_destroy_args))
DRM_IOCTL_PRIME_HANDLE_TO_FD = _IOWR(DRM_IOCTL_BASE, 0x2d, ctypes.sizeof(drm_prime_handle))


class Allocation:
    def __init__(self, fd: int, size: int):
        self.size = size
        self.create = nvdla_gem_create_args(size=size)
        ioctl(fd, DRM_IOCTL_NVDLA_GEM_CREATE, self.create)
        self.prime = drm_prime_handle(handle=self.create.handle, flags=DRM_CLOEXEC)
        ioctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, self.prime)
        self.map_offset = nvdla_gem_map_offset_args(handle=self.create.handle)
        ioctl(fd, DRM_IOCTL_NVDLA_GEM_MMAP, self.map_offset)
        self.map = mmap.mmap(fd, size, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE, offset=self.map_offset.offset)
        print(
            f"alloc size={size} handle={self.create.handle} prime_fd={self.prime.fd} "
            f"mmap_offset=0x{self.map_offset.offset:x}"
        )

    def close(self, fd: int) -> None:
        self.map.close()
        os.close(self.prime.fd)
        ioctl(fd, DRM_IOCTL_NVDLA_GEM_DESTROY, nvdla_gem_destroy_args(handle=self.create.handle))


# Captured strace showed 10 GEM allocation groups before submit for test_Add.
# Sizes are placeholders until the ioctl argument bytes are captured by an
# LD_PRELOAD/KMD hook.  The allocation count/order is useful for validating ABI.
CAPTURED_ALLOC_SIZES = [4096] * 10


def main() -> int:
    fd = os.open("/dev/dri/renderD128", os.O_RDWR)
    allocs: list[Allocation] = []
    try:
        for size in CAPTURED_ALLOC_SIZES:
            allocs.append(Allocation(fd, size))

        print("NVDLA ioctl numbers:")
        print(f"  GEM_CREATE=0x{DRM_IOCTL_NVDLA_GEM_CREATE:08x}")
        print(f"  GEM_MMAP=0x{DRM_IOCTL_NVDLA_GEM_MMAP:08x}")
        print(f"  SUBMIT=0x{DRM_IOCTL_NVDLA_SUBMIT:08x}")
        print("Not submitting: captured task/address-list bytes are still required.")
        return 77
    finally:
        for alloc in reversed(allocs):
            alloc.close(fd)
        os.close(fd)


if __name__ == "__main__":
    raise SystemExit(main())
