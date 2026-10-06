//
// Android Vulkan ICD loader bridge for MojoLauncher.
//
#include <android/api-level.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <jni.h>

#include <driver_helper/nsbypass.h>
#include <android/dlext.h>
#include <mojoexec.h>

/* Android's loader remains the entry point. The selected Mesa ICD is opened
 * in the app's isolated linker namespace and the linker hook makes loader
 * requests for vulkan.* resolve to that already-open ICD. This is the reliable
 * Android path; an APK-private ICD manifest alone is not sufficient. */
static const char* custom_vulkan_driver = NULL;

#ifdef ENABLE_TURNIP_LOADER
static bool load_custom_vulkan() {
    static bool driver_loaded = false;
    if (driver_loaded) return true;
    if (custom_vulkan_driver == NULL) return false;

    const char* cache_dir = getenv("TMPDIR");
    if (!linker_ns_load(mojoexec_native_dir)) return false;
    void* linkerhook = linker_ns_dlopen("liblinkerhook.so", RTLD_LOCAL | RTLD_NOW);
    if (linkerhook == NULL) return false;

    void* driver_handle = linker_ns_dlopen(custom_vulkan_driver, RTLD_LOCAL | RTLD_NOW);
    if (driver_handle == NULL) {
        printf("MojoExec: Failed to load custom Vulkan ICD %s: %s\n",
               custom_vulkan_driver, dlerror());
        dlclose(linkerhook);
        return false;
    }

    void* dl_android = linker_ns_dlopen("libdl_android.so", RTLD_LOCAL | RTLD_LAZY);
    if (dl_android == NULL) goto fail_driver;
    void* android_get_exported_namespace =
        dlsym(dl_android, "android_get_exported_namespace");
    void (*linkerhook_pass_handles)(void*, void*, void*) =
        dlsym(linkerhook, "app__pojav_linkerhook_pass_handles");
    if (linkerhook_pass_handles == NULL || android_get_exported_namespace == NULL)
        goto fail_dl;

    linkerhook_pass_handles(driver_handle, android_dlopen_ext,
                            android_get_exported_namespace);
    void* libvulkan = linker_ns_dlopen_unique(cache_dir, "libvulkan.so",
                                               "libmjlvlk.so",
                                               RTLD_LOCAL | RTLD_NOW);
    printf("MojoExec: Loaded Android Vulkan loader, ICD=%s, ptr=%p\n",
           custom_vulkan_driver, libvulkan);
    if (libvulkan) {
        driver_loaded = true;
        return true;
    }

fail_dl:
    dlclose(dl_android);
fail_driver:
    dlclose(driver_handle);
    dlclose(linkerhook);
    return false;
}
#endif

void* mojoexec_acq_vulkan_handle() {
    int flags = RTLD_LOCAL | RTLD_NOW;
#ifdef ENABLE_TURNIP_LOADER
    if (android_get_device_api_level() >= 28 && custom_vulkan_driver != NULL) {
        if (load_custom_vulkan())
            return linker_ns_dlopen("libmjlvlk.so", flags);
    }
#endif
    void* vulkan_ptr = dlopen("libvulkan.so", flags);
    printf("MojoExec: loaded system vulkan, ptr=%p\n", vulkan_ptr);
    return vulkan_ptr;
}

JNIEXPORT void JNICALL
Java_git_artdeell_mojoexec_MojoExec_setUseTurnip(JNIEnv *env, jclass clazz,
                                                  jboolean enable) {
    custom_vulkan_driver = enable ? "libvulkan_freedreno.so" : NULL;
}

JNIEXPORT void JNICALL
Java_git_artdeell_mojoexec_MojoExec_setUsePanvk(JNIEnv *env, jclass clazz,
                                                 jboolean enable) {
    custom_vulkan_driver = enable ? "libvulkan_panfrost.so" : NULL;
}

JNIEXPORT void JNICALL
Java_git_artdeell_mojoexec_MojoExec_preloadVulkan(JNIEnv *env, jclass clazz) {
#ifdef ENABLE_TURNIP_LOADER
    if (custom_vulkan_driver == NULL) return;
    if (!load_custom_vulkan())
        printf("MojoExec: Failed to preload custom Vulkan ICD %s\n",
               custom_vulkan_driver);
#endif
}
