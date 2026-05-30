#!/usr/bin/env python3
"""Self-contained NVDLA SDP ADD runner.

Computes FP16 output = input + 1.0 using direct DRM/KMD submit. No loadable
file and no nvdla_runtime are used at execution time.
"""
from fcntl import ioctl
import ctypes
import glob
import mmap
import os
import signal
import struct
import sys

FEATURE_ATOMIC_SIZE = 16
BPE = 2

IN_W = 7
IN_H = 5
IN_C = 1

XSTRIDE = FEATURE_ATOMIC_SIZE * BPE
LINE_STRIDE = IN_W * XSTRIDE
SURF_STRIDE = IN_H * LINE_STRIDE

SDP_OP = 2
DLA_MEM_MC = 0
EVENT_OP_COMPLETED = 1
SUBMIT_TIMEOUT_SECS = int(os.environ.get("NVDLA_SUBMIT_TIMEOUT", "30"))


class nvdla_gem_create(ctypes.Structure):
    _fields_ = [
        ("handle", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
        ("size", ctypes.c_uint64),
    ]


class nvdla_gem_map_offset(ctypes.Structure):
    _fields_ = [
        ("handle", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32),
        ("offset", ctypes.c_uint64),
    ]


class nvdla_gem_destroy(ctypes.Structure):
    _fields_ = [("handle", ctypes.c_uint32)]


class drm_prime_handle(ctypes.Structure):
    _fields_ = [
        ("handle", ctypes.c_uint32),
        ("flags", ctypes.c_uint32),
        ("fd", ctypes.c_int32),
    ]


class nvdla_mem_handle(ctypes.Structure):
    _fields_ = [
        ("handle", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32),
        ("offset", ctypes.c_uint64),
    ]


class nvdla_ioctl_submit_task(ctypes.Structure):
    _fields_ = [
        ("num_addresses", ctypes.c_uint32),
        ("timeout", ctypes.c_uint32),
        ("address_list", ctypes.c_uint64),
    ]


class nvdla_submit_args(ctypes.Structure):
    _fields_ = [
        ("tasks", ctypes.c_uint64),
        ("num_tasks", ctypes.c_uint16),
        ("flags", ctypes.c_uint16),
        ("version", ctypes.c_uint32),
    ]


def _IOC(direction, type_, nr, size):
    return (direction << 30) | (type_ << 8) | nr | (size << 16)


def _IOWR(type_, nr, size):
    return _IOC(3, type_, nr, size)


DRM_IOCTL_BASE = ord("d")
GEM_CREATE = _IOWR(DRM_IOCTL_BASE, 0x41, ctypes.sizeof(nvdla_gem_create))
GEM_MMAP = _IOWR(DRM_IOCTL_BASE, 0x42, ctypes.sizeof(nvdla_gem_map_offset))
GEM_DESTROY = _IOWR(DRM_IOCTL_BASE, 0x43, ctypes.sizeof(nvdla_gem_destroy))
PRIME_HANDLE_TO_FD = _IOWR(DRM_IOCTL_BASE, 0x2d, ctypes.sizeof(drm_prime_handle))
NVDLA_SUBMIT = _IOWR(DRM_IOCTL_BASE, 0x40, ctypes.sizeof(nvdla_submit_args))


class NvdlaBO:
    __slots__ = ("handle", "size", "map", "prime_fd")

    def __init__(self, handle, size, map_, prime_fd):
        self.handle = int(handle)
        self.size = int(size)
        self.map = map_
        self.prime_fd = int(prime_fd)


def open_nvdla_device():
    path = os.environ.get("NVDLA_DEVICE")
    if path:
        return os.open(path, os.O_RDWR)
    candidates = ["/dev/dri/renderD128"] + sorted(glob.glob("/dev/dri/renderD*"))
    for candidate in dict.fromkeys(candidates):
        try:
            return os.open(candidate, os.O_RDWR)
        except OSError:
            pass
    raise FileNotFoundError("No NVDLA DRM render node found")


def _align(x, a):
    return ((x + a - 1) // a) * a


def mem_allocate(fd, size, data=b""):
    bo = nvdla_gem_create(size=size)
    ioctl(fd, GEM_CREATE, bo)
    map_offset = nvdla_gem_map_offset(handle=bo.handle)
    ioctl(fd, GEM_MMAP, map_offset)
    map_ = mmap.mmap(
        fd, _align(size, 4096), mmap.MAP_SHARED,
        mmap.PROT_READ | mmap.PROT_WRITE, offset=map_offset.offset,
    )
    if data:
        map_[:len(data)] = data
    prime = drm_prime_handle(handle=bo.handle, flags=0x80000)
    ioctl(fd, PRIME_HANDLE_TO_FD, prime)
    return NvdlaBO(bo.handle, size, map_, prime.fd)


def mem_destroy(fd, bo):
    try:
        os.close(bo.prime_fd)
    except OSError:
        pass
    bo.map.close()
    try:
        ioctl(fd, GEM_DESTROY, nvdla_gem_destroy(handle=bo.handle))
    except OSError:
        pass


def nvdla_submit(fd, bos, address_slots):
    mem_handles = (nvdla_mem_handle * len(address_slots))()
    for i, slot in enumerate(address_slots):
        mem_handles[i].handle = bos[slot - 1].prime_fd

    task = nvdla_ioctl_submit_task(
        num_addresses=len(address_slots),
        timeout=0,
        address_list=ctypes.addressof(mem_handles),
    )
    submit = nvdla_submit_args(
        tasks=ctypes.addressof(task),
        num_tasks=1,
        flags=0,
        version=0,
    )
    def handle_timeout(signum, frame):
        raise TimeoutError("timed out in NVDLA_SUBMIT")

    previous_handler = signal.signal(signal.SIGALRM, handle_timeout)
    signal.alarm(SUBMIT_TIMEOUT_SECS)
    try:
        return ioctl(fd, NVDLA_SUBMIT, submit)
    finally:
        signal.alarm(0)
        signal.signal(signal.SIGALRM, previous_handler)


def mem_read(fd, bo, size):
    map_offset = nvdla_gem_map_offset(handle=bo.handle)
    ioctl(fd, GEM_MMAP, map_offset)
    with mmap.mmap(
        fd, _align(size, 4096), mmap.MAP_SHARED,
        mmap.PROT_READ, offset=map_offset.offset,
    ) as map_:
        return bytes(map_[:size])


def pack_net_desc(operation_desc_index, surface_desc_index, dependency_graph_index,
                  op_head, num_operations, num_addresses, lut_data_index=-1,
                  roi_array_index=-1, surface_index=-1, stat_list_index=-1,
                  num_rois=1, num_luts=0, input_layer=0, dynamic_roi=0):
    return struct.pack(
        "<8h6h4HhBB",
        operation_desc_index, surface_desc_index, dependency_graph_index,
        lut_data_index, roi_array_index, surface_index, stat_list_index, 0,
        *op_head, num_rois, num_operations, num_luts, num_addresses,
        input_layer, dynamic_roi, 0,
    )


def pack_consumer(index=-1, event=EVENT_OP_COMPLETED):
    return struct.pack("<hBB", index, event, 0)


def pack_common_op(
    index, op_type, dependency_count, consumers,
    fused_parent=(-1, EVENT_OP_COMPLETED),
):
    return (
        struct.pack("<hbBB3x", index, 0, op_type, dependency_count) +
        b"".join(pack_consumer(*consumer) for consumer in consumers) +
        pack_consumer(*fused_parent)
    )


def pack_cvt(scale=0, truncate=0, enable=0, offset=0):
    return struct.pack("<hBBi", scale, truncate, enable, offset)


def pack_sdp_unit(enable=0, alu_type=0, op_type=0, mode=0, act=0,
                  shift=0, truncate=0, precision=0, alu_operand=0,
                  mul_operand=0, alu_cvt=(0, 0, 0, 0), mul_cvt=(0, 0, 0, 0)):
    return (
        struct.pack("<BBBBBBBBii", enable, alu_type, op_type, mode, act,
                    shift, truncate, precision, alu_operand, mul_operand) +
        pack_cvt(*alu_cvt) + pack_cvt(*mul_cvt)
    )


def pack_sdp_op():
    return (
        struct.pack("<BBh", 2, 2, -1) +
        pack_cvt(0, 0, 0, 0) +
        struct.pack("<BBHI", 0, 1, 0, 0) +
        pack_sdp_unit(1, 2, 2, 2, 0, 0, 0, 2, 0, 0) +
        pack_sdp_unit() +
        pack_sdp_unit()
    )


def pack_cube(mem_type=0, address=0, size=0, width=0, height=0, channel=0,
              line_stride=0, surf_stride=0, plane_stride=0):
    return struct.pack(
        "<HhIHHHHIII", mem_type, address, size, width, height,
        channel, 0, line_stride, surf_stride, plane_stride,
    )


def pack_sdp_surface():
    surface = b"".join([
        pack_cube(
            DLA_MEM_MC, 1, SURF_STRIDE, IN_W, IN_H, IN_C,
            LINE_STRIDE, SURF_STRIDE,
        ),
        pack_cube(
            DLA_MEM_MC, 2, SURF_STRIDE, IN_W, IN_H, IN_C,
            LINE_STRIDE, SURF_STRIDE,
        ),
        pack_cube(),
        pack_cube(),
        pack_cube(
            DLA_MEM_MC, 3, SURF_STRIDE, IN_W, IN_H, IN_C,
            LINE_STRIDE, SURF_STRIDE,
        ),
    ])
    return surface + b"\x00" * (644 - len(surface))


def build_descriptors():
    net_desc = pack_net_desc(6, 7, 5, [-1, -1, 0, -1, -1, -1], 1, 9)
    dep_graph = pack_common_op(0, SDP_OP, 0, [(-1, EVENT_OP_COMPLETED)] * 6)
    op_list = pack_sdp_op()
    surf_list = pack_sdp_surface()
    assert (len(net_desc), len(dep_graph), len(op_list), len(surf_list)) == (40, 36, 116, 644)
    return net_desc, dep_graph, op_list, surf_list


def pack_add_operand():
    raw = bytearray(SURF_STRIDE)
    for h in range(IN_H):
        for w in range(IN_W):
            off = h * LINE_STRIDE + w * XSTRIDE
            raw[off:off + BPE] = struct.pack("<e", 1.0)
    return bytes(raw)


def pack_input():
    raw = bytearray(SURF_STRIDE)
    for h in range(IN_H):
        for w in range(IN_W):
            off = h * LINE_STRIDE + w * XSTRIDE
            raw[off:off + BPE] = struct.pack("<e", float(h * IN_W + w))
    return bytes(raw)


def read_output(output_map):
    values = []
    for h in range(IN_H):
        row = []
        for w in range(IN_W):
            off = h * LINE_STRIDE + w * XSTRIDE
            row.append(struct.unpack_from("<e", output_map, off)[0])
        values.append(row)
    return values


def expected_output():
    return [[float(h * IN_W + w + 1) for w in range(IN_W)] for h in range(IN_H)]


def run_add():
    net_desc, dep_graph, op_list, surf_list = build_descriptors()
    sizes = [
        4096, SURF_STRIDE, 40, 36, 116, 644, 4096,
        SURF_STRIDE, SURF_STRIDE,
    ]
    contents = [
        b"\x00" * 4096,
        pack_add_operand(),
        net_desc,
        dep_graph,
        op_list,
        surf_list,
        b"\x00" * 4096,
        pack_input(),
        b"\x00" * SURF_STRIDE,
    ]
    address_slots = [3, 8, 2, 9, 3, 4, 5, 6, 7]

    fd = open_nvdla_device()
    bos = []
    try:
        bos = [mem_allocate(fd, size, data) for size, data in zip(sizes, contents)]
        print("SUBMIT...", flush=True)
        ret = nvdla_submit(fd, bos, address_slots)
        print("SUBMIT done", flush=True)
        result = read_output(mem_read(fd, bos[8], SURF_STRIDE))
    finally:
        for bo in bos:
            mem_destroy(fd, bo)
        os.close(fd)

    expected = expected_output()
    diff = max(
        abs(result[h][w] - expected[h][w])
        for h in range(IN_H)
        for w in range(IN_W)
    )
    ok = diff == 0.0
    print(f"SUBMIT ret={ret}")
    print(
        f"simple_add NPU={sum(result, [])} expected={sum(expected, [])} "
        f"{'PASS' if ok else 'FAIL'} (max_diff={diff})"
    )
    return ok


if __name__ == "__main__":
    sys.exit(0 if run_add() else 1)
