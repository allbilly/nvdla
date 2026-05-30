#!/usr/bin/env python3
"""Self-contained nv_small NVDLA 1x1x8 convolution runner.

The descriptors and weights are embedded from a validated nv_small loadable, so
execution does not need nvdla_runtime, a .nvdla file, or a parser module.
"""
import ctypes
import glob
import mmap
import os
import struct
import sys
from fcntl import ioctl


NET_DESC = bytes.fromhex(
    "060007000500ffffffffffffffff0000ffff00000100ffffffffffff01000200"
    "0000090000000000"
)
DEP_GRAPH = bytes.fromhex(
    "0000000101000000ffff0100ffff010001000200ffff0100ffff0100ffff0100"
    "ffff01000100000201000000ffff0100ffff0100ffff0100ffff0100ffff0100"
    "ffff010000000300"
)
OP_LIST = bytes.fromhex(
    "0000000000000100240001000000000000000000010001010000000000000100"
    "0100010008000100010008000100010010000000000000000000000000010100"
    "0000000101000000020200000000000000000000010000010000000000000000"
    "00000000000000000000000000000000000000000202ffff0100000100000000"
    "0001000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000"
)
SURF_LIST = bytes.fromhex(
    "000001008000000001000100080000000000000000000000000000000200ffff"
    "0000000000000000000000000000000000000000000000000200ffff00000000"
    "0000000000000000000000000000000000000000000002002000000001000100"
    "080000002000000020000000000000000200ffff200000000100010001000000"
    "2000000020000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "000000000200ffff200000000100010001000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000003002000000001000100"
    "0100000020000000200000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000"
)
WEIGHTS = bytes.fromhex(
    "003c004000420044004500460047004800000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
)

assert (len(NET_DESC), len(DEP_GRAPH), len(OP_LIST), len(SURF_LIST), len(WEIGHTS)) == (40, 72, 232, 1288, 128)


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


def _ioc(direction, type_, nr, size):
    return (direction << 30) | (type_ << 8) | nr | (size << 16)


def _iowr(type_, nr, size):
    return _ioc(3, type_, nr, size)


DRM_IOCTL_BASE = ord("d")
GEM_CREATE = _iowr(DRM_IOCTL_BASE, 0x41, ctypes.sizeof(nvdla_gem_create))
GEM_MMAP = _iowr(DRM_IOCTL_BASE, 0x42, ctypes.sizeof(nvdla_gem_map_offset))
GEM_DESTROY = _iowr(DRM_IOCTL_BASE, 0x43, ctypes.sizeof(nvdla_gem_destroy))
PRIME_HANDLE_TO_FD = _iowr(DRM_IOCTL_BASE, 0x2D, ctypes.sizeof(drm_prime_handle))
NVDLA_SUBMIT = _iowr(DRM_IOCTL_BASE, 0x40, ctypes.sizeof(nvdla_submit_args))


def align(x, a):
    return ((x + a - 1) // a) * a


def open_nvdla_device():
    path = os.environ.get("NVDLA_DEVICE")
    if path:
        return os.open(path, os.O_RDWR)
    for candidate in dict.fromkeys(["/dev/dri/renderD128"] + sorted(glob.glob("/dev/dri/renderD*"))):
        try:
            return os.open(candidate, os.O_RDWR)
        except OSError:
            pass
    raise FileNotFoundError("No NVDLA DRM render node found")


def create_bo(fd, size, data=b""):
    create = nvdla_gem_create(size=size)
    ioctl(fd, GEM_CREATE, create)
    map_offset = nvdla_gem_map_offset(handle=create.handle)
    ioctl(fd, GEM_MMAP, map_offset)
    map_ = mmap.mmap(fd, align(size, 4096), mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE, offset=map_offset.offset)
    if data:
        map_[:len(data)] = data
    prime = drm_prime_handle(handle=create.handle, flags=0x80000)
    ioctl(fd, PRIME_HANDLE_TO_FD, prime)
    return {"handle": create.handle, "prime_fd": prime.fd, "map": map_, "size": size}


def destroy_bo(fd, bo):
    try:
        os.close(bo["prime_fd"])
    except OSError:
        pass
    bo["map"].close()
    try:
        ioctl(fd, GEM_DESTROY, nvdla_gem_destroy(handle=bo["handle"]))
    except OSError:
        pass


def submit(fd, bos, address_slots):
    addresses = (nvdla_mem_handle * len(address_slots))()
    for i, slot in enumerate(address_slots):
        addresses[i].handle = bos[slot - 1]["prime_fd"]
        addresses[i].offset = 0
    task = nvdla_ioctl_submit_task(num_addresses=len(address_slots), timeout=0, address_list=ctypes.addressof(addresses))
    args = nvdla_submit_args(tasks=ctypes.addressof(task), num_tasks=1, flags=0, version=0)
    return ioctl(fd, NVDLA_SUBMIT, args)


def build_input():
    data = bytearray(32)
    data[:2] = struct.pack("<e", 56.0)
    return bytes(data)


def run():
    # UMD allocation order captured from the working nv_small loadable run.
    sizes = [4096, 128, 40, 72, 232, 1288, 4096, 32, 32]
    contents = [
        b"\x00" * 4096,
        WEIGHTS,
        NET_DESC,
        DEP_GRAPH,
        OP_LIST,
        SURF_LIST,
        b"\x00" * 4096,
        build_input(),
        b"\x00" * 32,
    ]
    address_slots = [3, 8, 2, 9, 3, 4, 5, 6, 7]

    fd = open_nvdla_device()
    bos = []
    try:
        bos = [create_bo(fd, size, data) for size, data in zip(sizes, contents)]
        ret = submit(fd, bos, address_slots)
        output = bytes(bos[8]["map"][:32])
    finally:
        for bo in bos:
            destroy_bo(fd, bo)
        os.close(fd)

    result = struct.unpack_from("<e", output, 0)[0]
    decoded = [struct.unpack_from("<e", output, i)[0] for i in range(0, len(output), 2)]
    ok = result == 56.0
    print(f"SUBMIT ret={ret}")
    print(
        f"conv_1x1x8 NPU={result} expected=56.0 {'PASS' if ok else 'FAIL'} "
        f"decoded={decoded} raw={output.hex()}"
    )
    return ok


if __name__ == "__main__":
    sys.exit(0 if run() else 1)
