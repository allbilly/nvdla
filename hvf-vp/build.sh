#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/.build"
QBOX_DIR="$BUILD_DIR/qbox"
QBOX_BUILD="$QBOX_DIR/build"
HW_DIR="$BUILD_DIR/nvdla-hw"
GUEST_DIR="$BUILD_DIR/guest"
ASSET_SRC="$BUILD_DIR/assets-src"
QBOX_REV="21d100d98ab5a936c4ed98fa24a81c36a8ec15c4"
NVDLA_HW_REV="8e06b1b9d85aab65b40d43d08eec5ea4681ff715"
UV_IMAGE="ghcr.io/astral-sh/uv:0.12.3"
ROOTFS_BYTES=$((4 * 1024 * 1024 * 1024))

if [[ "$(uname -s)" != "Darwin" || "$(uname -m)" != "arm64" ]]; then
    echo "error: this launcher requires native arm64 macOS (Apple Silicon)" >&2
    exit 1
fi

required=(brew git make perl python3 fdtput)
for command_name in "${required[@]}"; do
    command -v "$command_name" >/dev/null || {
        echo "error: missing command: $command_name" >&2
        exit 1
    }
done

CMAKE_BIN="$(brew --prefix cmake)/bin/cmake"
if [[ ! -x "$CMAKE_BIN" ]]; then
    echo "error: Homebrew CMake was not found; run: brew install cmake" >&2
    exit 1
fi
export PATH="$(dirname "$CMAKE_BIN"):$PATH"

E2FSPROGS_PREFIX="$(brew --prefix e2fsprogs 2>/dev/null || true)"
E2FSCK_BIN="$E2FSPROGS_PREFIX/sbin/e2fsck"
RESIZE2FS_BIN="$E2FSPROGS_PREFIX/sbin/resize2fs"
if [[ ! -x "$E2FSCK_BIN" || ! -x "$RESIZE2FS_BIN" ]]; then
    echo "error: Homebrew e2fsprogs was not found; run: brew install e2fsprogs" >&2
    exit 1
fi

if command -v docker >/dev/null; then
    CONTAINER=docker
elif command -v podman >/dev/null; then
    CONTAINER=podman
else
    echo "error: install Docker or Podman to extract the official 4.13.3 guest kernel" >&2
    exit 1
fi

GCC_PREFIX="$(brew --prefix gcc)"
CPP_BIN="$(find "$GCC_PREFIX/bin" -maxdepth 1 -type f -name 'cpp-*' | sort | tail -n 1)"
if [[ -z "$CPP_BIN" ]]; then
    echo "error: GNU cpp was not found; run: brew install gcc" >&2
    exit 1
fi

mkdir -p "$BUILD_DIR" "$GUEST_DIR"
rm -rf "$ASSET_SRC"
mkdir -p "$ASSET_SRC"

if [[ ! -d "$QBOX_DIR/.git" ]]; then
    git clone https://github.com/qualcomm/qbox.git "$QBOX_DIR"
    git -C "$QBOX_DIR" checkout --detach "$QBOX_REV"
elif [[ "$(git -C "$QBOX_DIR" rev-parse HEAD)" != "$QBOX_REV" ]]; then
    echo "error: $QBOX_DIR is not at the pinned QBox revision $QBOX_REV" >&2
    exit 1
fi

if git -C "$QBOX_DIR" apply --unidiff-zero --check "$SCRIPT_DIR/qbox.patch" 2>/dev/null; then
    git -C "$QBOX_DIR" apply --unidiff-zero "$SCRIPT_DIR/qbox.patch"
elif ! git -C "$QBOX_DIR" apply --unidiff-zero --reverse --check "$SCRIPT_DIR/qbox.patch" 2>/dev/null; then
    echo "error: QBox has edits that conflict with qbox.patch" >&2
    exit 1
fi

if [[ ! -d "$HW_DIR/.git" ]]; then
    git clone --branch nvdlav1 https://github.com/nvdla/hw.git "$HW_DIR"
    git -C "$HW_DIR" checkout --detach "$NVDLA_HW_REV"
elif [[ "$(git -C "$HW_DIR" rev-parse HEAD)" != "$NVDLA_HW_REV" ]]; then
    echo "error: $HW_DIR is not at the pinned NVDLA revision $NVDLA_HW_REV" >&2
    exit 1
fi

if git -C "$HW_DIR" apply --unidiff-zero --check "$SCRIPT_DIR/nvdla-hw-macos.patch" 2>/dev/null; then
    git -C "$HW_DIR" apply --unidiff-zero "$SCRIPT_DIR/nvdla-hw-macos.patch"
elif ! git -C "$HW_DIR" apply --unidiff-zero --reverse --check "$SCRIPT_DIR/nvdla-hw-macos.patch" 2>/dev/null; then
    echo "error: NVDLA hardware sources conflict with nvdla-hw-macos.patch" >&2
    exit 1
fi

"$CMAKE_BIN" --preset mac -S "$QBOX_DIR" \
    -DLIBQEMU_TARGETS=aarch64 \
    -DENABLE_PYTHON_BINDER=OFF \
    -DNVDLA_QBOX_COMPONENT_DIR="$SCRIPT_DIR/qbox-nvdla" \
    -DNVDLA_HW_DIR="$HW_DIR" \
    -DNVDLA_PROJECT=nv_full
"$CMAKE_BIN" --build "$QBOX_BUILD" --target qbox --parallel "$(sysctl -n hw.ncpu)"

SYSTEMC_SRC="$QBOX_BUILD/_deps/systemclanguage-src"
SYSTEMC_BUILD="$QBOX_BUILD/_deps/systemclanguage-build/src"

printf '%s\n' \
    nv_full "$CPP_BIN" /usr/bin/c++ /usr/bin/perl /usr/bin/java \
    "$SYSTEMC_SRC" verilator clang | make -B -C "$HW_DIR" tree.make

CMOD_FLAGS="-std=c++17 -DSC_ALLOW_MACROS_WITHOUT_SEMICOLON -Wno-error=missing-template-arg-list-after-template-kw -Wno-error=c++11-narrowing"
make -C "$HW_DIR/cmod" PROJECT=nv_full TARGET=libnvdla_cmod.dylib \
    SYSTEMC="$SYSTEMC_SRC" \
    SYSTEMC_INC_DIR="$SYSTEMC_SRC/src" \
    SYSTEMC_LIB_DIR="$SYSTEMC_BUILD" \
    CXXFLAGS="$CMOD_FLAGS" \
    LDFLAGS="-dynamiclib -Wl,-install_name,@rpath/libnvdla_cmod.dylib -L$SYSTEMC_BUILD -lsystemc -Wl,-rpath,$SYSTEMC_BUILD"

"$CMAKE_BIN" --build "$QBOX_BUILD" --parallel "$(sysctl -n hw.ncpu)" --target \
    platforms-vp Nvdla router gs_memory loader char_backend_stdio uart-pl011 \
    arm_gicv2 pl031 virtio_mmio_blk virtio_mmio_net cpu_arm_host global_peripheral_initiator

if [[ ! -f "$GUEST_DIR/Image-4.13.3" ]]; then
    container_id="$($CONTAINER create --platform linux/amd64 onnc/vp:latest)"
    cleanup_container() { "$CONTAINER" rm "$container_id" >/dev/null 2>&1 || true; }
    trap cleanup_container EXIT
    "$CONTAINER" cp "$container_id:/usr/local/nvdla/Image" "$GUEST_DIR/Image-4.13.3"
    cleanup_container
    trap - EXIT
fi

cp "$REPO_DIR/ref/vp/drm.ko" "$ASSET_SRC/drm.ko"
cp "$REPO_DIR/ref/vp/opendla.ko" "$ASSET_SRC/opendla.ko"
cp "$REPO_DIR/ref/vp/nvdla_runtime" "$ASSET_SRC/nvdla_runtime"
cp "$REPO_DIR/ref/vp/libnvdla_runtime.so" "$ASSET_SRC/libnvdla_runtime.so"
cp "$REPO_DIR/examples/vp/test_Add.nvdla" "$ASSET_SRC/test_Add.nvdla"
cp "$REPO_DIR/examples/vp/input1x5x7.pgm" "$ASSET_SRC/input1x5x7.pgm"
cp "$REPO_DIR/examples/simple_add.py" "$ASSET_SRC/simple_add.py"
cp "$REPO_DIR/examples/conv.py" "$ASSET_SRC/conv.py"
cp "$REPO_DIR/examples/conv_1x1x8.py" "$ASSET_SRC/conv_1x1x8.py"
cp "$SCRIPT_DIR/setup-ml.sh" "$ASSET_SRC/setup-ml.sh"

uv_container="$($CONTAINER create --platform linux/arm64 "$UV_IMAGE")"
cleanup_uv_container() { "$CONTAINER" rm "$uv_container" >/dev/null 2>&1 || true; }
trap cleanup_uv_container EXIT
"$CONTAINER" cp "$uv_container:/uv" "$ASSET_SRC/uv"
cleanup_uv_container
trap - EXIT

"$CONTAINER" run --rm --platform linux/amd64 --entrypoint /bin/sh \
    -v "$ASSET_SRC:/assets:ro" -v "$GUEST_DIR:/out" onnc/vp:latest -c '
        set -e
        dd if=/dev/zero of=/out/assets.ext4 bs=1M count=128
        mkfs.ext4 -F /out/assets.ext4
        for source in /assets/*; do
            name=${source##*/}
            debugfs -w -R "write $source $name" /out/assets.ext4
        done
    '

cp "$SCRIPT_DIR/old-nvdla-vp.dtb" "$GUEST_DIR/nvdla-hvf.dtb"
fdtput -t s "$GUEST_DIR/nvdla-hvf.dtb" /chosen bootargs \
    "root=/dev/vda rw console=ttyAMA0 earlycon=pl011,0x09000000 nokaslr"

if [[ ! -f "$GUEST_DIR/rootfs-rw.ext4" ]]; then
    cp "$REPO_DIR/examples/vp/rootfs.ext4" "$GUEST_DIR/rootfs-rw.ext4"
fi

rootfs_size="$(stat -f %z "$GUEST_DIR/rootfs-rw.ext4")"
if (( rootfs_size < ROOTFS_BYTES )); then
    truncate -s "$ROOTFS_BYTES" "$GUEST_DIR/rootfs-rw.ext4"
    set +e
    "$E2FSCK_BIN" -pf "$GUEST_DIR/rootfs-rw.ext4"
    e2fsck_status=$?
    set -e
    if (( e2fsck_status > 1 )); then
        echo "error: e2fsck failed with status $e2fsck_status" >&2
        exit "$e2fsck_status"
    fi
    "$RESIZE2FS_BIN" "$GUEST_DIR/rootfs-rw.ext4"
fi

echo
echo "HVF NVDLA VP build complete. Start it with:"
echo "  $SCRIPT_DIR/run.sh"
