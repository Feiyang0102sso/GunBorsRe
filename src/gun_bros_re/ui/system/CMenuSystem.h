#pragma once
#include "gun_bros_re/ui/menus/CMenuChallenges.h"
#include "gun_bros_re/ui/system/CMenuStack.h"
#include "gun_bros_re/cheats/CheatFeedback.h"
#include "gun_bros_re/ui/menus/CMenuMission.h"
#include "gun_bros_re/ui/menus/CMenuStore.h"
#include "gun_bros_re/ui/menus/CMenuMissionInfo.h"
#include "gun_bros_re/ui/menus/CMenuMovieMultiplayerOverlay.h"
#include "gun_bros_re/ui/menus/CMenuPostGame.h"
#include "gun_bros_re/ui/menus/CMenuGameResources.h"
#include "gun_bros_re/ui/menus/CMenuGreeting.h"
#include "gun_bros_re/ui/menus/CMenuPlayerSelect.h"
#include "gun_bros_re/ui/menus/CMenuList.h"
#include "gun_bros_re/ui/menus/CMenuFriends.h"
#include "gun_bros_re/ui/controls/CMenuMesh.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/controls/CMenuPopupPrompt.h"
#include "gun_bros_re/ui/controls/ZPromotionPopup.h"
#include "gun_bros_re/host/ZLocalOnlineServices.h"
#include "gun_bros_re/host/ZHostSettings.h"
#include "gun_bros_re/debug/DebugMaps.h"
#include "gun_bros_re/gameplay/multiplayer/bot/ZLocalBotFriend.h"
#include "gun_bros_re/gameplay/game/CGameFlow.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
class CMenuSystem {
public:
    ZLocalBotFriend *botFriend = nullptr;
    ZLocalBotRoster *botRoster = nullptr;
    ZLocalBotFriend *matchedBot = nullptr;
    bool rematchingBot = false;
    ZLocalOnlineServices online;
    bool matchingPrompt = false;
    bool currencySimulated = false;
    std::string currencyOfferProduct;
    bool resumeAfterDebugTutorial = false; // Returning from a no-save replay must not trigger a menu checkpoint.
    DebugMapSelection debugMap;

    CMenuStack stack;
    unsigned planet = 0;
    unsigned slot = 0;
    unsigned itemPage = 0;
    int selectedItem = -1;
    GameCheats::CheatFeedback feedback;
    unsigned detail = 0;
    unsigned hordeStart = 0;
    unsigned currencyTab = 0, currencyPage = 0;
    int currencyItem = -1;
    CMenuMesh playerMesh;
    std::uint64_t playerMeshLastTick = 0;
    // Last popup update tick; the resource chapters own its playback cursor.
    std::uint64_t masteryOpened = 0;
    CMenuUpgradePopup masteryPopup;
    unsigned gameMode = 0;
    GameObjectRef selectedMission;
    bool currencyPending = false;
    CMenuPopupPrompt storePopup;
    std::uint64_t storePopupLastTick = 0;
    unsigned storePromptSpriteTime = 0;
    std::string challengeRewardTitle, challengeRewardBody;
    const char *storePromptTable = "MDS_IAP_PLEASE_WAIT";
    unsigned storePromptIndex = 0;
    bool storePromptRequested = false;
    bool storePromptSideVisual = true;
    bool storePromptDismiss = false;
    const char *storePromptButtons = nullptr;
    unsigned failedCurrency = 0, failedPrice = 0, failedMissing = 0;
    int currencyOffer = -1;
    std::uint64_t currencyReadyAt = 0;
    CGameFlow::Result result;
    GameObjectRef masteryWeapon;
    bool refinementRequired = false;
    ZPromotionPopup promotion;
    std::uint64_t promotionTick = 0;

    CMenuMission starMap;
    CMenuStore store;
    CMenuMissionInfo missions;
    CMenuMovieMultiplayerOverlay mode;
    CMenuPostGame postGame;
    CMenuGameResources refinery;
    CMenuGreeting greeting;
    CMenuPlayerSelect selection;
    CMenuList settings;
    CMenuFriends social;
    CMenuChallenges challenges;

    void ShowStorePrompt(const char *table, bool sideVisual, bool dismissible, unsigned index = 0) {
        challengeRewardTitle.clear();
        challengeRewardBody.clear();
        storePromptTable = table;
        storePromptIndex = index;
        storePromptSideVisual = sideVisual;
        storePromptDismiss = dismissible;
        storePromptButtons = nullptr;
        storePromptRequested = true;
        storePopup = CMenuPopupPrompt();
    }

    void BeginOfflineIAP(int item, std::uint64_t clock, const std::string &product = {}) {
        if (currencyPending) { return; }
        online.SetConnected(GameHostSettings().isConnected);
        currencySimulated = online.IsConnected();
        if (currencySimulated && !online.BeginPurchase(product, clock)) {
            ShowStorePrompt("MDS_STORE_PROMPT_UNAVAILABLE", false, true);
            return;
        }
        currencyItem = item;
        currencyPending = true;
        currencyReadyAt = clock + 4000; // User-authorized offline wait, UI_sample/ui.md.
        ShowStorePrompt("MDS_IAP_PLEASE_WAIT", true, false);
    }

    // Every nested page remembers its caller; trunk navigation starts a new path.
    void Navigate(unsigned target, bool root = false);
    void Back();
    /** Commit only between frames, after the active page has finished its exit. */
    bool UpdateNavigation();
    unsigned ContentPage() const;

};
}
