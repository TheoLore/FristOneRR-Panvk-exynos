package net.kdt.pojavlaunch.game.renderer.impl;

import android.content.Context;
import android.os.Build;
import android.util.Log;

import net.kdt.pojavlaunch.Tools;
import net.kdt.pojavlaunch.game.renderer.RenderSpec;
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
 * Direct PanVK Vulkan renderer for Minecraft Java's native Vulkan backend.
 *
 * This is intentionally not a Mesa/Zink renderer. Mojang's current Java
 * renderer can create a Vulkan instance directly; MojoExec injects the
 * app-private PanVK ICD into Android's Vulkan loader before that happens.
 */
public final class PanvkRenderSpec implements RenderSpec {
    private static final String TAG = "PanVK-G52";
    private static final String DRIVER = "libvulkan_panfrost.so";
    private static final String LIBDRM = "libdrm.so";
    private static final String MANIFEST_ASSET = "panvk/panfrost_kbase_icd.template.json";

    private File manifest;

    private File nativeFile(String name) {
        return new File(Tools.NATIVE_LIB_DIR, name);
    }

    @Override
    public boolean compatibleDevice(Context context) {
        if (Build.VERSION.SDK_INT < 26 || !isArm64()) return false;
        if (!nativeFile(DRIVER).isFile() || !nativeFile(LIBDRM).isFile()) return false;
        if (!GpuUtils.checkVulkanSupport(context.getPackageManager())) return false;
        try {
            GpuUtils.GLInfo glInfo = GpuUtils.getGlInfo();
            // This fork targets the Samsung Mali-G52 Bifrost/JM kbase path.
            // Do not expose it on unrelated Mali generations by guesswork.
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
        return "PanVK Vulkan (Mali-G52)";
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
    public String library() {
        // RenderSpec requires a library name; native Vulkan setup does not call
        // MojoExec.prepareEgl() or treat this as an EGL/GLES renderer.
        return DRIVER;
    }

    @Override
    public void setupEnvironment(Context context, Map<String, String> envMap) {
        manifest = makeManifest(context);
        if (manifest == null) return;

        // Native Minecraft Vulkan and Vulkan mods can use the standard loader
        // variables. MojoExec's linker-namespace bridge is the authoritative
        // Android selection path because APK-private manifests are unreliable.
        envMap.put("VK_DRIVER_FILES", manifest.getAbsolutePath());
        envMap.put("VK_ICD_FILENAMES", manifest.getAbsolutePath());
        envMap.put("PANVK_ENABLE_EXPERIMENTAL", "1");
        envMap.put("PANVK_KBASE_DRI3", "0");
        envMap.put("PANVK_KBASE_DMA_HEAP", "/dev/dma_heap/system");
        envMap.put("PANVK_TRACE", "0");
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
        if (!nativeFile(DRIVER).isFile() || !nativeFile(LIBDRM).isFile() || manifest == null) {
            return false;
        }
        // Must happen before Minecraft creates its native Vulkan instance.
        MojoExec.setUsePanvk(true);
        MojoExec.preloadVulkan();
        return true;
    }
}
