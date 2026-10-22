package com.winlator.cmod.core;

import android.util.Log;
import com.winlator.cmod.xenvironment.ImageFs;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/** Bounds-only repair of the exact bundled open-source wrapper; never edit the original.
 * Upstream b8663654 (spirv_patcher.cpp) receives bytes but indexes uint32_t words.
 * The two instructions retain the shader transformation and correct its traversal limit.
 */
public final class WrapperShaderBoundsFix {
    private static final String ORIGINAL = "3b4c285550847a0caa6d110614298d6c0c57feefd9c96460713ad707a3c8304e";
    private static final String FIXED = "6b229b929ee543440eb992c7163cbc4fcbe6765c252af10a97bc47e835be638b";
    private static final String LIBRARY = "usr/lib/libvulkan_wrapper_dimensity.so";
    private static final String MANIFEST = "usr/share/vulkan/icd.d/dimensity_wrapper_bounds.json";

    private WrapperShaderBoundsFix() {}

    public static void install(ImageFs imageFs) throws IOException {
        File original = new File(imageFs.getRootDir(), "usr/lib/libvulkan_wrapper.so");
        File manifest = manifest(imageFs);
        if (!ORIGINAL.equals(hash(original))) {
            // A user-supplied driver must never inherit an old patched ICD selection.
            Files.deleteIfExists(manifest.toPath());
            Log.w("DimensityMali", "Unrecognized wrapper; bounds repair not applied");
            return;
        }
        File fixed = new File(imageFs.getRootDir(), LIBRARY);
        if (!FIXED.equals(hash(fixed))) {
            File temporary = new File(fixed.getPath() + ".part");
            try {
                Files.copy(original.toPath(), temporary.toPath(), StandardCopyOption.REPLACE_EXISTING);
                try (RandomAccessFile output = new RandomAccessFile(temporary, "rw")) {
                    replace(output, 0x110b04, 0x7100183f, 0x7100603f); // cmp w1, #24 (six words)
                    replace(output, 0x110b24, 0x2a0103f3, 0x53027c33); // lsr w19, w1, #2
                }
                if (!FIXED.equals(hash(temporary))) throw new IOException("Wrapper bounds repair integrity mismatch");
                Files.move(temporary.toPath(), fixed.toPath(), StandardCopyOption.REPLACE_EXISTING);
            } finally {
                Files.deleteIfExists(temporary.toPath());
            }
        }
        try {
            JSONObject icd = new JSONObject().put("api_version", "1.3.303")
                    .put("library_path", fixed.getAbsolutePath());
            Files.write(manifest.toPath(), new JSONObject().put("file_format_version", "1.0.0")
                    .put("ICD", icd).toString(2).getBytes(StandardCharsets.UTF_8));
        } catch (JSONException exception) {
            throw new IOException("Cannot prepare wrapper bounds manifest", exception);
        }
    }

    public static void configure(ImageFs imageFs, EnvVars environment) {
        if ("0".equals(environment.get("DIMENSITY_WRAPPER_BOUNDS"))) return;
        File manifest = manifest(imageFs);
        if (manifest.isFile()) {
            // Both names are supported by the bundled Vulkan loader. Setting both
            // prevents a saved VK_DRIVER_FILES from overriding the selected ICD.
            environment.put("VK_ICD_FILENAMES", manifest.getAbsolutePath());
            environment.put("VK_DRIVER_FILES", manifest.getAbsolutePath());
            Log.i("DimensityMali", "Application-private wrapper shader bounds repair selected");
        }
    }

    private static File manifest(ImageFs imageFs) { return new File(imageFs.getRootDir(), MANIFEST); }

    private static void replace(RandomAccessFile file, long offset, int before, int after) throws IOException {
        file.seek(offset);
        if (Integer.reverseBytes(file.readInt()) != before) throw new IOException("Unexpected wrapper instruction");
        file.seek(offset);
        file.writeInt(Integer.reverseBytes(after));
    }

    private static String hash(File file) throws IOException {
        if (!file.isFile()) return "";
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            byte[] buffer = new byte[64 * 1024];
            try (InputStream input = Files.newInputStream(file.toPath())) {
                int count;
                while ((count = input.read(buffer)) != -1) digest.update(buffer, 0, count);
            }
            StringBuilder value = new StringBuilder(64);
            for (byte b : digest.digest()) value.append(String.format("%02x", b & 255));
            return value.toString();
        } catch (NoSuchAlgorithmException exception) { throw new IOException(exception); }
    }
}
