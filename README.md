# NVDLA
This repo aims do pure registers programming on the NVDLA NPU. 

# 1. Compile with ONNC

Old vp image was built on ubuntu 14.04 with no arm supprot, so you must run on x86_64
note: docker mount :z is needed for centos/fedora

Clone repo and run onnc/onnc-community container
```bash
cd ref
git clone https://github.com/ONNC/onnc
git clone https://github.com/ONNC/onnc-tutorial
cd ../
docker run -ti --rm -v ./ref/onnc:/onnc/onnc:z -v ./ref/onnc-tutorial:/tutorial:z onnc/onnc-community
```

within container
```bash
cd /onnc/onnc-umbrella/build-normal && smake -j8 install
```

# 1.1 Compile lenet

```bash
onnc -mquadruple nvdla /tutorial/models/lenet/lenet.onnx
sudo mv out.nvdla /tutorial/models/lenet/
```

# 2. Compile simple ADD with ONNC

```bash
onnc -mquadruple nvdla /tutorial/models/test_Add/test_Add.onnx
sudo mv out.nvdla /tutorial/models/test_Add/
```

# 3. Run .nvdla

## 3.1 onnc/vp

run inference of the compiled lenet
nvdla login: root
Password: nvdla

```bash
docker run -ti --rm -v ./ref/onnc-tutorial:/tutorial:z onnc/vp
cd /usr/local/nvdla
cp /tutorial/models/lenet/* .
aarch64_toplevel -c aarch64_nvdla.lua

mount -t 9p -o trans=virtio r /mnt && cd /mnt
insmod drm.ko && insmod opendla.ko
./nvdla_runtime --loadable out.nvdla --image input0.pgm --rawdump
cat output.dimg
poweroff
```

run inference of the compiled simple ADD, repeat same commands except
```bash
docker run -ti --rm -v ./ref/onnc-tutorial:/tutorial:z onnc/vp
cd /usr/local/nvdla
cp /tutorial/models/test_Add/input1x5x7.pgm .
cp /tutorial/models/test_Add/out.nvdla .
cp /tutorial/models/test_Add/test_Add.nvdla .
aarch64_toplevel -c aarch64_nvdla.lua

mount -t 9p -o trans=virtio r /mnt && cd /mnt
insmod drm.ko && insmod opendla.ko
./nvdla_runtime --loadable out.nvdla --image input1x5x7.pgm --rawdump
cat output.dimg
rm output.dimg

./nvdla_runtime --loadable test_Add.nvdla --image input1x5x7.pgm --rawdump
cat output.dimg
poweroff
```



## 3.2 nvdla/vp

note nvdla/vp cannot run lenet.nvdla out.nvdla compiled from ONNC
maybe it was compiled nv_small, or input problem?

# reference
https://github.com/ONNC/onnc-tutorial/blob/master/lab_1_Environment_Setup/lab_1.md
https://zhuanlan.zhihu.com/p/630822241
