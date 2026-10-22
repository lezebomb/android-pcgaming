package com.winlator.cmod.renderer;

import androidx.annotation.Keep;
import com.winlator.cmod.xserver.Drawable;
import java.nio.ByteBuffer;

public class GPUImage extends Texture {
    public long hardwareBufferPtr;
    public int format = 5;
    private long imageKHRPtr;
    private ByteBuffer virtualData;
    private boolean cpuMapped;
    private boolean renderTarget;
    private long renderTargetUsage;
    private boolean importFailureLogged;
    private long nextImportAttemptNs;
    public short stride;
    private static boolean supported = false;

    static {
        System.loadLibrary("winlator");
    }

    public GPUImage(short width, short height) {
        hardwareBufferPtr = createHardwareBuffer(width, height);
        if (hardwareBufferPtr != 0) {
            virtualData = lockHardwareBuffer(hardwareBufferPtr);
            cpuMapped = virtualData != null;
            if (virtualData == null) {
                System.err.println("Error: Failed to lock hardware buffer");
                destroyHardwareBuffer(hardwareBufferPtr, false);
                hardwareBufferPtr = 0;
            }
        } else {
            System.err.println("Error: Failed to create hardware buffer");
        }
    }
    
    public GPUImage(int socketFd) {
        hardwareBufferPtr = hardwareBufferFromSocket(socketFd);
        if (hardwareBufferPtr != 0) {
            // Imported swapchain buffers belong to a GPU producer. CPU mapping
            // is neither required for EGL import nor valid for every usage/layout.
            long info = describeHardwareBuffer(hardwareBufferPtr);
            stride = (short)(info >>> 32);
            format = (int)info;
        } else {
            System.err.println("Error: Failed to create hardware buffer");
        }
    }

    private GPUImage(short width, short height, boolean bgra, long usage) {
        // Vulkan produces this image. Permanently CPU-locking it would violate
        // producer ownership; EGL can import it without a CPU mapping.
        hardwareBufferPtr = createRenderTarget(width, height, bgra, usage);
        renderTargetUsage = usage;
        renderTarget = true;
        if (hardwareBufferPtr != 0) {
            long info = describeHardwareBuffer(hardwareBufferPtr);
            stride = (short)(info >>> 32);
            format = (int)info;
        }
    }

    public static GPUImage createRenderTargetImage(short width, short height, boolean bgra, long usage) {
        GPUImage image = new GPUImage(width, height, bgra, usage);
        return image.hardwareBufferPtr != 0 ? image : null;
    }

    public boolean isRenderTarget() { return renderTarget; }

    public boolean supportsRenderTargetUsage(long usage) { return (renderTargetUsage & usage) == usage; }

    /** Caller must release this reference after Vulkan has imported it. */
    public long acquireForVulkan() {
        return hardwareBufferPtr != 0 ? acquireHardwareBuffer(hardwareBufferPtr) : 0;
    }

    @Override
    public void allocateTexture(short width, short height, ByteBuffer data) {
        if (isAllocated()) return;
        if (hardwareBufferPtr == 0 || System.nanoTime() < nextImportAttemptNs) return;
        if (imageKHRPtr != 0) {
            destroyImageKHR(imageKHRPtr);
            imageKHRPtr = 0;
        }
        super.allocateTexture(width, height, null);
        if (hardwareBufferPtr != 0) {
            imageKHRPtr = createImageKHR(hardwareBufferPtr, textureId);
            if (imageKHRPtr == 0) {
                if (!importFailureLogged) System.err.println("EGL import failed; retaining producer AHB for retry");
                importFailureLogged = true;
                // A failed EGL import does not invalidate the producer's AHB.
                // Discard the incomplete GL texture so the next frame can retry.
                super.destroy();
                nextImportAttemptNs = System.nanoTime() + 100_000_000L;
            } else {
                importFailureLogged = false;
                nextImportAttemptNs = 0;
            }
        }
    }

    @Override
    public void updateFromDrawable(Drawable drawable) {
        if (!isAllocated()) allocateTexture(drawable.width, drawable.height, null);
        needsUpdate = false;
    }

    public short getStride() {
        return stride;
    }

    @Keep
    private void setStride(short stride) {
        this.stride = stride;
    }

    public ByteBuffer getVirtualData() {
        return virtualData;
    }

    @Override
    public void destroy() {
        if (imageKHRPtr != 0) {
            destroyImageKHR(imageKHRPtr);
            imageKHRPtr = 0;
        }
        if (hardwareBufferPtr != 0) {
            destroyHardwareBuffer(hardwareBufferPtr, cpuMapped);
            hardwareBufferPtr = 0;
        }
        cpuMapped = false;
        virtualData = null;
        super.destroy();
    }

    public static boolean isSupported() {
        return supported;
    }

    public static void checkIsSupported() {
        final short size = 8;
        GPUImage gpuImage = new GPUImage(size, size);
        gpuImage.allocateTexture(size, size, null);
        supported = gpuImage.hardwareBufferPtr != 0 && gpuImage.imageKHRPtr != 0 && gpuImage.virtualData != null;
        gpuImage.destroy();
    }

    private native long hardwareBufferFromSocket(int fd);
    
    private native long createHardwareBuffer(short width, short height);

    private native long createRenderTarget(short width, short height, boolean bgra, long usage);

    private native long acquireHardwareBuffer(long hardwareBufferPtr);

    private native void destroyHardwareBuffer(long hardwareBufferPtr, boolean cpuMapped);

    private native long describeHardwareBuffer(long hardwareBufferPtr);

    private native ByteBuffer lockHardwareBuffer(long hardwareBufferPtr);

    private native long createImageKHR(long hardwareBufferPtr, int textureId);

    private native void destroyImageKHR(long imageKHRPtr);
}
