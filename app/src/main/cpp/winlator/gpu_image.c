#include <android/log.h>
#include <android/hardware_buffer.h>
#include <android/native_window.h>

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <jni.h>
#include <unistd.h>
#include <string.h>

#define LOG_TAG "System.out"
#define printf(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define HAL_PIXEL_FORMAT_BGRA_8888 5

// Function to create an EGL image from a hardware buffer
EGLImageKHR createImageKHR(AHardwareBuffer* hardwareBuffer, int textureId) {
    if (!hardwareBuffer) {
        printf("createImageKHR: Invalid AHardwareBuffer pointer\n");
        return NULL;
    }

    const EGLint attribList[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};

    EGLClientBuffer clientBuffer = eglGetNativeClientBufferANDROID(hardwareBuffer);
    if (!clientBuffer) {
        printf("Failed to get native client buffer\n");
        return NULL;
    }

    EGLDisplay eglDisplay = eglGetCurrentDisplay();
    if (eglDisplay == EGL_NO_DISPLAY) {
        printf("Invalid EGLDisplay\n");
        return NULL;
    }

    EGLImageKHR imageKHR = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attribList);
    if (!imageKHR) {
        printf("Failed to create EGLImageKHR\n");
        return NULL;
    }

    // An unrelated earlier GL error must not invalidate this import.
    for (int i = 0; i < 8 && glGetError() != GL_NO_ERROR; i++) {}
    glBindTexture(GL_TEXTURE_2D, textureId);
    if (glGetError() != GL_NO_ERROR) {
        printf("Failed to bind texture\n");
        eglDestroyImageKHR(eglDisplay, imageKHR);
        return NULL;
    }

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, imageKHR);
    if (glGetError() != GL_NO_ERROR) {
        printf("Failed to bind EGLImage to texture\n");
        eglDestroyImageKHR(eglDisplay, imageKHR);
        return NULL;
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    return imageKHR;
}

// Function to create a hardware buffer
AHardwareBuffer* createHardwareBuffer(int width, int height) {
    AHardwareBuffer_Desc buffDesc = {};
    buffDesc.width = width;
    buffDesc.height = height;
    buffDesc.layers = 1;
    buffDesc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
    buffDesc.format = HAL_PIXEL_FORMAT_BGRA_8888;

    AHardwareBuffer *hardwareBuffer = NULL;
    if (AHardwareBuffer_allocate(&buffDesc, &hardwareBuffer) != 0) {
        printf("Failed to allocate AHardwareBuffer\n");
        return NULL;
    }

    return hardwareBuffer;
}

// GPU-only output storage, separate from the CPU-mapped GDI path.
JNIEXPORT jlong JNICALL
Java_com_winlator_cmod_renderer_GPUImage_createRenderTarget(JNIEnv *env, jobject obj,
                                                          jshort width, jshort height, jboolean bgra, jlong usage) {
    if (width <= 0 || height <= 0) return 0;
    AHardwareBuffer_Desc desc = {0};
    desc.width = width;
    desc.height = height;
    desc.layers = 1;
    desc.format = bgra ? HAL_PIXEL_FORMAT_BGRA_8888 : AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    desc.usage = (uint64_t)usage | AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    AHardwareBuffer* buffer = NULL;
    int result = AHardwareBuffer_allocate(&desc, &buffer);
    if (result != 0) {
        printf("GPU output AHB allocation failed: result=%d size=%ux%u format=%u usage=%llx",
                result, desc.width, desc.height, desc.format, (unsigned long long)desc.usage);
        return 0;
    }
    return (jlong)buffer;
}

JNIEXPORT jlong JNICALL
Java_com_winlator_cmod_renderer_GPUImage_acquireHardwareBuffer(JNIEnv *env, jobject obj, jlong ptr) {
    if (ptr) AHardwareBuffer_acquire((AHardwareBuffer*)ptr);
    return ptr;
}

// JNI method to extract a hardware buffer from a socketpair
JNIEXPORT jlong JNICALL
Java_com_winlator_cmod_renderer_GPUImage_hardwareBufferFromSocket(JNIEnv *env, jclass obj, jint fd) {
    AHardwareBuffer *ahb;
    
    uint8_t buf = 1;
    
    if ((write(fd, &buf, 1)) == -1) {
        printf("Failed to write data to socketpair");
        return 0;
    }
    
    if ((AHardwareBuffer_recvHandleFromUnixSocket(fd, &ahb)) != 0) {
        printf("Failed to extract hardware buffer from socketpair");
        return 0;
    }
    
    return (jlong)ahb;
}

// JNI method to create a hardware buffer
JNIEXPORT jlong JNICALL
Java_com_winlator_cmod_renderer_GPUImage_createHardwareBuffer(JNIEnv *env, jclass obj, jshort width, jshort height) {
    AHardwareBuffer *buffer = createHardwareBuffer(width, height);
    if (!buffer) {
        printf("Failed to create hardware buffer\n");
        return 0;
    }
    return (jlong)buffer;
}

// JNI method to create an EGL image
JNIEXPORT jlong JNICALL
Java_com_winlator_cmod_renderer_GPUImage_createImageKHR(JNIEnv *env, jclass obj, jlong hardwareBufferPtr, jint textureId) {
    AHardwareBuffer* hardwareBuffer = (AHardwareBuffer*)hardwareBufferPtr;
    if (!hardwareBuffer) {
        printf("Invalid AHardwareBuffer pointer\n");
        return 0;
    }
    return (jlong)createImageKHR(hardwareBuffer, textureId);
}

// JNI method to destroy a hardware buffer
JNIEXPORT void JNICALL
Java_com_winlator_cmod_renderer_GPUImage_destroyHardwareBuffer(JNIEnv *env, jclass obj, jlong hardwareBufferPtr, jboolean cpuMapped) {
    AHardwareBuffer* hardwareBuffer = (AHardwareBuffer*)hardwareBufferPtr;
    if (hardwareBuffer) {
        if (cpuMapped) AHardwareBuffer_unlock(hardwareBuffer, NULL);
        AHardwareBuffer_release(hardwareBuffer);
    }
}

JNIEXPORT jlong JNICALL
Java_com_winlator_cmod_renderer_GPUImage_describeHardwareBuffer(JNIEnv *env, jclass obj, jlong hardwareBufferPtr) {
    if (!hardwareBufferPtr) return 0;
    AHardwareBuffer_Desc desc = {0};
    AHardwareBuffer_describe((AHardwareBuffer *)hardwareBufferPtr, &desc);
    return (jlong)(((uint64_t)desc.stride << 32) | desc.format);
}

// JNI method to lock a hardware buffer
JNIEXPORT jobject JNICALL
Java_com_winlator_cmod_renderer_GPUImage_lockHardwareBuffer(JNIEnv *env, jclass obj, jlong hardwareBufferPtr) {
    AHardwareBuffer* hardwareBuffer = (AHardwareBuffer*)hardwareBufferPtr;
    if (!hardwareBuffer) {
        printf("Invalid AHardwareBuffer pointer\n");
        return NULL;
    }
    
    void *virtualAddr;
    if (AHardwareBuffer_lock(hardwareBuffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1, NULL, &virtualAddr) != 0) {
        printf("Failed to lock AHardwareBuffer\n");
        return NULL;
    }

    AHardwareBuffer_Desc buffDesc;
    AHardwareBuffer_describe(hardwareBuffer, &buffDesc);

    jclass cls = (*env)->GetObjectClass(env, obj);
    if (cls == NULL) {
        printf("Failed to get Java class reference\n");
        AHardwareBuffer_unlock(hardwareBuffer, NULL);
        return NULL;
    }

    jmethodID setStride = (*env)->GetMethodID(env, cls, "setStride", "(S)V");
    if (setStride == NULL) {
        printf("Failed to get setStride method ID\n");
        AHardwareBuffer_unlock(hardwareBuffer, NULL);
        return NULL;
    }
    (*env)->CallVoidMethod(env, obj, setStride, (jshort)buffDesc.stride);

    jlong size = buffDesc.stride * buffDesc.height * 4;
    jobject buffer = (*env)->NewDirectByteBuffer(env, virtualAddr, size);
    if (buffer == NULL) {
        printf("Failed to create Java ByteBuffer\n");
        AHardwareBuffer_unlock(hardwareBuffer, NULL);
    }

    return buffer;
}

// JNI method to destroy an EGL image
JNIEXPORT void JNICALL
Java_com_winlator_cmod_renderer_GPUImage_destroyImageKHR(JNIEnv *env, jclass obj, jlong imageKHRPtr) {
    EGLImageKHR imageKHR = (EGLImageKHR)imageKHRPtr;
    if (imageKHR) {
        EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglDestroyImageKHR(eglDisplay, imageKHR);
    }
}
