package com.winlator.cmod.dimensity;

import com.winlator.cmod.core.EnvVars;
import java.util.regex.Pattern;

/** Avoid an auxiliary WineD3D GL adapter query while D3D rendering uses DXVK. */
public final class LegacyGraphicsQuery {
    public static final String EXTRA = "legacyGraphicsQuery";
    public static final String GDI = "gdi";
    private static final Pattern RENDERER = Pattern.compile("(?i)(^|,)\\s*renderer\\s*=");

    private LegacyGraphicsQuery() {}

    public static void configure(EnvVars environment, String mode) {
        if (!GDI.equals(mode)) return;
        String existing = environment.get("WINE_D3D_CONFIG");
        // An explicit custom renderer remains authoritative; preserve other keys.
        if (RENDERER.matcher(existing).find()) return;
        environment.put("WINE_D3D_CONFIG", existing.isEmpty() ? "renderer=gdi"
                : existing + (existing.endsWith(",") ? "" : ",") + "renderer=gdi");
    }
}
