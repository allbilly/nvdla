import argparse
import os
import shutil

import numpy as np
import onnx
from onnx import TensorProto, helper


SHAPES = [
    # pointwise_y_tile_hardcoded from examples/shape_stratgery.md
    dict(strategy="pointwise_y_tile_hardcoded", name="conv2d_b1_c144_h28_w28_oc32_wic144_k1x1_g1", batch=1, in_c=144, in_h=28, in_w=28, out_c=32, weight_in_c=144, kh=1, kw=1, groups=1),
    dict(strategy="pointwise_y_tile_hardcoded", name="conv2d_b1_c192_h28_w28_oc32_wic192_k1x1_g1", batch=1, in_c=192, in_h=28, in_w=28, out_c=32, weight_in_c=192, kh=1, kw=1, groups=1),

    # Related pointwise shapes mentioned in examples/shape_stratgery.md.
    dict(strategy="pointwise_oc_tile_related", name="conv2d_b1_c96_h56_w56_oc24_wic96_k1x1_g1", batch=1, in_c=96, in_h=56, in_w=56, out_c=24, weight_in_c=96, kh=1, kw=1, groups=1),
    dict(strategy="pointwise_oc_tile_related", name="conv2d_b1_c144_h56_w56_oc24_wic144_k1x1_g1", batch=1, in_c=144, in_h=56, in_w=56, out_c=24, weight_in_c=144, kh=1, kw=1, groups=1),
    dict(strategy="pointwise_oc_tile_related", name="conv2d_b1_c192_h28_w28_oc16_wic192_k1x1_g1", batch=1, in_c=192, in_h=28, in_w=28, out_c=16, weight_in_c=192, kh=1, kw=1, groups=1),
    dict(strategy="pointwise_oc_tile_related", name="conv2d_b1_c256_h28_w28_oc32_wic256_k1x1_g1", batch=1, in_c=256, in_h=28, in_w=28, out_c=32, weight_in_c=256, kh=1, kw=1, groups=1),

    # spatial_im2col from examples/shape_stratgery.md
    dict(strategy="spatial_im2col", name="b1_c160_h14_w14_oc320_wic160_k3x3_g1_s1_pvalid", batch=1, in_c=160, in_h=14, in_w=14, out_c=320, weight_in_c=160, kh=3, kw=3, groups=1),
    dict(strategy="spatial_im2col", name="b1_c160_h7_w7_oc320_wic160_k3x3_g1_s1_pvalid", batch=1, in_c=160, in_h=7, in_w=7, out_c=320, weight_in_c=160, kh=3, kw=3, groups=1),
    dict(strategy="spatial_im2col", name="b1_c192_h7_w7_oc384_wic192_k3x3_g1_s1_pvalid", batch=1, in_c=192, in_h=7, in_w=7, out_c=384, weight_in_c=192, kh=3, kw=3, groups=1),
    dict(strategy="spatial_im2col", name="b1_c256_h10_w10_oc512_wic256_k3x3_g1_s1_pvalid", batch=1, in_c=256, in_h=10, in_w=10, out_c=512, weight_in_c=256, kh=3, kw=3, groups=1),
    dict(strategy="spatial_im2col", name="b1_c128_h5_w5_oc256_wic128_k3x3_g1_s1_pvalid", batch=1, in_c=128, in_h=5, in_w=5, out_c=256, weight_in_c=128, kh=3, kw=3, groups=1),
    dict(strategy="spatial_im2col", name="b1_c128_h3_w3_oc256_wic128_k3x3_g1_s1_pvalid", batch=1, in_c=128, in_h=3, in_w=3, out_c=256, weight_in_c=128, kh=3, kw=3, groups=1),
    dict(strategy="spatial_im2col", name="b1_c3_h320_w320_oc32_wic3_k3x3_g1_s1_pvalid", batch=1, in_c=3, in_h=320, in_w=320, out_c=32, weight_in_c=3, kh=3, kw=3, groups=1),
    dict(strategy="spatial_im2col", name="b1_c16_h160_w160_oc128_wic16_k3x3_g1_s1_pvalid", batch=1, in_c=16, in_h=160, in_w=160, out_c=128, weight_in_c=16, kh=3, kw=3, groups=1),
    dict(strategy="spatial_im2col", name="b1_c16_h80_w80_oc128_wic16_k5x5_g1_s1_pvalid", batch=1, in_c=16, in_h=80, in_w=80, out_c=128, weight_in_c=16, kh=5, kw=5, groups=1),
    dict(strategy="spatial_im2col", name="b1_c40_h40_w40_oc160_wic40_k3x3_g1_s1_pvalid", batch=1, in_c=40, in_h=40, in_w=40, out_c=160, weight_in_c=40, kh=3, kw=3, groups=1),
    dict(strategy="spatial_im2col", name="b1_c72_h20_w20_oc288_wic72_k3x3_g1_s1_pvalid", batch=1, in_c=72, in_h=20, in_w=20, out_c=288, weight_in_c=72, kh=3, kw=3, groups=1),
]

SHAPES.append(
    dict(strategy="fallback_flat_y_tile", name="conv2d_b1_c3_h52_w52_oc6_wic3_k1x1_g1", batch=1, in_c=3, in_h=52, in_w=52, out_c=6, weight_in_c=3, kh=1, kw=1, groups=1)
)

SHAPES += [
    dict(strategy="fallback_flat_y_tile", name="conv2d_1x3_{}x{}_k1".format(n, n), batch=1, in_c=3, in_h=n, in_w=n, out_c=6, weight_in_c=3, kh=1, kw=1, groups=1)
    for n in range(54, 74, 2)
]


def tensor(name, shape, values):
    return helper.make_tensor(
        name=name,
        data_type=TensorProto.FLOAT,
        dims=shape,
        vals=np.asarray(values, dtype=np.float32).flatten().tolist(),
    )


def deterministic_weights(shape):
    count = int(np.prod(shape))
    values = (np.arange(count, dtype=np.float32) % 17) - 8
    values /= 16.0
    return values.reshape(shape)


def make_model(shape):
    batch = shape["batch"]
    in_c = shape["in_c"]
    in_h = shape["in_h"]
    in_w = shape["in_w"]
    out_c = shape["out_c"]
    weight_in_c = shape["weight_in_c"]
    kh = shape["kh"]
    kw = shape["kw"]
    groups = shape["groups"]
    out_h = in_h - kh + 1
    out_w = in_w - kw + 1

    if weight_in_c != in_c // groups:
        raise ValueError("{}: weight_in_c must equal in_c // groups".format(shape["name"]))

    input_info = helper.make_tensor_value_info("x", TensorProto.FLOAT, [batch, in_c, in_h, in_w])
    weight_info = helper.make_tensor_value_info("W", TensorProto.FLOAT, [out_c, weight_in_c, kh, kw])
    output_info = helper.make_tensor_value_info("y", TensorProto.FLOAT, [batch, out_c, out_h, out_w])
    weights = deterministic_weights([out_c, weight_in_c, kh, kw])

    conv = helper.make_node(
        "Conv",
        inputs=["x", "W"],
        outputs=["y"],
        kernel_shape=[kh, kw],
        strides=[1, 1],
        pads=[0, 0, 0, 0],
        group=groups,
    )
    graph = helper.make_graph(
        [conv],
        shape["name"],
        [input_info, weight_info],
        [output_info],
        [tensor("W", [out_c, weight_in_c, kh, kw], weights)],
    )
    model = helper.make_model(graph, producer_name="nvdla-pure-registers")
    onnx.checker.check_model(model)
    return model


def write_model(shape, out_dir):
    model_dir = os.path.join(out_dir, shape["name"])
    if not os.path.isdir(model_dir):
        os.makedirs(model_dir)
    model_path = os.path.join(model_dir, "{}.onnx".format(shape["name"]))
    onnx.save(make_model(shape), model_path)
    return model_path


def input_pgm_name(shape):
    return "input{}x{}.pgm".format(shape["in_h"], shape["in_w"])


def write_input_pgm(shape, vp_dir):
    path = os.path.join(vp_dir, input_pgm_name(shape))
    width = shape["in_w"]
    height = shape["in_h"]
    header = "P5\n{} {}\n255\n".format(width, height).encode("ascii")
    data = (np.arange(width * height, dtype=np.uint32) * 13 + width + height) % 256
    with open(path, "wb") as f:
        f.write(header)
        f.write(data.astype(np.uint8).tostring())
    return path


def prepare_vp_assets(shapes, models_dir, vp_dir):
    if not os.path.isdir(vp_dir):
        os.makedirs(vp_dir)

    manifest_path = os.path.join(vp_dir, "shape_strategy_manifest.txt")
    seen_inputs = set()
    with open(manifest_path, "w") as manifest:
        for shape in shapes:
            name = shape["name"]
            src = os.path.join(models_dir, name, "out.nvdla")
            if not os.path.exists(src):
                raise SystemExit("missing compiled loadable: {}".format(src))
            loadable_name = "{}.nvdla".format(name)
            dst = os.path.join(vp_dir, loadable_name)
            shutil.copyfile(src, dst)

            pgm_name = input_pgm_name(shape)
            if pgm_name not in seen_inputs:
                write_input_pgm(shape, vp_dir)
                seen_inputs.add(pgm_name)
            manifest.write("{} {} {}\n".format(shape["strategy"], loadable_name, pgm_name))
    return manifest_path


def main():
    parser = argparse.ArgumentParser(description="Generate ONNX Conv models for shape-strategy probe cases.")
    parser.add_argument("--out-dir", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser.add_argument("--prepare-vp-dir", help="Copy compiled loadables and generated PGM inputs into a VP share.")
    parser.add_argument("--only", action="append", default=[], help="Generate one shape name; repeatable.")
    parser.add_argument("--list", action="store_true", help="List shapes and exit.")
    args = parser.parse_args()

    selected = [shape for shape in SHAPES if not args.only or shape["name"] in args.only]
    if args.list:
        for shape in selected:
            print("{} {}".format(shape["strategy"], shape["name"]))
        return
    if args.only and len(selected) != len(args.only):
        known = set(shape["name"] for shape in SHAPES)
        missing = sorted(set(args.only) - known)
        raise SystemExit("unknown shape(s): {}".format(", ".join(missing)))

    if args.prepare_vp_dir:
        print(prepare_vp_assets(selected, args.out_dir, args.prepare_vp_dir))
        return

    for shape in selected:
        path = write_model(shape, args.out_dir)
        print(path)


if __name__ == "__main__":
    main()
