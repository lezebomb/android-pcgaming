package com.winlator.cmod.inputcontrols;

import android.content.Context;
import android.os.SystemClock;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** Opt-in, bounded gamepad-only trace in this app's private diagnostics directory. */
public final class GamepadDiagnostics {
    private static volatile File output;
    private static volatile int remaining;

    private GamepadDiagnostics() {}

    public static synchronized void initialize(Context context) {
        File directory = new File(context.getFilesDir(), "diagnostics");
        output = new File(directory, "gamepad-trace.enabled").isFile()
                ? new File(directory, "gamepad-trace.txt") : null;
        remaining = 256;
        if (output != null) {
            try (FileOutputStream stream = new FileOutputStream(output, false)) {
                stream.write("gamepad trace (no keyboard text)\n".getBytes(StandardCharsets.UTF_8));
            } catch (IOException e) { output = null; }
        }
    }

    public static boolean isEnabled() { return output != null && remaining > 0; }

    public static synchronized void record(String event) {
        if (output == null || remaining-- <= 0) return;
        try (FileOutputStream stream = new FileOutputStream(output, true)) {
            stream.write((SystemClock.uptimeMillis() + " " + event + "\n")
                    .getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) { output = null; }
    }
}
