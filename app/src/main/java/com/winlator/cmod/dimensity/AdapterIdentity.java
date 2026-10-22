package com.winlator.cmod.dimensity;

import com.winlator.cmod.core.EnvVars;

/** Optional DXGI vendor identity for launchers that reject an ARM adapter. */
public final class AdapterIdentity {
    public static final String EXTRA = "adapterIdentity";
    public static final String NVIDIA = "nvidia";
    public static final String NVIDIA_MAIN = "nvidia-main";

    private AdapterIdentity() {}

    public static boolean enabled(String mode) {
        return NVIDIA.equals(mode) || NVIDIA_MAIN.equals(mode);
    }

    /** DXVK matches section names against the exact executable filename. */
    public static String executableName(String path) {
        try {
            String target = new com.winlator.cmod.core.WindowsLaunchCommand(path, "", "").executable;
            String name = target.substring(target.lastIndexOf('/') + 1);
            if (!name.toLowerCase(java.util.Locale.ROOT).endsWith(".exe") ||
                    name.indexOf('[') >= 0 || name.indexOf(']') >= 0 || name.indexOf(';') >= 0) return null;
            return name;
        } catch (IllegalArgumentException error) { return null; }
    }

    public static void configure(EnvVars environment, String mode, String executablePath) {
        if (NVIDIA.equals(mode)) configure(environment);
        else if (NVIDIA_MAIN.equals(mode)) {
            String name = executableName(executablePath);
            if (name == null) return; // Do not broaden an invalid scope to all children.
            String existing = environment.get("DXVK_CONFIG");
            environment.put("DXVK_CONFIG", (existing.isEmpty() ? "" : existing + ";") +
                    "[" + name + "];dxgi.customVendorId=10de;dxgi.hideNvidiaGpu=False");
        }
    }

    public static void configure(EnvVars environment) {
        String existing = environment.get("DXVK_CONFIG");
        environment.put("DXVK_CONFIG", (existing.isEmpty() ? "" : existing + ";")
                + "dxgi.customVendorId=10de;dxgi.hideNvidiaGpu=False");
    }
}
