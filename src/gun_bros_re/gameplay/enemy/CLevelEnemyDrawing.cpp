/** @file CLevelEnemyDrawing.cpp
 * Original: src/gunbros/level.cpp DrawEnemyHealthBars :120454.
 * Windows adaptation returns the draw geometry to the shared renderer.
 */
#include "gun_bros_re/gameplay/level/CLevel.h"
#include <algorithm>
#include <cmath>

std::vector<CLevel::HealthBar> CLevel::EnemyHealthBars(float viewportScale) const {
    // CLevel::DrawEnemyHealthBars :120454: native 30x4 and inset 1.
    // BIG controls visibility and the larger-bar flag; it has no size table.
    // Correction: mem+311032 is LEVEL variable 4 (the script's boss-wave
    // flag), NOT the revolution. Camera mem+0 is min(width/480,height/320),
    // NOT SnapScale/GetScale. These sizes are already physical screen pixels.
    float waveScale = 1;
    if (HasLargeEnemyHealthBars()) { waveScale = 2; }
    const float width = int(30 * viewportScale * waveScale);
    const float height = int(4 * viewportScale * waveScale);
    const float border = int(viewportScale * waveScale);
    std::vector<HealthBar> bars;
    if (IsDeathmatch() && m_brother != nullptr && !m_brother->vitals.dead && m_brother->vitals.health > 0 &&
        m_brotherModel->IsVisible()) {
        // CLevel::DrawBrotherHealthBar :120249 uses the original native 30x4.
        const auto bounds = m_brotherModel->GetBounds();
        const float scale = m_brotherModel->GetWorldScale(m_playerGameScale, m_cameraScale);
        bars.push_back({m_brother->x, m_brother->y - bounds.maxZ * scale,
            float(int(30 * viewportScale)), float(int(4 * viewportScale)), float(int(viewportScale)),
            std::min(1.0f, m_brother->vitals.health / m_brother->vitals.maximum), 0, 199 / 255.0f, 8 / 255.0f});
    }
    for (const auto &actor : m_objects.GetEnemies()) {
        const CEnemy &enemy = *actor;
        const CEnemy::CombatState &state = enemy.combat;
        if (!state.enabled || state.removed || state.dead || state.health <= 0 ||
            state.maxHealth <= 0 || state.variables[15] == 0) { continue; }
        const CMesh *body = enemy.GetPart(0).controller.GetAnimation().GetMesh();
        if (body == nullptr) { continue; }
        const float scale = body->GetBounds().inverseExtent * actor->data->gameScale;
        bool first = true;
        float left = 0, right = 0, top = 0;
        for (unsigned part = 0; part < enemy.GetPartCount(); ++part) {
            const CMesh *mesh = enemy.GetPart(part).controller.GetAnimation().GetMesh();
            if (mesh == nullptr) { continue; }
            const ZMeshBounds &bounds = mesh->GetBounds();
            const int extent = int(std::max(std::abs(bounds.maxX - bounds.minX),
                std::abs(bounds.maxY - bounds.minY)) * scale);
            if (extent == 0) { continue; }
            const float partLeft = int(bounds.centerX) - extent / 2;
            const float partTop = int(bounds.centerY) - extent / 2;
            if (first) { left = partLeft; right = partLeft + extent; top = partTop; first = false; }
            else { left = std::min(left, partLeft); right = std::max(right, partLeft + extent); top = std::min(top, partTop); }
        }
        if (first) { continue; }
        // GetBounds :67485 adds a native 20-unit margin. The damage pulse is
        // cos((remainingMs/1000+1)*pi/2)*-50 added to original red 0xC80000.
        const float bright = std::cos((state.healthBarFlashMs * 0.001f + 1) * 3.14159265f * 0.5f) * -50;
        // Preserve the world-space top centre until projection. Width is in
        // screen pixels and must not become a camera-scaled world offset.
        bars.push_back({state.x + (left + right) * 0.5f,
            state.y + top - 20, width, height, border,
            std::min(1.0f, state.health / state.maxHealth), (200 + bright) / 255});
    }
    return bars;
}
