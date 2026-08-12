#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QBOX_BUILD="$SCRIPT_DIR/.build/qbox/build"
GUEST_DIR="$SCRIPT_DIR/.build/guest"
VP="$QBOX_BUILD/platforms-vp"

for required_file in \
    "$VP" \
    "$GUEST_DIR/Image-4.13.3" \
    "$GUEST_DIR/nvdla-hvf.dtb" \
    "$GUEST_DIR/assets.ext4" \
    "$GUEST_DIR/rootfs-rw.ext4"; do
    if [[ ! -f "$required_file" ]]; then
        echo "error: missing $required_file; run $SCRIPT_DIR/build.sh first" >&2
        exit 1
    fi
done

export NVDLA_QBOX_BUILD="$QBOX_BUILD"
export NVDLA_KERNEL="$GUEST_DIR/Image-4.13.3"
export NVDLA_DTB="$GUEST_DIR/nvdla-hvf.dtb"
export NVDLA_ROOTFS="$GUEST_DIR/rootfs-rw.ext4"
export NVDLA_ASSETS="$GUEST_DIR/assets.ext4"
NVDLA_CMOD="$SCRIPT_DIR/.build/nvdla-hw/outdir/nv_full/cmod/release/lib"
SYSTEMC_LIB="$QBOX_BUILD/_deps/systemclanguage-build/src"
QEMU_LIB="$QBOX_BUILD/_deps/libqemu-build/qemu-prefix/lib"
export DYLD_LIBRARY_PATH="$NVDLA_CMOD:$QBOX_BUILD:$SYSTEMC_LIB:$QEMU_LIB:${DYLD_LIBRARY_PATH:-}"
export SC_SIGNAL_WRITE_CHECK=DISABLE

echo "Starting native Apple Silicon NVDLA VP with QEMU accel=hvf."
echo "Guest login: root (no password)"
echo "Then run:"
echo "  mkdir -p /mnt/nvdla && mount /dev/vdb /mnt/nvdla"
echo "  insmod /mnt/nvdla/drm.ko"
echo "  insmod /mnt/nvdla/opendla.ko"
echo "  python3 /mnt/nvdla/simple_add.py"
echo "  python3 /mnt/nvdla/conv.py"
echo "  python3 /mnt/nvdla/conv_1x1x8.py"
echo "  sh /mnt/nvdla/setup-ml.sh  # optional: uv + Torch + tinygrad"
echo

(
    cd "$GUEST_DIR"
    "$VP" -l "$SCRIPT_DIR/conf_hvf.lua"
)
