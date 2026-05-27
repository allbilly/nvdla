#!/usr/bin/env python3
"""Run ONNC test_Add in the NVDLA VP through the console.

This is a host-side smoke test. It requires pexpect and the VP artifacts under
ref/vp.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pexpect

from patch_test_add_loadable import patch_loadable


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def run(module: str, timeout: int, debug_mmio: bool = False) -> int:
    vp = repo_root() / "ref" / "vp"
    model_dir = vp / "onnc_models" / "test_Add"
    patch_loadable(model_dir / "test_Add.nvdla", model_dir / "test_Add.nvsmall.nvdla")
    cmd = "SC_SIGNAL_WRITE_CHECK=DISABLE ./build/bin/aarch64_toplevel --conf conf/aarch64_nvdla.lua"
    child = pexpect.spawn("/bin/sh", ["-lc", cmd], cwd=str(vp), encoding="utf-8", timeout=timeout)
    child.logfile_read = sys.stdout
    transcript: list[str] = []

    def expect(patterns: list[str | type[pexpect.EOF] | type[pexpect.TIMEOUT]], wait: int | None = None) -> int:
        old_timeout = child.timeout
        if wait is not None:
            child.timeout = wait
        idx = child.expect(patterns)
        child.timeout = old_timeout
        transcript.append(child.before)
        transcript.append(child.after if isinstance(child.after, str) else "")
        return idx

    def run_shell(command: str, idx: int, wait: int | None = None) -> bool:
        marker = f"CMD_DONE_{idx}="
        child.sendline(f"{command}; echo {marker}$?")
        result = expect([marker, pexpect.EOF, pexpect.TIMEOUT], wait=wait)
        if result != 0:
            return False
        expect(["# ", pexpect.EOF, pexpect.TIMEOUT], wait=wait)
        return True

    try:
        expect(["nvdla login:"])
        child.sendline("root")
        expect(["# "])
        commands = [
            "mount -t 9p -o trans=virtio r /mnt",
            "cd /mnt",
            "insmod drm.ko",
            f"insmod {module}",
            "mkdir -p /tmp/nvdla_test_Add && cd /tmp/nvdla_test_Add",
            "cp /mnt/onnc_models/test_Add/test_Add.nvsmall.nvdla test_Add.nvdla",
            "cp /mnt/onnc_models/test_Add/input1x5x7.pgm .",
            "LD_LIBRARY_PATH=/mnt /mnt/nvdla_runtime --loadable test_Add.nvdla --image input1x5x7.pgm --rawdump; echo TEST_ADD_RC=$?",
        ]
        for idx, command in enumerate(commands[:-1]):
            if not run_shell(command, idx):
                break

        if debug_mmio:
            debug_commands = [
                "(which devmem || true)",
                "LD_LIBRARY_PATH=/mnt /mnt/nvdla_runtime --loadable test_Add.nvdla --image input1x5x7.pgm --rawdump >/tmp/test_add.log 2>&1 & echo RUNTIME_PID=$!",
                "sleep 5",
                "cat /tmp/test_add.log",
                "devmem 0x10201004 32 || true",
                "devmem 0x1020100c 32 || true",
                "devmem 0x10209038 32 || true",
                "devmem 0x10209000 32 || true",
                "kill $(cat /tmp/runtime_pid 2>/dev/null) 2>/dev/null || true",
            ]
            child.sendline("LD_LIBRARY_PATH=/mnt /mnt/nvdla_runtime --loadable test_Add.nvdla --image input1x5x7.pgm --rawdump >/tmp/test_add.log 2>&1 & echo $! >/tmp/runtime_pid; echo RUNTIME_STARTED=$!")
            expect(["RUNTIME_STARTED=", pexpect.EOF, pexpect.TIMEOUT], wait=timeout)
            expect(["# ", pexpect.EOF, pexpect.TIMEOUT])
            for offset, command in enumerate(debug_commands[:1] + debug_commands[2:]):
                run_shell(command, 100 + offset, wait=8)
            text = "".join(transcript)
            return 12 if "TEST_ADD_RC=0" not in text else 0

        child.sendline(commands[-1])
        expect(["TEST_ADD_RC=0", "TEST_ADD_RC=", "invalid csb address", "Assertion `", "Fatal:", pexpect.EOF, pexpect.TIMEOUT], wait=timeout)
        if child.isalive():
            child.sendline("poweroff")
            try:
                expect([pexpect.EOF], wait=10)
            except pexpect.ExceptionPexpect:
                pass
    finally:
        if child.isalive():
            child.close(force=True)
    text = "".join(transcript)
    if "invalid csb address" in text:
        return 10
    if "Assertion `" in text or "Fatal:" in text:
        return 11
    if "TEST_ADD_RC=0" in text:
        return 0
    if "TEST_ADD_RC=" in text:
        return 12
    return 12


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", default="opendla_small.ko")
    parser.add_argument("--timeout", type=int, default=180)
    parser.add_argument("--debug-mmio", action="store_true")
    args = parser.parse_args()
    return run(args.module, args.timeout, args.debug_mmio)


if __name__ == "__main__":
    raise SystemExit(main())
