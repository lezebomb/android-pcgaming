package com.winlator.cmod.dimensity;

import android.content.Context;
import com.winlator.cmod.core.Callback;
import com.winlator.cmod.core.ProcessHelper;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

/** Bounded recent output plus a separate error/context trail; no transcript in heap. */
public final class RuntimeLog implements Callback<String>, AutoCloseable {
    private final Segment recent, errors;
    private int errorContext;

    public RuntimeLog(Context context) {
        File directory = new File(context.getFilesDir(), "diagnostics");
        directory.mkdirs();
        recent = new Segment(directory, "wine.log", 512 * 1024);
        errors = new Segment(directory, "wine-errors.log", 64 * 1024);
        ProcessHelper.addDebugCallback(this);
    }

    @Override public synchronized void call(String line) {
        if (line == null) return;
        String bounded = line.length() > 4096 ? line.substring(0, 4096) : line;
        byte[] bytes = (bounded + "\n").getBytes(StandardCharsets.UTF_8);
        boolean critical = line.contains(":err:") || line.contains("[error]")
                || line.contains("handle_syscall_fault") || line.contains(":seh:dispatch_exception")
                || line.contains("_wassert") || line.contains("Unhandled") || line.contains("terminating");
        recent.append(bytes, critical);
        if (critical) errorContext = 20;
        if (critical || errorContext > 0) {
            errors.append(bytes, critical);
            if (!critical && --errorContext == 0) errors.flush();
        }
    }

    @Override public void close() {
        ProcessHelper.removeDebugCallback(this);
        synchronized (this) { recent.close(); errors.close(); }
    }

    private static final class Segment {
        private final File current, previous;
        private final int limit;
        private OutputStream stream;
        private int written, lines;
        private long flushed;

        Segment(File directory, String name, int limit) {
            current = new File(directory, name);
            previous = new File(directory, name + ".previous");
            this.limit = limit;
            try { Files.deleteIfExists(previous.toPath()); open(); }
            catch (IOException ignored) { close(); }
        }

        private void open() throws IOException {
            stream = new BufferedOutputStream(new FileOutputStream(current), 16 * 1024);
            written = lines = 0;
            flushed = System.nanoTime();
        }

        void append(byte[] bytes, boolean critical) {
            if (stream == null) return;
            try {
                if (written + bytes.length > limit) {
                    stream.close();
                    stream = null;
                    Files.move(current.toPath(), previous.toPath(), StandardCopyOption.REPLACE_EXISTING);
                    open();
                }
                stream.write(bytes);
                written += bytes.length;
                long now = System.nanoTime();
                if (critical || ++lines >= 64 || now - flushed >= 1_000_000_000L) {
                    stream.flush(); lines = 0; flushed = now;
                }
            } catch (IOException ignored) { close(); }
        }

        void flush() {
            if (stream != null) try { stream.flush(); } catch (IOException ignored) { close(); }
        }

        void close() {
            if (stream != null) try { stream.close(); } catch (IOException ignored) { }
            stream = null;
        }
    }
}