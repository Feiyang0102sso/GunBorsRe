#pragma once
#include <string>
#include "engine/graphics/ZPNGEncode.h"
class ZWindow;
namespace Capture {
    /**
     * Read the drawn frame back and write it out as a PNG.
     *
     * The whole reason the PNG encoder exists: a run that saves its first
     * frame can be signed off without a human at the keyboard. Call it
     * before Present.
     */

bool SaveFrame(const ZWindow &window, const std::string &path);
}
