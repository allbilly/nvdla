#!/usr/bin/env python3
"""Standalone NVDLA VP simple Add example.

Default mode runs ONNC's test_Add NVDLA loadable with nvdla_runtime. The nv_small
VP needs the nv_small KMD register map; the legacy opendla.ko writes GLB at 0x4
and aborts in the C-model CSB decoder.

Use --raw-sdp-ew to exercise a generated nv_small SDP EW register trace through
/dev/mem directly. That mode is useful for low-level SDP debugging, while the
default Add example follows the NVDLA loadable/KMD/runtime flow.
"""
from __future__ import annotations

import argparse
import binascii
import mmap
import os
import re
import shutil
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


DEFAULT_RAW_TEST = "sdp_3x3x33_ew_int8_reg_0"
NVDLA_MMIO_BASE = 0x10200000
NVDLA_MMIO_SIZE = 0x10220000 - 0x10200000
INTR_STATUS = 0x100C


@dataclass(frozen=True)
class RegWrite:
    name: str
    offset: int
    value: int
    enable: bool


@dataclass(frozen=True)
class MemOp:
    addr: int
    size: int = 0
    path: Path | None = None


@dataclass(frozen=True)
class CrcCheck:
    addr: int
    size: int
    expected: int


def repo_root() -> Path:
    local = Path(__file__).resolve().parents[1]
    if (local / "ref").exists():
        return local
    return Path("/mnt")


def vp_root() -> Path:
    local = repo_root() / "ref" / "vp"
    return local if local.exists() else Path("/mnt")


def add_model_dir() -> Path:
    root = repo_root()
    candidates = [
        root / "ref" / "vp" / "onnc_models" / "test_Add",
        root / "ref" / "onnc-tutorial" / "models" / "test_Add",
        Path("/mnt/onnc_models/test_Add"),
    ]
    for path in candidates:
        if (path / "test_Add.nvdla").exists() and (path / "input1x5x7.pgm").exists():
            return path
    raise FileNotFoundError("cannot find test_Add.nvdla and input1x5x7.pgm")


def run_cmd(cmd: list[str], cwd: Path) -> int:
    print("+ " + " ".join(cmd))
    env = os.environ.copy()
    libdir = str(vp_root())
    env["LD_LIBRARY_PATH"] = libdir if not env.get("LD_LIBRARY_PATH") else f"{libdir}:{env['LD_LIBRARY_PATH']}"
    return subprocess.run(cmd, cwd=cwd, env=env).returncode


def ensure_kernel_modules() -> None:
    if not Path("/dev/mem").exists():
        return
    nvdla_module = vp_root() / "opendla_small.ko"
    if not nvdla_module.exists():
        nvdla_module = vp_root() / "opendla.ko"
    for module in (vp_root() / "drm.ko", nvdla_module):
        if not module.exists():
            continue
        proc = subprocess.run(["insmod", str(module)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if proc.returncode != 0 and "File exists" not in proc.stdout:
            print(proc.stdout.strip())


def run_loadable(dry_run: bool) -> int:
    model_dir = add_model_dir()
    runtime_dir = vp_root()
    loadable = model_dir / "test_Add.nvsmall.nvdla"
    if not loadable.exists():
        from patch_test_add_loadable import patch_loadable

        patch_loadable(model_dir / "test_Add.nvdla", loadable)
    image = model_dir / "input1x5x7.pgm"
    runtime = runtime_dir / "nvdla_runtime"
    print(f"loadable={loadable}")
    print(f"image={image}")
    print(f"runtime={runtime}")
    if dry_run:
        return 0

    if not runtime.exists():
        raise FileNotFoundError(runtime)
    if not Path("/dev/mem").exists():
        raise RuntimeError("nvdla_runtime must run inside the VP guest")

    ensure_kernel_modules()
    workdir = Path("/tmp/nvdla_test_Add")
    workdir.mkdir(exist_ok=True)
    shutil.copy2(loadable, workdir / "test_Add.nvdla")
    shutil.copy2(image, workdir / "input1x5x7.pgm")
    rc = run_cmd([str(runtime), "--loadable", "test_Add.nvdla", "--image", "input1x5x7.pgm", "--rawdump"], workdir)
    if rc == 0 and (workdir / "output.dimg").exists():
        print((workdir / "output.dimg").read_text(errors="replace").strip())
    return rc


def tests_root() -> Path:
    local = repo_root() / "ref" / "vp" / "tests" / "nv_small_tests"
    return local if local.exists() else Path("/mnt/nv_small_tests")


def relocate_addr(addr: int) -> int:
    region = (addr >> 24) & 0xFF
    if region == 0x80:
        return addr - 0x80000000 + 0xC0000000
    if region == 0x90:
        return addr - 0x90000000 + 0xD0000000
    if region == 0xA0:
        return addr - 0xA0000000 + 0xF0000000
    return addr


def reg_offsets() -> dict[str, int]:
    manual = repo_root() / "ref" / "hw" / "outdir" / "nv_small" / "spec" / "manual"
    out: dict[str, int] = {}
    for header in manual.glob("NVDLA_*.h"):
        for line in header.read_text(errors="replace").splitlines():
            m = re.match(r"#define\s+(NVDLA_\S+_0)\s+.*?_MK_ADDR_CONST\((0x[0-9a-fA-F]+)\)", line)
            if m:
                out[m.group(1)] = int(m.group(2), 16)
    if not out:
        raise RuntimeError(f"missing NVDLA register headers under {manual}")
    return out


def parse_dat(path: Path) -> list[tuple[int, bytes]]:
    text = path.read_text(errors="replace")
    pattern = r"\{offset:(0x[0-9a-fA-F]+),\s*size:(\d+),\s*payload:((?:0x[0-9a-fA-F]+\s*)+)\}"
    entries = []
    for m in re.finditer(pattern, text):
        offset = int(m.group(1), 16)
        size = int(m.group(2))
        payload = bytes(int(tok, 16) for tok in m.group(3).replace("0x", "").split())
        entries.append((offset, payload[:size]))
    return entries


def parse_raw_test(name: str) -> tuple[list[MemOp], list[MemOp], list[RegWrite], list[CrcCheck]]:
    root = tests_root() / name
    cfg = root / f"{name}.cfg"
    regs = reg_offsets()
    mem_inits: list[MemOp] = []
    mem_loads: list[MemOp] = []
    writes: list[RegWrite] = []
    crcs: list[CrcCheck] = []
    addr_re = re.compile(r"(?:BASE_)?ADDR_(?:LOW|HIGH)|DATAOUT_ADDR")
    enable_phase = False

    for raw in cfg.read_text().splitlines():
        line = raw.strip()
        m = re.match(r'mem_init\(pri_mem,\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*ALL_ZERO\)', line)
        if m:
            mem_inits.append(MemOp(relocate_addr(int(m.group(1), 16)), int(m.group(2), 16)))
            continue
        m = re.match(r'mem_load\(pri_mem,\s*(0x[0-9a-fA-F]+),\s*"([^"]+\.dat)"\)', line)
        if m:
            mem_loads.append(MemOp(relocate_addr(int(m.group(1), 16)), path=root / m.group(2)))
            continue
        m = re.match(r"reg_write\((\S+),\s*(0x[0-9a-fA-F]+)\)", line)
        if m:
            macro = m.group(1).replace(".", "_")
            value = int(m.group(2), 16)
            if addr_re.search(macro):
                value = relocate_addr(value)
            enable_phase = enable_phase or macro.endswith("_D_OP_ENABLE_0")
            writes.append(RegWrite(macro, regs[macro], value, enable_phase))
            continue
        m = re.match(r"check_crc\(.*?,\s*\d+,\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\)", line)
        if m:
            crcs.append(CrcCheck(relocate_addr(int(m.group(1), 16)), int(m.group(2), 16), int(m.group(3), 16)))
    return mem_inits, mem_loads, writes, crcs


def map_region(fd: int, addr: int, size: int, prot: int) -> tuple[mmap.mmap, int]:
    page = addr & ~0xFFF
    off = addr & 0xFFF
    map_size = ((off + size + 4095) // 4096) * 4096
    return mmap.mmap(fd, map_size, mmap.MAP_SHARED, prot, offset=page), off


def write32(mm: mmap.mmap, offset: int, value: int) -> None:
    mm[offset:offset + 4] = struct.pack("<I", value & 0xFFFFFFFF)


def read32(mm: mmap.mmap, offset: int) -> int:
    return struct.unpack("<I", mm[offset:offset + 4])[0]


def run_raw_sdp(name: str, dry_run: bool) -> int:
    mem_inits, mem_loads, writes, crcs = parse_raw_test(name)
    initial = [w for w in writes if not w.enable]
    enable = [w for w in writes if w.enable]
    print(f"raw_test={name} mem_init={len(mem_inits)} mem_load={len(mem_loads)} regs={len(writes)} crc={len(crcs)}")
    if dry_run:
        return 0

    fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
    try:
        for op in mem_inits:
            mm, off = map_region(fd, op.addr, op.size, mmap.PROT_READ | mmap.PROT_WRITE)
            mm[off:off + op.size] = b"\x00" * op.size
            mm.close()
        for op in mem_loads:
            entries = parse_dat(op.path)
            size = max(offset + len(payload) for offset, payload in entries)
            mm, off = map_region(fd, op.addr, size, mmap.PROT_READ | mmap.PROT_WRITE)
            for offset, payload in entries:
                mm[off + offset:off + offset + len(payload)] = payload
            mm.close()

        mmio, _ = map_region(fd, NVDLA_MMIO_BASE, NVDLA_MMIO_SIZE, mmap.PROT_READ | mmap.PROT_WRITE)
        try:
            for write in initial:
                write32(mmio, write.offset, write.value)
            for check in crcs:
                mm, off = map_region(fd, check.addr, check.size, mmap.PROT_READ | mmap.PROT_WRITE)
                mm[off:off + check.size] = b"\x00" * check.size
                mm.close()
            for write in enable:
                write32(mmio, write.offset, write.value)
            for _ in range(100_000_000):
                if read32(mmio, INTR_STATUS):
                    break
            else:
                raise TimeoutError("NVDLA done interrupt timeout")
        finally:
            mmio.close()

        failures = 0
        for idx, check in enumerate(crcs):
            mm, off = map_region(fd, check.addr, check.size, mmap.PROT_READ)
            crc = binascii.crc32(mm[off:off + check.size]) & 0xFFFFFFFF
            mm.close()
            ok = crc == check.expected
            print(f"CRC[{idx}] = 0x{crc:08x} expect 0x{check.expected:08x} {'PASS' if ok else 'FAIL'}")
            failures += 0 if ok else 1
        return failures
    finally:
        os.close(fd)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--raw-sdp-ew", action="store_true", help="run generated nv_small SDP EW cfg directly")
    parser.add_argument("--raw-test", default=DEFAULT_RAW_TEST)
    args = parser.parse_args()
    if args.raw_sdp_ew:
        return run_raw_sdp(args.raw_test, args.dry_run)
    return run_loadable(args.dry_run)


if __name__ == "__main__":
    raise SystemExit(main())
