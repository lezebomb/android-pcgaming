package com.winlator.cmod.dimensity;
import com.winlator.cmod.container.Container;
import com.winlator.cmod.box64.Box64Preset;
import com.winlator.cmod.fexcore.FEXCorePreset;
import com.winlator.cmod.core.KeyValueSet;

public final class DeviceProfiles {
    public static final String[] NAMES = {"低内存优先", "均衡（默认）", "性能优先"};
    public static final String[] RESOLUTIONS = {"960x600", "1280x800", "1280x800"};
    public static final String[] DETAILS = {
        "960×600 / 30 FPS；保留缓存，正确性优先的转译参数。游戏内请手动选择低纹理、低阴影。",
        "1280×800 / 30 FPS；保留缓存、同步屏障和驱动功能检测。游戏内请手动选择低画质。",
        "1280×800 / 60 FPS；保留缓存与浮点、内存顺序正确性；实际帧率取决于游戏。"
    };
    public static void apply(Container c, int index) {
        if (index < 0 || index >= NAMES.length) throw new IllegalArgumentException("profile");
        c.setScreenSize(RESOLUTIONS[index]);
        c.setEnvVars(Container.DEFAULT_ENV_VARS);
        c.setDXWrapper("dxvk");
        KeyValueSet config = new KeyValueSet(Container.DEFAULT_DXWRAPPERCONFIG);
        config.put("framerate", index == 2 ? "60" : "30");
        config.put("dxvkConfig", "1");
        c.setDXWrapperConfig(config.toString());
        c.setGraphicsDriver("wrapper");
        c.setGraphicsDriverConfig(Container.DEFAULT_GRAPHICSDRIVERCONFIG);
        c.setBox64Preset(Box64Preset.STABILITY);
        c.setFEXCorePreset(FEXCorePreset.STABILITY);
        c.setRamBoosterEnabled(false);
        c.setShowFPS(true);
        c.putExtra("dimensityProfile", String.valueOf(index));
        c.saveData();
    }
}
