#!/usr/bin/env python3
"""Patch ONNC tutorial test_Add for the current nv_small KMD ABI.

The historical tutorial loadable stores `dla_data_cube` records in the old
28-byte layout:

  type, address, offset, width, height, channel, line_stride, surf_stride,
  plane_stride

The nv_small `opendla_small.ko` firmware interface in this tree expects the
newer 32-byte layout:

  type, address, offset, size, width, height, channel, reserved,
  line_stride, surf_stride, plane_stride

This tool rewrites only the `task-0-surf_list` blob payload. It keeps the
FlatBuffer size unchanged by expanding the first five surface cubes in-place
and leaving the remaining zero padding as padding.
"""
from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path

from inspect_loadable import indirect, string_at, table_field, u16, u32, vector


@dataclass(frozen=True)
class OldCube:
    typ: int
    address: int
    offset: int
    width: int
    height: int
    channel: int
    line_stride: int
    surf_stride: int
    plane_stride: int


def parse_old_cube(data: bytes, off: int) -> OldCube:
    typ, address = struct.unpack_from("<Hh", data, off)
    offset = struct.unpack_from("<I", data, off + 4)[0]
    width, height, channel, _reserved = struct.unpack_from("<HHHH", data, off + 8)
    line_stride, surf_stride, plane_stride = struct.unpack_from("<III", data, off + 16)
    return OldCube(typ, address, offset, width, height, channel, line_stride, surf_stride, plane_stride)


def pack_new_cube(cube: OldCube) -> bytes:
    size = cube.surf_stride if cube.surf_stride else cube.line_stride * max(cube.height, 1)
    return struct.pack(
        "<HhIIHHHHIII",
        cube.typ,
        cube.address,
        cube.offset,
        size,
        cube.width,
        cube.height,
        cube.channel,
        0,
        cube.line_stride,
        cube.surf_stride,
        cube.plane_stride,
    )


def blob_data_span(buf: bytes, blob_name: str) -> tuple[int, int]:
    root = indirect(buf, 0)
    start, count = vector(buf, root, 5)
    for i in range(count):
        tab = indirect(buf, start + i * 4)
        name_off = table_field(buf, tab, 0)
        name = string_at(buf, tab + name_off) if name_off else ""
        if name != blob_name:
            continue
        data_field = table_field(buf, tab, 5)
        if not data_field:
            raise ValueError(f"{blob_name} has no data vector")
        vec = indirect(buf, tab + data_field)
        return vec + 4, u32(buf, vec)
    raise ValueError(f"cannot find blob {blob_name}")


def patch_loadable(src: Path, dst: Path) -> None:
    buf = bytearray(src.read_bytes())
    data_start, data_len = blob_data_span(buf, "task-0-surf_list")
    data = bytes(buf[data_start:data_start + data_len])
    old_offsets = [0, 28, 56, 84, 112]
    cubes = [parse_old_cube(data, off) for off in old_offsets]
    patched = b"".join(pack_new_cube(cube) for cube in cubes)
    if len(patched) > data_len:
        raise ValueError("patched surface blob does not fit existing vector")
    buf[data_start:data_start + data_len] = patched + b"\x00" * (data_len - len(patched))
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(buf)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("src", nargs="?", default="ref/vp/onnc_models/test_Add/test_Add.nvdla")
    parser.add_argument("dst", nargs="?", default="ref/vp/onnc_models/test_Add/test_Add.nvsmall.nvdla")
    args = parser.parse_args()
    patch_loadable(Path(args.src), Path(args.dst))
    print(args.dst)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
