/** @file CPowerupActions.cpp
 * Actor natives from CPowerup::FunctionResolver :188349-188550.
 * Script-only research emits actions; a bound runtime mutates its actor directly.
 */
#include "gun_bros_re/gameplay/powerup/CPowerup.h"
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "engine/core/CStringToKey.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

void CPowerup::BindActor(CBrother &player, ZPlayerVitals &vitals) {
    m_player = &player;
    m_vitals = &vitals;
}

std::int16_t CPowerup::ResolveNativeFunction(std::uint16_t id,
    const std::int16_t *arguments, std::uint8_t count) {
    // Cancellation only releases a pause acquired by this Flow execution.
    // A timed, non-visual consumable must not clear an unrelated level pause.
    if (id == 0x0540 && GetLevelContext() != nullptr && !GetLevelContext()->IsPaused()) { m_pausedLevel = true; }
    if (id == 0x0541) { m_pausedLevel = false; }
    return ZGameScriptObject::ResolveNativeFunction(id, arguments, count);
}

ZPowerupStatus CPowerup::ReadActorStatus() const {
    if (m_player == nullptr) { return m_status; }
    ZPowerupStatus status;
    status.healthPercent = static_cast<int>(std::lround(m_vitals->health * 100 / m_vitals->maximum));
    if (!m_player->weapon) { return status; }
    const CBrother &brother = (*m_player);
    status.shield = brother.IsShield();
    status.frenzy = brother.IsFrenzy();
    status.autoFire = brother.IsAutoFire();
    status.turret = brother.IsTurretActive();
    for (unsigned type = 0; type < 3; ++type) { status.frenzyTypes[type] = brother.IsFrenzyType(type); }
    return status;
}

bool CPowerup::ApplyActorAction(const ZPowerupAction &action) {
    if (!m_player->weapon) { return false; }
    CBrother &brother = (*m_player);
    if (action.function == 10) {
        if (!m_vitals->dead) { m_vitals->health = std::min(m_vitals->maximum, m_vitals->health + action.arguments[0]); }
    } else if (action.function == 11) {
        if (!m_vitals->dead) { m_vitals->health = m_vitals->maximum; }
    } else if (action.function == 16) {
        brother.StartShield(action.resource, action.arguments[1] * 1000 / 256);
    } else if (action.function == 22) {
        brother.StartAutoFire(action.resource, action.arguments[1]);
    } else if (action.function == 17) {
        brother.StartFrenzy(action.resource, action.arguments[1] * 1000 / 256,
            action.arguments[2] / 256.0f, action.arguments[3] / 256.0f, action.arguments[4] / 256.0f);
    } else if (action.function == 27) {
        brother.StartFrenzyType(action.resource, action.arguments[1] * 1000 / 256,
            action.arguments[2] / 256.0f, action.arguments[3]);
    } else if (action.function == 24) {
        // Keep the equipped reference stable while an animation is pending.
        if (!m_pendingGrenade.IsNull()) { return false; }
        m_pendingGrenade = m_resource;
        brother.SetGrenade(0, action.resource, m_stock);
    } else if (action.function == 25) {
        if (m_actorActionFailed || !brother.OnThrowGrenade(0)) {
            if (!brother.HasGrenadeRequest(0)) { m_pendingGrenade = {}; }
            return false;
        }
        // CBrother::OnPowerupButton :138000 identifies this authored turret item.
        if (m_resource.packHash == CStringToKey("pack5") && m_resource.localIndex == 19) {
            brother.SetTurretIsActive(true);
        }
    }
    return true;
}

unsigned CPowerup::TakeThrownPowerups(GameObjectRef &resource) {
    resource = {};
    if (m_player == nullptr || !m_player->weapon) { return 0; }
    CBrother &brother = (*m_player);
    const unsigned thrown = brother.TakeThrownGrenades(0);
    if (thrown > 0) {
        resource = m_pendingGrenade;
        m_pendingGrenade = {};
        return thrown;
    }
    // A cancelled throw must not reserve inventory forever. Death and weapon
    // replacement destroy pending character animation; inventory stays intact.
    if (m_vitals->dead || !brother.HasGrenadeRequest(0)) {
        if (!m_pendingGrenade.IsNull() && m_pendingGrenade.packHash == CStringToKey("pack5") && m_pendingGrenade.localIndex == 19) {
            brother.SetTurretIsActive(false);
        }
        m_pendingGrenade = {};
    }
    return 0;
}

void CPowerup::Reset() {
    ResetExecution();
    m_pendingGrenade = {};
}
