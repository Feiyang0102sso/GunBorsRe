#include "gun_bros_viewer/scenes/ArenaTools.h"
#include "engine/core/CStringToKey.h"
#include <cstdio>

namespace ArenaDetail {
bool LoadArenaGrenades(CResTOCManager &toc, ZPackTables &tables,
    std::array<ZPowerupEntry, 3> &grenades) {
    std::vector<ZPowerupEntry> catalog;
    if (!LoadPowerupCatalog(toc, tables, catalog)) { return false; }
    // Shortcut identities only: pack5 POWERUP 13/15/14 are Grenade/Freeze G./
    // Shock G. (powerup-check.txt). Bullet refs and effects come from exports
    // 5/6, following powerup_template.bt and CPowerup::Equip/Use :188694.
    constexpr unsigned ordinals[] = {13, 15, 14};
    for (unsigned slot = 0; slot < grenades.size(); ++slot) {
        bool found = false;
        for (const auto &entry : catalog) {
            if (entry.resource.packHash == CStringToKey("pack5") && entry.resource.localIndex == ordinals[slot]) {
                grenades[slot] = entry;
                found = true;
                break;
            }
        }
        if (!found) {
            std::printf("[arena] missing grenade pack5 POWERUP %u\n", ordinals[slot]);
            return false;
        }
    }
    return true;
}

bool ThrowArenaGrenade(CBrother &brother, const ZPowerupEntry &entry) {
    // Do not replace the reference used by an in-flight throw animation.
    if (!brother.CanMove() || brother.HasGrenadeRequest(0)) { return false; }
    CPowerup powerup;
    powerup.Bind(entry.data);
    if (!powerup.Query(0) || !powerup.Query(1)) { return false; }
    powerup.Equip();
    powerup.Use();
    bool requested = false;
    for (const auto &action : powerup.TakeActions()) {
        if (action.function == 24) {
            // A fresh laboratory supply per request; no profile is modified.
            brother.SetGrenade(0, action.resource, 1);
        } else if (action.function == 25) {
            requested = brother.OnThrowGrenade(0);
        } else {
            std::printf("[arena] unsupported grenade action=%u resource=%s\n", action.function, entry.owner.c_str());
            return false;
        }
    }
    return requested;
}
}
