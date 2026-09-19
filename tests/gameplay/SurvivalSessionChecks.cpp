#include "gun_bros_re/gameplay/game/CGame.h"
#include "gun_bros_re/data/store/CStoreItem.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
unsigned CheckLevelSounds(CLevel &level, CLevel &effects) {
    unsigned failures = 0, checked = 0;
    const auto &resources = level.GetTemplate().script.GetResources();
    for (unsigned index = 0; index < resources.size(); ++index) {
        if (resources[index].sectionOrType != static_cast<unsigned>(ZGameSection::SoundEffect) - 1) { continue; }
        // Each reference is checked independently. Some authored entries share
        // a WAV, so checking them in one tick would correctly coalesce them.
        effects.BeginAudioFrame();
        const std::size_t before = effects.GetSoundCueCount();
        const std::int16_t argument = static_cast<std::int16_t>(index);
        level.FunctionResolver(20, &argument, 1);
        if (effects.GetSoundCueCount() != before + 1) { ++failures; }
        ++checked;
    }
    std::printf("[level-sound-check] references=%u failures=%u\n", checked, failures);
    return failures;
}
unsigned CheckTriggerRoutes(CGame &session, CMap &map, CLevel &scene, float startX, float startY, float startFacing) {
    CLevel &level = session.GetLevel();
    unsigned failures = 0, tested = 0;
    for (unsigned layerIndex = 0; layerIndex < map.GetCollisionLayerCount(); ++layerIndex) {
        const auto &layer = map.GetCollisionLayer(layerIndex);
        if (static_cast<int>(layer.GetLayerIndex()) != level.GetTriggerLayer()) { continue; }
        const auto &geometry = layer.GetCollision();
        std::vector<unsigned> groups;
        for (const auto &edge : geometry.GetEdges()) {
            if (std::find(groups.begin(), groups.end(), edge.group) != groups.end()) { continue; }
            bool reached = false;
            const auto &a = geometry.GetVertices()[edge.firstVertex];
            const auto &b = geometry.GetVertices()[edge.secondVertex];
            const float length = std::hypot(b.x - a.x, b.y - a.y);
            if (length == 0) { continue; }
            const float nx = -(b.y - a.y) / length, ny = (b.x - a.x) / length;
            for (int side : {-1, 1}) {
                session.Restart(startX, startY, startFacing);
                // Let the original intro complete before supplying movement.
                for (unsigned tick = 0; tick < 250; ++tick) { session.Update(16, 0, 0, false); }
                scene.GetPlayer().x = (a.x + b.x) * 0.5f + nx * 45 * side;
                scene.GetPlayer().y = (a.y + b.y) * 0.5f + ny * 45 * side;
                const unsigned before = level.GetTriggerCount();
                for (unsigned tick = 0; tick < 45; ++tick) { session.Update(16, -nx * side, -ny * side, false); }
                reached = level.GetTriggerCount() > before;
                if (reached) { break; }
            }
            std::printf("[map-trigger-check] layer=%u group=%u reached=%d position=%.1f,%.1f\n",
                layer.GetLayerIndex(), edge.group, reached, scene.GetPlayer().x, scene.GetPlayer().y);
            if (reached) { groups.push_back(edge.group); ++tested; }
        }
        std::vector<unsigned> expected;
        for (const auto &edge : geometry.GetEdges()) {
            if (std::find(expected.begin(), expected.end(), edge.group) == expected.end()) { expected.push_back(edge.group); }
        }
        if (groups.size() != expected.size()) { ++failures; }
    }
    session.Restart(startX, startY, startFacing);
    std::printf("[map-trigger-check] groups=%u failures=%u\n", tested, failures);
    return failures;
}
