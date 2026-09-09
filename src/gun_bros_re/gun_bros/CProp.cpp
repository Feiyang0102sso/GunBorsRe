/**
 * @file CProp.cpp
 * @brief The template behind a placed prop: which sprite it draws.
 */

#include "gun_bros/CProp.h"

#include "gun_bros/CGameAssetRef.h"

#include <cstdio>
#include <cmath>

CProp::Template::Template()
    : m_foregroundAnimation(255), m_backgroundAnimation(255), m_persistent(0) {}

bool CProp::Template::Init(CArrayInputStream &stream) {
    m_sprite.Init(stream);

    // Foreground first on the wire, background second.
    m_foregroundAnimation = stream.ReadUInt8();
    m_backgroundAnimation = stream.ReadUInt8();

    if (!m_collision.Load(stream)) {
        return false;
    }
    if (!m_bulletCollision.Load(stream)) {
        return false;
    }

    m_persistent = stream.ReadUInt8();
    m_script.Load(stream);
    // The historical preview stopped here. Runtime now consumes CMoveSet too.
    if (!m_moveSet.Init(stream)) { return false; }

    if (stream.Overran()) {
        std::printf("[prop] template truncated\n");
        return false;
    }

    return true;
}

void CProp::Bind(const Template &data, const std::vector<std::vector<std::uint16_t>> *durations) {
    m_template = &data;
    m_durations = durations;
    m_health = 0;
    m_damage = 0;
    m_damageFlags = 0;
    m_damageOwner = 0;
    m_timerMs = 0;
    m_move = -1;
    m_moveSlot = 1;
    m_lastMoveStep = -1;
    m_checkEntry = false;
    m_inside = false;
    m_unsupported = 0;
    m_actions.clear();
    m_collision = data.GetCollision();
    m_bulletCollision = data.GetBulletCollision();
    m_collisionChanged = true;
    // Native slots are background/main/foreground. DrawBackground :123549
    // consumes +208 (slot 0); DrawForeground :123555 consumes +156 (slot 2).
    SetAnimation(0, data.GetBackgroundAnimation());
    SetAnimation(1, data.GetMainAnimation());
    SetAnimation(2, data.GetForegroundAnimation());
    if (data.GetScript().IsPresent()) {
        m_interpreter.SetScript(data.GetScript(), *this);
        m_interpreter.CallExportFunction(0);
    }
}

void CProp::SetAnimation(int slot, int animation) {
    if (slot < 0 || slot > 2) { ++m_unsupported; return; }
    m_animations[slot] = animation;
    m_players[slot].SetLooping(true);
    m_players[slot].SetAnimation(nullptr);
    if (m_durations != nullptr && animation >= 0 && animation < static_cast<int>(m_durations->size())) {
        m_players[slot].SetAnimation(&(*m_durations)[animation]);
    }
}

void CProp::Update(int deltaMs, bool playerInside) {
    if (IsRemoved() || m_template == nullptr) { return; }
    if (m_timerMs > 0) {
        m_timerMs -= deltaMs;
        if (m_timerMs <= 0) { m_timerMs = 0; m_interpreter.HandleEvent(10, 1); }
    }
    if (m_checkEntry && playerInside != m_inside) {
        m_inside = playerInside;
        if (playerInside) {
            m_interpreter.HandleEvent(10, 4);
            PropAction action;
            action.kind = PropAction::Kind::Entered;
            m_actions.push_back(action);
        } else { m_interpreter.HandleEvent(10, 5); }
    }
    for (CSpritePlayer &player : m_players) { player.Update(static_cast<std::uint16_t>(deltaMs)); }
    if (m_move >= 0) {
        const auto &move = m_template->GetMoveSet().moves[m_move];
        const int step = m_players[m_moveSlot].GetStep();
        if (step != m_lastMoveStep && step < static_cast<int>(move.frames.size())) {
            m_lastMoveStep = step;
            const unsigned sound = move.frames[step].sound;
            if (sound != 255) {
                PropAction action;
                action.kind = PropAction::Kind::Sound;
                action.resource.packHash = m_template->GetMoveSet().packHash;
                action.resource.localIndex = static_cast<std::uint8_t>(sound);
                m_actions.push_back(action);
            }
        }
        m_interpreter.Refresh();
    }
}

void CProp::HandleMessage(int message) {
    // CProp::HandleMessage :123941: enable, disable, activate -> events 2/3/6.
    if (message == 0) { m_interpreter.HandleEvent(10, 2); }
    else if (message == 1) { m_interpreter.HandleEvent(10, 3); }
    else if (message == 2) { m_interpreter.HandleEvent(10, 6); }
}

void CProp::Damage(float amount, std::uint32_t flags) {
    if (IsRemoved() || m_template == nullptr || !m_template->GetScript().IsPresent()) { return; }
    m_damage = amount;
    m_damageFlags = flags;
    m_interpreter.HandleEvent(10, 0);
}

void CProp::SetScriptSequenceFrame(std::uint8_t index) {
    if (m_template == nullptr || index >= m_template->GetMoveSet().moves.size()) {
        ++m_unsupported;
        return;
    }
    m_move = index;
    m_lastMoveStep = -1;
    const auto &move = m_template->GetMoveSet().moves[index];
    SetAnimation(m_moveSlot, move.animation);
    m_players[m_moveSlot].SetLooping(move.looping);
}

bool CProp::IsScriptSequenceFrameFinished() {
    return m_move >= 0 && m_players[m_moveSlot].HasFinished();
}

void CProp::QueueResource(PropAction::Kind kind, int resource, int group) {
    PropAction action;
    action.kind = kind;
    action.group = group;
    std::uint32_t index = 0;
    if (!m_interpreter.GetResource(resource, action.resource.packHash, index)) { ++m_unsupported; return; }
    action.resource.localIndex = static_cast<std::uint8_t>(index);
    m_actions.push_back(action);
}

std::int16_t CProp::FunctionResolver(std::uint8_t function, const std::int16_t *arguments, std::uint8_t count) {
    int first = 0;
    int second = 0;
    if (count > 0) { first = arguments[0]; }
    if (count > 1) { second = arguments[1]; }
    switch (function) {
    case 0: SetAnimation(first, second); break;
    case 1: m_timerMs = first * 1000; break;
    case 2:
        if (count < 2) { second = 3; }
        QueueResource(PropAction::Kind::Effect, first, second);
        break;
    case 3:
        if (m_health > 0) {
            m_health -= m_damage;
            if (m_health <= 0) {
                m_health = 0;
                m_interpreter.HandleEvent(10, 7);
                m_collisionChanged = true;
            }
        }
        break;
    case 4: QueueResource(PropAction::Kind::Sound, first); break;
    case 5:
    case 6:
        if (first == 0) { m_collision.SetGroupEnabled(second, function == 6); }
        else if (first == 1) { m_bulletCollision.SetGroupEnabled(second, function == 6); }
        else { ++m_unsupported; }
        m_collisionChanged = true;
        break;
    case 7:
    case 10: {
        PropAction action;
        action.kind = PropAction::Kind::Splash;
        action.radius = first;
        action.damage = second;
        action.playersOnly = function == 10;
        action.damageOwner = m_damageOwner;
        if (function == 10 && count >= 4) { action.force = arguments[2]; action.forceMs = arguments[3]; }
        m_actions.push_back(action);
        break;
    }
    case 8:
        if (first < 0 || first >= 32) { return 0; }
        return (m_damageFlags & (1u << first)) != 0;
    case 9: {
        PropAction action;
        action.kind = PropAction::Kind::Destroyed;
        m_actions.push_back(action);
        m_collisionChanged = true;
        break;
    }
    case 11: SetAnimation(first, 255); break;
    case 12: m_checkEntry = true; m_collisionChanged = true; break;
    case 13: m_checkEntry = false; m_collisionChanged = true; break;
    case 14: m_moveSlot = first; break;
    case 15: QueueResource(PropAction::Kind::Portal, first); break;
    case 16: QueueResource(PropAction::Kind::AttachedEffect, first, second); break;
    case 17: {
        PropAction action;
        action.kind = PropAction::Kind::StopEffect;
        m_actions.push_back(action);
        break;
    }
    case 18: m_health = static_cast<float>(first); m_collisionChanged = true; break;
    case 19: return static_cast<std::int16_t>(std::round(m_health));
    default: ++m_unsupported; std::printf("[prop] unsupported native=%u\n", function); break;
    }
    return 0;
}

std::int16_t *CProp::VariableResolver(std::uint8_t variable) {
    if (variable == 0) { return &m_damageOwner; }
    return nullptr;
}

const CCollisionData &CProp::GetCollision(bool bullets) const {
    if (bullets || m_checkEntry) { return m_bulletCollision; }
    return m_collision;
}

std::vector<PropAction> CProp::TakeActions() {
    std::vector<PropAction> actions;
    actions.swap(m_actions);
    return actions;
}
