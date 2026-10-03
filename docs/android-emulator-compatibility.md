# PanVK Android emulator compatibility

This fork targets **Mali-G52 Bifrost v7** devices using the kbase path, such
as the Galaxy A04s/Exynos 850 configuration. It is an experimental Mesa
driver, not a universal Vulkan or emulator compatibility layer.

## Important behavior

PanVK no longer enables Mesa's experimental-driver bypass implicitly. This is
deliberate: an Android Vulkan loader must be able to reject an experimental
device and let an emulator choose another backend instead of receiving a
partially tested device.

For an explicit test run only, set one of these in the **same process
environment that loads the ICD**:

```sh
PANVK_ENABLE_EXPERIMENTAL=1
```

or the upstream Mesa switch:

```sh
PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1
```

Neither switch makes the driver conformant, adds missing features, or
guarantees that a game will work. Keep a stock/system Vulkan driver available
as the fallback.

## Backend boundaries

| Consumer | What PanVK must provide | What this repository cannot provide by itself |
| --- | --- | --- |
| Winlator + DXVK | A working Vulkan device and swapchain for the Winlator surface | DXVK DLLs, Wine loader configuration, and per-game D3D9/D3D11 behavior |
| Eden or another Switch emulator | Android surface, swapchain, shader, descriptor, synchronization and format paths | Emulator-specific renderer work and its required Vulkan version |
| Azahar/Citra | Vulkan 1.1-class runtime behavior plus Android WSI | 3DS renderer fallbacks and emulator feature policy |
| Dolphin | Android WSI, present synchronization and the optional features it actually enables | Dolphin's backend and game-specific workarounds |
| PPSSPP | Android WSI, swapchain and working shader/image paths | PPSSPP settings and its OpenGL fallback |

An extension name or a higher `apiVersion` is **not** a substitute for an
implementation. Do not add extension names, feature bits, limits, GPU IDs, or
Vulkan versions merely to bypass an emulator check.

For Android rendering, the relevant baseline is the actual implementation of
`VK_KHR_surface`, `VK_KHR_android_surface`, and `VK_KHR_swapchain`, including
surface support, image acquisition, presentation, synchronization, and
swapchain recreation. The names alone are not sufficient.

## DXVK diagnostics

`DXVK_HUD` is parsed by DXVK, not by the Vulkan driver. For a Winlator test,
set these in the game process and inspect both the DXVK log and the PanVK log:

```sh
DXVK_HUD=devinfo,api,version
DXVK_LOG_LEVEL=info
PANVK_TRACE=1
PANVK_ENABLE_EXPERIMENTAL=1
```

If the HUD is absent, first verify that the variables reach the Wine game
process and that the expected `d3d9.dll` or `d3d11.dll`/`dxgi.dll` is loaded.
The driver cannot create the DXVK HUD on behalf of DXVK.

## Required device validation

Before calling a backend supported, record on the A04s itself:

* `VkPhysicalDeviceProperties2` and `VkPhysicalDeviceFeatures2`;
* instance/device extension lists and the actual entry points;
* queue-family surface support, surface formats, present modes and swapchain
  creation/present results;
* memory heaps/types, external-memory properties and Android buffer behavior;
* Android loader/HAL path, kernel/kbase version, and the exact driver commit.

The acceptance criterion is a successful Android surface/swapchain lifecycle
and correct rendering without validation errors, device loss or corruption.
Build success on Linux, `vkcube`, or one game is not sufficient evidence for
all emulators.
