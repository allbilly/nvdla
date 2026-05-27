#!/usr/bin/env python3
"""
Automate NVDLA VP test:
  1. Launch VP (aarch64_toplevel)
  2. Wait for boot
  3. Mount 9p shared directory
  4. Load data/weights via devmem
  5. Run the test binary
  6. Capture output
  7. Power off
"""
import sys
import os
import pexpect
import subprocess

VP_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
VP_BIN = os.path.join(VP_DIR, "build/bin/aarch64_toplevel")
VP_CONF = os.path.join(VP_DIR, "conf/aarch64_nvdla.lua")
TEST_DIR = "/mnt/tests/dc_1x1x8_1x1x8x1_int8_0"
TIMEOUT = 600  # seconds (VP can be slow)

def main():
    if not os.path.exists(VP_BIN):
        print(f"VP binary not found: {VP_BIN}")
        sys.exit(1)

    os.environ["SC_SIGNAL_WRITE_CHECK"] = "DISABLE"
    os.chdir(VP_DIR)

    print(f"Launching VP from {VP_DIR}...")
    child = pexpect.spawn(
        f"{VP_BIN} -c {VP_CONF}",
        timeout=TIMEOUT,
        encoding='utf-8',
        codec_errors='replace',
    )
    child.logfile = sys.stdout

    try:
        # Wait for login prompt or shell
        # Login: root / nvdla
        child.expect('login:', timeout=120)
        child.sendline('root')
        child.expect_exact('Password:', timeout=30)
        child.sendline('nvdla')
        child.expect_exact(['# ', '$ '], timeout=30)

        # Mount 9p
        child.sendline('mount -t 9p -o trans=virtio r /mnt')
        child.expect(r'[$#] ', timeout=30)

        # cd to test dir
        child.sendline(f'cd {TEST_DIR}')
        child.expect(r'[$#] ', timeout=10)

        # Load data
        child.sendline('./dat_load.sh')
        child.expect(r'[$#] ', timeout=30)

        # Load weights
        child.sendline('./wt_load.sh')
        child.expect(r'[$#] ', timeout=30)

        # Run test
        child.sendline('./dc_1x1x8_1x1x8x1_int8_0_test')
        child.expect(r'[$#] ', timeout=300)

        # Dump output
        child.sendline('hexdump -C sdp2mcif_output.dat')
        child.expect(r'[$#] ', timeout=30)

        # Power off
        child.sendline('poweroff')
        child.expect(pexpect.EOF, timeout=30)

    except pexpect.TIMEOUT as e:
        print(f"\nTIMEOUT: {e}")
        child.close(force=True)
        sys.exit(1)
    except pexpect.EOF:
        print("\nVP exited")
        child.close()
        sys.exit(0)

    print("\nVP test complete.")
    child.close()

if __name__ == "__main__":
    main()
