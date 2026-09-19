#include "gun_bros_re/data/mission/Planet.h"
#include "gun_bros_re/data/mission/Mission.h"
#include <cstdio>
bool Planet::LoadSurvivalLevels(CResTOCManager &toc, CGunBros &tables,
    std::map<unsigned, GameObjectRef> &retailLevels) {
    // Original CMenuMission::Bind :162800 orders planets by mapSlot; retail
    // Mission.type=1 separates the four survival entries from tutorial/BOKOR.
    // The host's four-element progress array is this filtered order, not mapSlot.
    retailLevels.clear();
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const unsigned hash = toc.GetPack(packIndex)->GetPackHash();
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Planet);
        for (unsigned index = 0; index < count; ++index) {
            const Planet *source = Load(toc, tables, GameObjectRef{hash, static_cast<std::uint8_t>(index)});
            if (source == nullptr) { return false; }
            const Planet &planet = *source;
            for (const GameObjectRef &missionRef : planet.missions) {
                const Mission *sourceMission = Mission::Load(toc, tables, missionRef);
                if (sourceMission == nullptr) { return false; }
                const Mission &mission = *sourceMission;
                if (mission.type != 1) { continue; }
                GameObjectRef &level = retailLevels[planet.mapSlot];
                if (!level.IsNull() && (level.packHash != mission.level.packHash || level.localIndex != mission.level.localIndex)) {
                    std::printf("[native-profile] multiple levels in retail slot=%u\n", planet.mapSlot);
                    return false;
                }
                level = mission.level;
            }
        }
    }
    return true;
}

#include "gun_bros_re/data/objects/CGameObjectPackObjects.h"
/** Planet::Init :169935; entries/planet_entry.bt, with pack-owned lifetime. */
const Planet *Planet::Load(CResTOCManager &toc, CGunBros &tables, const GameObjectRef &ref) {
    const int packIndex = toc.GetPackIndexFromHash(ref.packHash);
    if (ref.IsNull() || ref.localIndex == 255 || packIndex < 0) { return nullptr; }
    auto &objects = tables.GetObjectPack(packIndex).GetObjects().planets;
    const auto found = objects.find(ref.localIndex);
    if (found != objects.end()) { return &found->second; }
    std::vector<std::uint8_t> bytes;
    if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Planet, ref.localIndex, bytes)) { return nullptr; }
    CArrayInputStream input(bytes);
    Planet value;
    if (!value.Init(input) || input.Available() != 0) {
        std::printf("[planet] invalid pack=%u ordinal=%u remaining=%zu\n", ref.packHash, unsigned(ref.localIndex), input.Available());
        return nullptr;
    }
    const auto inserted = objects.emplace(ref.localIndex, std::move(value));
    return &inserted.first->second;
}
