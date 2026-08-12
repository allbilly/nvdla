#!/bin/sh
set -eu

ASSET_DIR="${ASSET_DIR:-/mnt/nvdla}"
ENV_DIR="${ENV_DIR:-/opt/nvdla-ml}"

if [ ! -f "$ASSET_DIR/uv" ]; then
    echo "error: $ASSET_DIR/uv not found; mount /dev/vdb at $ASSET_DIR first" >&2
    exit 1
fi

cp "$ASSET_DIR/uv" /usr/bin/uv
chmod 755 /usr/bin/uv

uv python install 3.11
if [ ! -x "$ENV_DIR/bin/python" ]; then
    uv venv --python 3.11 "$ENV_DIR"
fi
uv pip install --python "$ENV_DIR/bin/python" \
    'numpy==1.26.4' \
    'tinygrad==0.13.0' \
    'torch==2.5.1'

time "$ENV_DIR/bin/python" -c \
    'import torch; print("torch", torch.__version__, torch.arange(4) + 1)'
DEV=PYTHON time "$ENV_DIR/bin/python" -c \
    'from tinygrad import Device, Tensor; print("tinygrad", Device.DEFAULT, (Tensor([1,2,3]) + Tensor([4,5,6])).numpy().tolist())'
