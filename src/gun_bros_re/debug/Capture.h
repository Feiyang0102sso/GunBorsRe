#ifdef __cplusplus
#pragma once
#include <string>
#include "engine/graphics/PNGEncode.h"
class CWindow;
namespace Capture {
    /**
     * Read the drawn frame back and write it out as a PNG.
     *
     * The whole reason the PNG encoder exists: a run that saves its first
     * frame can be signed off without a human at the keyboard. Call it
     * before Present.
     */

bool SaveFrame(const CWindow &window, const std::string &path);
}
#define GB_SAVE_FRAME(window, path) Capture::SaveFrame(window, path)

#endif
