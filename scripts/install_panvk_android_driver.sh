#!/usr/bin/env bash
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${ROOT}/mojo-launcher/app_pojavlauncher/src/main/jniLibs/arm64-v8a"
DRIVER="${1:-${PANVK_ANDROID_DRIVER:-}}"
LIBDRM="${2:-${PANVK_ANDROID_LIBDRM:-}}"
EGL="${3:-${PANVK_ANDROID_EGL:-}}"
if [[ $# -ge 3 ]]; then shift 3; else shift $#; fi

if [[ -z "${DRIVER}" || -z "${LIBDRM}" || -z "${EGL}" ]]; then
  echo "usage: $0 <libvulkan_panfrost.so> <libdrm.so> <libEGL_mesa.so> [additional Mesa .so files...]" >&2
  exit 2
fi
for LIB in "${DRIVER}" "${LIBDRM}" "${EGL}" "$@"; do
  [[ -f "${LIB}" ]] || { echo "missing library: ${LIB}" >&2; exit 1; }
done

mkdir -p "${DEST}"
install -m 755 "${DRIVER}" "${DEST}/libvulkan_panfrost.so"
install -m 755 "${LIBDRM}" "${DEST}/libdrm.so"
install -m 755 "${EGL}" "${DEST}/libEGL_mesa.so"
for LIB in "$@"; do
  install -m 755 "${LIB}" "${DEST}/$(basename "${LIB}")"
done

for LIB in "${DEST}/libvulkan_panfrost.so" "${DEST}/libdrm.so" "${DEST}/libEGL_mesa.so"; do
  file "${LIB}" | grep -q 'ARM aarch64' || {
    echo "library is not an AArch64 Android library: ${LIB}" >&2
    exit 1
  }
done

echo "Installed PanVK + Mesa Zink runtime into ${DEST}"
