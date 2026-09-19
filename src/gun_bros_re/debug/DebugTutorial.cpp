/** Menu debug replay; original LEVEL Flow still controls every tutorial step. */
#include "gun_bros_re/debug/DebugTutorial.h"
#include "gun_bros_re/data/ZProfileStorage.h"
#include "gun_bros_re/gameplay/game/CGameFlow.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/data/ZPackTables.h"
#include <cstdio>

bool PrepareDebugTutorial(CResTOCManager &toc, ZPackTables &tables, CGameFlow &context,
    CGame::Launch &launch) {
    if (!CreateTransientProfile(toc, tables, context.profile)) { return false; }
    context.tutorial = true;
    context.debugTutorial = true;
    context.persistProgress = false;
    context.savePath.clear();
    // Same resource chain as first launch: native survival slot -> LEVEL -> map.
    const auto &level = context.profile.nativeArchive->survivalLevels[0];
    std::vector<std::uint8_t> bytes;
    if (!tables.ReadSectionResource(level.packHash, ZGameSection::Level, level.localIndex, bytes)) { return false; }
    CArrayInputStream input(bytes);
    CLevel::Template data;
    if (!data.Init(input) || input.Available() != 0) { return false; }
    launch.packShortName = tables.GetPackName(data.mapRef.packHash);
    launch.mapIndex = data.mapRef.localIndex;
    launch.gameContext = &context;
    launch.withBrother = true;
    std::printf("[debug-tutorial] start map=%s:%u no-save=1\n", launch.packShortName.c_str(), launch.mapIndex);
    return true;
}
