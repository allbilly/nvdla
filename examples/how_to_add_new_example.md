# How To Add A New NVDLA Example From ONNX

This document describes the workflow used to add a new CONV shape example, starting from an ONNX model and ending with a Python replay script that submits captured NVDLA loadable blobs directly through the DRM/KMD interface.

The concrete example used here is `conv_1x1x8`, a `1x1x8 -> 1x1x1` convolution for the `nv_small` VP.

## Overview

The fastest safe workflow is:

1. Create a model folder under `ref/onnc-tutorial/models/`.
2. Generate the ONNX model in that folder.
3. Compile the ONNX model to an NVDLA loadable with ONNC.
4. Copy the loadable and input data into `examples/vp/`.
5. Boot VP using the README SSH flow.
6. First verify the loadable with normal `opendla.ko`.
7. Only after it passes, rerun on a fresh VP with `opendla_logged.ko` to capture blobs and register writes.
8. Write a Python replay script in `examples/` using the same loadable blobs and address ordering.
9. Test the replay script on VP with normal `opendla.ko`.
10. Decode blobs progressively into explicit Python descriptor builders, like `examples/conv.py`.

Do not start from `opendla_logged.ko`. The logged KMD can hang or fail submit because it dumps task buffers and register activity. Always prove the loadable works first with normal `opendla.ko`.

## Prerequisites

This repo assumes the VP and ONNC containers are available locally.

Use `podman` if `docker` is not available.

Check container images:

```bash
podman images
```

Expected useful images:

```text
docker.io/onnc/onnc-community:latest
docker.io/onnc/vp:latest
```

Check basic host tools:

```bash
command -v podman
command -v ssh
command -v sshpass
```

## Directory Layout

Use this layout for a new shape named `<example_name>`:

```text
ref/onnc-tutorial/models/<example_name>/
  gen_<example_name>.py
  <example_name>.onnx
  out.nvdla
  out_nv_small.nvdla
  input*.pgm

examples/vp/
  <example_name>_nv_small.nvdla
  input*.pgm
  <example_name>_opendla.log
  <example_name>_logged_capture.log
  <example_name>_decode.txt

examples/
  <example_name>.py
```

For the `conv_1x1x8` example this became:

```text
ref/onnc-tutorial/models/conv_1x1x8/gen_conv_1x1x8.py
ref/onnc-tutorial/models/conv_1x1x8/conv_1x1x8.onnx
ref/onnc-tutorial/models/conv_1x1x8/out_nv_small.nvdla
ref/onnc-tutorial/models/conv_1x1x8/input1x1.pgm
examples/vp/conv_1x1x8_nv_small.nvdla
examples/vp/input1x1.pgm
examples/conv_1x1x8.py
```

## Step 1: Create A Model Folder

Create a new folder under the ONNC tutorial models directory:

```bash
mkdir -p ref/onnc-tutorial/models/<example_name>
```

Example:

```bash
mkdir -p ref/onnc-tutorial/models/conv_1x1x8
```

## Step 2: Generate The ONNX Model

Add a generator script inside the new model folder.

Example: `ref/onnc-tutorial/models/conv_1x1x8/gen_conv_1x1x8.py`

```python
import numpy as np
import onnx
from onnx import TensorProto, helper


def tensor(name, shape, values):
    return helper.make_tensor(
        name=name,
        data_type=TensorProto.FLOAT,
        dims=shape,
        vals=np.asarray(values, dtype=np.float32).flatten().tolist(),
    )


input_info = helper.make_tensor_value_info("x", TensorProto.FLOAT, [1, 8, 1, 1])
weight_info = helper.make_tensor_value_info("W", TensorProto.FLOAT, [1, 8, 1, 1])
output_info = helper.make_tensor_value_info("y", TensorProto.FLOAT, [1, 1, 1, 1])

weights = np.arange(1, 9, dtype=np.float32).reshape(1, 8, 1, 1)

conv = helper.make_node(
    "Conv",
    inputs=["x", "W"],
    outputs=["y"],
    kernel_shape=[1, 1],
    strides=[1, 1],
    pads=[0, 0, 0, 0],
)

graph = helper.make_graph(
    [conv],
    "conv_1x1x8",
    [input_info, weight_info],
    [output_info],
    [tensor("W", [1, 8, 1, 1], weights)],
)

model = helper.make_model(graph, producer_name="nvdla-pure-registers")
onnx.save(model, "conv_1x1x8.onnx")
```

Important ONNC compatibility rule:

ONNC 1.2.0 expects initializer tensors to also be listed as graph inputs. If you only put `W` in the initializer list, ONNC may fail with:

```text
Fatal: Cannot import onnx model, got exception from ONNX: illegal mode: lose input W
```

Therefore include both:

```python
weight_info = helper.make_tensor_value_info("W", TensorProto.FLOAT, [1, 8, 1, 1])
...
[input_info, weight_info]
```

Generate the ONNX inside the ONNC container because the host may not have `onnx` installed:

```bash
podman run --rm --user 0 --security-opt label=disable \
  -v /home/fedora/nvdla/ref/onnc-tutorial/models:/tutorial/models \
  -w /tutorial/models/<example_name> \
  docker.io/onnc/onnc-community:latest \
  python gen_<example_name>.py
```

Example:

```bash
podman run --rm --user 0 --security-opt label=disable \
  -v /home/fedora/nvdla/ref/onnc-tutorial/models:/tutorial/models \
  -w /tutorial/models/conv_1x1x8 \
  docker.io/onnc/onnc-community:latest \
  python gen_conv_1x1x8.py
```

Expected output file:

```text
ref/onnc-tutorial/models/<example_name>/<example_name>.onnx
```

## Step 3: Compile The NVDLA Loadable

Compile with ONNC.

For this VP, use `-march nv_small`. Without `-march nv_small`, the produced loadable may target the wrong NVDLA configuration and can hang on VP.

Command:

```bash
podman run --rm --user 0 --security-opt label=disable \
  -v /home/fedora/nvdla/ref/onnc-tutorial/models:/tutorial/models \
  -w /onnc/onnc-umbrella/build-normal \
  docker.io/onnc/onnc-community:latest \
  sh -c 'onnc -mquadruple nvdla -march nv_small /tutorial/models/<example_name>/<example_name>.onnx && cp out.nvdla /tutorial/models/<example_name>/out_nv_small.nvdla'
```

Example:

```bash
podman run --rm --user 0 --security-opt label=disable \
  -v /home/fedora/nvdla/ref/onnc-tutorial/models:/tutorial/models \
  -w /onnc/onnc-umbrella/build-normal \
  docker.io/onnc/onnc-community:latest \
  sh -c 'onnc -mquadruple nvdla -march nv_small /tutorial/models/conv_1x1x8/conv_1x1x8.onnx && cp out.nvdla /tutorial/models/conv_1x1x8/out_nv_small.nvdla'
```

Expected output:

```text
ref/onnc-tutorial/models/<example_name>/out_nv_small.nvdla
```

## Step 4: Create Input Data

The tutorial `nvdla_runtime` takes a PGM image and converts it to DIMG internally.

For a `1x1` single-pixel input, create a PGM file:

```text
P5
1 1
255
8
```

For `conv_1x1x8`, this is `input1x1.pgm`.

Why the pixel value is `8`:

The ONNC runtime path converts the single grayscale value to a feature buffer. The observed replay expected output for weights `1..8` is `56.0`, so the loadable/runtime path being captured uses an input activation that yields that result. Always validate expected output from the successful VP run or from the replay output; do not assume host-side ONNX semantics exactly match the runtime PGM conversion.

For larger inputs, follow existing examples like:

```text
examples/vp/input1x5x7.pgm
```

Its format is:

```text
P5
7 5
255
<35 raw bytes>
```

## Step 5: Copy Loadable And Input Into VP Share

Copy files into `examples/vp/` so the VP guest can access them through the 9p mount:

```bash
cp ref/onnc-tutorial/models/<example_name>/out_nv_small.nvdla examples/vp/<example_name>_nv_small.nvdla
cp ref/onnc-tutorial/models/<example_name>/input*.pgm examples/vp/
```

Example:

```bash
cp ref/onnc-tutorial/models/conv_1x1x8/out_nv_small.nvdla examples/vp/conv_1x1x8_nv_small.nvdla
cp ref/onnc-tutorial/models/conv_1x1x8/input1x1.pgm examples/vp/input1x1.pgm
```

## Step 6: Boot VP Using SSH Flow

Use the README flow. Do not automate the serial console with `expect`.

Start VP detached:

```bash
podman run -it --rm -p 6667:6667 -d \
  -v /home/fedora/nvdla/examples/vp/test_Add.nvdla:/usr/local/nvdla/test_Add.nvdla:z \
  -v /home/fedora/nvdla/examples/vp/input1x5x7.pgm:/usr/local/nvdla/input1x5x7.pgm:z \
  -v /home/fedora/nvdla/examples/vp/Image:/usr/local/nvdla/Image:z \
  -v /home/fedora/nvdla/examples/vp/rootfs.ext4:/usr/local/nvdla/rootfs.ext4:z \
  -v /home/fedora/nvdla/examples/vp:/usr/local/nvdla/vp:z \
  -v /home/fedora/nvdla/examples:/usr/local/nvdla/examples:z \
  -w /usr/local/nvdla/ \
  -e SC_SIGNAL_WRITE_CHECK=DISABLE \
  --name nvdla-example-vp \
  docker.io/onnc/vp:latest \
  aarch64_toplevel -c aarch64_nvdla.lua
```

Wait for SSH:

```bash
for i in $(seq 1 90); do
  sshpass -p nvdla ssh \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    -o ConnectTimeout=2 \
    -p 6667 root@127.0.0.1 'true' >/dev/null 2>&1 && break
  sleep 2
done
```

Manual SSH login also works:

```bash
ssh -p 6667 root@127.0.0.1
```

Password:

```text
nvdla
```

## Step 7: Verify With Normal KMD First

This is mandatory.

Use normal `opendla.ko`, not `opendla_logged.ko`:

```bash
sshpass -p nvdla ssh \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  -p 6667 root@127.0.0.1 \
  'mount -t 9p -o trans=virtio r /mnt || true; cd /mnt/vp; insmod drm.ko; insmod opendla.ko; ./nvdla_runtime --loadable <example_name>_nv_small.nvdla --image input1x1.pgm --rawdump; rc=$?; if [ -f output.dimg ]; then echo OUTPUT_DIMG; od -Ax -tx1 -N64 output.dimg; fi; exit $rc' \
  > examples/vp/<example_name>_opendla.log 2>&1
```

Example:

```bash
sshpass -p nvdla ssh \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  -p 6667 root@127.0.0.1 \
  'mount -t 9p -o trans=virtio r /mnt || true; cd /mnt/vp; insmod drm.ko; insmod opendla.ko; ./nvdla_runtime --loadable conv_1x1x8_nv_small.nvdla --image input1x1.pgm --rawdump; rc=$?; if [ -f output.dimg ]; then echo OUTPUT_DIMG; od -Ax -tx1 -N64 output.dimg; fi; exit $rc' \
  > examples/vp/conv_1x1x8_nv_small_opendla.log 2>&1
```

Success looks like:

```text
creating new runtime context...
Emulator starting
ppgminfo 1 1 1
pgm2dimg 1 1 1 1 32 32 32
submitting tasks...
Shutdown signal received, exiting
Test pass
```

If this does not pass, do not continue to logged capture. Fix the ONNX, input, `-march`, or shape first.

Common failure modes:

```text
Fatal: Cannot import onnx model, got exception from ONNX: illegal mode: lose input W
```

Fix: add initializer tensors as graph inputs.

```text
runtime hangs after submitting tasks
```

Likely causes:

1. Compiled without `-march nv_small`.
2. Shape unsupported by emitted loadable.
3. Bad input format.
4. VP/KMD left dirty from an earlier hung logged run.

Restart VP before retrying.

```text
NvDlaSubmit: Error IOCTL failed (Cannot allocate memory)
```

Likely causes:

1. Previous hung run left KMD or engine state bad.
2. Logged KMD was used for a pass/fail test.
3. VP needs restart.

Restart VP and verify with normal `opendla.ko`.

## Step 8: Capture With Logged KMD

Only do this after normal `opendla.ko` passes.

Restart VP before using `opendla_logged.ko`:

```bash
podman stop nvdla-example-vp
```

Then boot VP again using the same command from Step 6.

Run capture:

```bash
sshpass -p nvdla ssh \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  -o ConnectTimeout=4 \
  -p 6667 root@127.0.0.1 \
  'mount -t 9p -o trans=virtio r /mnt || true; cd /mnt/vp; insmod drm.ko; insmod opendla_logged.ko; dmesg -c >/dev/null; ./nvdla_runtime --loadable <example_name>_nv_small.nvdla --image input1x1.pgm --rawdump >/tmp/<example_name>_logged.out 2>&1 & pid=$!; doneflag=0; for i in $(seq 1 60); do kill -0 $pid 2>/dev/null || { doneflag=1; break; }; sleep 1; done; if [ $doneflag -eq 0 ]; then echo TIMEOUT >&2; kill $pid 2>/dev/null || true; fi; cat /tmp/<example_name>_logged.out; dmesg' \
  > examples/vp/<example_name>_logged_capture.log 2>&1
```

Example:

```bash
sshpass -p nvdla ssh \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  -o ConnectTimeout=4 \
  -p 6667 root@127.0.0.1 \
  'mount -t 9p -o trans=virtio r /mnt || true; cd /mnt/vp; insmod drm.ko; insmod opendla_logged.ko; dmesg -c >/dev/null; ./nvdla_runtime --loadable conv_1x1x8_nv_small.nvdla --image input1x1.pgm --rawdump >/tmp/conv_1x1x8_logged.out 2>&1 & pid=$!; doneflag=0; for i in $(seq 1 60); do kill -0 $pid 2>/dev/null || { doneflag=1; break; }; sleep 1; done; if [ $doneflag -eq 0 ]; then echo TIMEOUT >&2; kill $pid 2>/dev/null || true; fi; cat /tmp/conv_1x1x8_logged.out; dmesg' \
  > examples/vp/conv_1x1x8_logged_capture.log 2>&1
```

It is acceptable for the logged run to timeout or fail. The purpose is to capture:

```text
nvdla_blob[N] ... full_4096_bytes
nvdla_dma_address index=...
nvdla_reg_write offset=... value=...
```

For `conv_1x1x8`, the logged capture showed these important buffers:

```text
nvdla_blob[1] ... weights/tb-0
nvdla_blob[2] ... input tensor
nvdla_blob[3] ... output tensor
nvdla_blob[7] ... surface list
nvdla_blob[8] ... scratch/output-related buffer
```

And register writes like:

```text
nvdla_reg_write offset=0x00009004 value=0x00000000
nvdla_reg_write offset=0x00007004 value=0x00000000
nvdla_reg_write offset=0x00008004 value=0x00000000
nvdla_reg_write offset=0x00006004 value=0x00000000
nvdla_reg_write offset=0x00005004 value=0x00000000
...
nvdla_reg_write offset=0x0000b038 value=0x00000001
...
nvdla_reg_write offset=0x00005010 value=0x00000001
```

## Step 9: Parse The Loadable

Use `examples/vp/parse.py` to inspect the generated loadable:

```bash
python3 examples/vp/parse.py ref/onnc-tutorial/models/<example_name>/out_nv_small.nvdla \
  > examples/vp/<example_name>_decode.txt
```

Example:

```bash
python3 examples/vp/parse.py ref/onnc-tutorial/models/conv_1x1x8/out_nv_small.nvdla \
  > examples/vp/conv_1x1x8_decode.txt
```

Current parser behavior:

1. It can parse the loadable structure.
2. It can decode common op descriptors.
3. It can decode SDP descriptors/registers.
4. CONV register decode is still not fully implemented.

Expected partial decode for `conv_1x1x8`:

```text
task[0] NVDLA decoded registers
layer[0] op_type=1:CONV
  register decode for this op type is not implemented yet
layer[1] op_type=2:SDP
  cube src_data: type=2 address=-1 offset=0 size=32 dims=1x1x1 line=0 surf=0
  cube dst_data: type=0 address=3 offset=0 size=32 dims=1x1x1 line=32 surf=32
  regs:
    NVDLA_SDP_D_DATA_CUBE_WIDTH_0 @ 0xb03c = 0x00000001 (1)
    ...
```

Note about dimensions:

Loadable descriptors store logical dimensions as normal values, but many hardware registers are programmed with `dimension - 1`. When comparing parser output to `nvdla_reg_write`, expect this mismatch. For example, a decoded `1x1x1` cube can produce register writes of `0x00000000` for width/height/channel fields.

## Step 10: Write The Python Replay Script

Create `examples/<example_name>.py`.

Recommended first version:

1. Reuse the DRM/KMD ioctl structs from `examples/conv.py` or `examples/conv_1x1x8.py`.
2. Load the validated `.nvdla` file and extract blobs using `examples/vp/parse.py`.
3. Allocate GEM buffers in the same order as the UMD runtime.
4. Fill each buffer with the captured or loadable data.
5. Submit with the captured task address list order.
6. Read output buffer and compare to expected result.

For `conv_1x1x8`, the loadable metadata is:

```text
task address_list: [4, 1, 2, 3, 4, 5, 6, 7, 8]
```

The replay uses UMD allocation order:

```python
sizes = [4096, 128, 40, 72, 232, 1288, 4096, 32, 32]
contents = [
    scratch0,
    weights_tb0,
    net_desc,
    dep_graph,
    op_list,
    surf_list,
    scratch1,
    input_tensor,
    output_tensor,
]
address_slots = [3, 8, 2, 9, 3, 4, 5, 6, 7]
```

Why `address_slots` differs from the loadable `address_list`:

The loadable task address list contains address IDs. The Python replay needs the actual UMD/GEM allocation slot numbers. For `conv_1x1x8`, the mapping is:

```text
loadable address IDs: [4, 1, 2, 3, 4, 5, 6, 7, 8]
UMD/GEM slot order:   [3, 8, 2, 9, 3, 4, 5, 6, 7]
```

Derive this from `memory_list`, `address_list`, and the UMD allocation order. You can inspect it with:

```bash
python3 - <<'PY'
import sys
sys.path.insert(0, '/home/fedora/nvdla/examples/vp')
import parse as p
with open('/home/fedora/nvdla/ref/onnc-tutorial/models/conv_1x1x8/out_nv_small.nvdla', 'rb') as f:
    loadable = p.parse_loadable(f.read())
print('tasks')
for task in loadable['task_list']:
    print(task)
print('memory')
for memory in loadable['memory_list']:
    print(memory)
print('addresses')
for address in loadable['address_list']:
    print(address)
print('blobs')
for blob in loadable['blob_list']:
    print(blob['name'], blob['size'], len(blob['data']))
PY
```

## Step 11: Test The Replay Script On VP

Restart VP and use normal `opendla.ko` again.

```bash
sshpass -p nvdla ssh \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  -p 6667 root@127.0.0.1 \
  'mount -t 9p -o trans=virtio r /mnt || true; cd /mnt; insmod vp/drm.ko; insmod vp/opendla.ko; python3 examples/<example_name>.py' \
  > examples/vp/<example_name>_replay_opendla.log 2>&1
```

Example:

```bash
sshpass -p nvdla ssh \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  -p 6667 root@127.0.0.1 \
  'mount -t 9p -o trans=virtio r /mnt || true; cd /mnt; insmod vp/drm.ko; insmod vp/opendla.ko; python3 examples/conv_1x1x8.py' \
  > examples/vp/conv_1x1x8_replay_opendla.log 2>&1
```

Expected result:

```text
SUBMIT ret=0
conv_1x1x8 NPU=56.0 expected=56.0 PASS output=0053000000000000000000000000000000000000000000000000000000000000
```

## Step 12: Decode Opaque Blobs Into Python Builders

Once replay works, replace opaque blobs gradually.

Recommended order:

1. Keep loading the `.nvdla` initially for safety.
2. Decode and rebuild `network_descriptor` first. It is small and stable.
3. Decode and rebuild `dependency_graph` next.
4. Decode and rebuild SDP op/surface descriptors.
5. Decode CONV op/surface descriptors.
6. Only then replace weight/input packing with generated tensors.

Do not try to decode everything in one step. Keep each step tested with VP.

For SDP, `examples/vp/parse.py` already has helpers:

```python
parse_network_desc(data)
parse_common_op(data, idx)
parse_sdp_surface(data, idx)
parse_sdp_op_desc(data, idx)
sdp_op_regs(surface, op)
```

For CONV, decoding still needs to be implemented. Use these references:

```text
ref/hw/cmod/cdma/*
ref/hw/cmod/csc/NV_NVDLA_csc.cpp
ref/hw/cmod/cmac/NV_NVDLA_cmac.cpp
ref/hw/cmod/cacc/NV_NVDLA_cacc.cpp
ref/hw/cmod/*/*_reg_model.cpp
ref/hw/vmod/nvdla/NV_NVDLA_*_regfile.v
ref/sw/kmd
```

When unsure about hardware behavior, ask DeepWiki for:

```text
nvdla/hw
nvdla/sw
ONNC/onnc
ONNC/onnc-tutorial
```

## Cleaning Up VP

Stop VP after tests:

```bash
podman stop nvdla-example-vp
```

If you used a different container name, inspect and stop it:

```bash
podman ps
podman stop <name-or-id>
```

## Checklist For A New Shape

Use this checklist before considering a new example done:

```text
[ ] New folder exists under ref/onnc-tutorial/models/<example_name>/
[ ] ONNX generator is committed
[ ] ONNX file is generated
[ ] Loadable is compiled with -march nv_small
[ ] Input PGM exists
[ ] Loadable and input are copied into examples/vp/
[ ] VP normal opendla.ko runtime says Test pass
[ ] Logged KMD capture exists, even if it times out
[ ] Python replay script exists under examples/
[ ] Python replay passes on VP with normal opendla.ko
[ ] Decode output exists for loadable inspection
[ ] Opaque blobs are progressively decoded where practical
```

## Current Known Limitations

The `conv_1x1x8` example added with this workflow is an FP16 ONNC/NVDLA loadable for the correct shape. It is not yet a true INT8 parity replacement for `dc_1x1x8_1x1x8x1_int8_0`.

To get true INT8 parity, use one of these paths:

1. Generate a quantized INT8 ONNX that ONNC lowers to INT8 NVDLA CONV.
2. Start from the existing direct trace `dc_1x1x8_1x1x8x1_int8_0_test.c` and port/register-replay it.
3. Decode the ONNC CONV descriptors fully, then manually adjust precision and packing once the register meanings are understood.

Do not mix the goals:

```text
ONNX -> ONNC -> loadable replay
```

is the fastest way to get a working shape baseline.

```text
pure register generator
```

requires full CDMA/CSC/CMAC/CACC/SDP register and tensor layout decoding.
