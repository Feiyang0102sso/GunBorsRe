/** Shared diagnostic drawing of the geometry consumed by movement and projectile tests. */
#include "gun_bros_re/debug/CollisionOverlay.h"
#include "gun_bros_re/gameplay/ZMapWorldInternal.h"
#include "gun_bros_re/debug/DebugConfig.h"

namespace {
constexpr float kRadians = 3.14159265f / 180;

void DrawLines(ZMarkerBatch &markers, const ZShaderProgram &program, const float *projection,
    const DebugConfig::LineStyle &style) {
    const auto &color = style.color;
    markers.Draw(program, projection, color.red, color.green, color.blue, color.alpha);
}

void Circle(ZMarkerBatch &markers, float x, float y, float radius, float width) {
    if (radius <= 0) { return; }
    for (unsigned index = 0; index < DebugConfig::CircleSegments; ++index) {
        const float first = index * 360.0f / DebugConfig::CircleSegments * kRadians;
        const float second = (index + 1) * 360.0f / DebugConfig::CircleSegments * kRadians;
        markers.AddSegment(x + std::cos(first) * radius, y + std::sin(first) * radius,
            x + std::cos(second) * radius, y + std::sin(second) * radius, width);
    }
}

void Edges(ZMarkerBatch &markers, const CCollisionData &collision, bool enabled,
    float width, float x = 0, float y = 0, float facing = 0, float scale = 1) {
    const auto &vertices = collision.GetVertices();
    const float cosine = std::cos(facing * kRadians);
    const float sine = std::sin(facing * kRadians);
    for (const auto &edge : collision.GetEdges()) {
        if (edge.enabled != enabled) { continue; }
        const auto &first = vertices[edge.firstVertex];
        const auto &second = vertices[edge.secondVertex];
        markers.AddSegment(x + (first.x * cosine - first.y * sine) * scale,
            y + (first.x * sine + first.y * cosine) * scale,
            x + (second.x * cosine - second.y * sine) * scale,
            y + (second.x * sine + second.y * cosine) * scale, width);
    }
}

void Enemy(ZMarkerBatch &markers, const CEnemy &enemy, float gameScale,
    float x, float y, float width, bool enabled) {
    const auto &state = enemy.combat;
    if (state.removed || state.dead || !state.enabled) { return; }
    // CEnemy::TestCollisions :73079 selects complex edges OR visible part circles.
    if (!state.collision.GetEdges().empty()) {
        Edges(markers, state.collision, enabled, width, x, y, state.facing, state.scaleFactor);
    } else if (enabled) {
        for (unsigned part = 0; part < enemy.GetPartCount(); ++part) {
            if (!enemy.GetPart(part).visible) { continue; }
            float centerX = x, centerY = y, radius = 0;
            enemy.GetCollisionCircle(gameScale, part, centerX, centerY, radius);
            Circle(markers, centerX, centerY, radius, width);
        }
    }
}
}

void DrawCollisionOverlay(ZMarkerBatch &markers, const ZShaderProgram &program,
    const float *projection, float pixelSize, const MapDetail::ZLoadedMap *map,
    const CLevel *combat, const CBrotherAI *brother, const CLevel *effects) {
    if (map != nullptr) {
        /** Collect the exact collision scene used by player movement. */
        // Nested widths keep coincident movement / bullet / terrain edges visible.
        markers.Begin();
        Edges(markers, map->collisionScene, true, DebugConfig::Body.width * pixelSize);
        DrawLines(markers, program, projection, DebugConfig::Body);
        markers.Begin();
        Edges(markers, map->weaponCollision.walls, true, DebugConfig::BulletWall.width * pixelSize);
        DrawLines(markers, program, projection, DebugConfig::BulletWall);
        markers.Begin();
        Edges(markers, map->weaponCollision.terrain, true, DebugConfig::Terrain.width * pixelSize);
        DrawLines(markers, program, projection, DebugConfig::Terrain);
    }
    for (bool enabled : {false, true}) {
        const DebugConfig::LineStyle *style = &DebugConfig::Enemy;
        if (!enabled) { style = &DebugConfig::Disabled; }
        markers.Begin();
        if (!enabled && map != nullptr) {
            Edges(markers, map->collisionScene, false, style->width * pixelSize);
            Edges(markers, map->weaponCollision.walls, false, style->width * pixelSize);
            Edges(markers, map->weaponCollision.terrain, false, style->width * pixelSize);
        }
        if (combat != nullptr) {
            for (const auto &actor : combat->GetEnemies()) {
                const auto &state = actor->combat;
                Enemy(markers, *actor, actor->data->gameScale,
                    state.x, state.y, style->width * pixelSize, enabled);
            }
        } else if (map != nullptr) {
            for (const auto &actor : map->enemies) {
                Enemy(markers, *actor, actor->data->gameScale, actor->combat.x, actor->combat.y, style->width * pixelSize, enabled);
            }
        }
        DrawLines(markers, program, projection, *style);
    }
    markers.Begin();
    if (combat != nullptr) {
        Circle(markers, combat->GetPlayer().x, combat->GetPlayer().y, combat->GetPlayerRadius(), DebugConfig::Brother.width * pixelSize);
        if (brother != nullptr && !brother->vitals.dead) {
            Circle(markers, brother->x, brother->y, combat->GetPlayerRadius(), DebugConfig::Brother.width * pixelSize);
        }
    } else if (map != nullptr) {
        for (const auto &player : map->players) {
            Circle(markers, player.x, player.y, MapDetail::kPlayerCollisionRadius, DebugConfig::Brother.width * pixelSize);
        }
    }
    DrawLines(markers, program, projection, DebugConfig::Brother);
    if (effects != nullptr) {
        const auto projectiles = effects->GetProjectileStates();
        for (bool enabled : {false, true}) {
            const DebugConfig::LineStyle *style = &DebugConfig::Projectile;
            if (!enabled) { style = &DebugConfig::Disabled; }
            markers.Begin();
            for (const auto &shot : projectiles) {
                if (shot.collisionEnabled != enabled) { continue; }
                // CBullet::UpdateBeam -> RayCastNearest traces a zero-radius line.
                if (shot.beam) {
                    const float angle = shot.direction * kRadians;
                    markers.AddSegment(shot.x, shot.y, shot.x + std::cos(angle) * shot.length,
                        shot.y + std::sin(angle) * shot.length, style->width * pixelSize);
                } else { Circle(markers, shot.x, shot.y, shot.collisionRadius, style->width * pixelSize); }
            }
            DrawLines(markers, program, projection, *style);
        }
    }
}
