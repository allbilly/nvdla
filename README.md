# NVDLA
This repo aims do pure registers programming on the NVDLA NPU. 

## 1. Quick start

Run test case in VP
```
cd examples
SC_SIGNAL_WRITE_CHECK=DISABLE ./vp/aarch64_toplevel --conf ./vp/aarch64_nvdla.lua

# Login: root / nvdla
mount -t 9p -o trans=virtio r /mnt && cd /mnt
./dc_1x1x8_1x1x8x1_int8_0_test
```

## 2. Parse nvdla loadable
https://zhuanlan.zhihu.com/p/378122624

# reference
- https://github.com/nvdla/vp
- https://github.com/ONNC/onnc-tutorial/blob/master/lab_1_Environment_Setup/lab_1.md
- https://zhuanlan.zhihu.com/p/630822241
- https://zhuanlan.zhihu.com/p/378122624
- https://github.com/LeiWang1999/nvdla-parser
- https://nvdla.org/hw/v2/environment_setup_guide.html
- https://github.com/prasshantg/personal