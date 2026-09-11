#pragma once
#include "gun_bros_re/ui/MenuInternal.h"

namespace MenuDetail {
class StoreRegionClip {
public:
    StoreRegionClip(GameMenu &view, const MovieRegion &area) {
        enabled = glIsEnabled(GL_SCISSOR_TEST);
        glGetIntegerv(GL_SCISSOR_BOX, previous);
        view.Clip(area.x, area.y, std::max(0.0f, area.width), std::max(0.0f, area.height));
        if (enabled) {
            GLint current[4];
            glGetIntegerv(GL_SCISSOR_BOX, current);
            const int left = std::max(previous[0], current[0]);
            const int bottom = std::max(previous[1], current[1]);
            const int right = std::min(previous[0] + previous[2], current[0] + current[2]);
            const int top = std::min(previous[1] + previous[3], current[1] + current[3]);
            glScissor(left, bottom, std::max(0, right - left), std::max(0, top - bottom));
        }
    }
    ~StoreRegionClip() {
        glScissor(previous[0], previous[1], previous[2], previous[3]);
        if (!enabled) { glDisable(GL_SCISSOR_TEST); }
    }
private:
    GLboolean enabled = GL_FALSE;
    GLint previous[4]{};
};
}
