#pragma once

/** Desktop window presets. These are host choices, not original game resource data. */
enum class ZScreenResolution {
    Size640x480,
    Size800x600,
    Size1024x768,
    Size1280x960,
    Size1600x1200,
    Size1920x1440,
    Size2048x1536,
    Saved
};

struct ZScreenResolutionOption {
    ZScreenResolution value;
    int width;
    int height;
};
