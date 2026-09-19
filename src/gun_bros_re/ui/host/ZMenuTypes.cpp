#include "gun_bros_re/ui/host/ZMenuTypes.h"

namespace MenuDetail {

std::int64_t CurrentSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

bool SameObject(const GameObjectRef &first, const GameObjectRef &second) {
    return first.packHash == second.packHash && first.localIndex == second.localIndex;
}

/** Which navigation branch a host page sits in, named by that branch's own page.
 *
 * CMenuSystem::SetBranch :96614 leaves through its first test when the branch
 * asked for is the one already shown, and PushMenu/SetMenu :96666/:96700 route
 * every in-branch menu through that same early exit -- only the other path
 * restarts the WIPE movie with CMovie::SetTime(..., 0). So the sweep belongs to
 * navigation between branches. Menus inside one branch never play it: the store
 * category buttons carry action 64, which DoAction :93478 hands to the store
 * menu's own handler :95106 without going near SetBranch, and a planet click
 * pushes the REV list into the branch it is already in.
 *
 * The groups below are the ones the header already lights up as one option.
 */
unsigned MenuBranchPage(unsigned page) {
    if (page == 1 || page == 17 || page == 18) { return 2; }
    if (page == 16 || page == 19 || page == 21 || page == 22 || page == 23) { return 0; }
    if (page == 8 || page == 9 || page == 11) { return 6; }
    if (page == 13) { return 5; }
    if (page == 29) { return 4; }
    // CMenuPostGame changes its current view inside the same menu (:165242).
    if (page == 28) { return 27; }
    return page;
}

} // namespace MenuDetail
