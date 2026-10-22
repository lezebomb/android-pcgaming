package com.winlator.cmod.renderer;

import android.opengl.GLES11Ext;
import android.opengl.GLES20;

import com.winlator.cmod.XrActivity;
import com.winlator.cmod.xserver.Drawable;

import java.nio.ByteBuffer;

public class Texture {
    protected int textureId = 0;
    private int wrapS = GLES20.GL_CLAMP_TO_EDGE;
    private int wrapT = GLES20.GL_CLAMP_TO_EDGE;
    private int magFilter = GLES20.GL_LINEAR;
    private int minFilter = GLES20.GL_LINEAR;
    protected int format = GLES11Ext.GL_BGRA;
    protected boolean needsUpdate = true;
    protected byte unpackAlignment = 4; // or add a getter method
    private boolean flipY = false;
    private boolean swapRedBlue = false;

    public boolean isSwapRedBlue() {
        return swapRedBlue;
    }

    public boolean isFlipY() {
        return flipY;
    }

    public void setFlipY(boolean flipY) {
        this.flipY = flipY;
    }


    public void allocateTexture(short width, short height, ByteBuffer data) {
        allocateTexture(width, height, width, data);
    }

    public void allocateTexture(short width, short height, short stride, ByteBuffer data) {
        int[] textureIds = new int[1];
        GLES20.glGenTextures(1, textureIds, 0);
        textureId = textureIds[0];

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 4);
        if (stride > 0 && stride != width) {
            android.opengl.GLES30.glPixelStorei(android.opengl.GLES30.GL_UNPACK_ROW_LENGTH, stride);
        }
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);

        if (data != null) {
            GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, format, width, height, 0, format, GLES20.GL_UNSIGNED_BYTE, data);
        }

        if (stride > 0 && stride != width) {
            android.opengl.GLES30.glPixelStorei(android.opengl.GLES30.GL_UNPACK_ROW_LENGTH, 0);
        }

        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, wrapS);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, wrapT);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, magFilter);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, minFilter);

        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
    }

    public int getWrapS() {
        return wrapS;
    }

    public void setWrapS(int wrapS) {
        this.wrapS = wrapS;
    }

    public int getWrapT() {
        return wrapT;
    }

    public void setWrapT(int wrapT) {
        this.wrapT = wrapT;
    }

    public int getMagFilter() {
        return magFilter;
    }

    public void setMagFilter(int magFilter) {
        this.magFilter = magFilter;
    }

    public int getMinFilter() {
        return minFilter;
    }

    public void setMinFilter(int minFilter) {
        this.minFilter = minFilter;
    }

    public int getFormat() {
        return format;
    }

    public void setFormat(int format) {
        this.format = format;
    }

    public boolean isNeedsUpdate() {
        return needsUpdate;
    }

    public void setNeedsUpdate(boolean needsUpdate) {
        this.needsUpdate = needsUpdate;
    }

    public void updateFromDrawable(Drawable drawable) {
        ByteBuffer data = drawable.getData();
        if (data == null) return;

        if (!isAllocated()) {
            allocateTexture(drawable.width, drawable.height, drawable.getStride(), data);
            swapRedBlue = false;
        }
        else if (needsUpdate) {
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
            if (drawable.getStride() > 0 && drawable.getStride() != drawable.width) {
                android.opengl.GLES30.glPixelStorei(android.opengl.GLES30.GL_UNPACK_ROW_LENGTH, drawable.getStride());
            }
            GLES20.glTexSubImage2D(GLES20.GL_TEXTURE_2D, 0, 0, 0, drawable.width, drawable.height, format, GLES20.GL_UNSIGNED_BYTE, data);
            if (drawable.getStride() > 0 && drawable.getStride() != drawable.width) {
                android.opengl.GLES30.glPixelStorei(android.opengl.GLES30.GL_UNPACK_ROW_LENGTH, 0);
            }
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
            needsUpdate = false;
            swapRedBlue = false;
        }
    }

    public boolean isAllocated() {
        return textureId > 0;
    }

    // Called on the renderer's GL thread. GPU-produced AHB pixels must not be read
    // through the permanently mapped CPU write pointer used for GDI drawing.
    public boolean copyFromGpuDrawable(Drawable destination, Drawable source, int x, int y, int framebuffer) {
        return copyFromGpuDrawable(destination, source, x, y, framebuffer, true);
    }

    public boolean copyFromGpuDrawable(Drawable destination, Drawable source, int x, int y,
                                       int framebuffer, boolean x11BgraBytes) {
        int srcX = Math.max(0, -x), srcY = Math.max(0, -y);
        int dstX = Math.max(0, x), dstY = Math.max(0, y);
        int width = Math.min(source.width - srcX, destination.width - dstX);
        int height = Math.min(source.height - srcY, destination.height - dstY);
        if (width <= 0 || height <= 0) return true;
        // Ignore errors left by unrelated drawing, then check this operation.
        for (int i = 0; i < 8 && GLES20.glGetError() != GLES20.GL_NO_ERROR; i++) {}
        Texture sourceTexture = source.getTexture();
        sourceTexture.updateFromDrawable(source);
        if (sourceTexture instanceof GPUImage && ((GPUImage) sourceTexture).hardwareBufferPtr == 0) return false;
        updateFromDrawable(destination);
        int[] previous = new int[1];
        android.opengl.GLES30.glGetIntegerv(android.opengl.GLES30.GL_READ_FRAMEBUFFER_BINDING, previous, 0);
        android.opengl.GLES30.glBindFramebuffer(android.opengl.GLES30.GL_READ_FRAMEBUFFER, framebuffer);
        try {
            android.opengl.GLES30.glFramebufferTexture2D(android.opengl.GLES30.GL_READ_FRAMEBUFFER,
                    GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, sourceTexture.getTextureId(), 0);
            if (android.opengl.GLES30.glCheckFramebufferStatus(android.opengl.GLES30.GL_READ_FRAMEBUFFER)
                    != GLES20.GL_FRAMEBUFFER_COMPLETE) return false;
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
            GLES20.glCopyTexSubImage2D(GLES20.GL_TEXTURE_2D, 0, dstX, dstY, srcX, srcY, width, height);
            if (GLES20.glGetError() != GLES20.GL_NO_ERROR) return false;
            needsUpdate = false;
            // DRI3 carries X11 BGRA bytes. EGL already interprets format 5 as
            // BGRA; RGBA/RGBX AHB fallback needs the same correction as DisplayX.
            swapRedBlue = x11BgraBytes && sourceTexture instanceof GPUImage && ((GPUImage) sourceTexture).format != 5;
            // PresentIdle permits the producer to reuse its buffer. Complete the
            // copy before releasing that buffer; the destination belongs to us.
            GLES20.glFinish();
            return true;
        } finally {
            android.opengl.GLES30.glFramebufferTexture2D(android.opengl.GLES30.GL_READ_FRAMEBUFFER,
                    GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, 0, 0);
            android.opengl.GLES30.glBindFramebuffer(android.opengl.GLES30.GL_READ_FRAMEBUFFER, previous[0]);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        }
    }

    public int getTextureId() {
        return textureId;
    }

    public void copyFromFramebuffer(int framebuffer, short width, short height) {
        if (!isAllocated()) allocateTexture(width, height, null);
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, framebuffer);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        GLES20.glCopyTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, 0, 0, width, height, 0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
        if (XrActivity.isEnabled(null)) XrActivity.getInstance().bindFramebuffer();
    }

    public void copyFromReadBuffer(short width, short height) {
        if (!isAllocated()) allocateTexture(width, height, null);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        GLES20.glCopyTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, 0, 0, width, height, 0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
    }

    public void destroy() {
        if (textureId > 0) {
            int[] textureIds = new int[]{textureId};
            GLES20.glDeleteTextures(textureIds.length, textureIds, 0);
            textureId = 0;
        }
    }

    protected void generateTextureId() {
        int[] textureIds = new int[1];
        GLES20.glGenTextures(1, textureIds, 0);
        textureId = textureIds[0];
    }

    protected void setTextureParameters() {
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, wrapS);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, wrapT);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, magFilter);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, minFilter);
    }

}
