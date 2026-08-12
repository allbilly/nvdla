#!/usr/bin/env python3
"""Self-contained nv_small NVDLA replay for b1_c144_h28_w28_oc32 1x1 CONV.

The descriptors and weights are embedded from:
ref/onnc-tutorial/models/conv2d_b1_c144_h28_w28_oc32_wic144_k1x1_g1/out_nv_small.nvdla
"""
import ctypes
import glob
import math
import mmap
import os
import struct
import sys
from fcntl import ioctl


IN_C = 144
IN_H = 28
IN_W = 28
OUT_C = 32
OUT_H = 28
OUT_W = 28
ATOM_C = 16
BPE = 2
DLA_MEM_MC = 0
DLA_MEM_HW = 2
CONV_OP = 1
SDP_OP = 2
EVENT_OP_COMPLETED = 1
EVENT_OP_PROGRAMMED = 2
EVENT_OP_ENABLED = 3

IN_CGROUPS = (IN_C + ATOM_C - 1) // ATOM_C
OUT_CGROUPS = (OUT_C + ATOM_C - 1) // ATOM_C
IN_LINE_STRIDE = IN_W * ATOM_C * BPE
IN_SURF_STRIDE = IN_H * IN_LINE_STRIDE
INPUT_BYTES = IN_CGROUPS * IN_SURF_STRIDE
OUT_LINE_STRIDE = OUT_W * ATOM_C * BPE
OUT_SURF_STRIDE = OUT_H * OUT_LINE_STRIDE
OUTPUT_BYTES = OUT_CGROUPS * OUT_SURF_STRIDE
WEIGHT_BYTES = 9216


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


def pack_common_op(index, op_type, dependency_count, consumers,
                   fused_parent=(-1, EVENT_OP_COMPLETED)):
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
        pack_cvt(1, 0, 1, 0) +
        struct.pack("<BBHI", 0, 1, 0, 0) +
        pack_sdp_unit() +
        pack_sdp_unit() +
        pack_sdp_unit()
    )


def pack_conv_op():
    op = (
        struct.pack("<BBBBBBH", 0, 0, 0, 0, 0, 0, IN_C * 7 // ATOM_C) +
        struct.pack("<BBH", 36, 0, 1) +
        b"\x00" * 8 +
        struct.pack("<BBBB", 1, 0, IN_CGROUPS - 2, 1) +
        struct.pack("<I", 0) +
        struct.pack("<BBH", 0, 0, OUT_H) +
        struct.pack("<HHHHHHHH", IN_W, IN_H, IN_C, 1, 1, IN_C, OUT_W, OUT_H) +
        struct.pack("<I", IN_C * BPE) +
        struct.pack("<hhhh", 0, 0, 0, 0) +
        struct.pack("<BBBBBBBBB", 0, 1, 1, 0, 0, 0, 0, 1, 1) +
        b"\x00" * 3 +
        struct.pack("<BBB", 2, 2, 0) +
        struct.pack("<h", 0) +
        pack_cvt(0, 0, 0, 0x01000000) +
        pack_cvt(0, 1, 0, 0)
    )
    return op + b"\x00" * (116 - len(op))


NET_DESC = pack_net_desc(6, 7, 5, [-1, 0, 1, -1, -1, -1], 2, 9)
DEP_GRAPH = b"".join([
    pack_common_op(
        0, CONV_OP, 1,
        [(-1, EVENT_OP_COMPLETED), (-1, EVENT_OP_COMPLETED),
         (1, EVENT_OP_PROGRAMMED), (-1, EVENT_OP_COMPLETED),
         (-1, EVENT_OP_COMPLETED), (-1, EVENT_OP_COMPLETED)],
    ),
    pack_common_op(1, SDP_OP, 1, [(-1, EVENT_OP_COMPLETED)] * 6,
                   fused_parent=(0, EVENT_OP_ENABLED)),
])
OP_LIST = pack_conv_op() + pack_sdp_op()


def pack_cube(mem_type=0, address=0, size=0, width=0, height=0, channel=0,
              line_stride=0, surf_stride=0, plane_stride=0):
    return struct.pack(
        "<HhIHHHHIII", mem_type, address, size, width, height,
        channel, 0, line_stride, surf_stride, plane_stride,
    )


def pack_surface_container(*cubes):
    surface = b"".join(pack_cube(*cube) for cube in cubes)
    return surface + b"\x00" * (644 - len(surface))


SURF_LIST = b"".join([
    pack_surface_container(
        (DLA_MEM_MC, 1, WEIGHT_BYTES, 1, 1, IN_C, 0, 0, 0),
        (DLA_MEM_HW, -1, 0, 0, 0, 0, 0, 0, 0),
        (DLA_MEM_HW, -1, 0, 0, 0, 0, 0, 0, 0),
        (DLA_MEM_MC, 2, INPUT_BYTES, IN_W, IN_H, IN_C, IN_LINE_STRIDE, IN_SURF_STRIDE, 0),
        (DLA_MEM_HW, -1, OUTPUT_BYTES, OUT_W, OUT_H, OUT_C, OUT_LINE_STRIDE, OUT_SURF_STRIDE, 0),
    ),
    pack_surface_container(
        (DLA_MEM_HW, -1, OUTPUT_BYTES, OUT_W, OUT_H, OUT_C, 0, 0, 0),
        (), (), (),
        (DLA_MEM_MC, 3, OUTPUT_BYTES, OUT_W, OUT_H, OUT_C, OUT_LINE_STRIDE, OUT_SURF_STRIDE, 0),
    ),
])

WEIGHT_ROW_STARTS = (
    0, 16, 15, 14, 8, 7, 6, 5, 16, 15, 14, 13, 7, 6, 5, 4, 15, 14, 13, 12, 6, 5, 4, 3,
    14, 13, 12, 11, 5, 4, 3, 2, 13, 12, 11, 10, 4, 3, 2, 1, 12, 11, 10, 9, 3, 2, 1, 0,
    11, 10, 9, 8, 2, 1, 0, 16, 10, 9, 8, 7, 1, 0, 16, 15, 13, 12, 11, 10, 4, 3, 2, 1,
    12, 11, 10, 9, 3, 2, 1, 0, 11, 10, 9, 8, 2, 1, 0, 16, 10, 9, 8, 7, 1, 0, 16, 15,
    9, 8, 7, 6, 0, 16, 15, 14, 8, 7, 6, 5, 16, 15, 14, 13, 7, 6, 5, 4, 15, 14, 13, 12,
    6, 5, 4, 3, 14, 13, 12, 11, 9, 0, 8, 16, 7, 15, 6, 14, 5, 13, 4, 12, 3, 11, 2, 10,
    9, 8, 7, 6, 0, 16, 15, 14, 8, 7, 6, 5, 16, 15, 14, 13, 7, 6, 5, 4, 15, 14, 13, 12,
    6, 5, 4, 3, 14, 13, 12, 11, 5, 4, 3, 2, 13, 12, 11, 10, 4, 3, 2, 1, 12, 11, 10, 9,
    3, 2, 1, 0, 11, 10, 9, 8, 2, 1, 0, 16, 10, 9, 8, 7, 5, 4, 3, 2, 13, 12, 11, 10,
    4, 3, 2, 1, 12, 11, 10, 9, 3, 2, 1, 0, 11, 10, 9, 8, 2, 1, 0, 16, 10, 9, 8, 7,
    1, 0, 16, 15, 9, 8, 7, 6, 0, 16, 15, 14, 8, 7, 6, 5, 16, 15, 14, 13, 7, 6, 5, 4,
    15, 14, 13, 12, 6, 5, 4, 3, 1, 9, 0, 8, 16, 7, 15, 6, 14, 5, 13, 4, 12, 3, 11, 2,
)


def build_weights():
    data = bytearray()
    for start in WEIGHT_ROW_STARTS:
        for lane in range(ATOM_C):
            data += struct.pack("<e", ((start + lane) % 17 - 8) / 16)
    return bytes(data)


WEIGHTS = build_weights()

assert (len(NET_DESC), len(DEP_GRAPH), len(OP_LIST), len(SURF_LIST), len(WEIGHTS)) == (40, 72, 232, 1288, WEIGHT_BYTES)
assert (INPUT_BYTES, OUTPUT_BYTES) == (225792, 50176)


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
    data = bytearray(INPUT_BYTES)
    one = struct.pack("<e", 1.0)
    for c in range(IN_C):
        cgroup = c // ATOM_C
        lane = c % ATOM_C
        for h in range(IN_H):
            row = cgroup * IN_SURF_STRIDE + h * IN_LINE_STRIDE
            for w in range(IN_W):
                off = row + w * ATOM_C * BPE + lane * BPE
                data[off:off + BPE] = one
    return bytes(data)


def decode_output(output):
    channels = []
    for c in range(OUT_C):
        cgroup = c // ATOM_C
        lane = c % ATOM_C
        off = cgroup * OUT_SURF_STRIDE + lane * BPE
        channels.append(struct.unpack_from("<e", output, off)[0])
    return channels


def run():
    # UMD allocation order observed for ONNC nv_small single-task loadables.
    sizes = [4096, len(WEIGHTS), 40, 72, 232, 1288, 4096, INPUT_BYTES, OUTPUT_BYTES]
    contents = [
        b"\x00" * 4096,
        WEIGHTS,
        NET_DESC,
        DEP_GRAPH,
        OP_LIST,
        SURF_LIST,
        b"\x00" * 4096,
        build_input(),
        b"\x00" * OUTPUT_BYTES,
    ]
    address_slots = [3, 8, 2, 9, 3, 4, 5, 6, 7]

    fd = open_nvdla_device()
    bos = []
    try:
        bos = [create_bo(fd, size, data) for size, data in zip(sizes, contents)]
        ret = submit(fd, bos, address_slots)
        output = bytes(bos[8]["map"][:OUTPUT_BYTES])
    finally:
        for bo in bos:
            destroy_bo(fd, bo)
        os.close(fd)

    first_pixel = decode_output(output)
    decoded = [struct.unpack_from("<e", output, i)[0] for i in range(0, len(output), BPE)]
    nonzero = sum(1 for i in range(0, len(output), BPE) if struct.unpack_from("<H", output, i)[0])
    nan_count = sum(1 for value in decoded if math.isnan(value))
    finite_sum = sum(value for value in decoded if math.isfinite(value))
    ok = ret == 0 and first_pixel == [-0.5] * OUT_C
    print(f"SUBMIT ret={ret}")
    print(
        "conv2d_b1_c144_h28_w28_oc32_wic144_k1x1_g1 "
        f"first_pixel={first_pixel} expected_first_pixel={[-0.5] * OUT_C} "
        f"nonzero_fp16={nonzero} nan_count={nan_count} finite_sum={finite_sum} "
        f"{'PASS' if ok else 'FAIL'}"
    )
    return ok


if __name__ == "__main__":
    sys.exit(0 if run() else 1)
