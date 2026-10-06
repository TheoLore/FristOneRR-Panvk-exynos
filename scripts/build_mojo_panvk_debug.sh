#!/usr/bin/env bash
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
DRIVER="${1:-${PANVK_ANDROID_DRIVER:-}}"
LIBDRM="${2:-${PANVK_ANDROID_LIBDRM:-}}"
EGL="${3:-${PANVK_ANDROID_EGL:-}}"
if [[ -z "${DRIVER}" || -z "${LIBDRM}" || -z "${EGL}" ]]; then
  echo "usage: $0 <libvulkan_panfrost.so> <libdrm.so> <libEGL_mesa.so> [additional Mesa .so files...]" >&2
  exit 2
fi
shift $(( $# >= 3 ? 3 : $# )) || true
"${ROOT}/scripts/install_panvk_android_driver.sh" "${DRIVER}" "${LIBDRM}" "${EGL}" "$@"
cd "${ROOT}/mojo-launcher"
git submodule update --init --depth=1
./gradlew :app_pojavlauncher:assembleFullDebug --no-daemon
printf 'APK: %s\n' "${ROOT}/mojo-launcher/app_pojavlauncher/build/outputs/apk/full/debug/"
