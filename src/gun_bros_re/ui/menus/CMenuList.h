#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/data/profile/CProfileManager.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
class CMenuList {
public:
    bool Draw(ZMenuSurface &view, CMenuSystem &state, CProfileManager &profile, bool &saveChanged);

    float optionsScroll = 0;
    unsigned optionsFocus = 0;
    unsigned optionsReturnFocus = 0;
    float optionsReturnScroll = 0;
    bool optionsBound = false;
    unsigned optionsOpening = 0;
    // Shared Movie 9 pauses while hidden; only a whole menu-system reset clears it.
    unsigned backgroundTime = 0;
    unsigned optionsBodyTime = 0;
    unsigned optionsScrollbarTime = 0;
    float optionsTarget = 0;
    float optionsBodyScroll = 0;
    std::uint64_t optionsLastTick = 0;
    std::vector<unsigned> optionsButtonTimes;

};
}

namespace MenuDetail {

/** CMenuDataProvider::CreateContentString :151507 resolves the action only
 * when the corresponding original MDS string slot is null. */
std::string OptionsText(ZMenuSurface &view, const CProfileManager &profile, unsigned index, unsigned slot, const char *table = "MDS_OPTIONS");

/** MENU_OPTIONS at original VA 0x402e50 selects LIST_MENU, list offset 2,
 * bounds 1/1, LIST_MENU_BUTTON, LIST_MENU_TEXT; all geometry stays in BIG.
 * CMenuList :140175..140657, CMenuListOption :144029..144317, ui_movie.bt. */

}
