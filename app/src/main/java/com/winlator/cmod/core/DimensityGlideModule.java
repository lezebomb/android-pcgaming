package com.winlator.cmod.core;

import com.bumptech.glide.annotation.GlideModule;
import com.bumptech.glide.module.AppGlideModule;

/** No legacy manifest modules are used; avoid the package metadata lookup on startup. */
@GlideModule
public final class DimensityGlideModule extends AppGlideModule {
    @Override public boolean isManifestParsingEnabled() { return false; }
}
