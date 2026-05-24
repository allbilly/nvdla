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

Within container, compile Lenet and simple ADD
```bash
cd /onnc/onnc-umbrella/build-normal && smake -j8 install

onnc -mquadruple nvdla /tutorial/models/lenet/lenet.onnx
sudo mv out.nvdla /tutorial/models/lenet/

onnc -mquadruple nvdla /tutorial/models/test_Add/test_Add.onnx
sudo mv out.nvdla /tutorial/models/test_Add/
```

# 2. Run nvdla loadable

## 2.1 onnc/vp

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

## 2.2 nvdla/vp

note nvdla/vp cannot run lenet.nvdla out.nvdla compiled from ONNC
maybe it was compiled nv_small, or input problem?

# 3. Parse nvdla loadable

https://zhuanlan.zhihu.com/p/378122624

# 4. Run test (WIP)

Download and build the NVDLA CMOD
```bash
sudo dnf install perl-YAML "perl(IO::Tee)" g++ cmake lua-devel perl-FindBin perl-Capture-Tiny perl-XML-Simple boost-devel python3-devel swig
cd ref/hw
git checkout nv_small
make

vim tree.make
    CPP  := /usr/bin/cpp
    GCC  := /usr/bin/gcc
    CXX  := /usr/bin/g++
    PERL := /usr/bin/perl
    JAVA := /usr/lib/jvm/java-25-openjdk/bin/java
    SYSTEMC := /usr/local/systemc-2.3.0
    PYTHON := /usr/bin/python

vim cmod/Makefile
    #	$(SRC_DIR)/hls_wrapper/cdma_hls_wrapper.cpp \
./tools/bin/tmake -build cmod_top

cd ../vp
cmake -DCMAKE_INSTALL_PREFIX=build -DSYSTEMC_PREFIX=/usr/local/systemc-2.3.0/ -DNVDLA_HW_PREFIX=/home/fedora/nvdla/ref/hw -DNVDLA_HW_PROJECT=nv_small -DCMAKE_POLICY_VERSION_MINIMUM=3.5
```

# reference
https://github.com/nvdla/vp
https://github.com/ONNC/onnc-tutorial/blob/master/lab_1_Environment_Setup/lab_1.md
https://zhuanlan.zhihu.com/p/630822241
https://zhuanlan.zhihu.com/p/378122624
https://github.com/LeiWang1999/nvdla-parser
https://nvdla.org/hw/v2/environment_setup_guide.html