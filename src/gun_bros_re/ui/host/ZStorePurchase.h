#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
/** Desktop purchase completion and native modal presentation bindings. */

/** GetLastFailPurchaseInfo :156610; ARM 0xD25A8/0xD25F8 confirms the total
 * price and missing balance arguments omitted by the decompiler. */
bool StoreFailureText(ZMenuSurface &view, const CMenuSystem &state, std::string &body);

void ShowStoreFundsPrompt(CMenuSystem &state, const std::vector<ZStoreEntry> &store,
    const CProfileManager &profile, unsigned currency, unsigned price, bool inGame = true);

bool CompleteOfflineIAP(std::uint64_t clock, CMenuSystem &state, CProfileManager &profile,
    const std::vector<ZStoreEntry> &store, const std::filesystem::path &savePath);

/** IAP is a standard modal prompt, layout mode 1 (visual left), no buttons.
 * CMenuSystem::ShowPopup :96455 selects fonts 0/0/1/5 and GLU_MOVIE_POPUP.
 * BindContent :207403 derives its target size from fonts and sprite bounds. */
bool DrawStorePrompt(ZMenuSurface &view, CMenuSystem &state);
}
