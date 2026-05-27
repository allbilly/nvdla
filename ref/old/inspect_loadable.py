#!/usr/bin/env python3
"""Standalone NVDLA loadable inspector.

This intentionally parses only the fields needed to debug the NVDLA examples:
memory entries, address entries, tensor descriptors, relocations, and DLA blobs.
It does not require flatc or the Python flatbuffers package.
"""
from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path


def u8(buf: bytes, off: int) -> int:
    return buf[off]


def u16(buf: bytes, off: int) -> int:
    return struct.unpack_from("<H", buf, off)[0]


def i16(buf: bytes, off: int) -> int:
    return struct.unpack_from("<h", buf, off)[0]


def u32(buf: bytes, off: int) -> int:
    return struct.unpack_from("<I", buf, off)[0]


def i32(buf: bytes, off: int) -> int:
    return struct.unpack_from("<i", buf, off)[0]


def u64(buf: bytes, off: int) -> int:
    return struct.unpack_from("<Q", buf, off)[0]


def table_field(buf: bytes, table: int, field: int) -> int:
    vtable = table - i32(buf, table)
    vtable_size = u16(buf, vtable)
    slot = 4 + field * 2
    if slot >= vtable_size:
        return 0
    return u16(buf, vtable + slot)


def scalar(buf: bytes, table: int, field: int, default: int, read) -> int:
    off = table_field(buf, table, field)
    return default if off == 0 else read(buf, table + off)


def indirect(buf: bytes, off: int) -> int:
    return off + u32(buf, off)


def vector(buf: bytes, table: int, field: int) -> tuple[int, int]:
    off = table_field(buf, table, field)
    if off == 0:
        return 0, 0
    start = indirect(buf, table + off)
    return start + 4, u32(buf, start)


def string_at(buf: bytes, off: int) -> str:
    start = indirect(buf, off)
    size = u32(buf, start)
    return buf[start + 4:start + 4 + size].decode("utf-8", errors="replace")


def strings_vector(buf: bytes, table: int, field: int) -> list[str]:
    start, count = vector(buf, table, field)
    return [string_at(buf, start + i * 4) for i in range(count)]


def ulong_vector(buf: bytes, table: int, field: int) -> list[int]:
    start, count = vector(buf, table, field)
    return [u64(buf, start + i * 8) for i in range(count)]


def bytes_vector(buf: bytes, table: int, field: int) -> bytes:
    start, count = vector(buf, table, field)
    return buf[start:start + count]


@dataclass(frozen=True)
class DlaCube:
    typ: int
    address: int
    offset: int
    size: int
    width: int
    height: int
    channel: int
    line_stride: int
    surf_stride: int
    plane_stride: int


def parse_cube(data: bytes, off: int) -> DlaCube:
    return DlaCube(
        typ=u16(data, off),
        address=i16(data, off + 2),
        offset=u32(data, off + 4),
        size=u32(data, off + 8),
        width=u16(data, off + 12),
        height=u16(data, off + 14),
        channel=u16(data, off + 16),
        line_stride=u32(data, off + 20),
        surf_stride=u32(data, off + 24),
        plane_stride=u32(data, off + 28),
    )


def print_cube(label: str, cube: DlaCube) -> None:
    print(
        f"    {label}: type={cube.typ} addr_id={cube.address} offset=0x{cube.offset:x} "
        f"size=0x{cube.size:x} dims={cube.width}x{cube.height}x{cube.channel} "
        f"line=0x{cube.line_stride:x} surf=0x{cube.surf_stride:x} plane=0x{cube.plane_stride:x}"
    )


def inspect(path: Path) -> None:
    buf = path.read_bytes()
    root = indirect(buf, 0)
    print(f"loadable={path}")

    mem_start, mem_count = vector(buf, root, 2)
    print(f"memory_list={mem_count}")
    for i in range(mem_count):
        tab = indirect(buf, mem_start + i * 4)
        contents = strings_vector(buf, tab, 5)
        offsets = ulong_vector(buf, tab, 6)
        print(
            f"  mem[{i}] id={scalar(buf, tab, 0, 0, u16)} domain={scalar(buf, tab, 1, 0, u8)} "
            f"flags=0x{scalar(buf, tab, 2, 0, u16):x} size=0x{scalar(buf, tab, 3, 0, u64):x} "
            f"align={scalar(buf, tab, 4, 0, u32)} bind={scalar(buf, tab, 7, 0, u16)} "
            f"tensor={scalar(buf, tab, 8, 0, u16)} contents={contents} offsets={offsets}"
        )

    addr_start, addr_count = vector(buf, root, 3)
    print(f"address_list={addr_count}")
    for i in range(addr_count):
        tab = indirect(buf, addr_start + i * 4)
        print(
            f"  addr[{i}] id={scalar(buf, tab, 0, 0, u16)} mem={scalar(buf, tab, 1, 0, u16)} "
            f"offset=0x{scalar(buf, tab, 2, 0, u64):x} size=0x{scalar(buf, tab, 3, 0, u64):x}"
        )

    tensor_start, tensor_count = vector(buf, root, 6)
    print(f"tensor_desc_list={tensor_count}")
    for i in range(tensor_count):
        tab = indirect(buf, tensor_start + i * 4)
        name_off = table_field(buf, tab, 0)
        name = string_at(buf, tab + name_off) if name_off else ""
        strides = [scalar(buf, tab, 13 + s, 0, u32) for s in range(8)]
        print(
            f"  tensor[{i}] id={scalar(buf, tab, 1, 0, u16)} name={name} mem={scalar(buf, tab, 2, 0, u16)} "
            f"size=0x{scalar(buf, tab, 3, 0, u64):x} offset=0x{scalar(buf, tab, 4, 0, u64):x} "
            f"fmt={scalar(buf, tab, 5, 0, u8)} dtype={scalar(buf, tab, 6, 0, u8)} "
            f"cat={scalar(buf, tab, 7, 0, u8)} pixel={scalar(buf, tab, 8, 0, u8)} "
            f"nchw={scalar(buf, tab, 10, 0, i32)}x{scalar(buf, tab, 11, 0, i32)}x"
            f"{scalar(buf, tab, 12, 0, i32)}x{scalar(buf, tab, 13, 0, i32)} strides={strides}"
        )

    reloc_start, reloc_count = vector(buf, root, 7)
    print(f"reloc_list={reloc_count}")
    for i in range(reloc_count):
        tab = indirect(buf, reloc_start + i * 4)
        print(
            f"  reloc[{i}] address={scalar(buf, tab, 0, 0, u16)} write={scalar(buf, tab, 1, 0, u16)} "
            f"offset=0x{scalar(buf, tab, 2, 0, u64):x} iface={scalar(buf, tab, 3, 0, u32)} "
            f"sub={scalar(buf, tab, 4, 0, u32)} type={scalar(buf, tab, 5, 0, u8)}"
        )

    blob_start, blob_count = vector(buf, root, 5)
    print(f"blobs={blob_count}")
    for i in range(blob_count):
        tab = indirect(buf, blob_start + i * 4)
        name_off = table_field(buf, tab, 0)
        name = string_at(buf, tab + name_off) if name_off else ""
        data = bytes_vector(buf, tab, 5)
        iface = scalar(buf, tab, 2, 0, u8)
        sub = scalar(buf, tab, 3, 0, u32)
        print(f"  blob[{i}] name={name} iface={iface} sub={sub} size=0x{len(data):x}")
        if name.endswith("surf_list") and len(data) >= 160:
            for label, off in (("src", 0), ("x1", 32), ("x2", 64), ("y", 96), ("dst", 128)):
                print_cube(label, parse_cube(data, off))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("loadable", nargs="?", default="ref/vp/onnc_models/test_Add/test_Add.nvdla")
    args = parser.parse_args()
    inspect(Path(args.loadable))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
