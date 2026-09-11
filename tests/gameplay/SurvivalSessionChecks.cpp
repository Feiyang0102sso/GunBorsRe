#include "gun_bros_re/gameplay/SurvivalSession.h"
#include "gun_bros_re/data/StoreCatalog.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
unsigned SurvivalSession::CheckLevelSounds() {
    if (m_effects == nullptr) { return 1; }
    unsigned failures = 0, checked = 0;
    const auto &resources = m_template.script.GetResources();
    for (unsigned index = 0; index < resources.size(); ++index) {
        if (resources[index].sectionOrType != static_cast<unsigned>(GameSection::SoundEffect) - 1) { continue; }
        // Each reference is checked independently. Some authored entries share
        // a WAV, so checking them in one tick would correctly coalesce them.
        m_effects->BeginAudioFrame();
        const std::size_t before = m_effects->GetSoundCueCount();
        const std::int16_t argument = static_cast<std::int16_t>(index);
        m_level.FunctionResolver(20, &argument, 1);
        if (m_effects->GetSoundCueCount() != before + 1) { ++failures; }
        ++checked;
    }
    std::printf("[level-sound-check] references=%u failures=%u\n", checked, failures);
    return failures;
}
unsigned SurvivalSession::CheckTriggerRoutes(float startX, float startY, float startFacing) {
    unsigned failures = 0, tested = 0;
    for (unsigned layerIndex = 0; layerIndex < m_map.GetCollisionLayerCount(); ++layerIndex) {
        const auto &layer = m_map.GetCollisionLayer(layerIndex);
        if (static_cast<int>(layer.GetLayerIndex()) != m_level.GetTriggerLayer()) { continue; }
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
                Restart(startX, startY, startFacing);
                // Let the original intro complete before supplying movement.
                for (unsigned tick = 0; tick < 250; ++tick) { Update(16, 0, 0, false); }
                m_scene.playerX = (a.x + b.x) * 0.5f + nx * 45 * side;
                m_scene.playerY = (a.y + b.y) * 0.5f + ny * 45 * side;
                const unsigned before = m_level.GetTriggerCount();
                for (unsigned tick = 0; tick < 45; ++tick) { Update(16, -nx * side, -ny * side, false); }
                reached = m_level.GetTriggerCount() > before;
                if (reached) { break; }
            }
            std::printf("[map-trigger-check] layer=%u group=%u reached=%d position=%.1f,%.1f\n",
                layer.GetLayerIndex(), edge.group, reached, m_scene.playerX, m_scene.playerY);
            if (reached) { groups.push_back(edge.group); ++tested; }
        }
        std::vector<unsigned> expected;
        for (const auto &edge : geometry.GetEdges()) {
            if (std::find(expected.begin(), expected.end(), edge.group) == expected.end()) { expected.push_back(edge.group); }
        }
        if (groups.size() != expected.size()) { ++failures; }
    }
    Restart(startX, startY, startFacing);
    std::printf("[map-trigger-check] groups=%u failures=%u\n", tested, failures);
    return failures;
}