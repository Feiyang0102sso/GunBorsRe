/** @file CPlayerConfiguration.h
 * @brief The two weapon slots and four original armour slots.
 */
#ifndef GUN_BROS_RE_CPLAYERCONFIGURATION_H
#define GUN_BROS_RE_CPLAYERCONFIGURATION_H
#include "gun_bros/CGameAssetRef.h"
#include <array>

struct CPlayerConfiguration {
    std::array<GameObjectRef, 2> guns;
    std::array<GameObjectRef, 4> armor;

    void SetDefaults(std::uint32_t corePackHash) {
        for (GameObjectRef &gun : guns) {
            gun.packHash = corePackHash;
            gun.localIndex = 0;
        }
        // CPlayerConfiguration::SetDefaultArmor (:171753): legs 2, body 1, head 0.
        for (unsigned slot = 0; slot < 3; ++slot) {
            armor[slot].packHash = corePackHash;
            armor[slot].localIndex = static_cast<std::uint8_t>(2 - slot);
        }
        armor[3] = GameObjectRef();
    }
};
#endif
