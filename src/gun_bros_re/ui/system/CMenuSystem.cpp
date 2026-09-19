#include "gun_bros_re/ui/system/CMenuSystem.h"
namespace MenuDetail {
void CMenuSystem::Navigate(unsigned target, bool root) {
    stack.Push(target, root);
    store.CancelButtons();
    if (stack.page == 24 && greeting.greetingBound) {
        greeting.greetingTarget = target;
        greeting.greetingExitRequested = true;
    }
}
void CMenuSystem::Back() {
    if (stack.page == 24 && greeting.greetingBound) { Navigate(0, true); return; }
    store.CancelButtons();
    stack.Pop();
}
bool CMenuSystem::UpdateNavigation() {
    if (!stack.HasPending()) { return false; }
    // Greeting OnExit awards once and reverses its authored chapter to zero.
    // Other page exit choreography is not fabricated as a constant delay.
    const bool busy = stack.page == 24 && greeting.greetingBound && greeting.greetingExitRequested;
    const auto request = stack.Pending();
    const unsigned previous = stack.page;
    if (!stack.Commit(busy)) { return false; }
    const unsigned target = stack.page;
    if (target == previous) { return true; }
    // A hidden control must not resume an old fling on another page.
    store.shopMotion = ZMenuScrollMotion{};
    missions.missionMotion = ZMenuScrollMotion{};
    missions.waveMotion = ZMenuScrollMotion{};
    mode.modeLastTick = 0;
    if (target == 17) {
        store.shopCategory = 3;
        store.shopScroll = 0;
        store.shopFilter = 1u << (currencyTab + 14);
        store.filterAll = false;
        store.focused.shopDetailOpen = false;
    }
    if (target == 26) { masteryPopup = CMenuUpgradePopup(); }
    if (target == 6) { settings.optionsBound = false; }
    if (target == 8) {
        settings.optionsReturnFocus = settings.optionsFocus;
        settings.optionsReturnScroll = settings.optionsScroll;
        settings.optionsBound = false;
        settings.optionsFocus = 0;
        settings.optionsScroll = settings.optionsTarget = settings.optionsBodyScroll = 0;
    }
    if (target == 3) { refinery.refineryBound = false; }
    if (target == 24) { greeting.greetingBound = false; }
    if (target == 25 || target == 29) { selection.playerSelectBound = false; }
    if (target == 27 && previous != 26 && previous != 28) { postGame.postGameBound = false; }
    if (target == 0) { starMap.starBound = false; }
    if (target == 21) { missions.missionBound = false; }
    social.socialBound = false;
    if (request.operation == CMenuStack::Operation::Pop && previous == 8 && target == 6) {
        settings.optionsFocus = settings.optionsReturnFocus;
        settings.optionsScroll = settings.optionsReturnScroll;
    }
    itemPage = 0;
    selectedItem = -1;
    feedback.Clear();
    return true;
}
unsigned CMenuSystem::ContentPage() const {
    if (stack.page != 26) { return stack.page; }
    if (!stack.history.empty()) { return stack.history.back(); }
    return 27;
}
}
