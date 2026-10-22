package com.winlator.cmod.dimensity;
public final class VulkanProbe {
    static { System.loadLibrary("dimensity_probe"); }
    public static native String inspect();
}
