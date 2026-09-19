/**
 * @file CBrother.cpp
 * @brief The player template: a script, a model set, and a shadow sprite.
 */

#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "gun_bros_re/effects/CParticlePool.h"

#include <cstdio>
#include "gun_bros_re/gameplay/brother/CBrotherDrawing.h"
#include <algorithm>
#include <cmath>

namespace {
// ThrowGrenade :138729-138735 uses the same native magnitude, without a Flow factor.
constexpr float kGrenadeLaunchSpeed = 430.0f;
}

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
    m_drawing = std::make_unique<Drawing>();
    // CBrother::CBrother :139099 owns 25 slots for its native powerup players.
    m_powerupParticles = std::make_shared<PowerupParticles>();
    for (int i = 0; i < 11; ++i) { m_moveAliases[i] = -1; }
    for (int i = 0; i < 7; ++i) { m_variables[i] = 0; }
    m_variables[0] = 1;
    m_variables[1] = 1;
    m_variables[4] = 1;
}

CBrother::~CBrother() = default;

void CBrother::ClearScript() {
    m_interpreter = CScriptInterpreter();
    m_torso = CMoveSetMeshController();
    m_legs = CMoveSetMeshController();
    m_baseMoves = nullptr;
    m_gun = nullptr;
    m_bodyMeshes.clear();
    m_weaponMeshes.clear();
}

void CBrother::Bind(const CScript &script, const CMoveSetMesh &moves,
    const std::vector<const CMesh *> &bodyMeshes, CGun &gun,
    const std::vector<const CMesh *> &weaponMeshes) {
    // Full bind resets a life; SetUIGun deliberately preserves these timers.
    // The caller may pass GetScript() during respawn; copy before clearing.
    m_script = script;
    ClearScript();
    m_visible = true;
    m_spawned = true;
    m_immunityHidden = false;
    m_weaponSwapRequested = false;
    m_knockbackMs = 0;
    m_knockbackDurationMs = 0;
    m_triggerHeld = false;
    m_timer = 0;
    m_fireElapsed = 0;
    m_torsoUsesWeapon = false;
    m_moving = false;
    m_shooting = false;
    m_canFire = true;
    m_cues.clear();
    SetRandomSeed(1);
    for (int i = 0; i < 11; ++i) { m_moveAliases[i] = -1; }
    for (int i = 0; i < 7; ++i) { m_variables[i] = 0; }
    m_variables[0] = 1;
    m_variables[1] = 1;
    m_variables[2] = m_human;
    m_variables[4] = 1;
    for (unsigned i = 0; i < 2; ++i) {
        m_grenades[i] = {};
        m_grenadeStock[i] = 0;
        m_grenadesThrown[i] = 0;
        m_grenadePending[i] = false;
        m_grenadeAnimating[i] = false;
    }
    m_baseMoves = &moves;
    m_gun = &gun;
    m_bodyMeshes = bodyMeshes;
    m_weaponMeshes = weaponMeshes;
    m_torso.SetMoveSet(&moves, bodyMeshes);
    m_legs.SetMoveSet(&moves, bodyMeshes);
    m_interpreter.SetScript(m_script, *this);
    // Export 0 establishes the semantic move aliases; equipment replaces them.
    m_interpreter.CallExportFunction(0);
    gun.OnEquip();
    m_interpreter.CallExportFunction(1);
    m_fireElapsed = gun.GetTemplate()->GetFireIntervalMs();
    // A full reset may clear the shared pool; ordinary swaps retain it.
    m_powerupParticles = std::make_shared<PowerupParticles>();
    RestorePowerupEffects();
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
    if (m_torso.GetMoveSet() != moves) {
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
    if (deltaMs <= 0 || !m_spawned) { return; }
    if (powerups.legacyFrenzyMs > 0) {
        powerups.legacyFrenzyMs = std::max(0, powerups.legacyFrenzyMs - deltaMs);
        if (powerups.legacyFrenzyMs == 0) { StopFrenzy(); }
    }
    if (powerups.autoFireMs > 0) {
        powerups.autoFireMs = std::max(0, powerups.autoFireMs - deltaMs);
        if (powerups.autoFireMs == 0) { PowerupEffect({}, 104, false); }
    }
    if (powerups.shieldMs > 0) {
        powerups.shieldMs = std::max(0, powerups.shieldMs - deltaMs);
        if (powerups.shieldMs == 0) { PowerupEffect({}, 100, false); }
    }
    for (unsigned type = 0; type < 3; ++type) {
        if (powerups.frenzyMs[type] <= 0) { continue; }
        powerups.frenzyMs[type] = std::max(0, powerups.frenzyMs[type] - deltaMs);
        if (powerups.frenzyMs[type] == 0) {
            powerups.frenzyMultiplier[type] = 1;
            PowerupEffect({}, 101 + type, false);
        }
    }
    // Update :135184-135235 bypasses UpdateNormal during force/stun. Its
    // immunity and red-flash clocks resume after the forced action finishes.
    // Death still runs UpdateNormal (:135184-135189), so its hit red fades
    // at mem+1988 (:138246-138258) while the original death Flow advances.
    bool normalUpdate = m_knockbackMs == 0;
    if (m_vitals != nullptr && m_vitals->stunMs > 0) { normalUpdate = false; }
    if (normalUpdate && m_variables[3] > 0) {
        m_variables[3] = static_cast<std::int16_t>(std::max(0, m_variables[3] - deltaMs));
        if (m_variables[3] > 0) { m_immunityHidden = !m_immunityHidden; }
    }
    if (m_vitals != nullptr) {
        if (normalUpdate) { m_vitals->flash = std::max(0.0f, m_vitals->flash - deltaMs * 0.002f); }
        if (m_vitals->stunMs > 0) {
            m_vitals->stunMs = std::max(0, m_vitals->stunMs - deltaMs);
            if (m_vitals->stunMs == 0) { m_interpreter.HandleEvent(5, 8); }
        }
    }
    if (m_triggerHeld) { SetShooting(true); }
    m_torso.Update(deltaMs);
    m_legs.Update(deltaMs);
    m_gun->Update(deltaMs);
    if (m_knockbackMs > 0) {
        m_knockbackMs = std::max(0, m_knockbackMs - deltaMs);
        // CBrother::Update :135233 releases the forced state with event 7.
        if (m_knockbackMs == 0) { m_interpreter.HandleEvent(5, 7); }
    }
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
            if (!m_vitals->dead) {
                m_vitals->deathAnimationComplete = false;
                m_vitals->inputHidden = false;
            }
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
        ZGunCue cue;
        // FunctionResolver :138969 binds GetParticleEffectAnchor :134152.
        cue.anchorToActor = true;
        cue.kind = ZGunCue::Kind::Effect;
        std::uint32_t ordinal = 0;
        if (m_interpreter.GetResource(arguments[0], cue.resource.packHash, ordinal)) {
            cue.resource.localIndex = static_cast<std::uint8_t>(ordinal);
            m_cues.push_back(cue);
        }
        break;
    }
    case 13: {
        ZGunCue cue;
        cue.kind = ZGunCue::Kind::Splash;
        cue.damage = arguments[0] * 10.0f;
        cue.radius = arguments[1];
        m_cues.push_back(cue);
        break;
    }
    case 14: {
        const unsigned slot = static_cast<unsigned>(arguments[0]);
        if (!CanThrowGrenade(slot)) { break; }
        ZGunCue cue;
        cue.kind = ZGunCue::Kind::Grenade;
        cue.speed = kGrenadeLaunchSpeed;
        cue.resource = m_grenades[slot];
        cue.hand = slot;
        m_cues.push_back(cue);
        break;
    }
    case 1:
        // The death export calls this when its animation has finished.
        // FunctionResolver :138856 -> CLevel::OnPlayerKilled, not HP == 0.
        if (m_vitals != nullptr && m_vitals->dead && !m_vitals->deathAnimationComplete) {
            m_vitals->deathAnimationComplete = true;
            std::printf("[death] animation complete human=%d\n", m_variables[2]);
        }
        break;
    case 12:
        // FunctionResolver :138980 -> CInputPad::Hide.
        if (m_vitals != nullptr) { m_vitals->inputHidden = true; }
        break;
    case 15:
        // Health reset, control mode and spawn visibility do not alter this
        // unarmoured preview's equipment or mesh animation.
        break;
    case 18:
        // FunctionResolver :139014 stores visibility at runtime +2086.
        m_visible = arguments[0] > 0;
        break;
    default:
        std::printf("[player] native %u outside weapon preview\n", function);
        break;
    }
    return 0;
}

bool CBrother::SpawnForUI() {
    // :135894 -> CallScriptExportFunction(this, 0, 9, ...): 9 is the export,
    // not its argument. SetState starts the original state's first sequence.
    return m_interpreter.CallExportFunction(9);
}

void CBrother::SetUIGun(CGun &gun, const std::vector<const CMesh *> &weaponMeshes) {
    // CBrother::FunctionResolver native 3 (:138868): the current sequence
    // keeps its old mesh/range. Only the next SetMove consumes new overrides.
    m_gun = &gun;
    m_weaponMeshes = weaponMeshes;
    gun.OnEquip();
}

void CBrother::UpdateUI(std::int32_t deltaMs) {
    // CBrother::UpdateUI :137516 synchronizes legs to the previous torso time.
    // Menu previews do not run UpdateNormal's combat timers and fire input.
    auto &torso = m_torso.GetAnimation();
    auto &legs = m_legs.GetAnimation();
    const int previousTorsoTime = torso.GetTimeMs();
    const int previousLegsTime = legs.GetTimeMs();
    const int torsoDuration = torso.GetRangeDurationMs();
    const int legsDuration = legs.GetRangeDurationMs();
    // UpdateUI calls the animation controller directly and truncates the
    // scaled milliseconds (:137550), unlike UpdateNormal's rounded helper.
    const CMoveSetMesh *torsoMoves = m_torso.GetMoveSet();
    const int torsoMove = m_torso.GetMoveIndex();
    if (torsoMoves != nullptr && torsoMove >= 0) {
        torso.Update(static_cast<int>(torsoMoves->GetMoves()[torsoMove].speed * deltaMs));
    }
    if (torsoDuration > 0 && legsDuration > 0 && m_legs.GetMoveIndex() >= 0) {
        const float ratio = static_cast<float>(legsDuration) / torsoDuration;
        const float speed = m_baseMoves->GetMoves()[m_legs.GetMoveIndex()].speed;
        const int phase = static_cast<int>(previousTorsoTime * ratio * speed);
        legs.SetTimeMs(phase % legsDuration + legs.GetRangeStartMs());
    }
    // UpdateUI :137565 queries both move sets after advancing their clocks.
    m_torso.CollectSounds(previousTorsoTime);
    m_legs.CollectSounds(previousLegsTime);
    m_gun->Update(deltaMs);
    m_interpreter.Refresh();
}

Collision::HitResult CBrother::ReceiveDamage(float damage) {
    if (!m_spawned || m_vitals == nullptr || m_vitals->dead || damage <= 0 || m_variables[3] > 0 || IsShield()) {
        return Collision::HitResult::Ignored;
    }
    // HandleDamage (:136693) divides by the defense frenzy multiplier.
    damage /= GetFrenzyMultiplier(1);
    m_vitals->lastDamage = damage;
    m_vitals->incomingDamage += damage;
    m_vitals->flash = 1;
    ++m_vitals->hits;
    if (m_vitals->invincible) { return Collision::HitResult::Hit; }
    // Arena still dispatches HandleDamage's event 5/4 (:136791), including
    // hurt animations. Keep finite HP for percentage-based script queries.
    if (!m_vitals->unlimitedHealth) {
        m_vitals->health = std::max(0.0f, m_vitals->health - damage);
    }
    if (m_vitals->health <= 0) {
        StartDeath();
        return Collision::HitResult::Killed;
    }
    m_interpreter.HandleEvent(5, 4);
    return Collision::HitResult::Hit;
}

bool CBrother::StartDeath() {
    if (!m_spawned || m_vitals == nullptr || m_vitals->dead) { return false; }
    SetInput(false, false);
    m_vitals->health = 0;
    m_vitals->dead = true;
    m_vitals->deathAnimationComplete = false;
    m_vitals->stunMs = 0;
    m_weaponSwapRequested = false;
    m_knockbackMs = 0;
    for (bool &pending : m_grenadePending) { pending = false; }
    for (bool &animating : m_grenadeAnimating) { animating = false; }
    ++m_vitals->deaths;
    // HandleDamage :136779. BIG selects moves, timing and the completion native.
    m_interpreter.CallExportFunction(2);
    return true;
}

bool CBrother::BeginKnockback(int durationMs) {
    // Original SetForce rejects a second force, death, immunity and shield.
    if (!m_spawned || m_vitals == nullptr || m_vitals->dead || m_knockbackMs > 0 ||
        m_variables[3] > 0 || IsShield() || durationMs <= 0) { return false; }
    SetInput(false, false);
    m_knockbackMs = durationMs;
    m_knockbackDurationMs = durationMs;
    m_interpreter.CallExportFunction(4);
    return true;
}

void CBrother::WaitForSpawn() {
    SetInput(false, false);
    m_spawned = false;
    m_visible = false;
    m_cues.clear();
}

bool CBrother::Respawn() {
    m_spawned = true;
    m_visible = true;
    m_immunityHidden = false;
    return m_interpreter.CallExportFunction(8);
}

float CBrother::GetKnockbackStepSeconds(int deltaMs) const {
    if (m_knockbackMs <= 0 || deltaMs <= 0) { return 0; }
    const int elapsedMs = m_knockbackDurationMs - m_knockbackMs + deltaMs;
    // CBrother::Update :135201-135236 samples elapsed time after adding delta,
    // applies (1 + cos(pi * elapsed / duration)) / 2, and skips the final step.
    if (elapsedMs >= m_knockbackDurationMs) { return 0; }
    constexpr float kPi = 3.14159265f;
    const float progress = static_cast<float>(elapsedMs) / m_knockbackDurationMs;
    const float speedScale = (1.0f + std::cos(kPi * progress)) * 0.5f;
    return speedScale * deltaMs * 0.001f;
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

bool CBrother::OnRevive(unsigned reason) {
    if (m_vitals == nullptr || !m_vitals->dead || !m_vitals->deathAnimationComplete) { return false; }
    m_interpreter.CallExportFunction(7, static_cast<std::int16_t>(reason));
    std::printf("[local-live] revive human=%d health=%.1f\n", m_variables[2], m_vitals->health);
    return !m_vitals->dead;
}

std::vector<ZGunCue> CBrother::TakeCues() {
    std::vector<ZGunCue> cues;
    cues.swap(m_cues);
    return cues;
}

void CBrother::SetGrenade(unsigned slot, const GameObjectRef &resource, unsigned count) {
    if (slot >= 2) { return; }
    m_grenades[slot] = resource;
    m_grenadeStock[slot] = count;
}

bool CBrother::CanThrowGrenade(unsigned slot) const {
    return m_spawned && slot < 2 && m_grenadeStock[slot] > 0 && !m_grenades[slot].IsNull() &&
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

void CBrother::RestorePowerupEffects() {
    if (!powerups.particles) { powerups.particles = m_powerupParticles; }
    m_powerupParticles = powerups.particles;
    if (IsShield()) { PowerupEffect(powerups.effects[0], 100, true); }
    if (IsAutoFire()) { PowerupEffect(powerups.effects[4], 104, true); }
    if (IsFrenzy()) { PowerupEffect(powerups.effects[5], 105, true); }
    for (unsigned type = 0; type < 3; ++type) {
        if (IsFrenzyType(type)) { PowerupEffect(powerups.effects[type + 1], 101 + type, true); }
    }
}

void CBrother::PowerupEffect(const GameObjectRef &effect, int slot, bool active) {
    ZGunCue cue;
    cue.brotherPowerup = true;
    // StopShield/StopFrenzy use Stop, not a detached trail (:137300-137363).
    cue.stopParticlesImmediately = true;
    cue.kind = ZGunCue::Kind::StopTrail;
    if (active) { cue.kind = ZGunCue::Kind::Trail; }
    cue.resource = effect;
    cue.hand = slot;
    m_cues.push_back(cue);
}

void CBrother::StartShield(const GameObjectRef &effect, int durationMs) {
    powerups.shieldMs = std::max(0, durationMs);
    powerups.effects[0] = effect;
    PowerupEffect(effect, 100, durationMs > 0);
}

void CBrother::StartAutoFire(const GameObjectRef &effect, int durationSeconds) {
    // Unlike Q8 frenzy durations, native 22 passes whole seconds (:137216).
    powerups.autoFireMs = std::max(0, durationSeconds) * 1000;
    powerups.effects[4] = effect;
    PowerupEffect(effect, 104, durationSeconds > 0);
}

void CBrother::StartFrenzy(const GameObjectRef &effect, int durationMs, float attack, float defense, float speed) {
    if (durationMs <= 0) { StopFrenzy(); return; }
    powerups.legacyFrenzyMs = durationMs;
    // StartFrenzy :137333 retains these old fields. This iOS build's combat
    // reads the later per-type fields instead; do not invent a 1.5x combat boost.
    powerups.legacyFrenzyMultiplier[0] = attack;
    powerups.legacyFrenzyMultiplier[1] = defense;
    powerups.legacyFrenzyMultiplier[2] = speed;
    powerups.effects[5] = effect;
    PowerupEffect(effect, 105, true);
}

void CBrother::StopFrenzy() {
    powerups.legacyFrenzyMs = 0;
    for (float &multiplier : powerups.legacyFrenzyMultiplier) { multiplier = 1; }
    PowerupEffect({}, 105, false);
    // Original StopFrenzy :137295 also stops all three newer boost channels.
    for (unsigned type = 0; type < 3; ++type) { StartFrenzyType({}, 0, 1, type); }
}

void CBrother::StartFrenzyType(const GameObjectRef &effect, int durationMs, float multiplier, unsigned type) {
    if (type >= 3) { return; }
    powerups.frenzyMs[type] = std::max(0, durationMs);
    powerups.effects[type + 1] = effect;
    powerups.frenzyMultiplier[type] = multiplier;
    if (durationMs <= 0) { powerups.frenzyMultiplier[type] = 1; }
    PowerupEffect(effect, 101 + type, durationMs > 0);
}

float CBrother::GetFrenzyMultiplier(unsigned type) const {
    if (!IsFrenzyType(type)) { return 1; }
    return powerups.frenzyMultiplier[type];
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

/** Project the same animated muzzle transform used to draw the weapon. */
bool CBrother::ProjectMuzzle( const float *matrix, int hand, int node,
                   float &x, float &y, float &z) {
    ZMeshBoneTransform muzzle{};
    if (!GetMuzzle(hand, node, muzzle)) { return false; }
    x = matrix[0] * muzzle.posX + matrix[1] * muzzle.posY + matrix[2] * muzzle.posZ + matrix[3];
    y = matrix[4] * muzzle.posX + matrix[5] * muzzle.posY + matrix[6] * muzzle.posZ + matrix[7];
    z = matrix[8] * muzzle.posX + matrix[9] * muzzle.posY + matrix[10] * muzzle.posZ + matrix[11];
    return true;
}
