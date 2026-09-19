#include "gun_bros_re/data/ZPlanetCatalog.h"
bool LoadRetailSurvivalLevels(CResTOCManager &toc, ZPackTables &tables,
    std::map<unsigned, GameObjectRef> &retailLevels) {
    // Original CMenuMission::Bind :162800 orders planets by mapSlot; retail
    // Mission.type=1 separates the four survival entries from tutorial/BOKOR.
    // The host's four-element progress array is this filtered order, not mapSlot.
    retailLevels.clear();
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const unsigned hash = toc.GetPack(packIndex)->GetPackHash();
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Planet);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(hash, ZGameSection::Planet, index, bytes)) { return false; }
            CArrayInputStream input(bytes);
            Planet planet;
            if (!planet.Init(input) || input.Available() != 0) { return false; }
            for (const GameObjectRef &missionRef : planet.missions) {
                if (!tables.ReadSectionResource(missionRef.packHash, ZGameSection::Mission, missionRef.localIndex, bytes)) { return false; }
                CArrayInputStream missionInput(bytes);
                Mission mission;
                if (!mission.Init(missionInput) || missionInput.Available() != 0) { return false; }
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
