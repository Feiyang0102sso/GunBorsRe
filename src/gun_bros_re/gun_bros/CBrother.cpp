/**
 * @file CBrother.cpp
 * @brief The player template: a script, a model set, and a shadow sprite.
 */

#include "gun_bros/CBrother.h"

#include <cstdio>
#include <algorithm>

CBrother::Template::Template() : m_gameScale(0.0f) {}

bool CBrother::Template::Init(CArrayInputStream &stream) {
    m_script.Load(stream);

    if (!m_moveSet.Init(stream)) {
        return false;
    }

    m_objectRef.Init(stream);
    m_gameScale = static_cast<float>(stream.ReadUInt16());

    // Two more object references that the original reads into one stack slot
    // and never reads back -- the second overwrites the first. They are read
    // here for the same reason: to keep the stream in step.
    GameObjectRef discarded;
    discarded.Init(stream);
    discarded.Init(stream);

    m_shadowSprite.Init(stream);

    if (stream.Overran()) {
        std::printf("[brother] template truncated\n");
        return false;
    }

    return true;
}

CBrother::CBrother() : m_triggerHeld(false), m_baseMoves(nullptr), m_gun(nullptr), m_timer(0),
    m_fireElapsed(0), m_torsoUsesWeapon(false), m_moving(false),
    m_shooting(false), m_canFire(true) {
    for (int i = 0; i < 11; ++i) { m_moveAliases[i] = -1; }
    for (int i = 0; i < 7; ++i) { m_variables[i] = 0; }
    m_variables[0] = 1;
    m_variables[1] = 1;
    m_variables[4] = 1;
}

void CBrother::Bind(const CScript &script, const CMoveSetMesh &moves,
    const std::vector<const CMesh *> &bodyMeshes, CGun &gun,
    const std::vector<const CMesh *> &weaponMeshes) {
    m_baseMoves = &moves;
    m_gun = &gun;
    m_bodyMeshes = bodyMeshes;
    m_weaponMeshes = weaponMeshes;
    m_torso.SetMoveSet(&moves, bodyMeshes);
    m_legs.SetMoveSet(&moves, bodyMeshes);
    m_interpreter.SetScript(script, *this);
    // Export 0 establishes the semantic move aliases; equipment replaces them.
    m_interpreter.CallExportFunction(0);
    gun.OnEquip();
    m_interpreter.CallExportFunction(1);
    m_fireElapsed = gun.GetTemplate()->GetFireIntervalMs();
}

void CBrother::SetScriptSequenceFrame(std::uint8_t frame) {
    const CMoveSetMesh *moves = m_baseMoves;
    std::int32_t selected = frame;
    bool useWeapon = false;
    for (int i = 0; i < 11; ++i) {
        if (m_moveAliases[i] == frame && m_gun->GetOverrides()[i] >= 0) {
            selected = m_gun->GetOverrides()[i];
            moves = &m_gun->GetTemplate()->GetMoveSet();
            useWeapon = true;
            break;
        }
    }
    if (m_torsoUsesWeapon != useWeapon) {
        if (useWeapon) {
            m_torso.SetMoveSet(moves, m_weaponMeshes);
        } else {
            m_torso.SetMoveSet(moves, m_bodyMeshes);
        }
        m_torsoUsesWeapon = useWeapon;
    }
    // CBrother::SetMove always restarts the sequence's requested range.
    m_torso.SetMove(selected);
    if (selected >= 0 && static_cast<std::size_t>(selected) < moves->GetMoves().size()) {
        m_torso.GetAnimation().SetFrame(moves->GetMoves()[selected].firstFrame);
        m_torso.GetAnimation().SetLooped(false);
    }
}

bool CBrother::IsScriptSequenceFrameFinished() {
    return m_torso.GetAnimation().IsFinished();
}

void CBrother::OnScriptStateEntered() { m_timer = 0; }

void CBrother::SetInput(bool moving, bool shooting) {
    if (m_vitals != nullptr && m_vitals->dead) { return; }
    if (m_vitals != nullptr && m_vitals->stunMs > 0) { moving = false; shooting = false; }
    if (!CanMove()) { moving = false; }
    if (!CanShoot()) { shooting = false; }
    if (moving != m_moving) {
        m_moving = moving;
        if (moving) { m_interpreter.HandleEvent(5, 0); }
        else { m_interpreter.HandleEvent(5, 2); }
    }
    // Input intent survives a script-imposed stop while reloading/cooling.
    m_triggerHeld = shooting;
    SetShooting(shooting);
}

void CBrother::SetShooting(bool shooting) {
    // CBrother::OnShoot starts only when the gun's ready/ammo flag is set.
    if (shooting && !m_gun->CanFire()) { return; }
    if (shooting != m_shooting) {
        m_shooting = shooting;
        m_gun->SetShooting(shooting);
        if (shooting) {
            m_interpreter.HandleEvent(5, 1);
        } else {
            m_fireElapsed = 0;
            m_interpreter.HandleEvent(5, 3);
        }
    }
}

void CBrother::Update(std::int32_t deltaMs) {
    if (deltaMs <= 0) { return; }
    if (m_powerups != nullptr) {
        if (m_powerups->legacyFrenzyMs > 0) {
            m_powerups->legacyFrenzyMs = std::max(0, m_powerups->legacyFrenzyMs - deltaMs);
            if (m_powerups->legacyFrenzyMs == 0) { StopFrenzy(); }
        }
        if (m_powerups->autoFireMs > 0) {
            m_powerups->autoFireMs = std::max(0, m_powerups->autoFireMs - deltaMs);
            if (m_powerups->autoFireMs == 0) { PowerupEffect({}, 104, false); }
        }
        if (m_powerups->shieldMs > 0) {
            m_powerups->shieldMs = std::max(0, m_powerups->shieldMs - deltaMs);
            if (m_powerups->shieldMs == 0) { PowerupEffect({}, 100, false); }
        }
        for (unsigned type = 0; type < 3; ++type) {
            if (m_powerups->frenzyMs[type] <= 0) { continue; }
            m_powerups->frenzyMs[type] = std::max(0, m_powerups->frenzyMs[type] - deltaMs);
            if (m_powerups->frenzyMs[type] == 0) {
                m_powerups->frenzyMultiplier[type] = 1;
                PowerupEffect({}, 101 + type, false);
            }
        }
    }
    if (m_variables[3] > 0) {
        m_variables[3] = static_cast<std::int16_t>(std::max(0, m_variables[3] - deltaMs));
    }
    if (m_vitals != nullptr) {
        m_vitals->flash = std::max(0.0f, m_vitals->flash - deltaMs * 0.004f);
        if (m_vitals->stunMs > 0) {
            m_vitals->stunMs = std::max(0, m_vitals->stunMs - deltaMs);
            if (m_vitals->stunMs == 0) { m_interpreter.HandleEvent(5, 8); }
        }
    }
    if (m_triggerHeld) { SetShooting(true); }
    m_torso.Update(deltaMs);
    m_legs.Update(deltaMs);
    m_gun->Update(deltaMs);
    if (m_timer > 0) {
        m_timer -= deltaMs;
        if (m_timer <= 0) { m_interpreter.HandleEvent(5, 7); }
    }
    const int interval = std::max<int>(1, m_gun->GetFireRateMs());
    m_fireElapsed += deltaMs;
    if (!m_shooting) {
        m_fireElapsed = std::min(m_fireElapsed, interval);
    } else if (m_canFire && m_variables[0] != 0) {
        if (m_gun->GetFireMode() == 1) {
            m_canFire = false;
            m_gun->Fire();
        } else if (m_gun->GetFireMode() == 0 && m_gun->CanFire() && m_fireElapsed >= interval) {
            m_fireElapsed -= interval;
            m_gun->Fire();
        }
    }
    // CBrother::UpdateNormal (:138357): ordinary projectiles stop the attack
    // state as soon as the gun clears its ready flag. Beams keep their state.
    if (m_shooting && !m_gun->IsBeam() && !m_gun->CanFire()) {
        SetShooting(false);
    }
    m_interpreter.Refresh();
    // CBrother::Update (:135283) retries pending throws until the current
    // animation accepts its event. Never replace the character state directly.
    for (unsigned slot = 0; slot < 2; ++slot) {
        if (!m_grenadePending[slot]) { continue; }
        if (m_interpreter.HandleEvent(5, static_cast<std::uint8_t>(10 + slot))) {
            m_grenadePending[slot] = false;
            m_grenadeAnimating[slot] = true;
        }
        break;
    }
}

std::int16_t *CBrother::VariableResolver(std::uint8_t variable) {
    if (variable < 7) { return &m_variables[variable]; }
    return nullptr;
}

std::int16_t CBrother::FunctionResolver(std::uint8_t function,
    const std::int16_t *arguments, std::uint8_t argumentCount) {
    switch (function) {
    case 0:
        m_legs.SetMove(arguments[0]);
        m_legs.GetAnimation().SetLooped(argumentCount == 1 || arguments[1] == 1);
        break;
    case 2:
        if (arguments[0] >= 0 && arguments[0] < 11) {
            m_moveAliases[arguments[0]] = arguments[1];
        }
        break;
    case 4:
        if (m_gun->GetFireMode() == 1 && m_shooting && m_canFire) {
            m_canFire = false;
            m_gun->Fire();
        }
        break;
    case 5:
        m_canFire = true;
        break;
    case 9:
        m_timer = static_cast<int>(arguments[0] * (1000.0f / 256.0f));
        break;
    case 10:
        if (m_vitals != nullptr && argumentCount > 0) {
            m_vitals->health = m_vitals->maximum * std::clamp<int>(arguments[0], 0, 100) / 100.0f;
            m_vitals->dead = m_vitals->health <= 0;
            if (m_vitals->dead) { m_variables[0] = 0; m_variables[1] = 0; }
        }
        break;
    case 3:
        // The swap animation releases its old weapon at this native callback.
        m_weaponSwapRequested = true;
        break;
    case 6:
        SetShooting(false);
        break;
    case 7:
        if (m_vitals != nullptr) { m_vitals->stunMs = std::max(0, static_cast<int>(arguments[0])); }
        break;
    case 8:
        if (m_vitals != nullptr) { m_vitals->stunMs = 0; }
        break;
    case 11: {
        GunCue cue;
        cue.kind = GunCue::Kind::Effect;
        std::uint32_t ordinal = 0;
        if (m_interpreter.GetResource(arguments[0], cue.resource.packHash, ordinal)) {
            cue.resource.localIndex = static_cast<std::uint8_t>(ordinal);
            m_cues.push_back(cue);
        }
        break;
    }
    case 13: {
        GunCue cue;
        cue.kind = GunCue::Kind::Splash;
        cue.damage = arguments[0] * 10.0f;
        cue.radius = arguments[1];
        m_cues.push_back(cue);
        break;
    }
    case 14: {
        const unsigned slot = static_cast<unsigned>(arguments[0]);
        if (!CanThrowGrenade(slot)) { break; }
        GunCue cue;
        cue.kind = GunCue::Kind::Grenade;
        cue.resource = m_grenades[slot];
        cue.hand = slot;
        m_cues.push_back(cue);
        break;
    }
    case 1:
        // The death export calls this when its animation has finished.
        break;
    case 15:
    case 18:
        // Health reset, control mode and spawn visibility do not alter this
        // unarmoured preview's equipment or mesh animation.
        break;
    default:
        std::printf("[player] native %u outside weapon preview\n", function);
        break;
    }
    return 0;
}

HitResult CBrother::ReceiveDamage(float damage) {
    if (m_vitals == nullptr || m_vitals->dead || damage <= 0 || m_variables[3] > 0 || IsShield()) {
        return HitResult::Ignored;
    }
    // HandleDamage (:136693) divides by the defense frenzy multiplier.
    damage /= GetFrenzyMultiplier(1);
    m_vitals->lastDamage = damage;
    m_vitals->incomingDamage += damage;
    m_vitals->flash = 1;
    ++m_vitals->hits;
    if (m_vitals->invincible) { return HitResult::Hit; }
    m_vitals->health = std::max(0.0f, m_vitals->health - damage);
    if (m_vitals->health <= 0) {
        SetInput(false, false);
        m_vitals->dead = true;
        for (bool &pending : m_grenadePending) { pending = false; }
        for (bool &animating : m_grenadeAnimating) { animating = false; }
        ++m_vitals->deaths;
        m_interpreter.CallExportFunction(2);
        return HitResult::Killed;
    }
    m_interpreter.HandleEvent(5, 4);
    return HitResult::Hit;
}

void CBrother::Stun(int durationMs) {
    if (m_vitals == nullptr || m_vitals->dead || durationMs <= 0) { return; }
    SetInput(false, false);
    m_vitals->stunMs = durationMs;
    m_interpreter.CallExportFunction(5, static_cast<std::int16_t>(durationMs));
}

void CBrother::OnWaveCleared() {
    m_interpreter.CallExportFunction(6);
}

std::vector<GunCue> CBrother::TakeCues() {
    std::vector<GunCue> cues;
    cues.swap(m_cues);
    return cues;
}

void CBrother::SetGrenade(unsigned slot, const GameObjectRef &resource, unsigned count) {
    if (slot >= 2) { return; }
    m_grenades[slot] = resource;
    m_grenadeStock[slot] = count;
}

bool CBrother::CanThrowGrenade(unsigned slot) const {
    return slot < 2 && m_grenadeStock[slot] > 0 && !m_grenades[slot].IsNull() &&
        (m_vitals == nullptr || !m_vitals->dead);
}

bool CBrother::OnThrowGrenade(unsigned slot) {
    if (!CanThrowGrenade(slot) || !CanMove() || HasGrenadeRequest(slot)) { return false; }
    m_grenadePending[slot] = true;
    return true;
}

void CBrother::OnGrenadeThrown(unsigned slot) {
    if (slot >= 2 || m_grenadeStock[slot] == 0) { return; }
    --m_grenadeStock[slot];
    m_grenadeAnimating[slot] = false;
    ++m_grenadesThrown[slot];
}

unsigned CBrother::TakeThrownGrenades(unsigned slot) {
    if (slot >= 2) { return 0; }
    const unsigned count = m_grenadesThrown[slot];
    m_grenadesThrown[slot] = 0;
    return count;
}

void CBrother::SetPowerupState(PowerupState *powerups) {
    m_powerups = powerups;
    if (m_powerups == nullptr) { return; }
    if (IsShield()) { PowerupEffect(m_powerups->effects[0], 100, true); }
    if (IsAutoFire()) { PowerupEffect(m_powerups->effects[4], 104, true); }
    if (IsFrenzy()) { PowerupEffect(m_powerups->effects[5], 105, true); }
    for (unsigned type = 0; type < 3; ++type) {
        if (IsFrenzyType(type)) { PowerupEffect(m_powerups->effects[type + 1], 101 + type, true); }
    }
}

void CBrother::PowerupEffect(const GameObjectRef &effect, int slot, bool active) {
    GunCue cue;
    cue.kind = GunCue::Kind::StopTrail;
    if (active) { cue.kind = GunCue::Kind::Trail; }
    cue.resource = effect;
    cue.hand = slot;
    m_cues.push_back(cue);
}

void CBrother::StartShield(const GameObjectRef &effect, int durationMs) {
    if (m_powerups == nullptr) { return; }
    m_powerups->shieldMs = std::max(0, durationMs);
    m_powerups->effects[0] = effect;
    PowerupEffect(effect, 100, durationMs > 0);
}

void CBrother::StartAutoFire(const GameObjectRef &effect, int durationSeconds) {
    if (m_powerups == nullptr) { return; }
    // Unlike Q8 frenzy durations, native 22 passes whole seconds (:137216).
    m_powerups->autoFireMs = std::max(0, durationSeconds) * 1000;
    m_powerups->effects[4] = effect;
    PowerupEffect(effect, 104, durationSeconds > 0);
}

void CBrother::StartFrenzy(const GameObjectRef &effect, int durationMs, float attack, float defense, float speed) {
    if (m_powerups == nullptr) { return; }
    if (durationMs <= 0) { StopFrenzy(); return; }
    m_powerups->legacyFrenzyMs = durationMs;
    // StartFrenzy :137333 retains these old fields. This iOS build's combat
    // reads the later per-type fields instead; do not invent a 1.5x combat boost.
    m_powerups->legacyFrenzyMultiplier[0] = attack;
    m_powerups->legacyFrenzyMultiplier[1] = defense;
    m_powerups->legacyFrenzyMultiplier[2] = speed;
    m_powerups->effects[5] = effect;
    PowerupEffect(effect, 105, true);
}

void CBrother::StopFrenzy() {
    if (m_powerups == nullptr) { return; }
    m_powerups->legacyFrenzyMs = 0;
    for (float &multiplier : m_powerups->legacyFrenzyMultiplier) { multiplier = 1; }
    PowerupEffect({}, 105, false);
    // Original StopFrenzy :137295 also stops all three newer boost channels.
    for (unsigned type = 0; type < 3; ++type) { StartFrenzyType({}, 0, 1, type); }
}

void CBrother::StartFrenzyType(const GameObjectRef &effect, int durationMs, float multiplier, unsigned type) {
    if (m_powerups == nullptr || type >= 3) { return; }
    m_powerups->frenzyMs[type] = std::max(0, durationMs);
    m_powerups->effects[type + 1] = effect;
    m_powerups->frenzyMultiplier[type] = multiplier;
    if (durationMs <= 0) { m_powerups->frenzyMultiplier[type] = 1; }
    PowerupEffect(effect, 101 + type, durationMs > 0);
}

float CBrother::GetFrenzyMultiplier(unsigned type) const {
    if (!IsFrenzyType(type)) { return 1; }
    return m_powerups->frenzyMultiplier[type];
}

float CBrother::GetProjectilePowerupMultiplier() const {
    // FireBullet (:136422): later active types replace the multiplier against
    // base damage; the original does not multiply all three bonuses together.
    float multiplier = 1;
    for (unsigned type = 0; type < 3; ++type) {
        if (IsFrenzyType(type)) { multiplier = GetFrenzyMultiplier(type); }
    }
    return multiplier;
}
