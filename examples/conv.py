#!/usr/bin/env python3
"""Self-contained NVDLA CONV + SDP runner.

Runs FP16 2x2 convolution with 4 input channels and 6 output channels,
followed by SDP ReLU/identity stages, using direct DRM/KMD submit. No loadable
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

IN_W = 5
IN_H = 5
IN_C = 4
OUT_W = 4
OUT_H = 4
OUT_C = 6
KH = 2
KW = 2

IN_LINE_STRIDE = IN_W * FEATURE_ATOMIC_SIZE * BPE
IN_SURF_STRIDE = IN_H * IN_LINE_STRIDE
OUT_LINE_STRIDE = OUT_W * FEATURE_ATOMIC_SIZE * BPE
OUT_SURF_STRIDE = OUT_H * OUT_LINE_STRIDE
TOTAL_WEIGHT_BYTES = 256

DLA_MEM_MC = 0
DLA_MEM_HW = 2
CONV_OP = 1
SDP_OP = 2
EVENT_OP_COMPLETED = 1
EVENT_OP_PROGRAMMED = 2
EVENT_OP_ENABLED = 3
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


def pack_sdp_op(relu=False):
    x1_op = (1, 2, 0, 0, 1, 0, 0, 2, 0, 1) if relu else ()
    return (
        struct.pack("<BBh", 2, 2, -1) +
        pack_cvt(1, 0, 1, 0) +
        struct.pack("<BBHI", 0, 1, 0, 0) +
        pack_sdp_unit(*x1_op) +
        pack_sdp_unit() +
        pack_sdp_unit()
    )


def pack_conv_op():
    op = (
        struct.pack("<BBBBBBH", 0, 0, 0, 0, 0, 0, 2) +
        struct.pack("<BBH", 36, 0, 1) +
        b"\x00" * 8 +
        struct.pack("<BBBB", 1, 0, 1, 1) +
        struct.pack("<I", 0) +
        struct.pack("<BBH", 0, 0, 5) +
        struct.pack("<HHHHHHHH", IN_W, IN_H, IN_C, KW, KH, IN_C, OUT_W, OUT_H) +
        struct.pack("<I", 32) +
        struct.pack("<hhhh", 0, 0, 0, 0) +
        struct.pack("<BBBBBBBBB", 0, 1, 1, 0, 0, 0, 0, 1, 1) +
        b"\x00" * 3 +
        struct.pack("<BBB", 2, 2, 0) +
        struct.pack("<h", 0) +
        pack_cvt(0, 0, 0, 0x01000000) +
        pack_cvt(0, 1, 0, 0)
    )
    return op + b"\x00" * (116 - len(op))


def pack_cube(mem_type=0, address=0, size=0, width=0, height=0, channel=0,
              line_stride=0, surf_stride=0, plane_stride=0):
    return struct.pack(
        "<HhIHHHHIII", mem_type, address, size, width, height,
        channel, 0, line_stride, surf_stride, plane_stride,
    )


def pack_surface_container(*cubes):
    surface = b"".join(pack_cube(*cube) for cube in cubes)
    return surface + b"\x00" * (644 - len(surface))


def build_descriptors():
    net_desc = pack_net_desc(7, 8, 6, [-1, 0, 1, -1, -1, -1], 3, 10)
    dep_graph = b"".join([
        pack_common_op(
            0, CONV_OP, 1,
            [
                (-1, EVENT_OP_COMPLETED), (-1, EVENT_OP_COMPLETED),
                (1, EVENT_OP_PROGRAMMED), (-1, EVENT_OP_COMPLETED),
                (-1, EVENT_OP_COMPLETED), (-1, EVENT_OP_COMPLETED),
            ],
        ),
        pack_common_op(
            1, SDP_OP, 1,
            [
                (-1, EVENT_OP_COMPLETED), (-1, EVENT_OP_COMPLETED),
                (2, EVENT_OP_COMPLETED), (-1, EVENT_OP_COMPLETED),
                (-1, EVENT_OP_COMPLETED), (-1, EVENT_OP_COMPLETED),
            ],
            fused_parent=(0, EVENT_OP_ENABLED),
        ),
        pack_common_op(2, SDP_OP, 1, [(-1, 1)] * 6),
    ])
    op_list = b"".join([pack_conv_op(), pack_sdp_op(), pack_sdp_op(relu=True)])
    surf_list = b"".join([
        pack_surface_container(
            (DLA_MEM_MC, 1, TOTAL_WEIGHT_BYTES, KW, KH, IN_C, 0, 0, 0),
            (DLA_MEM_HW, -1, 0, 0, 0, 0, 0, 0, 0),
            (DLA_MEM_HW, -1, 0, 0, 0, 0, 0, 0, 0),
            (
                DLA_MEM_MC, 2, IN_SURF_STRIDE, IN_W, IN_H, IN_C,
                IN_LINE_STRIDE, IN_SURF_STRIDE, 0,
            ),
            (
                DLA_MEM_HW, -1, OUT_SURF_STRIDE, OUT_W, OUT_H, OUT_C,
                OUT_LINE_STRIDE, OUT_SURF_STRIDE, 0,
            ),
        ),
        pack_surface_container(
            (DLA_MEM_HW, -1, OUT_SURF_STRIDE, OUT_W, OUT_H, OUT_C, 0, 0, 0),
            (), (), (),
            (
                DLA_MEM_MC, 3, OUT_SURF_STRIDE, OUT_W, OUT_H, OUT_C,
                OUT_LINE_STRIDE, OUT_SURF_STRIDE, 0,
            ),
        ),
        pack_surface_container(
            (
                DLA_MEM_MC, 3, OUT_SURF_STRIDE, OUT_W, OUT_H, OUT_C,
                OUT_LINE_STRIDE, OUT_SURF_STRIDE, 0,
            ),
            (), (), (),
            (
                DLA_MEM_MC, 4, OUT_SURF_STRIDE, OUT_W, OUT_H, OUT_C,
                OUT_LINE_STRIDE, OUT_SURF_STRIDE, 0,
            ),
        ),
    ])
    assert (len(net_desc), len(dep_graph), len(op_list), len(surf_list)) == (40, 108, 348, 1932)
    return net_desc, dep_graph, op_list, surf_list


def pack_weights():
    raw = struct.pack("<96e", *([1.0] * 96))
    return raw + b"\x00" * (TOTAL_WEIGHT_BYTES - len(raw))


def pack_input():
    raw = bytearray(IN_SURF_STRIDE)
    for h in range(IN_H):
        for w in range(IN_W):
            for c in range(IN_C):
                off = h * IN_LINE_STRIDE + w * FEATURE_ATOMIC_SIZE * BPE + c * BPE
                raw[off:off + BPE] = struct.pack(
                    "<e", float(h * IN_W + w + 1 + c * 100),
                )
    return bytes(raw)


def read_output(output_map):
    out = []
    for h in range(OUT_H):
        row = []
        for w in range(OUT_W):
            pixel = []
            for c in range(OUT_C):
                off = h * OUT_LINE_STRIDE + w * FEATURE_ATOMIC_SIZE * BPE + c * BPE
                pixel.append(struct.unpack_from("<e", output_map, off)[0])
            row.append(pixel)
        out.append(row)
    return out


def expected_output():
    expected = []
    for h in range(OUT_H):
        row = []
        for w in range(OUT_W):
            acc = 0.0
            for ky in range(KH):
                for kx in range(KW):
                    for c in range(IN_C):
                        acc += float((h + ky) * IN_W + (w + kx) + 1 + c * 100)
            row.append([acc] * OUT_C)
        expected.append(row)
    return expected


def flatten_nhwc(x):
    return [v for row in x for pixel in row for v in pixel]


def run_conv2d_4x6_2x2():
    net_desc, dep_graph, op_list, surf_list = build_descriptors()
    sizes = [4096, TOTAL_WEIGHT_BYTES, IN_SURF_STRIDE, OUT_SURF_STRIDE,
             OUT_SURF_STRIDE, 40, 108, 348, 1932, 4096]
    contents = [
        b"\x00" * 4096,
        pack_weights(),
        pack_input(),
        b"\x00" * OUT_SURF_STRIDE,
        b"\x00" * OUT_SURF_STRIDE,
        net_desc,
        dep_graph,
        op_list,
        surf_list,
        b"\x00" * 4096,
    ]
    address_slots = [6, 2, 3, 4, 5, 6, 7, 8, 9, 10]

    fd = open_nvdla_device()
    bos = []
    try:
        bos = [mem_allocate(fd, size, data) for size, data in zip(sizes, contents)]
        print("SUBMIT...", flush=True)
        ret = nvdla_submit(fd, bos, address_slots)
        print("SUBMIT done", flush=True)
        result = read_output(mem_read(fd, bos[4], OUT_SURF_STRIDE))
    finally:
        for bo in bos:
            mem_destroy(fd, bo)
        os.close(fd)

    expected = expected_output()
    flat_result = flatten_nhwc(result)
    flat_expected = flatten_nhwc(expected)
    diff = max(abs(a - b) for a, b in zip(flat_result, flat_expected))
    ok = diff == 0.0
    print(f"SUBMIT ret={ret}")
    print(
        f"conv2d_4x6_2x2 NPU={flat_result} expected={flat_expected} "
        f"{'PASS' if ok else 'FAIL'} (max_diff={diff})"
    )
    return ok


if __name__ == "__main__":
    sys.exit(0 if run_conv2d_4x6_2x2() else 1)
