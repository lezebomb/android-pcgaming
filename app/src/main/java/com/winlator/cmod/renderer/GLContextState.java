package com.winlator.cmod.renderer;

import java.util.concurrent.atomic.AtomicInteger;

/** GL names belong to one context, even when a new context reuses the same numbers. */
public final class GLContextState {
    private static final AtomicInteger sequence = new AtomicInteger();
    private static final ThreadLocal<Integer> current = ThreadLocal.withInitial(() -> 0);

    private GLContextState() {}

    public static void created() { current.set(sequence.incrementAndGet()); }

    public static int current() { return current.get(); }
}
