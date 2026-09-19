#pragma once
#include <vector>
#include <map>
#include "gun_bros_re/ui/controls/CMenuMovieButton.h"

namespace MenuDetail {
/** CMenuStoreOptionGroup retains ordered item identities and their slot binding.
 * InitOption :233898 consumes CStoreAggregator's filtered STORE entries. */
class CMenuStoreOptionGroup {
public:
    std::vector<unsigned> items;
    std::vector<unsigned> itemSlots;
    std::map<unsigned, CMenuMovieButton> buttons;
};
}
