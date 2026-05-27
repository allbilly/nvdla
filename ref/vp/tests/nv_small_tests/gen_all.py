#!/usr/bin/env python3
"""Generate all nv_small VP tests from UVM trace .cfg files."""
import os
import sys
import glob
import shutil
import subprocess

GEN_SCRIPT = os.path.join(os.path.dirname(__file__), "gen_cfg2c.py")
UVM_DIR = os.path.join(os.path.dirname(__file__), "../../../hw/verif/tests/trace_tests/nv_small")
VP_TEST_DIR = os.path.dirname(__file__)  # ref/vp/tests/nv_small_tests/

# Find all test directories with .cfg files
test_dirs = sorted(glob.glob(os.path.join(UVM_DIR, "*/")))
total = 0
errors = []

for td in test_dirs:
    test_name = os.path.basename(td.rstrip('/'))
    cfg_path = os.path.join(td, f"{test_name}.cfg")
    if not os.path.exists(cfg_path):
        continue

    vp_test_dir = os.path.join(VP_TEST_DIR, test_name)
    os.makedirs(vp_test_dir, exist_ok=True)

    print(f"[{total+1}] {test_name}...", end=" ", flush=True)
    try:
        # Copy .cfg + .dat files to VP test dir
        for f in glob.glob(os.path.join(td, "*")):
            shutil.copy2(f, vp_test_dir)

        # Run generator from VP test dir (output goes to CWD = vp_test_dir)
        result = subprocess.run(
            [sys.executable, GEN_SCRIPT, os.path.join(vp_test_dir, f"{test_name}.cfg")],
            capture_output=True, text=True, cwd=vp_test_dir
        )
        if result.returncode != 0:
            errors.append(f"{test_name}: {result.stderr.strip()}")
            print("FAIL")
            continue

        for line in result.stdout.split('\n'):
            if any(x in line for x in ["Relocated", "CRC check", "UVM", "Generated"]):
                print(line.strip())
        print("  OK", flush=True)
        total += 1

    except Exception as e:
        errors.append(f"{test_name}: {e}")
        print(f"ERROR: {e}")

print(f"\n=== Generated {total} tests, {len(errors)} errors ===")
if errors:
    for e in errors:
        print(f"  ERROR: {e}")
