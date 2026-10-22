package com.winlator.cmod.dimensity;

import android.app.ActivityManager;
import android.content.Context;
import android.os.*;
import android.system.Os;
import android.util.Log;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;

/** Low-frequency, same-UID sampling. Missing procfs fields stay unavailable. */
public final class MemoryMonitor implements AutoCloseable {
    private final Context context;
    private final ScheduledExecutorService worker = Executors.newSingleThreadScheduledExecutor();
    private final long started = SystemClock.elapsedRealtime();
    private final File directory;
    private final int containerId;
    private PrintWriter csv;
    private long peakRssKiB, launchPeakRssKiB, samples;
    private int lowStreak;
    private boolean closed;

    public MemoryMonitor(Context context, int containerId) {
        this.context = context.getApplicationContext();
        this.containerId = containerId;
        directory = new File(context.getFilesDir(), "diagnostics");
        directory.mkdirs();
        try {
            csv = new PrintWriter(new OutputStreamWriter(new FileOutputStream(new File(directory, "memory.csv")), StandardCharsets.UTF_8));
            csv.println("elapsed_ms,total_bytes,available_bytes,low_memory,app_pss_kib,owned_rss_kib,owned_pss_kib,readable_processes,thermal_status");
            context.getSharedPreferences("dimensity", 0).edit().putBoolean("session_in_progress", true).apply();
        } catch (IOException e) { Log.w("DimensityMemory", "CSV unavailable", e); }
        worker.scheduleWithFixedDelay(this::sampleSafely, 0, 5, TimeUnit.SECONDS);
    }

    public static ActivityManager.MemoryInfo systemMemory(Context context) {
        ActivityManager.MemoryInfo info = new ActivityManager.MemoryInfo();
        ((ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE)).getMemoryInfo(info);
        return info;
    }

    public static String launchRisk(Context context, int containerId) {
        ActivityManager.MemoryInfo info = systemMemory(context);
        long history = context.getSharedPreferences("dimensity", 0).getLong("peak_rss_" + containerId, 0);
        if (info.lowMemory) return "Android 已处于低内存状态，已取消本次启动。请降低分辨率后重试。";
        if (info.availMem < 1536L * 1024 * 1024 || (history > 0 && info.availMem < history * 1024 + 512L * 1024 * 1024))
            return "可用内存偏低，建议低内存预设。历史 RSS 含共享页，仅作风险参考。";
        return "";
    }

    private void sampleSafely() {
        try { sample(); } catch (Exception e) { Log.w("DimensityMemory", "Sample unavailable", e); }
    }

    private synchronized void sample() throws Exception {
        if (closed || samples++ >= 10000) return;
        ActivityManager.MemoryInfo info = systemMemory(context);
        long rss = 0, pss = 0;
        boolean allPss = true;
        int count = 0;
        JSONArray processes = new JSONArray();
        File[] dirs = new File("/proc").listFiles(f -> f.getName().matches("[0-9]+"));
        if (dirs != null) for (File dir : dirs) {
            try {
                if (Os.stat(dir.getPath()).st_uid != android.os.Process.myUid()) continue;
                String name = "unavailable";
                Long procRss = null, procPss = null;
                try (BufferedReader r = new BufferedReader(new FileReader(new File(dir, "status")))) {
                    String line;
                    while ((line = r.readLine()) != null) {
                        if (line.startsWith("Name:")) name = line.substring(5).trim();
                        if (line.startsWith("VmRSS:")) procRss = kib(line);
                    }
                }
                try (BufferedReader r = new BufferedReader(new FileReader(new File(dir, "smaps_rollup")))) {
                    String line;
                    while ((line = r.readLine()) != null) if (line.startsWith("Pss:")) { procPss = kib(line); break; }
                } catch (IOException ignored) { }
                if (procRss != null) rss += procRss;
                if (procPss != null) pss += procPss; else allPss = false;
                count++;
                processes.put(new JSONObject().put("pid", dir.getName()).put("name", name)
                        .put("rss_kib", procRss == null ? "unavailable" : procRss)
                        .put("pss_kib", procPss == null ? "unavailable" : procPss));
            } catch (Exception ignored) { /* inaccessible or exited */ }
        }
        long elapsed = SystemClock.elapsedRealtime() - started;
        peakRssKiB = Math.max(peakRssKiB, rss);
        if (elapsed <= 60000) launchPeakRssKiB = Math.max(launchPeakRssKiB, rss);
        int thermal = -1;
        if (Build.VERSION.SDK_INT >= 29) thermal = ((PowerManager) context.getSystemService(Context.POWER_SERVICE)).getCurrentThermalStatus();
        if (csv != null) {
            csv.println(elapsed + "," + info.totalMem + "," + info.availMem + "," + info.lowMemory + "," + Debug.getPss()
                    + "," + (count > 0 ? rss : "unavailable") + "," + (count > 0 && allPss ? pss : "unavailable") + "," + count + "," + thermal);
            csv.flush();
        }
        try (Writer out = new OutputStreamWriter(new FileOutputStream(new File(directory, "processes.json")), StandardCharsets.UTF_8)) {
            out.write(new JSONObject().put("elapsed_ms", elapsed).put("processes", processes)
                    .put("scope", "readable same-UID processes; RSS includes shared pages; visibility may be incomplete")
                    .put("gpu_allocations", "unavailable").toString(2));
        }
        lowStreak = info.lowMemory ? lowStreak + 1 : 0;
        if (lowStreak == 3) new Handler(Looper.getMainLooper()).post(() -> android.widget.Toast.makeText(context,
                "持续低内存：下次启动建议选低内存预设。诊断已记录。", android.widget.Toast.LENGTH_LONG).show());
        context.getSharedPreferences("dimensity", 0).edit().putLong("peak_rss_" + containerId, peakRssKiB).apply();
    }

    private static long kib(String line) { return Long.parseLong(line.substring(line.indexOf(':') + 1).trim().split("\\s+")[0]); }

    @Override public synchronized void close() {
        if (closed) return;
        closed = true;
        worker.shutdownNow();
        if (csv != null) csv.close();
        context.getSharedPreferences("dimensity", 0).edit().putBoolean("session_in_progress", false).apply();
        try (Writer out = new OutputStreamWriter(new FileOutputStream(new File(directory, "session.json")), StandardCharsets.UTF_8)) {
            out.write(new JSONObject().put("container_id", containerId).put("peak_sampled_owned_rss_kib", peakRssKiB)
                    .put("first_60s_peak_rss_kib", launchPeakRssKiB).put("sample_interval_ms", 5000)
                    .put("shutdown_available_bytes", systemMemory(context).availMem)
                    .put("release_verified", false).put("note", "Sampled peaks can miss transient allocations; shutdown does not prove GPU release.").toString(2));
        } catch (Exception e) { Log.w("DimensityMemory", "Session summary unavailable", e); }
    }
}
