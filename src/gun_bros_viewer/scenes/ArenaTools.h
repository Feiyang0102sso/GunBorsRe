#pragma once
/** Windows arena camera and consumable shortcuts; no authored combat values. */
#include "gun_bros_re/data/ZPowerupCatalog.h"
#include "gun_bros_re/gameplay/CBrother.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace ArenaDetail {
constexpr float kDefaultZoom = 1.5f;
constexpr float kMinimumZoom = 0.5f;
constexpr float kMaximumZoom = 4.0f;
constexpr int kInfoHeight = 192;

struct ArenaCamera {
    float x = 0;
    float y = 0;
    float scale = 1;
    float zoom = kDefaultZoom;

    void Scroll(float notches) {
        zoom = std::clamp(zoom * std::pow(1.15f, notches), kMinimumZoom, kMaximumZoom);
    }
    void Follow(float playerX, float playerY, int width, int height) {
        // Fill the viewport without stretching. Leave more room ahead of the
        // initial upward aim, while keeping the player visible at every zoom.
        scale = std::max(width / 1200.0f, height / 900.0f) * zoom;
        x = playerX - width / scale * 0.5f;
        y = playerY - height / scale * 0.72f;
    }
    float WorldX(float pixelX) const { return x + pixelX / scale; }
    float WorldY(float pixelY) const { return y + pixelY / scale; }
};

bool LoadArenaGrenades(CResTOCManager &toc, ZPackTables &tables,
    std::array<ZPowerupEntry, 3> &grenades);
bool ThrowArenaGrenade(CBrother &brother, const ZPowerupEntry &entry);
}
