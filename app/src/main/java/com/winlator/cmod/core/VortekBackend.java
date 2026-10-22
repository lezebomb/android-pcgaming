package com.winlator.cmod.core;

import android.content.Context;
import com.winlator.cmod.container.Shortcut;
import com.winlator.cmod.xconnector.UnixSocketConfig;
import com.winlator.cmod.xenvironment.ImageFs;
import com.winlator.cmod.xenvironment.components.VortekRendererComponent;
import com.winlator.cmod.xserver.XServer;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;

/** Opt-in client/server pair, independent of the wrapper and its Mali layer. */
public final class VortekBackend {
    public static final String EXTRA = "vulkanBackend";
    public static final String NAME = "vortek";
    private static final String HASH = "0dd2b39d94c043d0c3724d841eeb442eeaa730247a9196eca1a36c27201e1d09";
    private static final String LIBRARY = "usr/lib/libvulkan_vortek.so";
    private static final String MANIFEST = "usr/share/vulkan/icd.d/dimensity_vortek.json";
    private static final String SOCKET = "/usr/tmp/.vortek/V0";

    private VortekBackend() {}

    public static boolean selected(Shortcut shortcut) {
        return shortcut != null && NAME.equals(shortcut.getExtra(EXTRA));
    }

    public static void install(Context context, ImageFs imageFs) throws IOException {
        File library = new File(imageFs.getRootDir(), LIBRARY);
        if (!HASH.equals(hash(library))) {
            File temporary = new File(library.getPath() + ".part");
            try {
                try (InputStream input = context.getAssets().open("vortek/libvulkan_vortek.so")) {
                    Files.copy(input, temporary.toPath(), StandardCopyOption.REPLACE_EXISTING);
                }
                if (!HASH.equals(hash(temporary))) throw new IOException("Vortek client integrity mismatch");
                Files.move(temporary.toPath(), library.toPath(), StandardCopyOption.REPLACE_EXISTING);
            } finally { Files.deleteIfExists(temporary.toPath()); }
        }
        File manifest = new File(imageFs.getRootDir(), MANIFEST);
        if (!manifest.getParentFile().isDirectory() && !manifest.getParentFile().mkdirs())
            throw new IOException("Cannot create Vortek ICD directory");
        try {
            JSONObject icd = new JSONObject().put("api_version", "1.3.128")
                    .put("library_path", library.getAbsolutePath());
            Files.write(manifest.toPath(), new JSONObject().put("file_format_version", "1.0.0")
                    .put("ICD", icd).toString(2).getBytes(StandardCharsets.UTF_8));
        } catch (JSONException e) { throw new IOException("Cannot write Vortek ICD", e); }
    }

    public static VortekRendererComponent configure(Context context, ImageFs imageFs,
                                                    XServer xServer, EnvVars env) throws IOException {
        UnixSocketConfig socket = UnixSocketConfig.createSocket(imageFs.getRootDir().getPath(), SOCKET);
        if (socket.path.getBytes(StandardCharsets.UTF_8).length >= 108)
            throw new IOException("Vortek socket path exceeds Unix socket limit");
        String manifest = new File(imageFs.getRootDir(), MANIFEST).getAbsolutePath();
        env.put("VK_ICD_FILENAMES", manifest);
        env.put("VK_DRIVER_FILES", manifest);
        env.put("VORTEK_SERVER_PATH", socket.path);
        // Do not carry wrapper-specific emulation/feature claims into this ICD.
        ArrayList<String> layers = new ArrayList<>();
        for (String layer : env.get("VK_INSTANCE_LAYERS").split(":"))
            if (!layer.isEmpty() && !MaliCompatLayer.NAME.equals(layer)) layers.add(layer);
        if (layers.isEmpty()) env.remove("VK_INSTANCE_LAYERS");
        else env.put("VK_INSTANCE_LAYERS", String.join(":", layers));
        try {
            return new VortekRendererComponent(context, xServer, socket, new VortekRendererComponent.Options());
        } catch (IllegalStateException | IllegalArgumentException e) {
            throw new IOException("Cannot initialize Vortek: " + e.getMessage(), e);
        }
    }

    private static String hash(File file) throws IOException {
        if (!file.isFile()) return "";
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            try (InputStream input = Files.newInputStream(file.toPath())) {
                byte[] buffer = new byte[65536];
                int count;
                while ((count = input.read(buffer)) != -1) digest.update(buffer, 0, count);
            }
            StringBuilder result = new StringBuilder(64);
            for (byte b : digest.digest()) result.append(String.format("%02x", b & 255));
            return result.toString();
        } catch (NoSuchAlgorithmException e) { throw new IOException(e); }
    }
}
