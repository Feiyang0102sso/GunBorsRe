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
    const int interval = std::max<int>(1, m_gun->GetTemplate()->GetFireIntervalMs());
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
        if (m_vitals != nullptr && !m_vitals->dead && argumentCount > 0) {
            m_vitals->health = m_vitals->maximum * std::clamp<int>(arguments[0], 0, 100) / 100.0f;
        }
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
    if (m_vitals == nullptr || m_vitals->dead || damage <= 0) {
        return HitResult::Ignored;
    }
    m_vitals->lastDamage = damage;
    m_vitals->incomingDamage += damage;
    m_vitals->flash = 1;
    ++m_vitals->hits;
    if (m_vitals->invincible) { return HitResult::Hit; }
    m_vitals->health = std::max(0.0f, m_vitals->health - damage);
    if (m_vitals->health <= 0) {
        SetInput(false, false);
        m_vitals->dead = true;
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

std::vector<GunCue> CBrother::TakeCues() {
    std::vector<GunCue> cues;
    cues.swap(m_cues);
    return cues;
}
