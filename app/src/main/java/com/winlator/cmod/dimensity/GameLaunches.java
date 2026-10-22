package com.winlator.cmod.dimensity;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import com.winlator.cmod.XServerDisplayActivity;
import com.winlator.cmod.container.Container;
import com.winlator.cmod.container.ContainerManager;
import com.winlator.cmod.container.Shortcut;
import com.winlator.cmod.core.WindowsLaunchCommand;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/** All entry points launch an existing local game; no store client is selected here. */
public final class GameLaunches {
    public static final String SKIP_AUTO_START = "skip_auto_start";
    /** Last entry only. Never record commands, arguments or authentication data. */
    public static void trace(Context context, String phase, int container) {
        try {
            File directory = new File(context.getFilesDir(), "diagnostics");
            directory.mkdirs();
            org.json.JSONObject report = new org.json.JSONObject().put("timestamp_ms", System.currentTimeMillis())
                    .put("phase", phase).put("container_id", container);
            Files.write(new File(directory, "launch-entry.json").toPath(), report.toString().getBytes(StandardCharsets.UTF_8));
        } catch (Exception ignored) { }
    }
    private static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences("game_launcher", Context.MODE_PRIVATE);
    }

    public static WindowsLaunchCommand command(Shortcut shortcut) {
        return new WindowsLaunchCommand(shortcut.getExtra("executablePath", shortcut.path),
                shortcut.getExtra("execArgs"), shortcut.getExtra("workingDirectory"));
    }

    public static boolean isAutomatic(Context context, Shortcut shortcut) {
        String uuid = shortcut.getExtra("uuid");
        return !uuid.isEmpty() && prefs(context).getInt("container", 0) == shortcut.container.id
                && uuid.equals(prefs(context).getString("uuid", ""));
    }

    public static void setAutomatic(Context context, Shortcut shortcut, boolean enabled) {
        if (enabled) {
            shortcut.genUUID();
            prefs(context).edit().putInt("container", shortcut.container.id)
                    .putString("uuid", shortcut.getExtra("uuid")).apply();
        } else if (shortcut == null || isAutomatic(context, shortcut)) {
            prefs(context).edit().clear().apply();
        }
    }

    public static Shortcut automatic(Context context, ContainerManager manager) {
        SharedPreferences prefs = prefs(context);
        String uuid = prefs.getString("uuid", "");
        Container container = manager.getContainerById(prefs.getInt("container", 0));
        if (uuid.isEmpty() || container == null) return null;
        File[] entries = container.getDesktopDir().listFiles((dir, name) -> name.endsWith(".desktop"));
        if (entries != null) for (File file : entries) {
            if (!file.isFile() || file.length() > 256 * 1024) continue;
            try {
                for (String line : Files.readAllLines(file.toPath(), StandardCharsets.UTF_8)) {
                    if (line.trim().equals("uuid=" + uuid)) return new Shortcut(container, file);
                }
            } catch (IOException ignored) { }
        }
        setAutomatic(context, null, false);
        return null;
    }

    /** External shortcuts may select only a saved game, never an arbitrary command or file. */
    public static Shortcut fromIntent(ContainerManager manager, Intent intent) throws IOException {
        Container container = manager.getContainerById(intent.getIntExtra("container_id", 0));
        String path = intent.getStringExtra("shortcut_path");
        if (container == null || path == null) throw new IOException("游戏快捷方式不存在");
        File file = new File(path).getCanonicalFile();
        if (!file.isFile() || file.length() > 256 * 1024 || !file.getName().endsWith(".desktop")
                || !container.getDesktopDir().getCanonicalFile().equals(file.getParentFile()))
            throw new IOException("请选择游戏库中已经保存的游戏");
        Shortcut shortcut = new Shortcut(container, file);
        command(shortcut); // Reject an incomplete saved entry before creating the runtime.
        return shortcut;
    }

    public static void start(Context context, Shortcut shortcut) {
        trace(context, "runtime_requested", shortcut.container.id);
        context.startActivity(new Intent(context, XServerDisplayActivity.class)
                .putExtra("container_id", shortcut.container.id)
                .putExtra("shortcut_path", shortcut.file.getPath())
                .putExtra("shortcut_name", shortcut.name));
    }
}
