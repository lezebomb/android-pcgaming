package com.winlator.cmod.core;

public class RamBooster {
    // Artificial memory pressure is permanently disabled in this build.
    public static boolean pressure(long targetBytes, int chunkSleepMs, int holdMs) {
        return false;
    }
}
