#ifndef DIMENSITY_WSI_H
#define DIMENSITY_WSI_H
#include <stdbool.h>
#include <string.h>

// These functions operate on X11 window ids inside the proxy, never on the
// system driver's Android surface handles.
static const char* dgImplementedInstanceExtensions[] = {
    "VK_KHR_surface", "VK_KHR_xlib_surface", "VK_KHR_xcb_surface",
    "VK_KHR_get_surface_capabilities2"
};

static inline bool dgUnsupportedInstanceExtension(const char* name) {
    static const char* names[] = {
        "VK_KHR_android_surface", "VK_KHR_wayland_surface", "VK_KHR_display",
        "VK_KHR_get_display_properties2", "VK_EXT_direct_mode_display",
        "VK_EXT_acquire_drm_display", "VK_EXT_acquire_xlib_display",
        "VK_EXT_display_surface_counter", "VK_EXT_headless_surface",
        "VK_EXT_surface_maintenance1", "VK_EXT_swapchain_colorspace",
        "VK_KHR_surface_protected_capabilities", "VK_GOOGLE_surfaceless_query",
        "VK_EXT_debug_utils", "VK_EXT_debug_report"
    };
    for (unsigned i = 0; i < sizeof(names)/sizeof(names[0]); i++)
        if (!strcmp(name, names[i])) return true;
    return false;
}

static inline bool dgUnsupportedDeviceExtension(const char* name) {
    static const char* names[] = {
        "VK_KHR_present_id", "VK_KHR_present_wait", "VK_GOOGLE_display_timing",
        "VK_KHR_present_id2", "VK_KHR_present_wait2", "VK_EXT_image_compression_control_swapchain",
        "VK_EXT_hdr_metadata", "VK_EXT_swapchain_maintenance1",
        "VK_KHR_swapchain_maintenance1", "VK_KHR_swapchain_mutable_format",
        "VK_KHR_shared_presentable_image", "VK_KHR_display_swapchain",
        "VK_EXT_full_screen_exclusive"
    };
    for (unsigned i = 0; i < sizeof(names)/sizeof(names[0]); i++)
        if (!strcmp(name, names[i])) return true;
    return false;
}
#endif
