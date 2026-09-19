/** Original social bindings plus the Windows Game Center boundary. */
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"

namespace MenuDetail {

void UpdateLocalConnection(CMenuSystem &state) {
    const bool wasConnected = state.online.IsConnected();
    state.online.SetConnected(GameHostSettings().isConnected);
    // Offline sessions use the original default brother, including cold starts
    // with a previously selected local friend (CFriendManager::SetActiveFriend).
    if (!state.online.IsConnected()) {
        if (state.botRoster != nullptr && state.botRoster->Selected() != 0 && !state.botRoster->Select(0)) {
            std::printf("[local-online] failed to save default brother selection\n");
        }
        state.social.selectedLocalFriend = 0;
        state.botFriend = nullptr;
        state.matchedBot = nullptr;
        state.rematchingBot = false;
    }
    if (wasConnected && !state.online.IsConnected()) {
        state.social.socialBound = false;
        if (state.gameMode == 1 || state.gameMode == 2) {
            state.gameMode = 0;
            state.mode = CMenuMovieMultiplayerOverlay{};
        }
        if (state.matchingPrompt) {
            state.matchingPrompt = false;
            state.ShowStorePrompt("MDS_PROMPT_MP_UNAVAILABLE", false, true, 0);
        }
    }
    if (state.matchingPrompt && !state.storePromptRequested && !state.storePopup.IsActive()) {
        state.online.CancelMatch();
        state.matchingPrompt = false;
    }
}

bool BeginLocalMatch(CMenuSystem &state) {
    UpdateLocalConnection(state);
    if (!state.online.BeginMatch(state.gameMode)) { return false; }
    state.ShowStorePrompt("MDS_PROMPT_MP_UNAVAILABLE", false, false);
    state.matchingPrompt = true;
    state.storePromptButtons = "MDS_BUTTON_MP_DATA_EXCHANGE";
    return true;
}

bool TakeLocalMatch(CMenuSystem &state, std::uint64_t clock) {
    UpdateLocalConnection(state);
    if (!state.matchingPrompt || !state.storePopup.IsReady() || !state.online.AdvanceMatch(clock)) { return false; }
    state.matchingPrompt = false;
    state.storePromptRequested = false;
    state.storePopup = CMenuPopupPrompt{};
    state.storePromptButtons = nullptr;
    return true;
}

} // namespace MenuDetail
