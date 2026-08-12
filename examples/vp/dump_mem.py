#!/usr/bin/env python3
import os
import sys

if sys.argv[1] == "auto":
    pids = [p for p in os.listdir("/proc") if p.isdigit()]
    matches = []
    for p in pids:
        try:
            with open(f"/proc/{p}/comm") as f:
                if f.read().strip() == "nvdla_runtime":
                    matches.append(int(p))
        except OSError:
            pass
    if not matches:
        raise SystemExit("nvdla_runtime pid not found")
    pid = max(matches)
else:
    pid = int(sys.argv[1])
addr = int(sys.argv[2], 0)
size = int(sys.argv[3], 0)
out = sys.argv[4]

with open(f"/proc/{pid}/mem", "rb", buffering=0) as mem:
    mem.seek(addr)
    data = mem.read(size)

with open(out, "wb") as f:
    f.write(data)

print(f"dumped {len(data)} bytes from 0x{addr:x} to {out}")
