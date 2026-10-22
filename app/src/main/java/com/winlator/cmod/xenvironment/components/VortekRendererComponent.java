// SPDX-License-Identifier: LGPL-2.1-only
// Based on the paired Winlator Vortek bridge; see cpp/vortekrenderer/LICENSE.
package com.winlator.cmod.xenvironment.components;

import android.content.Context;
import android.util.Log;
import androidx.annotation.Keep;
import com.winlator.cmod.renderer.GPUImage;
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
    private final HashMap<Integer, Output> outputs = new HashMap<>();
    private long copiedFrames;

    private static final class Output {
        final Drawable destination;
        final Drawable source;
        Output(Drawable destination, Drawable source) {
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

    @Keep private long getWindowHardwareBuffer(int id, boolean bgra) {
        try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER)) {
            Window window = xServer.windowManager.getWindow(id);
            if (window == null || stopped) return 0;
            synchronized (outputs) {
                Iterator<Map.Entry<Integer, Output>> iterator = outputs.entrySet().iterator();
                while (iterator.hasNext()) {
                    Map.Entry<Integer, Output> entry = iterator.next();
                    Window owner = xServer.windowManager.getWindow(entry.getKey());
                    if (owner == null || owner.getContent() != entry.getValue().destination) {
                        retire(entry.getValue());
                        iterator.remove();
                    }
                }
                Output output = outputs.get(id);
                if (output != null && output.source.getGPUImage().format != (bgra ? 5 : 1)) {
                    retire(output);
                    outputs.remove(id);
                    output = null;
                }
                if (output == null) {
                    Drawable destination = window.getContent();
                    GPUImage image = GPUImage.createRenderTargetImage(destination.width, destination.height, bgra);
                    if (image == null) return 0;
                    output = new Output(destination, Drawable.fromGpuOutput(destination.width, destination.height, image));
                    outputs.put(id, output);
                    Log.i("DimensityVortek", "GPU output allocated: " + destination.width + "x" + destination.height
                            + " format=" + image.format + " cpuMapped=false");
                }
                return output.source.getGPUImage().acquireForVulkan();
            }
        }
    }

    @Keep private boolean updateWindowContent(int id) {
        final Window window;
        final Output output;
        try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER)) {
            window = xServer.windowManager.getWindow(id);
            synchronized (outputs) { output = outputs.get(id); }
            if (window == null || output == null || window.getContent() != output.destination || stopped) return false;
        }
        // Do not hold an X11 lock while waiting for the GL compositor.
        CountDownLatch done = new CountDownLatch(1);
        AtomicBoolean copied = new AtomicBoolean();
        xServer.getRenderer().copyVulkanFrame(window, output.source, success -> {
            copied.set(success);
            done.countDown();
        });
        long start = System.nanoTime();
        boolean warned = false;
        try {
            while (!done.await(50, TimeUnit.MILLISECONDS)) {
                if (stopped) return false;
                if (!warned && System.nanoTime() - start > TimeUnit.SECONDS.toNanos(2)) {
                    warned = true;
                    Log.i("DimensityVortek", "Waiting for GL copy (surface may be paused)");
                }
            }
        } catch (InterruptedException interrupted) {
            Thread.currentThread().interrupt();
            return false;
        }
        if (copied.get() && ++copiedFrames == 1)
            Log.i("DimensityVortek", "First GPU frame copied and completed; gameplay not yet validated");
        return copied.get();
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
