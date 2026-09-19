#pragma once
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"

namespace MenuDetail {
// Connect the rebuilt offline account to actual gameplay.
// Internal menu collaboration interfaces; ZGameFrontEnd.h remains the
// production public entry.
/** Returns selected planet, -1 for quit, -2 after capture, -3 on failure. */
int ShowGameMenu(CResTOCManager &toc, ZPackTables &tables, CProfileManager &profile,
    const CPlayerProgress::Template &progressData, const CRefinementManager::Template &refinement,
    const std::vector<ZStoreEntry> &store, const std::vector<ZWeaponEntry> &weapons,
    const std::vector<ZArmorEntry> &armors, CMenuSystem &state, const std::filesystem::path &savePath,
    const std::string &capturePath, const std::vector<ZMenuInputFrame> *inputFrames = nullptr, bool originalProfile = false, ZWindow *sharedWindow = nullptr, bool animateTransitions = false, ZMenuTransitionTrace *transitionTrace = nullptr, CBGM *sharedMusic = nullptr);
}
