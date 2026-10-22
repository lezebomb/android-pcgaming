package com.winlator.cmod.core;

import android.content.Context;
import org.apache.commons.compress.archivers.tar.TarArchiveEntry;
import org.apache.commons.compress.archivers.tar.TarArchiveInputStream;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import java.util.zip.GZIPInputStream;

/** Unmodified, pinned upstream Sarek release; selected separately from defaults. */
public final class DxvkSarek {
    public static final String VERSION = "1.14.1-sarek";
    public static final String ASSET = "dxwrapper/dxvk-1.14.1-sarek.tgz";
    private static final String SHA256 = "07a6d5d889e9810ce856511e853cd72d2f547ec12c0dc5d7049b32e4bfa05c25";
    private static final String PREFIX = "dxvk-sarek-1.14.1/build/";
    private static final Set<String> DLLS = new HashSet<>(Arrays.asList(
            "d3d8.dll", "d3d9.dll", "d3d10core.dll", "d3d11.dll", "dxgi.dll"));

    private DxvkSarek() {}

    public static void install(Context context, File windowsDirectory,
            OnExtractFileListener mapper) throws IOException {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            try (InputStream input = context.getAssets().open(ASSET)) {
                byte[] buffer = new byte[65536];
                int length;
                while ((length = input.read(buffer)) != -1) digest.update(buffer, 0, length);
            }
            StringBuilder actual = new StringBuilder();
            for (byte b : digest.digest()) actual.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
            if (!SHA256.equals(actual.toString())) throw new IOException("Sarek archive integrity mismatch");
        } catch (NoSuchAlgorithmException e) { throw new IOException("SHA-256 unavailable", e); }

        // Sources and notices stay in the unchanged archive. Only the ten known
        // regular DLL members can become files; tar paths/links are never used
        // directly as destination paths. Stage all files before replacing DLLs.
        Map<Path, Path> staged = new LinkedHashMap<>();
        Set<String> seen = new HashSet<>();
        String boundary = windowsDirectory.getCanonicalPath() + File.separator;
        try {
            try (TarArchiveInputStream tar = new TarArchiveInputStream(new GZIPInputStream(
                    new BufferedInputStream(context.getAssets().open(ASSET))))) {
                TarArchiveEntry entry;
                while ((entry = tar.getNextTarEntry()) != null) {
                    String name = entry.getName();
                    String architecture;
                    if (name.startsWith(PREFIX + "x64/")) architecture = "x64";
                    else if (name.startsWith(PREFIX + "x32/")) architecture = "x32";
                    else continue;
                    String dll = name.substring((PREFIX + architecture + "/").length());
                    if (!DLLS.contains(dll)) continue;
                    if (!entry.isFile() || entry.isLink() || entry.isSymbolicLink()
                            || entry.getSize() <= 0 || entry.getSize() > 32L * 1024 * 1024
                            || !seen.add(name)) throw new IOException("Invalid Sarek DLL member");
                    File target = new File(windowsDirectory,
                            (architecture.equals("x64") ? "system32/" : "syswow64/") + dll);
                    if (mapper != null) target = mapper.onExtractFile(target, entry.getSize());
                    if (target == null) continue; // Win32 prefix deliberately skips x64.
                    if (!target.getCanonicalPath().startsWith(boundary)) throw new IOException("Invalid Sarek destination");
                    Path directory = target.getParentFile().toPath();
                    Files.createDirectories(directory);
                    Path temporary = Files.createTempFile(directory, ".dg-sarek-", ".part");
                    staged.put(temporary, target.toPath());
                    long copied = 0;
                    try (OutputStream output = Files.newOutputStream(temporary)) {
                        byte[] buffer = new byte[65536];
                        int length;
                        while ((length = tar.read(buffer)) != -1) { output.write(buffer, 0, length); copied += length; }
                    }
                    if (copied != entry.getSize()) throw new IOException("Truncated Sarek DLL");
                }
            }
            if (seen.size() != 2 * DLLS.size() || staged.isEmpty()) throw new IOException("Incomplete Sarek DLL set");
            for (Map.Entry<Path, Path> file : staged.entrySet())
                Files.move(file.getKey(), file.getValue(), StandardCopyOption.REPLACE_EXISTING);
        } finally {
            for (Path temporary : staged.keySet()) Files.deleteIfExists(temporary);
        }
    }
}
