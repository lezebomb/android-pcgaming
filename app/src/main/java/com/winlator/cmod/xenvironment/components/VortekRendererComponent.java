// SPDX-License-Identifier: LGPL-2.1-only
// Based on the paired Winlator Vortek bridge; see cpp/vortekrenderer/LICENSE.
package com.winlator.cmod.xenvironment.components;

import android.content.Context;
import android.util.Log;
import androidx.annotation.Keep;
import com.winlator.cmod.renderer.GPUImage;
import com.winlator.cmod.renderer.GLRenderer;
import com.winlator.cmod.xconnector.Client;
import com.winlator.cmod.xconnector.ConnectionHandler;
import com.winlator.cmod.xconnector.RequestHandler;
import com.winlator.cmod.xconnector.UnixSocketConfig;
import com.winlator.cmod.xconnector.XConnectorEpoll;
import com.winlator.cmod.xconnector.XInputStream;
import com.winlator.cmod.xenvironment.EnvironmentComponent;
import com.winlator.cmod.xserver.Drawable;
import com.winlator.cmod.xserver.Window;
import com.winlator.cmod.xserver.XLock;
import com.winlator.cmod.xserver.XServer;
import java.io.IOException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/** Experimental, explicitly selected per game; uses an independent GPU output. */
public final class VortekRendererComponent extends EnvironmentComponent
        implements ConnectionHandler, RequestHandler {
    private final XServer xServer;
    private final UnixSocketConfig socketConfig;
    private final Options options;
    private XConnectorEpoll connector;
    private volatile boolean stopped = true;
    private final HashMap<Long, Output> outputs = new HashMap<>();
    private long copiedFrames;

    private static final class Output {
        final int windowId;
        final Drawable destination;
        final Drawable source;
        Output(int windowId, Drawable destination, Drawable source) {
            this.windowId = windowId;
            this.destination = destination;
            this.source = source;
        }
    }

    static { System.loadLibrary("vortekrenderer"); }

    @Keep public static final class Options {
        public int vkMaxVersion = (1 << 22) | (3 << 12) | 128;
        public short maxDeviceMemory = 2048;
        public short imageCacheSize = 256;
        public byte resourceMemoryType = 0;
        public String[] exposedDeviceExtensions = null;
    }

    public VortekRendererComponent(Context context, XServer xServer,
                                   UnixSocketConfig socketConfig, Options options) {
        if (xServer.isDisplayX() || xServer.getRenderer() == null)
            throw new IllegalArgumentException("Vortek currently requires the GL compositor");
        this.xServer = xServer;
        this.socketConfig = socketConfig;
        this.options = options;
        if (!initVulkanWrapper(context.getCacheDir().getAbsolutePath()))
            throw new IllegalStateException("Cannot initialize Vortek system Vulkan/cache");
    }

    @Override public void start() {
        if (connector != null) return;
        connector = new XConnectorEpoll(socketConfig, this, this);
        connector.setInitialInputBufferCapacity(8);
        connector.setInitialOutputBufferCapacity(0);
        stopped = false;
        connector.start();
        Log.i("DimensityVortek", "Private system Vulkan server started");
    }

    @Override public void stop() {
        stopped = true;
        if (connector != null) {
            connector.stop();
            connector = null;
        }
        synchronized (outputs) {
            for (Output output : outputs.values()) retire(output);
            outputs.clear();
        }
    }

    private void retire(Output output) {
        xServer.getRenderer().xServerView.queueEvent(output.source.getTexture()::destroy);
    }

    @Keep private int getWindowWidth(int id) {
        try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER)) {
            Window window = xServer.windowManager.getWindow(id);
            return window != null ? window.getWidth() : 0;
        }
    }

    @Keep private int getWindowHeight(int id) {
        try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER)) {
            Window window = xServer.windowManager.getWindow(id);
            return window != null ? window.getHeight() : 0;
        }
    }

    @Keep private long getWindowHardwareBuffer(int id, boolean bgra, long usage, long key) {
        try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER)) {
            Window window = xServer.windowManager.getWindow(id);
            if (window == null || stopped) return 0;
            synchronized (outputs) {
                Iterator<Map.Entry<Long, Output>> iterator = outputs.entrySet().iterator();
                while (iterator.hasNext()) {
                    Map.Entry<Long, Output> entry = iterator.next();
                    Window owner = xServer.windowManager.getWindow(entry.getValue().windowId);
                    if (owner == null || owner.getContent() != entry.getValue().destination) {
                        retire(entry.getValue());
                        iterator.remove();
                    }
                }
                Output output = outputs.get(key);
                if (output != null && (output.source.getGPUImage().format != (bgra ? 5 : 1)
                        || !output.source.getGPUImage().supportsRenderTargetUsage(usage))) {
                    retire(output);
                    outputs.remove(key);
                    output = null;
                }
                if (output == null) {
                    Drawable destination = window.getContent();
                    GPUImage image = GPUImage.createRenderTargetImage(destination.width, destination.height, bgra, usage);
                    if (image == null) return 0;
                    output = new Output(id, destination, Drawable.fromGpuOutput(destination.width, destination.height, image));
                    outputs.put(key, output);
                    Log.i("DimensityVortek", "GPU output allocated: " + destination.width + "x" + destination.height
                            + " format=" + image.format + " cpuMapped=false");
                }
                return output.source.getGPUImage().acquireForVulkan();
            }
        }
    }

    // Vulkan result values cross only this private JNI bridge.
    private static final int VK_SUCCESS = 0;
    private static final int VK_ERROR_DEVICE_LOST = -4;
    private static final int VK_ERROR_SURFACE_LOST_KHR = -1000000000;
    private static final int VK_ERROR_OUT_OF_DATE_KHR = -1000001004;

    @Keep private void releaseOutput(long key) {
        synchronized (outputs) {
            Output output = outputs.remove(key);
            if (output != null) retire(output);
        }
    }

    @Keep private int updateWindowContent(int id, long key) {
        final Window window;
        final Output output;
        try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER)) {
            window = xServer.windowManager.getWindow(id);
            synchronized (outputs) { output = outputs.get(key); }
            if (stopped) return VK_ERROR_DEVICE_LOST;
            if (window == null) return VK_ERROR_SURFACE_LOST_KHR;
            if (output == null || window.getContent() != output.destination) return VK_ERROR_OUT_OF_DATE_KHR;
        }
        CountDownLatch done = new CountDownLatch(1);
        AtomicBoolean copied = new AtomicBoolean();
        GLRenderer.GpuCopyRequest request = xServer.getRenderer().copyVulkanFrame(window, output.source, success -> {
            copied.set(success);
            done.countDown();
        });
        long start = System.nanoTime();
        try {
            while (!done.await(50, TimeUnit.MILLISECONDS)) {
                if (stopped) {
                    request.cancelBeforeStart();
                    return VK_ERROR_DEVICE_LOST;
                }
                long elapsed = System.nanoTime() - start;
                // A paused surface must not leave a delayed copy reading a
                // producer buffer after it has been reacquired and overwritten.
                if (elapsed > TimeUnit.SECONDS.toNanos(1) && request.cancelBeforeStart())
                    return VK_ERROR_OUT_OF_DATE_KHR;
                // A copy already running cannot be cancelled safely. A stuck
                // GPU ends this device session, preventing buffer reuse.
                if (elapsed > TimeUnit.SECONDS.toNanos(5)) return VK_ERROR_DEVICE_LOST;
            }
        } catch (InterruptedException interrupted) {
            request.cancelBeforeStart();
            Thread.currentThread().interrupt();
            return VK_ERROR_DEVICE_LOST;
        }
        if (!copied.get()) return VK_ERROR_DEVICE_LOST;
        if (++copiedFrames == 1)
            Log.i("DimensityVortek", "First GPU frame copied and completed; gameplay not yet validated");
        return VK_SUCCESS;
    }

    @Override public void handleNewConnection(Client client) { client.createIOStreams(); }

    @Override public void handleConnectionShutdown(Client client) {
        Object tag = client.getTag();
        client.setTag(null);
        if (tag instanceof Long) destroyVkContext((Long)tag);
    }

    @Override public boolean handleRequest(Client client) throws IOException {
        XInputStream input = client.getInputStream();
        if (input.available() < 8) return false;
        int code = input.readInt();
        int length = input.readInt();
        if (length < 0) throw new IOException("Invalid Vortek request length");
        if (code == 1 && client.getTag() == null && length == 0) {
            long context = createVkContext(client.clientSocket.fd, options);
            if (context == 0) throw new IOException("Cannot create Vortek context");
            client.setTag(context);
        } else if ((code >>> 16) == 2 && client.getTag() instanceof Long) {
            if (!handleExtraDataRequest((Long)client.getTag(), code & 0xffff, length))
                throw new IOException("Vortek extra data failed");
        } else throw new IOException("Unexpected Vortek request");
        return true;
    }

    private native boolean initVulkanWrapper(String cacheDirectory);
    private native long createVkContext(int fd, Options options);
    private native void destroyVkContext(long context);
    private native boolean handleExtraDataRequest(long context, int code, int length);
}
