package com.winlator.cmod.dimensity;

import com.winlator.cmod.core.EnvVars;

/** Optional DXGI vendor identity for launchers that reject an ARM adapter. */
public final class AdapterIdentity {
    public static final String EXTRA = "adapterIdentity";
    public static final String NVIDIA = "nvidia";

    private AdapterIdentity() {}

    public static void configure(EnvVars environment) {
        String existing = environment.get("DXVK_CONFIG");
        environment.put("DXVK_CONFIG", (existing.isEmpty() ? "" : existing + ";")
                + "dxgi.customVendorId=10de;dxgi.hideNvidiaGpu=False");
    }
}
