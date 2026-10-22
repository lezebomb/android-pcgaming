#include <jni.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>

#include "vk_context.h"
#include "vortek_serializer.h"
#include "request_handler.h"
#include "vulkan_helper.h"

VulkanWrapper vulkanWrapper = {0};
bool vortekSerializerCastVkObject = true;
char vortekCacheDir[PATH_MAX] = {0};
char vortekCacheSizePath[PATH_MAX] = {0};
static void* systemVulkan;

// Called before accepting any guest connection. The directory comes from
// Context.getCacheDir(), never a guest environment or another package.
JNIEXPORT jboolean JNICALL
Java_com_winlator_cmod_xenvironment_components_VortekRendererComponent_initVulkanWrapper(
        JNIEnv* env, jobject obj, jstring cacheDirectory) {
    if (!cacheDirectory) return JNI_FALSE;
    const char* path = (*env)->GetStringUTFChars(env, cacheDirectory, NULL);
    if (!path) return JNI_FALSE;
    int length = snprintf(vortekCacheDir, sizeof(vortekCacheDir), "%s/vortek", path);
    bool valid = path[0] == '/' && length > 0 && length < sizeof(vortekCacheDir) - 80;
    (*env)->ReleaseStringUTFChars(env, cacheDirectory, path);
    if (!valid) return JNI_FALSE;
    if (mkdir(vortekCacheDir, 0700) != 0 && errno != EEXIST) return JNI_FALSE;
    struct stat info;
    if (stat(vortekCacheDir, &info) != 0 || !S_ISDIR(info.st_mode)) return JNI_FALSE;
    snprintf(vortekCacheSizePath, sizeof(vortekCacheSizePath), "%s/.cache-size", vortekCacheDir);

    if (!systemVulkan) systemVulkan = dlopen(LIBVULKAN_PATH, RTLD_NOW | RTLD_LOCAL);
    if (!systemVulkan) return JNI_FALSE;
    initVulkanWrapper(&vulkanWrapper, systemVulkan);
    return vulkanWrapper.vkCreateInstance && vulkanWrapper.vkGetInstanceProcAddr &&
           vulkanWrapper.vkGetDeviceProcAddr && vulkanWrapper.vkEnumerateInstanceExtensionProperties;
}

JNIEXPORT jlong JNICALL
Java_com_winlator_cmod_xenvironment_components_VortekRendererComponent_createVkContext(
        JNIEnv* env, jobject obj, jint clientFd, jobject options) {
    if (!vulkanWrapper.vkCreateInstance || !options) return 0;
    return (jlong)createVkContext(env, obj, clientFd, options);
}

JNIEXPORT void JNICALL
Java_com_winlator_cmod_xenvironment_components_VortekRendererComponent_destroyVkContext(
        JNIEnv* env, jobject obj, jlong contextPtr) {
    if (contextPtr) destroyVkContext(env, (VkContext*)contextPtr);
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_cmod_xenvironment_components_VortekRendererComponent_handleExtraDataRequest(
        JNIEnv* env, jobject obj, jlong contextPtr, jint requestId, jint requestLength) {
    return contextPtr && requestLength >= 0 &&
           handleExtraDataRequest((VkContext*)contextPtr, requestId, requestLength);
}
