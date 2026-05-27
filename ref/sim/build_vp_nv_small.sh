#!/usr/bin/env bash
set -euo pipefail

# Rebuild the patched NVDLA VP for the nv_small hardware project.
# This script assumes ref/vp and ref/hw are already checked out at the
# known-good patched forks/branches documented in build_vp.md.

ROOT_DIR="${ROOT_DIR:-/home/fedora/nvdla}"
VP_DIR="${VP_DIR:-${ROOT_DIR}/ref/vp}"
HW_DIR="${HW_DIR:-${ROOT_DIR}/ref/hw}"
INSTALL_PREFIX="${INSTALL_PREFIX:-build}"
SYSTEMC_PREFIX="${SYSTEMC_PREFIX:-/usr/local/systemc-2.3.0/}"
NVDLA_HW_PROJECT="${NVDLA_HW_PROJECT:-nv_small}"
CMAKE_POLICY_VERSION_MINIMUM="${CMAKE_POLICY_VERSION_MINIMUM:-3.5}"
JOBS="${JOBS:-$(nproc)}"

HOMEBREW_PREFIX="${HOMEBREW_PREFIX:-/home/linuxbrew/.linuxbrew}"
if [[ -d "${HOMEBREW_PREFIX}/bin" ]]; then
  export PATH="${HOMEBREW_PREFIX}/bin:${PATH}"
fi
if [[ -d "${HOMEBREW_PREFIX}/lib/pkgconfig" || -d "${HOMEBREW_PREFIX}/share/pkgconfig" ]]; then
  export PKG_CONFIG_PATH="${HOMEBREW_PREFIX}/lib/pkgconfig:${HOMEBREW_PREFIX}/share/pkgconfig:${PKG_CONFIG_PATH:-}"
fi

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "missing required command: $1" >&2
    exit 1
  }
}

need_path() {
  [[ -e "$1" ]] || {
    echo "missing required path: $1" >&2
    exit 1
  }
}

need_cmd cmake
need_cmd make
need_cmd gcc
need_cmd g++
need_cmd pkg-config
need_cmd python3

need_path "${VP_DIR}/CMakeLists.txt"
need_path "${HW_DIR}/cmod"
need_path "${HW_DIR}/outdir/${NVDLA_HW_PROJECT}/spec/defs"
need_path "${SYSTEMC_PREFIX}"
need_path "${SYSTEMC_PREFIX}/lib-linux64/libsystemc-2.3.0.so"

cd "${VP_DIR}"

if [[ -d libs/qbox/.git ]]; then
  git -C libs/qbox submodule update --init pixman dtc || true
fi

cmake \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DSYSTEMC_PREFIX="${SYSTEMC_PREFIX}" \
  -DNVDLA_HW_PREFIX="${HW_DIR}" \
  -DNVDLA_HW_PROJECT="${NVDLA_HW_PROJECT}" \
  -DCMAKE_POLICY_VERSION_MINIMUM="${CMAKE_POLICY_VERSION_MINIMUM}" \
  .

cmake --build . -- -j"${JOBS}"
cmake --install .

test -x "${INSTALL_PREFIX}/bin/aarch64_toplevel"
test -f "${INSTALL_PREFIX}/lib/libnvdla.so"
test -f "${INSTALL_PREFIX}/lib/libqbox-nvdla.so"

if ldd "${INSTALL_PREFIX}/bin/aarch64_toplevel" | grep -q "not found"; then
  echo "runtime link check failed:" >&2
  ldd "${INSTALL_PREFIX}/bin/aarch64_toplevel" >&2
  exit 1
fi

echo "NVDLA VP ${NVDLA_HW_PROJECT} build/install verified:"
echo "  ${VP_DIR}/${INSTALL_PREFIX}/bin/aarch64_toplevel"
echo "  ${VP_DIR}/${INSTALL_PREFIX}/lib/libnvdla.so"
echo "  ${VP_DIR}/${INSTALL_PREFIX}/lib/libqbox-nvdla.so"
