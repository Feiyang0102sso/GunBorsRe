#pragma once
#include "gun_bros_re/ui/ZMenuInternal.h"
using namespace MenuDetail;

int RunTutorialPlayCheck(const std::string &bigDirectory);

int RunProfilePlayCheck(const std::string &bigDirectory);

/** Exercise real card resources and the same renderer/input path as --game.
 * All money, XP, mutated templates and profile writes below are test fixtures. */
int CheckStoreCards(CResTOCManager &toc, ZPackTables &tables, CProfileManager &profile,
    const CPlayerProgress::Template &progress, const CRefinementManager::Template &refinement,
    const std::vector<ZStoreEntry> &store, const std::vector<ZWeaponEntry> &weapons,
    const std::vector<ZArmorEntry> &armor);

/** Native save fixtures and real greeting callbacks; no original saves change. */
int RunPostGameMenuCheck(const std::string &bigDirectory);

int RunPlayerSelectCheck(const std::string &bigDirectory);

int RunGreetingCheck(const std::string &bigDirectory);

int RunRefineryMenuCheck(const std::string &bigDirectory);
/** Advance the actual button Movie to action dispatch, checking no early transfer. */
bool FinishRefineryClick(ZGameMenu &view, ZMenuState &state, CProfileManager &profile,
    const CRefinementManager::Template &data, const std::filesystem::path &path, std::int64_t now, unsigned slot);
int CheckOnlineRefinery(CResTOCManager &toc, ZPackTables &tables, ZGameMenu &view,
    const CRefinementManager::Template &data);

int RunNavigationBarCheck(const std::string &bigDirectory);

int RunMissionMenuCheck(const std::string &bigDirectory);

/** Replay input through the same PLAY callbacks as the GUI, with native saves. */
int RunPlayInteractionCheck(const std::string &bigDirectory);

int RunPlanetMenuCheck(const std::string &bigDirectory);

int RunSocialOfflineCheck(const std::string &bigDirectory);
int RunLocalOnlineCheck(const std::string &bigDirectory);

int RunOptionsCheck(const std::string &bigDirectory);

int RunUpgradePopupCheck(const std::string &bigDirectory);

/** Real bank card/input/prompt path, with native saves and isolated fixtures. */
int CheckBank(CResTOCManager &toc, ZPackTables &tables, const CPlayerProgress::Template &progress,
    const CRefinementManager::Template &refinement, const std::vector<ZStoreEntry> &store,
    const std::vector<ZWeaponEntry> &weapons, const std::vector<ZArmorEntry> &armor);

/** Focused regression for the user's splash, package and clipped badge report. */
int CheckUiFeedback(CResTOCManager &toc, ZPackTables &tables, const CPlayerProgress::Template &progress,
    const CRefinementManager::Template &refinement, const std::vector<ZStoreEntry> &store,
    const std::vector<ZWeaponEntry> &weapons, const std::vector<ZArmorEntry> &armor);

/** Fresh inventory and one live menu, including purchase from an expanded card. */
int RunPackagePurchaseCheck(const std::string &bigDirectory);

int RunStoreTemplateCheck(const std::string &bigDirectory, bool cardsOnly = false, bool bankOnly = false, bool feedbackOnly = false);

/** Retained milestone: aggregate the original menu paths, never legacy mock UI. */
int RunGameMenuCheck(const std::string &bigDirectory);

int RunPromotionCheck(const std::string &bigDirectory);

int RunLoadingWipeCheck(const std::string &bigDirectory);

/** Real BIG card rendering and tab input, with isolated save data. */
int RunPostGamePresentationCheck(const std::string &bigDirectory);

/** Actual menu preview and scene handoffs, with isolated save data. */
int RunAudioTransitionsCheck(const std::string &bigDirectory);

int RunSceneTransitionCheck(const std::string &bigDirectory);

/** Real BIG cards, native save copies and actual expanded-card input. */
int RunDualWeaponCheck(const std::string &bigDirectory);

/* Validation declarations and notes preserved from the former public header:
int RunProfilePlayCheck(const std::string &bigDirectory);
int RunAudioTransitionsCheck(const std::string &bigDirectory);
int RunPostGamePresentationCheck(const std::string &bigDirectory);
int RunTutorialPlayCheck(const std::string &bigDirectory);
int RunGameMenuCheck(const std::string &bigDirectory);
int RunPackagePurchaseCheck(const std::string &bigDirectory);
/ ** Isolated store template playback, hit testing and screenshot acceptance. * /
int RunStoreTemplateCheck(const std::string &bigDirectory, bool cardsOnly = false, bool bankOnly = false, bool feedbackOnly = false);
/ ** Original upgrade phases, resource mutations and isolated purchase/reload. * /
int RunUpgradePopupCheck(const std::string &bigDirectory);
int RunOptionsCheck(const std::string &bigDirectory);
int RunSocialOfflineCheck(const std::string &bigDirectory);
int RunPlanetMenuCheck(const std::string &bigDirectory);
int RunPlayInteractionCheck(const std::string &bigDirectory);
int RunMissionMenuCheck(const std::string &bigDirectory);
int RunNavigationBarCheck(const std::string &bigDirectory);
int RunRefineryMenuCheck(const std::string &bigDirectory);
int RunGreetingCheck(const std::string &bigDirectory);
int RunPlayerSelectCheck(const std::string &bigDirectory);
int RunPostGameMenuCheck(const std::string &bigDirectory);
class CWindow;
int RunDualWeaponCheck(const std::string &bigDirectory);
int RunSceneTransitionCheck(const std::string &bigDirectory);
int RunPromotionCheck(const std::string &bigDirectory);
int RunLoadingWipeCheck(const std::string &bigDirectory);
int RunOriginalDialogCheck(const std::string &bigDirectory);

*/
