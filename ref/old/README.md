# NVDLA examples

These examples are standalone Python files for the NVDLA VP. The RK3588 files
in this directory are coding-style references only.

- `simple_add.py` runs ONNC `test_Add` through `nvdla_runtime`, preferring
  `opendla_small.ko` for the nv_small VP.
- `conv.py` parses and runs generated nv_small register traces directly through
  `/dev/mem`; use `--list`, `--kind`, `--test`, `--all`, and `--dry-run`.
- `vp_test_add_smoke.py` is a host-side pexpect smoke test for the VP
  `test_Add` path.
- `inspect_loadable.py` dumps enough FlatBuffer/loadable state to debug NVDLA
  descriptors without `flatc`.
- `patch_test_add_loadable.py` converts the historical `test_Add` SDP surface
  blob to the current nv_small KMD `dla_data_cube` layout.

Current `test_Add` status: the wrong KMD map issue is fixed by using
`opendla_small.ko`. The CVIF DMA response width assertion is fixed by the
`nvdla_config.h` fallback change and rebuilt C-model library. The remaining VP
issue is that `nvdla_runtime` waits for task completion after SDP is programmed
and enabled. The descriptor ABI mismatch is identified: the tutorial loadable
uses the older 28-byte `dla_data_cube`, while the current KMD expects 32 bytes.
The patcher fixes the decoded SDP dimensions to `7x5x1`; the next VP failure is
an MCIF WDMA command/data sequencing assertion on the tiny FP16 SDP output.
