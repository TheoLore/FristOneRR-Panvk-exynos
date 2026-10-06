# PanVK integration in MojoLauncher

This checkout contains the MojoLauncher source under `mojo-launcher/` and a
renderer integration for the PanVK kbase ICD built by the parent Mesa tree.
Minecraft Java reaches Vulkan through Mesa Zink, so the selectable renderer is
PanVK-backed Zink rather than an ICD-only renderer.

## What is bundled

The Android application does **not** silently replace the vendor Vulkan stack.
It exposes **PanVK kbase + Zink** only when all of these are true:

- the process is running on `arm64-v8a` and Android API 26 or newer;
- the device reports the tested ARM `Mali-G52` GLES renderer;
- `libEGL_mesa.so`, `libvulkan_panfrost.so`, and the matching `libdrm.so` are
  present in the application native directory.

The renderer prepares Mesa EGL, selects `MESA_LOADER_DRIVER_OVERRIDE=zink`,
creates an app-private ICD manifest for diagnostics, and sets the PanVK kbase
options. For the actual Android path, the patched MojoExec opens
`libvulkan_panfrost.so` in the app linker namespace and hooks the Android
loader's `vulkan.*` lookup to that handle. This is necessary because Android
loaders do not consistently discover ICD manifests stored inside an APK.
The system loader remains the Vulkan entry point, while PanVK is the selected
ICD and the system driver remains the fallback if the custom load fails.

## Supplying the driver

Build the parent repository's Android kbase package using
`.github/workflows/kbase-debug.yml`, or provide these two arm64 files before
assembling the app:

```text
mojo-launcher/app_pojavlauncher/src/main/jniLibs/arm64-v8a/libvulkan_panfrost.so
mojo-launcher/app_pojavlauncher/src/main/jniLibs/arm64-v8a/libdrm.so
mojo-launcher/app_pojavlauncher/src/main/jniLibs/arm64-v8a/libEGL_mesa.so
```

They must come from the same Mesa build. Do not copy a Linux/glibc build into
an Android APK. The driver uses the proprietary Mali kbase kernel interface;
the phone must expose a compatible `/dev/mali0` and appropriate permissions.

## Build

From this directory:

```bash
cd mojo-launcher
git submodule update --init
./gradlew :app_pojavlauncher:assembleFullDebug
```

The debug APK is produced under
`app_pojavlauncher/build/outputs/apk/full/debug/`.

If the arm64 libraries are absent, the app still builds but the PanVK option
is intentionally hidden. This prevents a false-positive “driver integrated”
state and preserves the normal Zink/system Vulkan fallback.

## Performance policy

The integration does not force a permanent maximum GPU frequency or unsafe
heap size. PanVK already enables its overlap path by default, uses the
userspace cache-sync fast path when supported, and sizes the shared-memory
heap conservatively. `PANVK_KBASE_DVFS=auto` can be supplied through the
launcher environment on a rooted/test device if the kernel permits sysfs
governor writes; it is intentionally not forced because doing so can fail,
overheat the phone, or be rejected by Android permissions. Performance must be
measured on the exact Exynos GPU/kernel pair with `vulkaninfo`, a crash-free
Minecraft session, and frame-time captures—not inferred from the renderer name.
