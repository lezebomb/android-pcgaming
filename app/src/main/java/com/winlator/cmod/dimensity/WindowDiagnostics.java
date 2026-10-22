package com.winlator.cmod.dimensity;

import android.content.Context;
import android.util.Log;
import com.winlator.cmod.renderer.GLRenderer;
import com.winlator.cmod.xserver.Drawable;
import com.winlator.cmod.xserver.Window;
import com.winlator.cmod.xserver.XLock;
import com.winlator.cmod.xserver.XServer;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

/** On-demand metadata from this session only. No window text or pixel contents. */
public final class WindowDiagnostics {
    public static void capture(Context context, XServer server, GLRenderer renderer) {
        Context app = context.getApplicationContext();
        Runnable write = () -> {
            try {
                JSONObject report = new JSONObject();
                JSONArray windows = new JSONArray();
                try (XLock ignored = server.lock(XServer.Lockable.WINDOW_MANAGER, XServer.Lockable.DRAWABLE_MANAGER)) {
                    collect(server.windowManager.rootWindow, windows, 0);
                    report.put("windows", windows);
                    JSONArray rendered = new JSONArray();
                    if (renderer != null) for (int id : renderer.getRenderableDrawableIds()) rendered.put(id);
                    report.put("rendered_drawables", rendered);
                }
                report.put("timestamp_ms", System.currentTimeMillis());
                File directory = new File(app.getFilesDir(), "diagnostics");
                directory.mkdirs();
                File temporary = new File(directory, "windows.json.part");
                Files.write(temporary.toPath(), report.toString(2).getBytes(StandardCharsets.UTF_8));
                Files.move(temporary.toPath(), new File(directory, "windows.json").toPath(), StandardCopyOption.REPLACE_EXISTING);
                Log.d("DimensityWindows", "Saved " + windows.length() + " session windows");
            } catch (Exception failure) { Log.w("DimensityWindows", "Snapshot unavailable", failure); }
        };
        if (renderer != null) renderer.xServerView.queueEvent(write);
        else new Thread(write, "WindowDiagnostics").start();
    }

    private static void collect(Window window, JSONArray entries, int depth) throws Exception {
        if (depth > 32 || entries.length() >= 128) return;
        Drawable content = window.getContent();
        JSONObject entry = new JSONObject().put("id", window.id)
                .put("parent", window.getParent() == null ? 0 : window.getParent().id)
                .put("class", window.getClassName()).put("mapped", window.attributes.isMapped())
                .put("transient_for", window.getTransientForId())
                .put("x", window.getRootX()).put("y", window.getRootY())
                .put("width", window.getWidth()).put("height", window.getHeight());
        if (content != null) entry.put("drawable", content.id)
                .put("texture", content.getTexture().getClass().getSimpleName())
                .put("allocated", content.getTexture().isAllocated())
                .put("dirty", content.getTexture().isNeedsUpdate())
                .put("cpu_buffer_bytes", content.getData() == null ? 0 : content.getData().capacity());
        entries.put(entry);
        for (Window child : window.getChildren()) collect(child, entries, depth + 1);
    }
}
