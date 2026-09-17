/** Original CFlock neighbour separation; route caching remains in ILayerPath. */
#pragma once
#include <vector>
#include "gun_bros_re/gameplay/CEnemy.h"

class CFlock {
public:
    // Refresh before any enemy moves. The host stores the cached vector on
    // each actor instead of looking up the original pointer/vector table.
    static void RefreshFlock(const std::vector<CEnemy::CombatState *> &enemies);
};
