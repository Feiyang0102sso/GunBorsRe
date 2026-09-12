#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
/** Persistent game-font FPS display, independent of DebugMode. */
#include "engine/platform/IWindowOverlay.h"
#include "engine/graphics/CBitmapFont.h"
#include <cstdint>
class CWindow;

// Empty bigDirectory uses the executable's normal BIG directory; an existing pass is retained.
bool SetDebugFPS(CWindow &window, bool enabled, const std::string &bigDirectory = "");

class FrameRateOverlay : public IWindowOverlay {
public:
    bool Init(const std::string &bigDirectory);
    void Tick(std::uint64_t now) override;
    void Draw(int width, int height) override;
private:
    CShaderProgram m_program;
    CBitmapFont m_font;
    CQuadBatch m_batch;
    std::uint64_t m_started = 0;
    unsigned m_frames = 0;
    float m_fps = 0;
};
