/** Original native friend tiers, CGunBros::Init :80477 and CFriendPowerManager.
 * These are executable constants, not a replacement for BIG resource data.
 */
#pragma once
class CFriendPowerManager {
public:
    struct Power { unsigned friends, type, percent; };
    inline static constexpr Power Powers[] = {
        {1,7,10}, {2,2,5}, {3,5,10}, {4,1,10}, {5,3,10},
        {6,7,10}, {7,2,5}, {8,0,10}, {9,6,10}, {10,1,5}
    };
    static unsigned Bonus(unsigned friends, unsigned type) {
        unsigned bonus = 0;
        for (const auto &power : Powers) {
            if (friends >= power.friends && power.type == type) { bonus += power.percent; }
        }
        return bonus;
    }
    static float Multiplier(unsigned friends, unsigned type) { return 1 + Bonus(friends, type) * 0.01f; }
};
