# Conv Shape Strategy ONNX Models

This folder generates ONNX `Conv` models for every literal shape mentioned in
`examples/shape_stratgery.md`: the rare/difficult tables, the related
`pointwise_oc_tile` family, and the `fallback_flat_y_tile` family.

Shape fields follow `examples/ref_rk3588_conv.py`:

- `batch`: ONNX `N`.
- `in_c`, `in_h`, `in_w`: ONNX input `C`, `H`, `W`.
- `out_c`: output channel count and weight tensor dimension 0.
- `weight_in_c`: weight tensor dimension 1, equal to `in_c // groups`.
- `kh`, `kw`: kernel height and width.
- `groups`: ONNX `Conv` group attribute.

All generated models use NCHW, `strides=[1, 1]`, `pads=[0, 0, 0, 0]`, no bias,
and deterministic FP32 weights. Initializer `W` is also listed as a graph input
for ONNC 1.2.0 compatibility.

Generate all models from the ONNC container:

```bash
podman run --rm --user 0 --security-opt label=disable \
  -v /home/fedora/nvdla/ref/onnc-tutorial/models:/tutorial/models \
  -w /tutorial/models/conv_shape_strategy \
  docker.io/onnc/onnc-community:latest \
  python gen_conv_shape_strategy.py
```

Compile one model for the ONNC FP16 / `nv_full`-style VP flow:

```bash
podman run --rm --user 0 --security-opt label=disable \
  -v /home/fedora/nvdla/ref/onnc-tutorial/models:/tutorial/models \
  -w /onnc/onnc-umbrella/build-normal \
  docker.io/onnc/onnc-community:latest \
  sh -c 'onnc -mquadruple nvdla /tutorial/models/conv2d_b1_c144_h28_w28_oc32_wic144_k1x1_g1/conv2d_b1_c144_h28_w28_oc32_wic144_k1x1_g1.onnx && cp out.nvdla /tutorial/models/conv2d_b1_c144_h28_w28_oc32_wic144_k1x1_g1/out.nvdla'
```
