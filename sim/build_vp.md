# Build NVDLA VP `nv_small`

This document records the known-good flow used to build `/home/fedora/nvdla/ref/vp` against `/home/fedora/nvdla/ref/hw` for the `nv_small` hardware project.

The important point: a dependency list and build command are not enough. This build currently requires source fixes in both `nvdla/vp` and `nvdla/hw`, so another host should use maintained forks or pinned branches that already contain those fixes.

## Source Layout

Expected workspace:

```text
/home/fedora/nvdla/
  ref/
    hw/   # patched nvdla/hw fork or branch
    vp/   # patched nvdla/vp fork or branch
  sim/
    build_vp.md
    build_vp_nv_small.sh
```

The script defaults to those paths. Override with environment variables if needed:

```bash
ROOT_DIR=/path/to/nvdla \
SYSTEMC_PREFIX=/usr/local/systemc-2.3.0/ \
NVDLA_HW_PROJECT=nv_small \
./sim/build_vp_nv_small.sh
```

## Required Source Fixes

Keep these changes in forks or branches. Do not rely on manually reapplying them from memory.

`ref/vp` requires:

- `CMakeLists.txt` updated to CMake 3.5 compatibility and C++11.
- Legacy warning suppressions added because the old VP/qbox/greenlib code is built with newer compilers.
- RPATH and link path include the install lib directory and `/home/linuxbrew/.linuxbrew/lib`.
- qbox external project configured with Homebrew `PATH`/`PKG_CONFIG_PATH`, Python 3 compatible flow, `--python=/usr/bin/python`, `--extra-cflags` with Homebrew include path, and `--extra-ldflags` with Homebrew lib path.
- qbox build limited to `subdir-aarch64-softmmu`; qbox install step disabled with a no-op.
- `aarch64_toplevel` links `bz2`.
- `models/nvdla/CMakeLists.txt` includes the required cmod/HLS/generated include directories from `ref/hw`.
- `models/nvdla/CMakeLists.txt` builds the extra cmod/HLS compatibility sources needed by the VP shared library.
- GreenLib Python 3 compatibility fixes.
- qbox Python 3/configure/C source compatibility fixes.
- qbox submodules such as `pixman` and `dtc` initialized.

`ref/hw` requires:

- HLS header fixes in `cmod/hls/include/ac_fixed.h` for dependent template calls like `.template slc<...>`.
- HLS header fixes in `cmod/hls/include/ac_int.h` to avoid invalid negative/zero template extension instantiations with newer compilers.
- Generated `outdir/nv_small/spec/defs` and related `nv_small` cmod outputs present.

## Host Dependencies

Known working assumptions:

- Linux host with GCC/G++ and Make.
- CMake available.
- SystemC 2.3.0 installed at `/usr/local/systemc-2.3.0/`.
- Python 3 available.
- `/usr/bin/python` available for qbox configure. If the host lacks it, create a Python 3 compatible symlink or adjust the qbox configure command in the patched VP CMake.
- `pkg-config` available.
- Lua 5.4 development/runtime libraries available.
- bzip2 development/runtime libraries available.
- Homebrew on Linux installed at `/home/linuxbrew/.linuxbrew` with build helpers used by qbox.

Packages that were needed during this build included:

```bash
brew install pixman autoconf automake libtool flex bison
```

Depending on the distro, equivalent system packages may work, but the current patched CMake explicitly uses `/home/linuxbrew/.linuxbrew` for qbox include/lib lookup and runtime RPATH.

## Build

From the workspace root:

```bash
chmod +x /home/fedora/nvdla/sim/build_vp_nv_small.sh
/home/fedora/nvdla/sim/build_vp_nv_small.sh
```

The script runs the same configure used for the verified build:

```bash
cd /home/fedora/nvdla/ref/vp
cmake \
  -DCMAKE_INSTALL_PREFIX=build \
  -DSYSTEMC_PREFIX=/usr/local/systemc-2.3.0/ \
  -DNVDLA_HW_PREFIX=/home/fedora/nvdla/ref/hw \
  -DNVDLA_HW_PROJECT=nv_small \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  .
cmake --build . -- -j"$(nproc)"
cmake --install .
```

## Verification

The script verifies:

```bash
test -x build/bin/aarch64_toplevel
test -f build/lib/libnvdla.so
test -f build/lib/libqbox-nvdla.so
ldd build/bin/aarch64_toplevel | grep "not found"
```

The final command must produce no output. On the known-good host, important runtime links resolve to:

```text
libsystemc-2.3.0.so => /usr/local/systemc-2.3.0/lib-linux64/libsystemc-2.3.0.so
libqbox-nvdla.so    => /home/fedora/nvdla/ref/vp/build/lib/libqbox-nvdla.so
libnvdla.so         => /home/fedora/nvdla/ref/vp/build/lib/libnvdla.so
libnvdla_cmod.so    => /home/fedora/nvdla/ref/vp/build/lib/libnvdla_cmod.so
libbz2.so.1.0       => /home/linuxbrew/.linuxbrew/lib/libbz2.so.1.0
```

## Expected Warnings

CMake may print deprecation and RPATH warnings. These were non-fatal in the verified build. Treat missing libraries, qbox configure failures, or missing `ref/hw/outdir/nv_small` files as real errors.

## Recommendation

For another host, use pinned forks/branches for `ref/hw` and `ref/vp`, then run this script. A standalone `build.md` is useful documentation, but it is not a substitute for versioned source patches.
