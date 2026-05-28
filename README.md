# NVDLA
This repo aims do pure registers programming on the NVDLA NPU. 

## 1. Quick start

```bash
docker run -it --rm -p 6667:6667 -d \
    -v ./examples/vp/test_Add.nvdla:/usr/local/nvdla/test_Add.nvdla:z \
    -v ./examples/vp/input1x5x7.pgm:/usr/local/nvdla/input1x5x7.pgm:z \
    -v ./examples/vp/rootfs.ext4:/usr/local/nvdla/rootfs.ext4:z \
    -v ./examples/vp:/usr/local/nvdla/vp:z \
    -w /usr/local/nvdla/ \
    -e SC_SIGNAL_WRITE_CHECK=DISABLE \
    onnc/vp aarch64_toplevel -c aarch64_nvdla.lua

ssh -p 6667 root@127.0.0.1

mount -t 9p -o trans=virtio r /mnt 
cd /mnt && insmod drm.ko && insmod opendla.ko
./nvdla_runtime --loadable test_Add.nvdla --image input1x5x7.pgm --rawdump
cat output.dimg
```

## 2. Parse nvdla loadable

```bash
python parse.py examples/vp/test_Add.nvdla
```

## Note
build rootfs.ext4
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