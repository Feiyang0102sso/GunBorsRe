#include "gun_bros_re/data/profile/CRefinementManager.h"
#include "gun_bros_re/data/profile/CPlayerProgress.h"
#pragma once
#include "gun_bros_re/ui/host/ZMenuSession.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuMovieMultiplayerOverlay.h"
#include "gun_bros_re/ui/menus/CMenuMissionInfo.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/menus/CMenuGameResources.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/menus/CMenuList.h"
#include "gun_bros_re/ui/menus/CMenuGreeting.h"
#include "gun_bros_re/ui/host/ZLoadingScreen.h"
#include "gun_bros_re/ui/host/ZMenuWipe.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
#include "gun_bros_re/cheats/CheatActions.h"
#include "gun_bros_re/data/profile/CProfileManager.h"
#include "gun_bros_re/gameplay/powerup/CPowerup.h"
#include "gun_bros_re/startup/ZStartupSequence.h"
#include "engine/glu/sprite/CSpriteIterator.h"
using namespace MenuDetail;

int RunTutorialPlayCheck(const std::string &bigDirectory);

int RunProfilePlayCheck(const std::string &bigDirectory);

/** Exercise real card resources and the same renderer/input path as --game.
 * All money, XP, mutated templates and profile writes below are test fixtures. */
int CheckStoreCards(CResTOCManager &toc, CGunBros &tables, CProfileManager &profile,
    const CPlayerProgress::Template &progress, const CRefinementManager::Template &refinement,
    const std::vector<CStoreItem::Entry> &store, const std::vector<CGun::Entry> &weapons,
    const std::vector<CArmor::Entry> &armor);

/** Native save fixtures and real greeting callbacks; no original saves change. */
int RunPostGameMenuCheck(const std::string &bigDirectory);

int RunPlayerSelectCheck(const std::string &bigDirectory);

int RunGreetingCheck(const std::string &bigDirectory);

int RunRefineryMenuCheck(const std::string &bigDirectory);
/** Advance the actual button Movie to action dispatch, checking no early transfer. */
bool FinishRefineryClick(ZMenuSurface &view, CMenuSystem &state, CProfileManager &profile,
    const CRefinementManager::Template &data, const std::filesystem::path &path, std::int64_t now, unsigned slot);
int CheckOnlineRefinery(CResTOCManager &toc, CGunBros &tables, ZMenuSurface &view,
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
int CheckBank(CResTOCManager &toc, CGunBros &tables, const CPlayerProgress::Template &progress,
    const CRefinementManager::Template &refinement, const std::vector<CStoreItem::Entry> &store,
    const std::vector<CGun::Entry> &weapons, const std::vector<CArmor::Entry> &armor);

/** Focused regression for the user's splash, package and clipped badge report. */
int CheckUiFeedback(CResTOCManager &toc, CGunBros &tables, const CPlayerProgress::Template &progress,
    const CRefinementManager::Template &refinement, const std::vector<CStoreItem::Entry> &store,
    const std::vector<CGun::Entry> &weapons, const std::vector<CArmor::Entry> &armor);

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

int CheckStoreFiltering(CResTOCManager &toc, const CRefinementManager::Template &refinement,
    const std::vector<CStoreItem::Entry> &store, const std::vector<CGun::Entry> &weapons,
    const std::vector<CArmor::Entry> &armor);

/** Complete the frame when a check drives a page without the host loop. */
inline bool FinishMenuFrame(bool drawn, CMenuSystem &state) {
    if (drawn) { state.UpdateNavigation(); }
    return drawn;
}

/** Actual folded/expanded card pixels for equipped and available actions. */
int RunStoreEquippedCheck(const std::string &bigDirectory);
