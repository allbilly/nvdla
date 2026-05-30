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

# One output channel, eight input channels, 1x1 kernel.  The asymmetric weights
# make it easy to identify whether all input channels participate.
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
