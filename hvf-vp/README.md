# Native NVDLA VP on Apple Silicon

This directory runs the NVDLA `nvdlav1` branch's `nv_full` C-model next to a
native Apple Silicon QEMU CPU accelerated by macOS Hypervisor.framework (HVF).
It replaces the old embedded QEMU/GreenSocs CPU side with Qualcomm QBox while
retaining the legacy VP memory map, NVDLA CSB window, DMA memory, and interrupt.

The full configuration is intentional: `simple_add.py` and `conv.py` program
FP16 tensors with 32-byte feature atoms and were validated against `nv_full` in
the archived artifact notes. Upstream `nv_small` is INT8-only and is not
compatible with those FP16 descriptors. `conv_1x1x8.py` originated from an
`nv_small` loadable, but its embedded operation descriptors also select FP16.

## 1. Install prerequisites

Use an Apple Silicon Mac and Homebrew:

```sh
uname -m
# arm64

brew install asio cmake dtc e2fsprogs gcc libelf libslirp libzip lld meson ninja sdl2
brew tap quic/quic https://github.com/quic/homebrew-quic.git
brew trust quic/quic
brew install quic/quic/virglrenderer
```

Install and start Docker Desktop, or install Podman. The container engine is
used only to extract the matching guest kernel, the official ARM64 `uv` binary,
and to create an ext4 asset image. It does not run the VP. The final QBox/QEMU
process is native macOS ARM64, which is what lets it use HVF.

## 2. Build

From the repository root:

```sh
./hvf-vp/build.sh
```

The first build clones pinned QBox and `nvdla/hw` revisions, compiles the
`nv_full` C-model and native QBox platform, extracts Linux 4.13.3, and prepares
the guest disks. It can take several minutes; later builds reuse the downloaded
source and compiled objects.

The writable guest filesystem is expanded to a sparse 4 GiB image so there is
room for Python packages. It consumes host storage as data is written rather
than allocating all 4 GiB immediately.

## 3. Boot and run NVDLA

```sh
./hvf-vp/run.sh
```

The launcher reports `QEMU accel=hvf`. Log in at the guest console as `root`
with no password. User-mode networking is enabled, and host TCP port 6667 is
forwarded to guest SSH port 22.

Mount the read-only asset disk, load the matching driver, and run the examples:

```sh
mkdir -p /mnt/nvdla
mount /dev/vdb /mnt/nvdla
insmod /mnt/nvdla/drm.ko
insmod /mnt/nvdla/opendla.ko

python3 /mnt/nvdla/simple_add.py
python3 /mnt/nvdla/conv.py
python3 /mnt/nvdla/conv_1x1x8.py
```

A successful probe prints `Initialized nvdla` for `10200000.nvdla` and creates
`/dev/dri/card0` and `/dev/dri/renderD128`. Each example compares the simulated
NVDLA result with its expected result and prints `PASS` before exiting zero.

## 4. Install uv, Torch, and tinygrad

The asset disk includes the official statically linked ARM64 `uv` 0.12.3
binary. After mounting `/dev/vdb`, run this once:

```sh
sh /mnt/nvdla/setup-ml.sh
```

It installs a `uv`-managed Python 3.11 and a persistent environment at
`/opt/nvdla-ml` with the versions verified on this old Linux image:

- Torch 2.5.1
- tinygrad 0.13.0
- NumPy 1.26.4

The setup ends by importing Torch and evaluating tensors with Torch and
tinygrad. tinygrad uses `DEV=PYTHON` because this minimal Buildroot guest does
not provide Clang, which its native CPU backend normally invokes.

Use the environment again after a reboot with:

```sh
/opt/nvdla-ml/bin/python -c 'import torch; print(torch.__version__)'
DEV=PYTHON /opt/nvdla-ml/bin/python -c \
  'from tinygrad import Tensor; print((Tensor([1,2,3]) + Tensor([4,5,6])).numpy())'
```

Shut down cleanly to save the persistent root filesystem:

```sh
poweroff
```

## Build details

The checked-in `examples/vp/Image` is Linux 4.13.11, but the supplied kernel
modules have Linux 4.13.3 vermagic. The build extracts the matching 4.13.3
kernel from `onnc/vp:latest`. The bundled `test_Add.nvdla` and
`input1x5x7.pgm` are retained for runtime compatibility; the self-contained
Python register drivers are the primary HVF smoke tests.

The writable root filesystem persists at `.build/guest/rootfs-rw.ext4`.
Deleting only that generated file and rerunning `build.sh` creates a fresh
guest. Downloaded and compiled host dependencies stay cached under
`hvf-vp/.build/`.
