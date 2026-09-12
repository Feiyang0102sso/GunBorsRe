#include "Capture.h"
#include "engine/platform/CWindow.h"
#include "engine/platform/GLLoader.h"
#include "engine/graphics/CPNG.h"
#include <cstring>
/**
 * Read the current frame back and save it.
 *
 * glReadPixels hands back rows bottom-up, so they are flipped on the way into
 * the image.
 */
bool Capture::SaveFrame(const CWindow &window, const std::string &path) {
    window.DrawPresentationOverlay();
    int width = 0;
    int height = 0;
    window.GetDrawableSize(width, height);

    PNGImage frame;
    frame.width = static_cast<std::uint32_t>(width);
    frame.height = static_cast<std::uint32_t>(height);
    frame.pixels.resize(static_cast<std::size_t>(width) * height * 4);

    // GL hands rows back bottom-up; PNG wants them top-down.
    std::vector<std::uint8_t> bottomUp(frame.pixels.size());
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bottomUp.data());
    if (!GLCheckErrors("glReadPixels")) {
        return false;
    }

    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
    for (int y = 0; y < height; ++y) {
        std::memcpy(&frame.pixels[static_cast<std::size_t>(y) * rowBytes],
                    &bottomUp[static_cast<std::size_t>(height - 1 - y) * rowBytes],
                    rowBytes);
    }

    return PNGEncode(frame, path);
}
