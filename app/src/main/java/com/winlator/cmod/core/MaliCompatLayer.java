package com.winlator.cmod.core;

import android.content.Context;
import android.util.Log;
import com.winlator.cmod.xenvironment.ImageFs;
import org.json.JSONObject;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;

/** Optional application-private layer. Feature claims do not establish game compatibility. */
public final class MaliCompatLayer {
    public static final String NAME = "VK_LAYER_COMPAT_DxvkMaliCompatLayer";
    private static final String HASH = "f023a7820abcde1af21e7fc8de1e3ca0d77682e66d93af048a136873a6e9c79d";

    private MaliCompatLayer() {}

    public static boolean enabled(KeyValueSet config) {
        return "1".equals(config.get("maliCompat", "0"));
    }

    public static void install(Context context, ImageFs imageFs) throws IOException {
        String renderer = GPUInformation.getRenderer(null, context);
        if (renderer == null || !renderer.contains("G615"))
            throw new IOException("Experimental Mali layer currently requires Mali-G615");
        File library = new File(imageFs.getRootDir(), "usr/lib/libdxvk_mali_compat_layer.so");
        if (!matches(library)) {
            File temporary = new File(library.getPath() + ".part");
            try {
                try (InputStream input = context.getAssets().open("mali_compat/libdxvk_mali_compat_layer.so")) {
                    Files.copy(input, temporary.toPath(), StandardCopyOption.REPLACE_EXISTING);
                }
                if (!matches(temporary)) throw new IOException("Mali compatibility layer integrity mismatch");
                Files.move(temporary.toPath(), library.toPath(), StandardCopyOption.REPLACE_EXISTING);
            } finally {
                Files.deleteIfExists(temporary.toPath());
            }
        }
        try {
            JSONObject layer = new JSONObject();
            layer.put("name", NAME);
            layer.put("type", "GLOBAL");
            layer.put("library_path", library.getAbsolutePath());
            layer.put("api_version", "1.3.0");
            layer.put("implementation_version", "1");
            layer.put("description", "Dimensity experimental Mali compatibility (explicit opt-in)");
            layer.put("functions", new JSONObject()
                    .put("vkGetInstanceProcAddr", "DxvkMaliCompatLayer_GetInstanceProcAddr")
                    .put("vkGetDeviceProcAddr", "DxvkMaliCompatLayer_GetDeviceProcAddr"));
            File directory = new File(imageFs.getRootDir(), "usr/share/vulkan/explicit_layer.d");
            if (!directory.isDirectory() && !directory.mkdirs()) throw new IOException("Cannot create explicit layer directory");
            Files.write(new File(directory, "dimensity_mali_compat.json").toPath(),
                    new JSONObject().put("file_format_version", "1.0.0").put("layer", layer)
                            .toString(2).getBytes(StandardCharsets.UTF_8));
        } catch (org.json.JSONException exception) {
            throw new IOException("Cannot prepare Mali compatibility layer manifest", exception);
        }
    }

    public static void configure(EnvVars environment, ImageFs imageFs) {
        WrapperShaderBoundsFix.configure(imageFs, environment);
        String layers = environment.get("VK_INSTANCE_LAYERS");
        boolean alreadySelected = false;
        for (String layer : layers.split(":")) if (NAME.equals(layer)) alreadySelected = true;
        if (!alreadySelected) environment.put("VK_INSTANCE_LAYERS", layers.isEmpty() ? NAME : layers + ":" + NAME);
        if (!environment.has("COMPAT_EMULATE_SPARSE_BINDING")) environment.put("COMPAT_EMULATE_SPARSE_BINDING", "1");
        // Dense fallback must not inherit the upstream 8 GiB default on an 8 GiB tablet.
        if (!environment.has("COMPAT_SPARSE_COMMIT_BUDGET")) environment.put("COMPAT_SPARSE_COMMIT_BUDGET", "1073741824");
        Log.i("DimensityMali", "Experimental compatibility selected; sparse commit default 1 GiB; game support unverified");
    }

    private static boolean matches(File file) throws IOException {
        if (!file.isFile()) return false;
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            try (InputStream input = new FileInputStream(file)) {
                byte[] buffer = new byte[64 * 1024];
                int count;
                while ((count = input.read(buffer)) != -1) digest.update(buffer, 0, count);
            }
            StringBuilder hash = new StringBuilder();
            for (byte value : digest.digest()) hash.append(String.format("%02x", value & 255));
            return HASH.contentEquals(hash);
        } catch (java.security.NoSuchAlgorithmException exception) {
            throw new IOException(exception);
        }
    }
}
