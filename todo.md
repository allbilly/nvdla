# TODO: Rewrite examples/conv.py in examples/ref_rk3588_conv.py Style

## Goal

Rewrite `examples/conv.py` from a generic dataclass-heavy nv_small cfg runner into a direct, register-list oriented Python example matching the coding style of `examples/ref_rk3588_conv.py`, while keeping NVDLA VP execution working according to `README.md`.

The rewritten script should still run the default VP test:

`dc_1x1x8_1x1x8x1_int8_0`

and verify the output CRC from the cfg trace.

## Constraints

- Keep this as a pure NVDLA register driver using `/dev/mem`.
- Do not use Rocket DRM ioctls from `ref_rk3588_conv.py`; those are RK3588-specific.
- Preserve VP address relocation:
- `0x80000000` region maps to `0xC0000000`.
- `0x90000000` region maps to `0xD0000000`.
- `0xA0000000` region maps to `0xF0000000`.
- Preserve README VP flow:
- Host starts VP from `examples`.
- Guest logs in as `root / nvdla`.
- Guest mounts repo with `mount -t 9p -o trans=virtio r /mnt`.
- Guest runs script from `/mnt`.
- Preserve cfg compatibility with generated nv_small tests under `ref/vp/tests/nv_small_tests` and fallback `/mnt/nv_small_tests`.
- Keep default test path compatible with the current `examples/conv.py`.

## Target Style

Mirror `examples/ref_rk3588_conv.py` structure:

1. Imports and constants at top.
2. Register constants grouped in a `class reg`.
3. Small ctypes/mmap helper structures and functions.
4. Allocation/loading helpers.
5. Register command builder returning a plain list.
6. Buffer setup.
7. Run function.
8. Output verification.
9. Simple `if __name__ == "__main__"` entry.

For NVDLA VP, the register list should be plain `(offset, value, name)` entries rather than RK3588 encoded 64-bit command words.

## Implementation Plan

### 1. Preserve current behavior before refactor

- Run host-side parse checks before editing:
- `python3 examples/conv.py --list`
- `python3 examples/conv.py --test dc_1x1x8_1x1x8x1_int8_0 --dry-run`
- Record expected dry-run summary:
- test name
- cfg path
- mem_init count
- mem_load count
- register count
- crc count

### 2. Simplify top-level constants

Keep constants near the top in `ref_rk3588_conv.py` style:

- `DEFAULT_TEST`
- `NVDLA_MMIO_BASE`
- `NVDLA_MMIO_SIZE`
- `INTR_STATUS`
- `CBUF_FLUSH_STATUS`
- `DONE_BITS`
- VP memory relocation constants:
- `UVM_FEATURE_BASE = 0x80000000`
- `VP_FEATURE_BASE = 0xC0000000`
- `UVM_WEIGHT_BASE = 0x90000000`
- `VP_WEIGHT_BASE = 0xD0000000`
- `UVM_OUTPUT_BASE = 0xA0000000`
- `VP_OUTPUT_BASE = 0xF0000000`

### 3. Replace dataclasses with simple data tuples or lightweight classes

Remove or minimize:

- `RegWrite`
- `MemLoad`
- `MemInit`
- `CrcCheck`
- `NvSmallTest`

Use direct structures closer to `ref_rk3588_conv.py`:

- mem init entries: `(addr, size)`
- mem load entries: `(addr, path)`
- register entries: `(offset, value, name)`
- CRC entries: `(addr, size, expected)`

Return a dict or tuple from the cfg parser:

`test = parse_test_cfg(tests_root, test_name)`

with keys:

- `name`
- `cfg`
- `mem_inits`
- `mem_loads`
- `initial_regs`
- `enable_regs`
- `crc_checks`

### 4. Add NVDLA register namespace

Add a `class reg` section like `ref_rk3588_conv.py`.

Use static offsets only for frequently referenced control registers:

- `GLB_S_INTR_STATUS = 0x100c`
- `CDMA_S_CBUF_FLUSH_STATUS = 0x300c`

For the full cfg register map, keep dynamic parsing from:

`ref/hw/outdir/nv_small/spec/manual/NVDLA_*.h`

This avoids manually copying every NVDLA register offset and keeps generated nv_small cfg support.

### 5. Keep generic cfg parsing but make it look register-list driven

Retain these core parser behaviors:

- Parse `mem_init(pri_mem, addr, size, ALL_ZERO)`.
- Parse `mem_load(pri_mem, addr, "file.dat")`.
- Parse `reg_write(NVDLA_*.REG_0, value)`.
- Split register writes before and after `poll_reg_equal(NVDLA_CDMA.S_CBUF_FLUSH_STATUS_0,0x1)` or any `D_OP_ENABLE_0`.
- Parse `check_crc(...)`.

Refactor parser names to style-match:

- `parse_reg_offsets()` stays or becomes `load_reg_map()`.
- `parse_cfg()` becomes `load_nv_small_test()`.
- `program_regs()` becomes `write_regs()`.
- `run_test()` becomes `run_nv_small_test()`.

### 6. Make register building explicit

Add a function similar to RK sample's `build_conv_regs(...)`:

`def build_conv_regs(test):`

This function should return:

- `initial_regs`
- `enable_regs`

For now it can forward parsed cfg register lists. The point is to make the execution path read like:

1. `test = load_nv_small_test(...)`
2. `initial_regs, enable_regs = build_conv_regs(test)`
3. `write_buffers(fd, test)`
4. `write_regs(mmio, initial_regs)`
5. `wait_cbuf_flush(mmio)`
6. `write_regs(mmio, enable_regs)`
7. `wait_done(mmio)`
8. `verify_output(fd, test)`

This matches the conceptual flow of `ref_rk3588_conv.py`.

### 7. Rename memory helpers to buffer-style names

Refactor helpers toward RK style:

- `zero_region()` becomes `zero_buffer()`.
- `load_dat()` becomes `load_buffer_dat()`.
- Add `write_buffers(fd, test)` that zeroes every `mem_init`, loads every `.dat`, and zeroes CRC output regions before launch.

Keep `.dat` parsing behavior unchanged.

### 8. Keep mmap helpers NVDLA-specific

Preserve:

- `map_region(fd, addr, size, prot)`
- `write32(mmio, offset, value)`
- `read32(mmio, offset)`

Do not introduce BO allocation or DRM paths.

### 9. Improve run sequence clarity

The main run function should be visibly simple:

`def run_conv_from_cfg(test, dry_run=False):`

Flow:

1. Print test summary.
2. If dry-run, return success.
3. Open `/dev/mem`.
4. Map NVDLA MMIO at `0x10200000`.
5. Call `write_buffers`.
6. Write initial registers.
7. Wait for CBUF flush if needed.
8. Write enable registers.
9. Wait for done interrupt.
10. Verify CRC output.
11. Close mappings and fd.

### 10. Preserve interrupt behavior

Keep current `wait_done()` behavior initially because it sees all done bits and waits briefly after the last observed bit.

Do not simplify it to only SDP done yet, because generic nv_small tests may use CDP/PDP/BDMA/RUBIK.

### 11. Preserve CLI compatibility

Keep current CLI options:

- `--tests-root`
- `--test`
- `--kind`
- `--list`
- `--all`
- `--dry-run`

The default command should still work:

`python3 examples/conv.py --dry-run`

In the VP guest, the default should run:

`python3 /mnt/examples/conv.py`

### 12. Add optional verbose register dump

Consider adding:

`--dump-regs`

When enabled, print register writes as:

`NVDLA_CDMA_D_OP_ENABLE_0 offset=0x3010 value=0x00000001`

Do not enable by default.

### 13. Host-side tests

Run from repo root:

`python3 examples/conv.py --list`

Run default dry-run:

`python3 examples/conv.py --dry-run`

Run all dc dry-runs:

`python3 examples/conv.py --all --kind dc --dry-run`

Run all dry-runs if not too slow:

`python3 examples/conv.py --all --dry-run`

Expected result:

- No unknown register errors.
- Default test reports one CRC check.
- Register counts match pre-refactor dry-run output.

### 14. VP manual test according to README

From host:

```sh
cd examples
SC_SIGNAL_WRITE_CHECK=DISABLE ./vp/aarch64_toplevel --conf ./vp/aarch64_nvdla.lua
```

In VP guest:

```sh
mount -t 9p -o trans=virtio r /mnt
cd /mnt
python3 examples/conv.py --test dc_1x1x8_1x1x8x1_int8_0
```

Expected output:

- Test summary is printed.
- Initial registers are programmed.
- CBUF flush completes or at least does not block forever.
- OP_ENABLE is fired.
- Done interrupt bits are printed.
- CRC line reports PASS with expected CRC `0x8f68a2ae`.
- Final result exits with status `0`.

### 15. Compare against generated C VP test

Run existing README binary path as baseline:

```sh
cd /mnt
./dc_1x1x8_1x1x8x1_int8_0_test
```

Compare behavior with Python runner:

- Same memory relocation.
- Same register offsets and values.
- Same CBUF flush phase.
- Same enable register order.
- Same output CRC.

### 16. Troubleshooting checklist

If VP hangs before CBUF flush:

- Check initial register split around `poll_reg_equal`.
- Check CDMA weight/data addresses after relocation.
- Check `/dev/mem` mapping size and base.
- Compare register list against `examples/dc_1x1x8_1x1x8x1_int8_0_test.c`.

If done interrupt never arrives:

- Check enable register order: SDP, CACC, CMAC_A, CMAC_B, CSC, CDMA.
- Check `S_INTR_MASK` and `S_INTR_STATUS` writes.
- Check whether `wait_done()` is returning too early or too late.

If CRC fails:

- Check `.dat` parser byte order.
- Check output region zeroing before launch.
- Check CRC address relocation from `0xA0000000` to `0xF0000000`.
- Compare output bytes with generated C test if possible.

### 17. Acceptance Criteria

- `examples/conv.py` reads like `examples/ref_rk3588_conv.py`:
- constants first
- register namespace
- helper functions
- build regs
- write buffers
- run function
- main block
- No Rocket-specific code is introduced.
- Existing default test still dry-runs on host.
- VP guest run passes CRC for `dc_1x1x8_1x1x8x1_int8_0`.
- CLI remains compatible with the current script.
