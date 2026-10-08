#!/usr/bin/env bash
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${ROOT}/mojo-launcher/app_pojavlauncher/src/main/jniLibs/arm64-v8a"
DRIVER="${1:-${PANVK_ANDROID_DRIVER:-}}"
LIBDRM="${2:-${PANVK_ANDROID_LIBDRM:-}}"
if [[ $# -ge 2 ]]; then shift 2; else shift $#; fi

if [[ -z "${DRIVER}" || -z "${LIBDRM}" ]]; then
  echo "usage: $0 <libvulkan_panfrost.so> <libdrm.so> [libc++_shared.so] [additional Vulkan .so files...]" >&2
  exit 2
fi
for LIB in "${DRIVER}" "${LIBDRM}" "$@"; do
  [[ -f "${LIB}" ]] || { echo "missing library: ${LIB}" >&2; exit 1; }
done

mkdir -p "${DEST}"
install -m 755 "${DRIVER}" "${DEST}/libvulkan_panfrost.so"
install -m 755 "${LIBDRM}" "${DEST}/libdrm.so"
if [[ $# -gt 0 && "$(basename "$1")" == "libc++_shared.so" ]]; then
  install -m 755 "$1" "${DEST}/libc++_shared.so"
  shift
fi
for LIB in "$@"; do
  install -m 755 "${LIB}" "${DEST}/$(basename "${LIB}")"
done

for LIB in "${DEST}/libvulkan_panfrost.so" "${DEST}/libdrm.so"; do
  file "${LIB}" | grep -q 'ARM aarch64' || {
    echo "library is not an AArch64 Android library: ${LIB}" >&2
    exit 1
  }
done

echo "Installed direct PanVK Vulkan ICD into ${DEST}"
