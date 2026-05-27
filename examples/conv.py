#!/usr/bin/env python3
"""Standalone NVDLA VP nv_small register runner.

Default:
  dc_1x1x8_1x1x8x1_int8_0

Run inside the VP guest after mounting this repo at /mnt, or use --list/--dry-run
on the host to inspect available generated tests.
"""
from __future__ import annotations

import argparse
import ctypes
import mmap
import os
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


DEFAULT_TEST = "dc_1x1x8_1x1x8x1_int8_0"
NVDLA_MMIO_BASE = 0x10200000
NVDLA_MMIO_SIZE = 0x10220000 - 0x10200000
INTR_STATUS = 0x100C

DONE_BITS = [
    ("SDP_DONE", 0),
    ("CDMA_DONE", 2),
    ("CSC_DONE", 3),
    ("CMAC_A_DONE", 4),
    ("CMAC_B_DONE", 5),
    ("CACC_DONE", 6),
    ("BDMA_DONE", 7),
    ("PDP_DONE", 8),
    ("CDP_DONE", 9),
    ("RUBIK_DONE", 10),
]


@dataclass(frozen=True)
class RegWrite:
    name: str
    offset: int
    value: int
    after_cbuf_poll: bool


@dataclass(frozen=True)
class MemLoad:
    addr: int
    path: Path


@dataclass(frozen=True)
class MemInit:
    addr: int
    size: int


@dataclass(frozen=True)
class CrcCheck:
    addr: int
    size: int
    expected: int


@dataclass
class NvSmallTest:
    name: str
    cfg: Path
    mem_inits: list[MemInit]
    mem_loads: list[MemLoad]
    initial_regs: list[RegWrite]
    enable_regs: list[RegWrite]
    crc_checks: list[CrcCheck]


def script_root() -> Path:
    return Path(__file__).resolve().parents[1]


def default_tests_root() -> Path:
    local = script_root() / "ref" / "vp" / "tests" / "nv_small_tests"
    if local.exists():
        return local
    return Path("/mnt/nv_small_tests")


def available_tests(root: Path, kind: str) -> list[str]:
    tests = []
    for path in root.iterdir():
        if not path.is_dir() or path.name.startswith("__"):
            continue
        if not (path / f"{path.name}.cfg").exists():
            continue
        if kind != "all" and not path.name.startswith(f"{kind}_"):
            continue
        tests.append(path.name)
    return sorted(tests)


def relocate_addr(addr: int) -> int:
    region = (addr >> 24) & 0xFF
    if region == 0x80:
        return addr - 0x80000000 + 0xC0000000
    if region == 0x90:
        return addr - 0x90000000 + 0xD0000000
    if region == 0xA0:
        return addr - 0xA0000000 + 0xF0000000
    return addr


def parse_reg_offsets() -> dict[str, int]:
    reg_map: dict[str, int] = {}
    manual_dir = script_root() / "ref" / "hw" / "outdir" / "nv_small" / "spec" / "manual"
    for header in manual_dir.glob("NVDLA_*.h"):
        for line in header.read_text(errors="replace").splitlines():
            match = re.match(r"#define\s+(NVDLA_\S+_0)\s+.*?_MK_ADDR_CONST\((0x[0-9a-fA-F]+)\)", line)
            if match:
                reg_map[match.group(1)] = int(match.group(2), 16)
    if not reg_map:
        raise RuntimeError(f"no NVDLA register headers found under {manual_dir}")
    return reg_map


def cfg_name_to_macro(name: str) -> str:
    return name.replace(".", "_")


def parse_cfg(root: Path, name: str, reg_map: dict[str, int]) -> NvSmallTest:
    cfg = root / name / f"{name}.cfg"
    text = cfg.read_text()
    mem_inits: list[MemInit] = []
    mem_loads: list[MemLoad] = []
    initial_regs: list[RegWrite] = []
    enable_regs: list[RegWrite] = []
    crc_checks: list[CrcCheck] = []
    saw_cbuf_poll = False
    enable_phase = False
    addr_re = re.compile(r"(?:BASE_)?ADDR_(?:LOW|HIGH)|DATAOUT_ADDR")

    for raw in text.splitlines():
        line = raw.strip()
        match = re.match(r'mem_init\(pri_mem,\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*ALL_ZERO\)', line)
        if match:
            mem_inits.append(MemInit(relocate_addr(int(match.group(1), 16)), int(match.group(2), 16)))
            continue

        match = re.match(r'mem_load\(pri_mem,\s*(0x[0-9a-fA-F]+),\s*"([^"]+\.dat)"\)', line)
        if match:
            mem_loads.append(MemLoad(relocate_addr(int(match.group(1), 16)), cfg.parent / match.group(2)))
            continue

        if re.match(r"poll_reg_equal\(NVDLA_CDMA\.S_CBUF_FLUSH_STATUS_0", line):
            saw_cbuf_poll = True
            continue

        match = re.match(r"reg_write\((\S+),\s*(0x[0-9a-fA-F]+)\)", line)
        if match:
            macro = cfg_name_to_macro(match.group(1))
            value = int(match.group(2), 16)
            if addr_re.search(macro):
                value = relocate_addr(value)
            offset = reg_map.get(macro)
            if offset is None:
                raise KeyError(f"unknown register {macro} in {cfg}")
            enable_phase = enable_phase or macro.endswith("_D_OP_ENABLE_0")
            write = RegWrite(macro, offset, value, saw_cbuf_poll)
            (enable_regs if saw_cbuf_poll or enable_phase else initial_regs).append(write)
            continue

        match = re.match(r"check_crc\(.*?,\s*\d+,\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\)", line)
        if match:
            crc_checks.append(CrcCheck(relocate_addr(int(match.group(1), 16)), int(match.group(2), 16), int(match.group(3), 16)))

    return NvSmallTest(name, cfg, mem_inits, mem_loads, initial_regs, enable_regs, crc_checks)


def parse_dat(path: Path) -> list[tuple[int, bytes]]:
    text = path.read_text(errors="replace")
    entries = []
    pattern = r"\{offset:(0x[0-9a-fA-F]+),\s*size:(\d+),\s*payload:((?:0x[0-9a-fA-F]+\s*)+)\}"
    for match in re.finditer(pattern, text):
        offset = int(match.group(1), 16)
        size = int(match.group(2))
        payload = bytes(int(token, 16) for token in match.group(3).replace("0x", "").split())
        entries.append((offset, payload[:size]))
    return entries


def map_region(fd: int, addr: int, size: int, prot: int) -> tuple[mmap.mmap, int]:
    page_base = addr & ~0xFFF
    page_off = addr & 0xFFF
    map_size = ((page_off + size + 4095) // 4096) * 4096
    return mmap.mmap(fd, map_size, mmap.MAP_SHARED, prot, offset=page_base), page_off


def write32(mm: mmap.mmap, offset: int, value: int) -> None:
    mm[offset:offset + 4] = struct.pack("<I", value & 0xFFFFFFFF)


def read32(mm: mmap.mmap, offset: int) -> int:
    return struct.unpack("<I", mm[offset:offset + 4])[0]


def zero_region(fd: int, init: MemInit) -> None:
    mm, off = map_region(fd, init.addr, init.size, mmap.PROT_READ | mmap.PROT_WRITE)
    try:
        mm[off:off + init.size] = b"\x00" * init.size
    finally:
        mm.close()


def load_dat(fd: int, load: MemLoad) -> None:
    entries = parse_dat(load.path)
    if not entries:
        return
    size = max(offset + len(payload) for offset, payload in entries)
    mm, base_off = map_region(fd, load.addr, size, mmap.PROT_READ | mmap.PROT_WRITE)
    try:
        for offset, payload in entries:
            mm[base_off + offset:base_off + offset + len(payload)] = payload
    finally:
        mm.close()


def program_regs(mmio: mmap.mmap, regs: list[RegWrite]) -> None:
    for write in regs:
        write32(mmio, write.offset, write.value)


def wait_cbuf_flush(mmio: mmap.mmap) -> None:
    cbuf_status = 0x300C
    for _ in range(1_000_000):
        if read32(mmio, cbuf_status) & 1:
            print("CBUF flush done")
            return
    print("CBUF flush timeout")


def wait_done(mmio: mmap.mmap) -> None:
    seen = 0
    extra_wait = 0
    for _ in range(100_000_000):
        status = read32(mmio, INTR_STATUS)
        new_bits = status & ~seen
        seen |= status
        for label, bit in DONE_BITS:
            if new_bits & (1 << bit):
                print(label, end=" ", flush=True)
                extra_wait = 10_000
        if extra_wait > 0:
            extra_wait -= 1
            if extra_wait == 0:
                print()
                return
    raise TimeoutError("NVDLA done interrupt timeout")


def verify_crc(fd: int, checks: list[CrcCheck]) -> int:
    failures = 0
    for idx, check in enumerate(checks):
        mm, off = map_region(fd, check.addr, check.size, mmap.PROT_READ)
        try:
            crc = ctypes.c_uint32(0)
            # binascii.crc32 is intentionally imported lazily to keep the top
            # section close to the register-oriented RK examples.
            import binascii
            crc.value = binascii.crc32(mm[off:off + check.size]) & 0xFFFFFFFF
        finally:
            mm.close()
        ok = crc.value == check.expected
        print(f"CRC[{idx}] = 0x{crc.value:08x} expect 0x{check.expected:08x} {'PASS' if ok else 'FAIL'}")
        failures += 0 if ok else 1
    return failures


def run_test(test: NvSmallTest, dry_run: bool) -> int:
    print(f"test={test.name}")
    print(f"cfg={test.cfg}")
    print(f"mem_init={len(test.mem_inits)} mem_load={len(test.mem_loads)} regs={len(test.initial_regs) + len(test.enable_regs)} crc={len(test.crc_checks)}")
    if dry_run:
        return 0

    fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
    try:
        mmio, _ = map_region(fd, NVDLA_MMIO_BASE, NVDLA_MMIO_SIZE, mmap.PROT_READ | mmap.PROT_WRITE)
        try:
            for init in test.mem_inits:
                zero_region(fd, init)
            for load in test.mem_loads:
                load_dat(fd, load)
            print("Programming initial registers")
            program_regs(mmio, test.initial_regs)
            if any(write.after_cbuf_poll for write in test.enable_regs):
                wait_cbuf_flush(mmio)
            for check in test.crc_checks:
                zero_region(fd, MemInit(check.addr, check.size))
            print("Firing OP_ENABLE")
            program_regs(mmio, test.enable_regs)
            wait_done(mmio)
            failures = verify_crc(fd, test.crc_checks)
            if not test.crc_checks:
                print("No CRC checks in cfg; run completed without output verification")
        finally:
            mmio.close()
    finally:
        os.close(fd)
    print(f"Results: {failures} failures")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tests-root", type=Path, default=default_tests_root())
    parser.add_argument("--test", default=DEFAULT_TEST)
    parser.add_argument("--kind", choices=("all", "dc", "sdp", "pdp", "cdp", "img"), default="all")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--all", action="store_true", help="run or dry-run every selected test")
    parser.add_argument("--dry-run", action="store_true", help="parse and print the selected test without touching /dev/mem")
    args = parser.parse_args()

    if args.list:
        for name in available_tests(args.tests_root, args.kind):
            print(name)
        return 0

    tests = available_tests(args.tests_root, args.kind if args.all else "all")
    if args.all:
        reg_map = parse_reg_offsets()
        failures = 0
        for name in tests:
            failures += run_test(parse_cfg(args.tests_root, name, reg_map), dry_run=args.dry_run)
        return failures

    if args.test not in tests:
        print(f"unknown nv_small test: {args.test}", file=sys.stderr)
        return 1

    test = parse_cfg(args.tests_root, args.test, parse_reg_offsets())
    return run_test(test, dry_run=args.dry_run)


if __name__ == "__main__":
    raise SystemExit(main())
