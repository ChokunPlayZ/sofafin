// Simulate the fatal Gamescope Vulkan-device path without a display or GPU.
extern "C" {
#include <libavutil/hwcontext.h>
}
#include <cerrno>
#include <unistd.h>

extern "C" int av_hwdevice_ctx_create(AVBufferRef **, AVHWDeviceType type,
                                      const char *, AVDictionary *, int) {
    if (type == AV_HWDEVICE_TYPE_VULKAN) {
        constexpr char message[] = "Unexpected Vulkan hardware-decoder probe\n";
        (void)write(STDERR_FILENO, message, sizeof(message) - 1);
        _exit(86);
    }
    // Make every allowed hardware device unavailable so software fallback is tested.
    return AVERROR(ENOSYS);
}
