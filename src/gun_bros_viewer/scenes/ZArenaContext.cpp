/** Original evidence: entries/level_template.bt, maps/map.bt;
 * CEnemy native 54 :72587 reads CLevel::GetResource, not the ENEMY table.
 * This host keeps the original LEVEL intact while omitting arena scenery.
 */
#include "gun_bros_viewer/scenes/ZArenaContext.h"
#include "gun_bros_re/data/objects/CGameObjectPack.h"
#include "gun_bros_re/gameplay/map/CCameraDrawing.h"
#include <cstdio>

namespace {
// Viewer placement preferences, not enemy behavior or authored map data.
constexpr unsigned kSpawnAttempts = 120;
constexpr float kSpawnAngleStep = 2.39996323f;
constexpr float kSpawnDistance = 280;
constexpr float kSpawnRingStep = 55;
constexpr float kSpawnClearance = 20;
}

bool ZArenaContext::Load(CResTOCManager &toc, CGunBros &tables) {
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const auto *pack = toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Level);
        for (unsigned index = 0; index < count; ++index) {
            Entry entry;
            entry.reference.packHash = pack->GetPackHash();
            entry.reference.localIndex = static_cast<std::uint8_t>(index);
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(entry.reference.packHash, ZGameSection::Level, index, payload)) { return false; }
            CArrayInputStream stream(payload);
            if (!entry.data.Init(stream) || stream.Available() != 0) { return false; }
            m_levels.push_back(std::move(entry));
        }
    }
    return !m_levels.empty();
}

bool ZArenaContext::Bind(CGunBros &tables, CLevel &scene, const CEnemy::Template &enemy) {
    const Entry *selected = nullptr;
    unsigned matches = 0;
    for (const Entry &entry : m_levels) {
        for (const auto &resource : entry.data.script.GetResources()) {
            if (resource.sectionOrType != static_cast<unsigned>(ZGameSection::Enemy) - 1 ||
                resource.packHash != enemy.packHash || resource.resourceId != enemy.ordinal) { continue; }
            ++matches;
            // A shared enemy can belong to multiple real levels. Prefer its
            // own pack; otherwise retain the first archive reference and report it.
            if (selected == nullptr || (selected->reference.packHash != enemy.packHash &&
                entry.reference.packHash == enemy.packHash)) { selected = &entry; }
            break;
        }
    }

    // Even unreferenced/unused catalog entries need real camera bounds for
    // CPlayer::Move. The fallback supplies geometry only, never a fake LEVEL table.
    const Entry *geometry = selected;
    if (geometry == nullptr) { geometry = &m_levels.front(); }
    const auto &map = geometry->data.mapRef;
    std::vector<std::uint8_t> payload;
    if (!tables.ReadSectionResource(map.packHash, ZGameSection::TileLayer, map.localIndex, payload)) { return false; }
    scene.Reset();
    CArrayInputStream stream(payload);
    if (!m_map.Init(stream) || stream.Available() != 0 || m_map.GetCameraExtent().IsEmpty()) { return false; }

    m_label = "LEVEL context unavailable";
    if (selected != nullptr) {
        scene.Bind(selected->data, m_map);
        char label[96];
        std::snprintf(label, sizeof(label), "LEVEL %08x:%u (%u matches)",
            selected->reference.packHash, selected->reference.localIndex, matches);
        m_label = label;
    }
    // Blank laboratory: no invisible walls or authored navigation corridors.
    // SetMap still connects CPlayer::Move to the same live enemy object pool.
    scene.SetMap(m_map, m_emptyCollision, m_emptyWeaponCollision,
        MapDetail::kLevelCameraScale, MapDetail::kPlayerCollisionRadius);
    const std::int16_t noPath = -1;
    scene.FunctionResolver(4, &noPath, 1);
    // Discard OnLevelStart's placed actors. The viewer manually chooses actors
    // and ticks combat; it does not advance the level's wave/spawner scheduler.
    scene.Reset();
    std::printf("[arena-context] enemy=%s %s map=%08x:%u empty geometry\n",
        enemy.owner.c_str(), m_label.c_str(), map.packHash, map.localIndex);
    return true;
}

CEnemy *ZArenaContext::SpawnNearby(CLevel &scene, std::size_t index, const CEnemy::Template &enemy) const {
    // The legacy shared arena shortcut assumes 1200x900. The blank viewer now
    // borrows actual BIG camera bounds, so place manually through CLevel::Spawn.
    const auto bounds = scene.GetPlayerMovementBounds();
    const float radius = std::max(40.0f, static_cast<float>(enemy.radius116));
    for (unsigned attempt = 0; attempt < kSpawnAttempts; ++attempt) {
        const float angle = (scene.GetEnemies().size() + attempt) * kSpawnAngleStep;
        const float distance = std::max(kSpawnDistance, radius + scene.GetPlayerRadius() + kSpawnClearance) +
            (attempt % 5) * kSpawnRingStep;
        const float x = scene.GetPlayer().x + std::sin(angle) * distance;
        const float y = scene.GetPlayer().y - std::cos(angle) * distance;
        if (x - radius < bounds.left || x + radius > bounds.right ||
            y - radius < bounds.top || y + radius > bounds.bottom) { continue; }
        bool free = true;
        for (const auto &actor : scene.GetEnemies()) {
            if (actor->combat.removed) { continue; }
            const float otherRadius = std::max(35.0f, actor->GetPart(0).radius);
            if (std::hypot(x - actor->combat.x, y - actor->combat.y) < radius + otherRadius + kSpawnClearance) {
                free = false;
                break;
            }
        }
        if (free) { return scene.Spawn(index, x, y); }
    }
    std::printf("[arena] no free spawn position\n");
    return nullptr;
}
