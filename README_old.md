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

## 2.2 docker nvdla/vp:1.4 latest

note 
- test_Add suceeded
- lenet failed
- onnc/vp was based on nvdla/vp:1.3, detail diff in README.md

## 2.3 build Image and rootfs.ext4
```

podman run --rm -it --security-opt label=disable \
  -v /home/fedora/nvdla:/nvdla \
  -w /nvdla/buildroot-2017.11-rc1-clean \
  nvdla-build \
  make menuconfig

podman run --rm --security-opt label=disable -v /home/fedora/nvdla:/nvdla -w /nvdla/buildroot-2017.11-rc1-clean nvdla-build bash -c "make olddefconfig && make -j\$(($(nproc)-1)) WGET='wget --no-config'"
```

## 2.4 build opendla.ko

Build the KMD against the same Linux tree and aarch64 toolchain used by
Buildroot. For ONNC VP loadables, keep `ref/sw` at the ONNC-compatible revision
`1f44c24`.

```bash
cd /home/fedora/nvdla/ref/sw
git checkout 1f44c24

make -C /home/fedora/nvdla/buildroot-2017.11-rc1-clean/output/build/linux-4.13.3 \
  M=/home/fedora/nvdla/ref/sw/kmd/port/linux \
  ARCH=arm64 \
  CROSS_COMPILE=/home/fedora/nvdla/buildroot-2017.11-rc1-clean/output/host/bin/aarch64-linux-gnu- \
  modules

cp /home/fedora/nvdla/ref/sw/kmd/port/linux/opendla.ko \
  /home/fedora/nvdla/examples/vp/opendla.ko
```

If you build a logging-patched KMD, install it with a distinct name:

```bash
# Full blob + register logging can be too verbose for large/tiled runs.
cp /home/fedora/nvdla/ref/sw/kmd/port/linux/opendla.ko \
  /home/fedora/nvdla/examples/vp/opendla_logged.ko

# Register/DMA-address logging only is safer for larger captures.
cp /home/fedora/nvdla/ref/sw/kmd/port/linux/opendla.ko \
  /home/fedora/nvdla/examples/vp/opendla_reglog.ko
```

## 2.5 build and run nv_small SystemC direct-register test

This is the tested flow for running
`dc_1x1x8_1x1x8x1_int8_0_test` directly against the NVDLA SystemC cmodel in
VP. The prebuilt `onnc/vp` and `nvdla/vp:latest` images do not match this test:
they decode SDP at `0xb000`, while this nv_small test expects SDP at `0x9000`.
You must rebuild VP against an nv_small cmod.

Clone a clean upstream hardware tree and check out `nv_small`:

```bash
cd /home/fedora/nvdla
git clone https://github.com/nvdla/hw ref/hw2
cd /home/fedora/nvdla/ref/hw2
git checkout -b nv_small origin/nv_small
```

Apply the required Fedora/GCC 16 compatibility patch. The patch also works
around a stale BDMA generated-model accessor that is not used by this conv test:

```bash
cd /home/fedora/nvdla/ref/hw2
git apply /home/fedora/nvdla/patches/hw2_nvsmall_build.patch
```

Generate `tree.make` for `nv_small` using system tools:

```bash
cd /home/fedora/nvdla/ref/hw2
make USE_VM_ENV=1 \
  VM_CPP=/usr/bin/cpp \
  VM_GCC=/usr/bin/gcc \
  VM_CXX=/usr/bin/g++ \
  VM_PERL=/usr/bin/perl \
  VM_JAVA=/usr/lib/jvm/java-25-openjdk/bin/java \
  VM_SYSTEMC=/usr/local/systemc-2.3.0 \
  VM_PYTHON=/usr/bin/python
```

Build the nv_small cmod:

```bash
cd /home/fedora/nvdla/ref/hw2
/usr/bin/perl ./tools/bin/tmake -build cmod_top
```

Configure and build VP against the nv_small cmod. Clear stale qbox/nvdla build
directories first, otherwise CMake can keep linking the previous cmod:

```bash
cd /home/fedora/nvdla/ref/vp
rm -rf CMakeCache.txt CMakeFiles cmake_install.cmake \
  CPackConfig.cmake CPackSourceConfig.cmake Makefile build \
  models/nvdla.build libs/qbox.build

cmake \
  -U NVDLA_CMOD_* \
  -DCMAKE_INSTALL_PREFIX=build \
  -DSYSTEMC_PREFIX=/usr/local/systemc-2.3.0/ \
  -DNVDLA_HW_PREFIX=/home/fedora/nvdla/ref/hw2 \
  -DNVDLA_HW_PROJECT=nv_small \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  .

cmake --build . -- -j$(nproc)
cmake --install .
```

On Fedora 44, qbox may try to build EGL/X11 even though this VP is run with
`-nographic`. If the build fails with `unknown type name 'Window'`, add
`--disable-opengl --disable-sdl` to the qbox `CONFIGURE_COMMAND` in
`ref/vp/CMakeLists.txt`, remove `libs/qbox.build`, then rerun the CMake build.

Regenerate and rebuild the direct test from the nv_small cfg. The generator in
this repo uses `ref/hw2/outdir/nv_small/spec/manual` for register offsets and
emits `dat_load.sh` and `wt_load.sh` into the model directory:

```bash
cd /home/fedora/nvdla
python3 examples/gen_cfg2c.py \
  /home/fedora/nvdla/ref/onnc-tutorial/models/dc_1x1x8_1x1x8x1_int8_0/dc_1x1x8_1x1x8x1_int8_0.cfg

podman run --rm --security-opt label=disable \
  -v /home/fedora/nvdla:/nvdla \
  -w /nvdla/ref/onnc-tutorial/models/dc_1x1x8_1x1x8x1_int8_0 \
  nvdla-build \
  bash -lc '/nvdla/buildroot-2017.11-rc1-clean/output/host/bin/aarch64-linux-gnu-gcc -static -O2 -Wall -Wextra -o dc_1x1x8_1x1x8x1_int8_0_test dc_1x1x8_1x1x8x1_int8_0_test.c'
```

Start the rebuilt local VP. This command uses the repo-root config
`ref/vp/conf/aarch64_nvdla_repo.lua`, which mounts the whole repo as `/mnt` and
uses `ref/sw/prebuilt/linux/Image` plus `rootfs.ext4`:

```bash
cd /home/fedora/nvdla
LD_LIBRARY_PATH=/home/fedora/nvdla/ref/vp/build/lib:/home/fedora/nvdla/ref/hw2/outdir/nv_small/cmod/release/lib:/home/linuxbrew/.linuxbrew/lib:/usr/local/systemc-2.3.0/lib-linux64:$LD_LIBRARY_PATH \
SC_SIGNAL_WRITE_CHECK=DISABLE \
SC_LOG='outfile:nvdla_sc_debug.log;verbosity_level:sc_debug' \
/home/fedora/nvdla/ref/vp/build/bin/aarch64_toplevel \
  -c /home/fedora/nvdla/ref/vp/conf/aarch64_nvdla_repo.lua
```

`SC_LOG` is required for CMOD `cslDebug(50/70)` traces. Without it, VP uses
`SC_MEDIUM` and detailed CMAC operand lines such as `CMAC_MAC_INPUT` are
filtered out.

The same setting can also be passed as a command-line option:

```bash
/home/fedora/nvdla/ref/vp/build/bin/aarch64_toplevel \
  -c /home/fedora/nvdla/ref/vp/conf/aarch64_nvdla_repo.lua \
  -s 'outfile:nvdla_sc_debug.log;verbosity_level:sc_debug'
```

In another terminal, log into the guest. The root password for this rootfs is
`nvdla`. For non-interactive runs, use `sshpass -p nvdla`:

```bash
ssh -p 6667 root@127.0.0.1

sshpass -p nvdla ssh \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/tmp/opencode/nvdla_known_hosts \
  -p 6667 root@127.0.0.1
```

Run the test in the guest:

```bash
mount -t 9p -o trans=virtio r /mnt || true
cd /mnt/ref/onnc-tutorial/models/dc_1x1x8_1x1x8x1_int8_0
./dat_load.sh
./wt_load.sh
./dc_1x1x8_1x1x8x1_int8_0_test
```

Expected output:

```text
Programming registers...
Done programming initial registers.
CDMA -> CBUF flush done
Firing OP_ENABLE...
SDP done interrupt received!
Reading output at OUT_BASE=0xf0000000...
Output written to sdp2mcif_output.dat
```

The output file is written at:

```text
/home/fedora/nvdla/ref/onnc-tutorial/models/dc_1x1x8_1x1x8x1_int8_0/sdp2mcif_output.dat
```

## 2.6 build vp

Tested on amd64 Fedora 44 `6.19.10-300.fc44.x86_64` with GCC 16.1.1.

Use `nvdla/hw` branch `nvdlav1` with hardware project `nv_full`. There is no
upstream `nv_full` branch; `nv_full` is the project selected in `tree.make` and
passed to VP CMake.

Install host packages:

```bash
sudo dnf install perl-YAML "perl(IO::Tee)" g++ cmake lua-devel perl-FindBin \
  perl-Capture-Tiny perl-XML-Simple boost-devel python3-devel swig \
  aarch64-linux-gnu-gcc
pip install pexpect
```

Build NVDLA CMOD:

```bash
cd /home/fedora/nvdla/ref/hw

# If ref/hw is a standalone clone, use:
#   git fetch origin nvdlav1
#   git checkout nvdlav1
# In this repo ref/hw is vendored, so it must already contain nvdla/hw:nvdlav1.

make

# tree.make must use nv_full and system tool paths:
#   PROJECTS := nv_full
#   CPP     := /usr/bin/cpp
#   GCC     := /usr/bin/gcc
#   CXX     := /usr/bin/g++
#   PERL    := /usr/bin/perl
#   JAVA    := /usr/lib/jvm/java-25-openjdk/bin/java
#   SYSTEMC := /usr/local/systemc-2.3.0
#   PYTHON  := /usr/bin/python
```

GCC 16 rejects two old Catapult `ac_*` header constructs. Patch these before
building CMOD:

```cpp
// cmod/hls/include/ac_fixed.h, in to_ac_int():
.template slc<AC_MAX(I,1)>(0)

// cmod/hls/include/ac_int.h, in iv_const_shift_l():
const int ishift = ((B >> 5) > Nr) ? Nr : (B >> 5);
const int M1 = AC_MIN(N+ishift,Nr);
```

Then build CMOD. Keep all HLS wrapper sources enabled in `cmod/Makefile`,
including `hls_wrapper/cdma_hls_wrapper.cpp`; otherwise `libnvdla_cmod.so` will
miss symbols used by VP.

```bash
cd /home/fedora/nvdla/ref/hw

# Use system Perl explicitly if PATH finds Linuxbrew Perl without YAML.pm.
/usr/bin/perl ./tools/bin/tmake -build cmod_top
```

Configure and build VP:

```bash
cd /home/fedora/nvdla/ref/vp

# Remove stale in-source CMake/qbox cache if this tree was ever configured for
# nv_small or a different CMOD path.
rm -rf CMakeCache.txt CMakeFiles cmake_install.cmake \
  CPackConfig.cmake CPackSourceConfig.cmake Makefile build \
  models/nvdla.build libs/qbox.build

cmake \
  -U NVDLA_CMOD_* \
  -DCMAKE_INSTALL_PREFIX=build \
  -DSYSTEMC_PREFIX=/usr/local/systemc-2.3.0/ \
  -DNVDLA_HW_PREFIX=/home/fedora/nvdla/ref/hw \
  -DNVDLA_HW_PROJECT=nv_full \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  .

cmake --build . -- -j$(nproc)
cmake --install .
```

`models/nvdla/CMakeLists.txt` should link against
`outdir/nv_full/cmod/release/lib/libnvdla_cmod.so`. If CMake errors on missing
`cmod/nvdla_clibs/NvdlaPacker.cpp`, remove the local `NVDLA_CMOD_COMPAT_FILES`
source-recompile block and link the CMOD library instead.

Install the guest kernel/rootfs expected by `conf/aarch64_nvdla.lua`:

```bash
cd /home/fedora/nvdla/ref/vp
mkdir -p images/linux-4.13.3

# Copy the Image and rootfs.ext4 built in section 2.3, or extract them from a
# known-good nvdla/vp image.
cp /path/to/Image images/linux-4.13.3/Image
cp /path/to/rootfs.ext4 images/linux-4.13.3/rootfs.ext4
```

## 2.7 run vp

Start the non-Docker VP:

```bash
cd /home/fedora/nvdla/ref/vp
SC_SIGNAL_WRITE_CHECK=DISABLE ./build/bin/aarch64_toplevel --conf conf/aarch64_nvdla.lua
```

`conf/aarch64_nvdla.lua` forwards host ports `6666` and `6667`. If either port
is already in use, copy the config and change the `hostfwd` host ports, for
example `6676` and `6677`, before launching VP.

In the guest console:

```bash
# Login: root / nvdla
mount -t 9p -o trans=virtio r /mnt
cd /mnt
```

Optional ONNC loadable smoke test from the shared 9p directory:

```bash
insmod drm.ko
insmod opendla.ko
./nvdla_runtime --loadable out.nvdla --image input0.pgm --rawdump
cat output.dimg
```

The direct Python register examples require a guest rootfs with Python. The
`examples/vp/Image` and `examples/vp/rootfs.ext4` pair has `/usr/bin/python3`;
clone `rootfs.ext4` before booting another VP instance so it does not share a
writable disk with an already-running Docker/Podman VP.

```bash
# In the guest after mounting /mnt and loading drm.ko/opendla.ko:
cd /mnt
python3 examples/simple_add.py
python3 examples/conv.py
```

Optional generated register test:

```bash
cd /home/fedora/nvdla/ref/vp/tests/nv_small_tests/dc_1x1x8_1x1x8x1_int8_0
python3 ../gen_cfg2c.py /home/fedora/nvdla/ref/hw/verif/tests/trace_tests/nv_small/dc_1x1x8_1x1x8x1_int8_0/dc_1x1x8_1x1x8x1_int8_0.cfg
make clean && make

# Then from the guest:
cd /mnt/tests/nv_small_tests/dc_1x1x8_1x1x8x1_int8_0
./dc_1x1x8_1x1x8x1_int8_0_test
```

## 2.8 build arm64 docker vp image

Do not use the old `docker/docker_gen.sh` flow for an ARM64 image. That script
builds `vp-build` on the host first and then copies it into the Docker image; on
an amd64 host this creates an ARM64-labeled image containing an amd64
`aarch64_toplevel` binary.

Use the target-platform Dockerfile instead. It builds SystemC, CMOD, qbox, and
`aarch64_toplevel` inside the `linux/arm64` build stage and checks the output
ELF architecture.

```bash
cd /home/fedora/nvdla
ref/vp/docker/build_arm64_image.sh nvdla-vp:arm64

# Or directly with docker:
docker buildx build \
  --platform linux/arm64 \
  -t nvdla-vp:arm64 \
  -f ref/vp/docker/Dockerfile.arm64 \
  .
```

The Dockerfile intentionally fails if either of these is not ARM64:

```bash
file ref/hw/outdir/nv_full/cmod/release/lib/libnvdla_cmod.so
file ref/vp/build/bin/aarch64_toplevel
```
