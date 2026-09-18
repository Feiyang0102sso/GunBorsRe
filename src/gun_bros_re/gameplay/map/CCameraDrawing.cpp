#include "gun_bros_re/gameplay/map/CCameraDrawing.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace MapDetail {

/** Centre the fixed GameView camera on the controlled player. */
void FollowPlayerCamera(const CMap &loaded, int viewWidth, int viewHeight,
                        CCamera::Viewport &camera) {
    if (loaded.GetResources().players.empty()) {
        return;
    }

    const float viewWorldWidth = static_cast<float>(viewWidth) / camera.zoom;
    const float viewWorldHeight = static_cast<float>(viewHeight) / camera.zoom;
    camera.x = loaded.GetResources().players[0].x - viewWorldWidth * 0.5f;
    camera.y = loaded.GetResources().players[0].y - viewWorldHeight * 0.5f;
    if (loaded.GetCamera().HasPosition()) {
        camera.x = loaded.GetCamera().GetX() - viewWorldWidth * 0.5f;
        camera.y = loaded.GetCamera().GetY() - viewWorldHeight * 0.5f;
    }

    const CLayerCamera::Rectangle bounds = loaded.GetVisibleBounds();
    if (bounds.IsEmpty()) {
        return;
    }

    const float left = static_cast<float>(bounds.x);
    const float top = static_cast<float>(bounds.y);
    const float right = static_cast<float>(bounds.x + bounds.width);
    const float bottom = static_cast<float>(bounds.y + bounds.height);

    if (viewWorldWidth >= static_cast<float>(bounds.width)) {
        camera.x = left + (static_cast<float>(bounds.width) - viewWorldWidth) *
                            0.5f;
    } else {
        if (camera.x < left) {
            camera.x = left;
        }
        if (camera.x + viewWorldWidth > right) {
            camera.x = right - viewWorldWidth;
        }
    }

    if (viewWorldHeight >= static_cast<float>(bounds.height)) {
        camera.y = top + (static_cast<float>(bounds.height) - viewWorldHeight) *
                           0.5f;
    } else {
        if (camera.y < top) {
            camera.y = top;
        }
        if (camera.y + viewWorldHeight > bottom) {
            camera.y = bottom - viewWorldHeight;
        }
    }
}

/** Keep the default stage's framing across maps; bounds only limit panning.
 * This viewer setting is deliberately independent of stage dimensions.
 */
float GameViewCameraZoom(int viewWidth,
                         int viewHeight) {
    const float logicalScaleX = static_cast<float>(viewWidth) /
                                kGameViewWorldWidth;
    const float logicalScaleY = static_cast<float>(viewHeight) /
                                kGameViewWorldHeight;
    float zoom = logicalScaleX;
    if (logicalScaleY > zoom) {
        zoom = logicalScaleY;
    }
    return zoom;
}

/**
 * Where the camera bounds land on the window, as a scissor rectangle.
 *
 * A map's tile layers run past the rectangle the game is ever allowed to show:
 * a lava or starfield layer wraps and keeps filling to the edge of the canvas,
 * and the terrain layer above it stops short. The engine never reveals that
 * because the camera stops at these bounds. This viewer fits whole maps on
 * screen, so it has to clip instead.
 *
 * @param scissor Filled with x, y, width, height in window pixels, GL's
 *                bottom-left origin.
 * @return false when the map declares no camera layer; nothing is clipped then.
 */
bool VisibleBoundsScissor(const CMap &loaded, const CCamera::Viewport &camera,
                          int drawableWidth, int drawableHeight, int scissor[4]) {
    const CLayerCamera::Rectangle bounds = loaded.GetCameraExtent();
    if (bounds.IsEmpty()) {
        return false;
    }

    const float left = (static_cast<float>(bounds.x) - camera.x) * camera.zoom;
    const float top = (static_cast<float>(bounds.y) - camera.y) * camera.zoom;
    const float right = left + static_cast<float>(bounds.width) * camera.zoom;
    const float bottom = top + static_cast<float>(bounds.height) * camera.zoom;

    // Flip to GL's bottom-left origin, then clamp to the window.
    float x0 = left;
    float x1 = right;
    float y0 = static_cast<float>(drawableHeight) - bottom;
    float y1 = static_cast<float>(drawableHeight) - top;

    if (x0 < 0.0f) {
        x0 = 0.0f;
    }
    if (y0 < 0.0f) {
        y0 = 0.0f;
    }
    if (x1 > static_cast<float>(drawableWidth)) {
        x1 = static_cast<float>(drawableWidth);
    }
    if (y1 > static_cast<float>(drawableHeight)) {
        y1 = static_cast<float>(drawableHeight);
    }

    scissor[0] = static_cast<int>(x0);
    scissor[1] = static_cast<int>(y0);
    scissor[2] = 0;
    scissor[3] = 0;

    // Panned fully off screen. Still clipping, with nothing left to draw.
    if (x1 > x0) {
        scissor[2] = static_cast<int>(x1 - x0);
    }
    if (y1 > y0) {
        scissor[3] = static_cast<int>(y1 - y0);
    }

    return true;
}

/** Convert world anchors and native pixel sizes to the HUD's logical canvas. */
void ProjectEnemyHealthBars(std::vector<CLevel::HealthBar> &bars,
    float cameraX, float cameraY, float zoom, int width, int height) {
    for (auto &bar : bars) {
        bar.x = (bar.x - cameraX) * zoom * 1024 / width;
        bar.y = (bar.y - cameraY) * zoom * 768 / height;
        bar.width *= 1024.0f / width;
        bar.height *= 768.0f / height;
        bar.border *= 1024.0f / width;
        // Centre in screen space, after the world anchor has been projected.
        bar.x -= bar.width * 0.5f;
    }
}
}
