# NVDLA
This repo aims do pure registers programming on the NVDLA NPU. 

## 1. Quick start

```bash
docker run -it --rm -p 6667:6667 -d \
    -v ./examples/vp/test_Add.nvdla:/usr/local/nvdla/test_Add.nvdla:z \
    -v ./examples/vp/input1x5x7.pgm:/usr/local/nvdla/input1x5x7.pgm:z \
    -v ./examples/vp/Image:/usr/local/nvdla/Image:z \
    -v ./examples/vp/rootfs.ext4:/usr/local/nvdla/rootfs.ext4:z \
    -v ./examples/vp:/usr/local/nvdla/vp:z \
    -v ./examples:/usr/local/nvdla/examples:z \
    -w /usr/local/nvdla/ \
    -e SC_SIGNAL_WRITE_CHECK=DISABLE \
    onnc/vp aarch64_toplevel -c aarch64_nvdla.lua

ssh -p 6667 root@127.0.0.1

mount -t 9p -o trans=virtio r /mnt 
cd /mnt && insmod drm.ko && insmod opendla.ko

./nvdla_runtime --loadable test_Add.nvdla --image input1x5x7.pgm --rawdump
cat output.dimg

python3 examples/simple_add.py
python3 nvdla_toy_sim.py
python3 examples/conv.py
python3 examples/conv_1x1x8.py

```

Open `nvdla_components.html` in a browser for an interactive explanation of the NVDLA blocks.
Open `cmac.html` for a focused interactive explanation of the CMAC multiply-add engine.
Open `ew_interactive.html` for a pitorch-style interactive NVDLA/SDP EW walkthrough.

Note:
- `insmod vp/opendla_logged.ko`, use this kmd to capture registers

## 2. Parse nvdla loadable

```bash
python parse.py examples/vp/test_Add.nvdla
```

## Note
- onnc can only compile loadable in fp16
- onnc/vp https://hub.docker.com/r/onnc/vp
  - The Docker image was built base on nvdla/vp:1.3 with the following updates
  - Source repository is on GitHub (nvdla/sw), revision = 1f44c24
  - User Mode Driver
    - Change hardcoded channel order for JPEG files.
    - Increase address list entry size to 30,000 to support bigger networks.
    - Add EMU operation support for ONNX operator, Log.
  - Kernel Mode Driver
    - Increase address list entry size to 30,000 to support bigger networks.
  - Affected files
    - /usr/local/nvdla/libnvdla_runtime.so
    - /usr/local/nvdla/nvdla_runtime
    - /usr/local/nvdla/opendla.ko
- build rootfs.ext4
```

podman run --rm -it --security-opt label=disable \
  -v /home/fedora/nvdla:/nvdla \
  -w /nvdla/buildroot-2017.11-rc1-clean \
  nvdla-build \
  make menuconfig

podman run --rm --security-opt label=disable -v /home/fedora/nvdla:/nvdla -w /nvdla/buildroot-2017.11-rc1-clean nvdla-build bash -c "make olddefconfig && make -j\$(($(nproc)-1)) WGET='wget --no-config'"
```

# reference
- https://github.com/nvdla/vp
- https://github.com/ONNC/onnc-tutorial/blob/master/lab_1_Environment_Setup/lab_1.md
- https://zhuanlan.zhihu.com/p/630822241
- https://zhuanlan.zhihu.com/p/378122624
- https://github.com/LeiWang1999/nvdla-parser
- https://nvdla.org/hw/v2/environment_setup_guide.html
- https://github.com/prasshantg/personal
