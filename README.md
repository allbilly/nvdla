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

Within container, compile all models
```bash
cd /onnc/onnc-umbrella/build-normal && smake -j8 install

# All models available (10 total):
#   lenet, quantized_mnist, test_Add, test_Conv_Relu, test_Log,
#   test_Mul_Add_Relu, test_Relu, test_Relu_Log_Relu, test_Shuffle, test_group_Conv

for model in lenet quantized_mnist test_Add test_Conv_Relu test_Log \
             test_Mul_Add_Relu test_Relu test_Relu_Log_Relu test_Shuffle test_group_Conv; do
    onnc -mquadruple nvdla /tutorial/models/$model/$model.onnx
    sudo mv out.nvdla /tutorial/models/$model/
done
```

# 2. Run nvdla loadable

## 2.1 docker onnc/vp

run inference of any compiled model
nvdla login: root/nvdla

```bash
docker run -ti --rm -v ./ref/onnc-tutorial:/tutorial:z onnc/vp
cd /usr/local/nvdla
cp /tutorial/models/lenet/* .   # change per test case
aarch64_toplevel -c aarch64_nvdla.lua

mount -t 9p -o trans=virtio r /mnt && cd /mnt
insmod drm.ko && insmod opendla.ko
./nvdla_runtime --loadable out.nvdla --image input0.pgm --rawdump
cat output.dimg
poweroff
```

The same process applies to all 10 models — just swap the model directory and input image:
- cp /tutorial/models/[TEST_CASE]/* .
- `lenet` → `input0.pgm`
- `quantized_mnist` → `input0.pgm`
- `test_Add` → `input1x5x7.pgm`
- `test_Conv_Relu` → `input1x5x5.pgm`
- `test_Log` → `input1x5x5.pgm`
- `test_Mul_Add_Relu` → `input1x5x5.pgm`
- `test_Relu` → `input1x5x5.pgm`
- `test_Relu_Log_Relu` → `input1x5x5.pgm`
- `test_Shuffle` → `input1x3x4.pgm`
- `test_group_Conv` → `input1x5x5.pgm`

## 2.2 docker nvdla/vp

note 
- test_Add suceeded
- lenet failed
- maybe it was compiled nv_small, or input problem?

## 2.3 build vp

Tested on amd64 Fedora 44 6.19.10-300.fc44.x86_64

Download and build the NVDLA CMOD
```bash
sudo dnf install perl-YAML "perl(IO::Tee)" g++ cmake lua-devel perl-FindBin perl-Capture-Tiny perl-XML-Simple boost-devel python3-devel swig aarch64-linux-gnu-gcc          
pip install pexpect
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
cmake \
  -DCMAKE_INSTALL_PREFIX=build \
  -DSYSTEMC_PREFIX=/usr/local/systemc-2.3.0/ \
  -DNVDLA_HW_PREFIX=/home/fedora/nvdla/ref/hw \
  -DNVDLA_HW_PROJECT=nv_small \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  .
make
make install  

mkdir -p images/linux-4.13.3
cp Image and rootfs.ext4 out from docker nvdla/vp 
images/linux-4.13.3/Image
images/linux-4.13.3/rootfs.ext4
# or build your own image https://nvdla.org/vp.html#building-linux-kernel-for-nvdla-virtual-simulator
```

## 2.4 run vp

Build test cases
```bash
cd ref/vp/tests/dc_1x1x8_1x1x8x1_int8_0

# Parse .cfg + .dat files, generate C source and devmem scripts
# TODO: is devmem already no longer used?
python3 gen_cfg2c.py ref/hw/verif/tests/trace_tests/nv_small/dc_1x1x8_1x1x8x1_int8_0/dc_1x1x8_1x1x8x1_int8_0.cfg
make clean && make
```

Start VP
```bash
cd ref/vp
SC_SIGNAL_WRITE_CHECK=DISABLE ./build/bin/aarch64_toplevel --conf conf/aarch64_nvdla.lua

Run the test case
```bash
# Login: root / nvdla
mount -t 9p -o trans=virtio r /mnt && cd /mnt
cd /mnt/tests/dc_1x1x8_1x1x8x1_int8_0
./dat_load.sh && ./wt_load.sh        
./dc_1x1x8_1x1x8x1_int8_0_test

Failed test_Add
```bash
cp /mnt/onnc_models/test_Add/test_Add.nvdla .
cp /mnt/onnc_models/test_Add/input1x5x7.pgm .

insmod drm.ko && insmod opendla.ko
./nvdla_runtime --loadable out.nvdla --image input1x5x7.pgm --rawdump
cat output.dimg
```

# 3. Parse nvdla loadable
https://zhuanlan.zhihu.com/p/378122624

# reference
https://github.com/nvdla/vp
https://github.com/ONNC/onnc-tutorial/blob/master/lab_1_Environment_Setup/lab_1.md
https://zhuanlan.zhihu.com/p/630822241
https://zhuanlan.zhihu.com/p/378122624
https://github.com/LeiWang1999/nvdla-parser
https://nvdla.org/hw/v2/environment_setup_guide.html