/** Actor eligibility from original CBrother::OnPowerupButton :137932-138060.
 * UsePowerup is this port's shared entry for equipped and selector requests.
 */
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "gun_bros_re/gameplay/multiplayer/CMPMatch.h"
#include "gun_bros_re/ui/hud/CPowerUpSelector.h"
#include "engine/core/CStringToKey.h"
#include <cmath>

bool CBrother::UsePowerup(CPowerUpSelector &selector, bool fromSelector) {
    if (selector.m_match != nullptr && selector.m_match->GetResult() != CMPMatch::Result::Playing) { return false; }
    if (selector.m_powerup->IsActive()) { return false; }
    const CPowerup::Entry *entry = selector.GetSelected();
    if (entry == nullptr || !selector.IsSupported(*entry) || selector.GetCount() == 0 || !selector.m_player->weapon) { return false; }
    if (!HasSpawned()) { return false; }
    if (selector.m_match != nullptr && selector.m_cooldowns[entry->resource.localIndex] > 0) { return false; }
    if (selector.m_vitals->dead != (entry->data.field112 != 0)) { return false; }
    // CBrother::OnPowerupButton :138000 guards this exact item before querying its
    // script. Native 29 exists, but the current turret's CanUse export is true.
    const bool turret = entry->resource.packHash == CStringToKey("pack5") && entry->resource.localIndex == 19;
    if (turret && IsTurretActive()) { return false; }
    ZPowerupStatus status;
    status.healthPercent = static_cast<int>(std::lround(selector.m_vitals->health * 100 / selector.m_vitals->maximum));
    status.shield = IsShield();
    status.frenzy = IsFrenzy();
    status.autoFire = IsAutoFire();
    status.turret = IsTurretActive();
    for (unsigned type = 0; type < 3; ++type) { status.frenzyTypes[type] = IsFrenzyType(type); }
    CPowerup query;
    query.SetLevelContext(selector.m_level);
    query.Bind(entry->data, status);
    if (!query.Query(1)) { return false; }
    if (fromSelector && !query.Query(2)) { return false; }
    if (!fromSelector && !query.Query(0)) { return false; }
    const bool decrement = query.Query(3);
    return selector.m_level->UsePowerup(selector, *entry, fromSelector, decrement);
}
