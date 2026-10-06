package net.kdt.pojavlaunch.game.renderer.impl;

import android.content.Context;
import android.os.Build;
import android.util.Log;

import net.kdt.pojavlaunch.Tools;
import net.kdt.pojavlaunch.game.renderer.def.Renderers;
import net.kdt.pojavlaunch.utils.GpuUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.Map;

import git.artdeell.mojo.R;
import git.artdeell.mojoexec.MojoExec;

/**
 * PanVK kbase as the Vulkan backend of Mesa Zink.
 *
 * Minecraft Java launched by Mojo uses the OpenGL/LWJGL path. Therefore a
 * PanVK-only RenderSpec is not enough: this spec must prepare Mesa EGL and
 * select Zink, while MojoExec injects the PanVK ICD underneath Zink.
 *
 * The target supported by this repository is the Samsung Exynos/Mali-G52
 * Bifrost kbase/JM path. It is intentionally not advertised as a generic
 * Mali driver because the kernel ABI and GPU model tables are device-specific.
 */
public final class PanvkRenderSpec extends MesaRenderSpec.ZinkRenderSpec {
    private static final String TAG = "PanVK-G52";
    private static final String DRIVER = "libvulkan_panfrost.so";
    private static final String LIBDRM = "libdrm.so";
    private static final String EGL = "libEGL_mesa.so";
    private static final String MANIFEST_ASSET = "panvk/panfrost_kbase_icd.template.json";

    private File manifest;

    private File nativeFile(String name) {
        return new File(Tools.NATIVE_LIB_DIR, name);
    }

    @Override
    public boolean compatibleDevice(Context context) {
        if (Build.VERSION.SDK_INT < 26 || !isArm64()) return false;
        if (!nativeFile(DRIVER).isFile() || !nativeFile(LIBDRM).isFile()) return false;
        if (!nativeFile(EGL).isFile()) return false;
        if (!GpuUtils.checkVulkanSupport(context.getPackageManager())) return false;
        try {
            GpuUtils.GLInfo glInfo = GpuUtils.getGlInfo();
            // This fork's tested target is the Samsung Mali-G52 Bifrost/JM
            // path. Do not expose it on G57/G68/Valhall devices by guesswork.
            return glInfo.isArm() && glInfo.renderer.toLowerCase().contains("mali-g52");
        } catch (RuntimeException e) {
            Log.w(TAG, "Could not query GLES GPU information", e);
            return false;
        }
    }

    private static boolean isArm64() {
        for (String abi : Build.SUPPORTED_ABIS) {
            if ("arm64-v8a".equals(abi)) return true;
        }
        return false;
    }

    @Override
    public String name() {
        return "PanVK kbase + Zink (Mali-G52)";
    }

    @Override
    public int displayName() {
        return R.string.mcl_setting_renderer_panvk_kbase;
    }

    @Override
    public String tag() {
        return Renderers.PANVK_RENDERER;
    }

    @Override
    public void setupEnvironment(Context context, Map<String, String> envMap) {
        // This supplies MESA_LOADER_DRIVER_OVERRIDE=zink, GL 4.6 overrides and
        // the shader cache directory required by the normal Mojo Zink path.
        super.setupEnvironment(context, envMap);

        manifest = makeManifest(context);
        if (manifest == null) return;

        // Keep these for Vulkan-loader diagnostics and for native Vulkan mods.
        // Actual Android ICD selection is performed by MojoExec injection.
        envMap.put("VK_DRIVER_FILES", manifest.getAbsolutePath());
        envMap.put("VK_ICD_FILENAMES", manifest.getAbsolutePath());

        // Supported by this repository's kbase implementation. DRI3=0 is the
        // safe Android fallback; raw DRI3 is only for a compatible X server.
        envMap.put("PANVK_ENABLE_EXPERIMENTAL", "1");
        envMap.put("PANVK_KBASE_DRI3", "0");
        envMap.put("PANVK_KBASE_DMA_HEAP", "/dev/dma_heap/system");
        envMap.put("PANVK_TRACE", "0");

        // Do not force PANVK_KBASE_DVFS=max or a large heap: on an unrooted
        // Samsung kernel that either fails silently or causes thermal/OOM
        // instability. The Mesa defaults are the safe performance baseline.
    }

    private File makeManifest(Context context) {
        try {
            File dir = context.getDir("panvk", Context.MODE_PRIVATE);
            File result = new File(dir, "panfrost_kbase_icd.json");
            InputStream in = context.getAssets().open(MANIFEST_ASSET);
            byte[] data = new byte[in.available()];
            int offset = 0;
            while (offset < data.length) {
                int read = in.read(data, offset, data.length - offset);
                if (read < 0) break;
                offset += read;
            }
            in.close();
            String json = new String(data, 0, offset, StandardCharsets.UTF_8)
                    .replace("__PANVK_DRIVER__", nativeFile(DRIVER).getAbsolutePath());
            FileOutputStream out = new FileOutputStream(result, false);
            out.write(json.getBytes(StandardCharsets.UTF_8));
            out.close();
            return result;
        } catch (IOException e) {
            Log.e(TAG, "Unable to prepare PanVK ICD manifest", e);
            return null;
        }
    }

    @Override
    public boolean setupRenderer() {
        if (!nativeFile(DRIVER).isFile() || !nativeFile(LIBDRM).isFile()
                || !nativeFile(EGL).isFile() || manifest == null) {
            return false;
        }

        // Must happen before Zink's Vulkan device is created.
        MojoExec.setUsePanvk(true);
        MojoExec.preloadVulkan();
        return super.setupRenderer();
    }
}
