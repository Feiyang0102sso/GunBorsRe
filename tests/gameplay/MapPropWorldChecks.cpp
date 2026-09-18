#include "gameplay/SurvivalChecks.h"

namespace MapDetail {

    unsigned CheckPropEntryRoutes(const CMap &map, const CLevel &scene) {
        unsigned tested = 0, failures = 0;
        for (const CProp &prop : map.GetResources().props) {
            if (!prop.active || !prop.HasScript() || !prop.ChecksEntry()) { continue; }
            const auto &vertices = prop.GetEntryCollision().GetVertices();
            if (vertices.empty()) { ++failures; continue; }
            float x = 0, y = 0;
            for (const auto &point : vertices) { x += point.x; y += point.y; }
            x = prop.x + x / vertices.size();
            y = prop.y + y / vertices.size();
            unsigned routes = 0;
            for (const auto &point : vertices) {
                const float dx = prop.x + point.x - x, dy = prop.y + point.y - y;
                if (scene.CanWalkTo(x + dx * 2, y + dy * 2, x, y)) { ++routes; }
            }
            std::printf("[map-entry-check] prop=%08x:%u id=%d centre=%.1f,%.1f vertices=%zu routes=%u\n",
                prop.resources->resource.packHash, prop.resources->resource.localIndex, prop.objectId, x, y, vertices.size(), routes);
            if (routes == 0) { ++failures; }
            ++tested;
        }
        std::printf("[map-entry-check] tested=%u failures=%u\n", tested, failures);
        return failures;
    }

    unsigned CheckPropDamageContracts(const CMap &map) {
        unsigned tested = 0, failures = 0;
        for (const CProp &prop : map.GetResources().props) {
            if (!prop.active || !prop.HasScript() || prop.GetHealth() <= 0) { continue; }
            // Independent instance: checking a barrel must not damage the
            // account or change its real level-script progress.
            CProp probe;
            probe.Bind(prop.resources->data, &prop.resources->durations);
            const unsigned initialState = probe.GetStateId();
            const float initialHealth = probe.GetHealth();
            probe.Damage(10000, 0xffffffffu);
            for (int elapsed = 0; elapsed < 2500; elapsed += 16) { probe.Update(16, false); }
            if (probe.GetStateId() == initialState && probe.GetHealth() == initialHealth) { ++failures; }
            failures += probe.GetUnsupportedCount();
            ++tested;
        }
        std::printf("[prop-check] independent damage/timer/animation templates=%u failures=%u\n", tested, failures);
        return failures;
    }
}
