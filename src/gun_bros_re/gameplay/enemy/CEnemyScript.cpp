/** @file CEnemyScript.cpp
 * Original: src/gunbros/enemy.cpp SetBehaviour :69707, Update :67732,
 * Damage :71552, ResolveFunctionLocally :71692. One CEnemy, split implementation.
 */
#include "gun_bros_re/gameplay/enemy/CEnemy.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kScriptSecondsToMs = 1000.0f / 256.0f;
}

GameObjectRef CEnemy::ScriptResource(int index) const {
    GameObjectRef result;
    std::uint32_t ordinal = 0;
    if (m_interpreter.GetResource(static_cast<std::uint16_t>(index), result.packHash, ordinal)) {
        result.localIndex = static_cast<std::uint8_t>(ordinal);
    }
    return result;
}

void CEnemy::QueueBullet(const GameObjectRef &resource, int part, int node, float direction) {
    if (combat.dead || resource.localIndex == 255) {
        return;
    }
    CEnemy::Action action;
    action.kind = CEnemy::Action::Kind::Bullet;
    action.resource = resource;
    action.part = part;
    action.node = node;
    action.direction = direction;
    action.speed = static_cast<float>(combat.variables[6]);
    combat.actions.push_back(action);
}

bool CEnemy::TriggerEvent(std::uint8_t event) {
    return m_interpreter.HandleEvent(6, event);
}

void CEnemy::HandleMessage(int message) {
    // CEnemy::HandleMessage :68024: messages 0..5 map to 4,9,10,11,12,13.
    if (message == 0) { TriggerEvent(4); }
    else if (message >= 1 && message <= 5) { TriggerEvent(static_cast<std::uint8_t>(message + 8)); }
}

void CEnemy::OnScriptStateEntered() {
    // OnStateChanged clears only the event timer (:67310), not the function timer.
    combat.eventTimer = 0;
}

std::vector<CEnemy::Action> CEnemy::TakeActions() {
    std::vector<CEnemy::Action> actions;
    actions.swap(combat.actions);
    return actions;
}

std::size_t CEnemy::GetUnsupportedFunctionCount() const {
    std::size_t count = 0;
    for (bool reported : m_reportedFunction) {
        if (reported) {
            ++count;
        }
    }
    return count;
}

void CEnemy::UpdateCombatTimers(int deltaMs) {
    if (combat.functionTimer > 0) {
        combat.functionTimer -= deltaMs;
        if (combat.functionTimer <= 0) {
            combat.functionTimer = 0;
            m_interpreter.CallFunctionDirect(static_cast<std::uint8_t>(combat.timerFunction));
        }
    }
    if (combat.eventTimer > 0) {
        combat.eventTimer -= deltaMs;
        if (combat.eventTimer <= 0) {
            combat.eventTimer = 0;
            TriggerEvent(3);
        }
    }
}

bool CEnemy::ResolveCombatFunction(std::uint8_t function, const std::int16_t *arguments,
    std::uint8_t argumentCount, std::int16_t &result) {
    // Native bytecode carries the argument count. Zero-fill optional slots once,
    // rather than reading beyond short calls such as FireBullet().
    std::array<std::int16_t, 8> args{};
    for (std::size_t i = 0; i < argumentCount && i < args.size(); ++i) {
        args[i] = arguments[i];
    }
    CEnemy::Action action;
    action.x = combat.x;
    action.y = combat.y;
    action.direction = combat.facing;
    action.part = combat.variables[14];
    switch (function) {
    case 0:
        SetBehaviour(args.data(), argumentCount);
        return true;
    case 1:
        combat.triggerInside = args[0] != 0;
        combat.triggerDistance = static_cast<float>(args[1]);
        combat.targetRange = combat.triggerDistance + 20;
        return true;
    case 2:
        combat.triggerDistance = 0;
        return true;
    case 3:
        combat.functionTimer = static_cast<int>(args[0] * kScriptSecondsToMs);
        combat.timerFunction = args[1];
        return true;
    case 4:
        combat.functionTimer = 0;
        return true;
    case 5:
        QueueBullet(combat.bullet, action.part, args[0], combat.facing);
        return true;
    case 6:
        combat.removed = true;
        return true;
    case 7:
        combat.facing = NormalizeAngle(static_cast<float>(args[0]));
        return true;
    case 8:
    case 9:
    case 27:
        action.kind = CEnemy::Action::Kind::Effect;
        if (function == 9) {
            action.kind = CEnemy::Action::Kind::Sound;
        } else if (function == 27) {
            action.kind = CEnemy::Action::Kind::LoopSound;
        }
        action.resource = ScriptResource(args[0]);
        break;
    case 13:
        if (argumentCount == 1) {
            combat.variables[2] = args[0];
        }
        ResolvePendingHit(true);
        return true;
    case 14:
        ResolvePendingHit(false);
        return true;
    case 17:
        combat.autoFireResource = args[0];
        combat.autoFireInterval = static_cast<int>(args[1] * kScriptSecondsToMs);
        combat.autoFireTimer = 0;
        return true;
    case 18:
        combat.autoFireInterval = 0;
        return true;
    case 19:
        QueueBullet(ScriptResource(args[0]), action.part, args[1],
            Random(static_cast<float>(args[2]), static_cast<float>(args[3])));
        return true;
    case 20: {
        const int count = args[4];
        const float first = static_cast<float>(std::min(args[2], args[3]));
        const float last = static_cast<float>(std::max(args[2], args[3]));
        for (int i = 0; i < count; ++i) {
            QueueBullet(ScriptResource(args[0]), action.part, args[1],
                first + (last - first) * i / count);
        }
        return true;
    }
    case 21:
        QueueBullet(ScriptResource(args[0]), action.part, args[1],
            TargetAngle() + Random(static_cast<float>(args[2]), static_cast<float>(args[3])));
        return true;
    case 22:
        combat.eventTimer = static_cast<int>(args[0] * kScriptSecondsToMs);
        return true;
    case 23:
        result = 1; // Empty Arena has no intervening collision edges.
        if (GetLevelContext() != nullptr) {
            result = GetLevelContext()->TestEnemyLineOfSight(combat.x, combat.y, combat.targetX, combat.targetY);
        }
        return true;
    case 24:
        // Link-path mode has no effect without a path layer.
        m_linkPathFinder.SetMode(args[0] == 1, combat.x, combat.y);
        return true;
    case 26:
        action.kind = CEnemy::Action::Kind::Broadcast;
        action.slot = args[0];
        break;
    case 28:
        action.kind = CEnemy::Action::Kind::StopSound;
        break;
    case 29:
        for (std::uint32_t i = 0; i < m_partCount; ++i) {
            if (argumentCount == 0 || args[0] == -1 || args[0] == static_cast<int>(i)) {
                m_parts[i].hitFlash = 1;
            }
        }
        return true;
    case 30:
        result = static_cast<std::int16_t>(std::min(32767.0f,
            std::hypot(combat.targetX - combat.x, combat.targetY - combat.y)));
        return true;
    case 31:
        // FunctionResolver :72312 accepts precisely these native overloads.
        if (argumentCount < 3 || argumentCount > 6) { return true; }
        action.kind = CEnemy::Action::Kind::LinkedEffect;
        action.slot = args[0];
        action.resource = ScriptResource(args[2]);
        action.node = args[1];
        action.alignEffect = true;
        if (argumentCount >= 4) { action.effectGroup = args[3]; }
        if (argumentCount >= 5) { action.alignEffect = args[4] != 0; }
        if (argumentCount == 6) { action.effectScale = args[5] / 256.0f; }
        break;
    case 32:
        action.kind = CEnemy::Action::Kind::StopEffect;
        action.slot = args[0];
        break;
    case 33:
    case 34:
        action.kind = CEnemy::Action::Kind::Splash;
        action.node = args[0];
        action.damage = static_cast<float>(args[1]);
        action.radius = static_cast<float>(args[2]);
        action.force = static_cast<float>(args[3]);
        action.durationMs = args[4];
        break;
    case 35:
        if (combat.targetAlive) {
            combat.facing = TargetAngle();
        }
        return true;
    case 36:
        action.kind = CEnemy::Action::Kind::Effect;
        action.resource = ScriptResource(args[0]);
        action.node = args[1];
        if (argumentCount >= 3) { action.effectGroup = args[2]; }
        break;
    case 37:
        if (args[0] >= 0 && static_cast<std::uint32_t>(args[0]) < m_partCount) {
            // Offset 168 selects the rotated or unrotated drawing pass; it
            // never hides a part (:67674). Turret bases need the latter.
            m_parts[args[0]].followsFacing = args[1] != 0;
        }
        return true;
    case 38:
        action.kind = CEnemy::Action::Kind::Stun;
        action.node = args[0];
        action.durationMs = args[1];
        action.radius = static_cast<float>(args[2]);
        result = combat.targetAlive && std::hypot(combat.targetX - combat.x,
            combat.targetY - combat.y) < action.radius;
        break;
    case 39: {
        const int part = combat.variables[14];
        if (part >= 0 && static_cast<std::uint32_t>(part) < m_partCount) {
            CMeshAnimationController &clock = m_parts[part].controller.GetAnimation();
            clock.SetTimeMs(clock.GetRangeStartMs() +
                static_cast<int>(Random(0, static_cast<float>(clock.GetRangeDurationMs()))));
        }
        return true;
    }
    case 40:
        action.kind = CEnemy::Action::Kind::Shake;
        action.durationMs = static_cast<int>(args[0] * kScriptSecondsToMs);
        break;
    case 42:
        QueueBullet(ScriptResource(args[0]), args[1], args[2],
            Random(static_cast<float>(args[3]), static_cast<float>(args[4])));
        return true;
    case 43: {
        result = 1;
        if (combat.y < combat.targetY) { result = 2; }
        if (combat.x < combat.targetX) { result |= 8; } else { result |= 4; }
        const float difference = AngleDifference(combat.facing, TargetAngle());
        if (std::abs(difference) < 90) { result |= 16; } else { result |= 32; }
        result |= 128;
        return true;
    }
    case 44:
        result = static_cast<std::int16_t>(std::min(32767.0f, std::abs(combat.targetX - combat.x)));
        return true;
    case 45:
        result = static_cast<std::int16_t>(std::min(32767.0f, std::abs(combat.targetY - combat.y)));
        return true;
    case 46:
        action.kind = CEnemy::Action::Kind::LevelEvent;
        action.slot = 1;
        break;
    case 47:
        // Level objectives/teleport notifications have no Arena owner.
        // World sessions consume the authored callback after the actor update.
        action.kind = CEnemy::Action::Kind::Teleported;
        break;
    case 56:
        // CEnemy native 0x38 forwards its argument to CLevel::HandleEvent.
        action.kind = CEnemy::Action::Kind::LevelEvent;
        action.slot = args[0];
        break;
    case 48:
        result = static_cast<std::int16_t>(TargetAngle());
        return true;
    case 49:
        if (args[0] >= 0 && args[0] < 32) {
            result = (combat.pendingHit.flags & (1u << args[0])) != 0;
        }
        return true;
    case 50:
        combat.health = std::max(0.0f, static_cast<float>(args[0]));
        if (GetLevelContext() != nullptr) {
            combat.health *= GetLevelContext()->GetEnemyMultiplier(combat.templateRef, 1);
        }
        combat.maxHealth = std::max(combat.maxHealth, combat.health);
        return true;
    case 51: {
        float health = combat.health;
        if (GetLevelContext() != nullptr) {
            const float multiplier = GetLevelContext()->GetEnemyMultiplier(combat.templateRef, 1);
            if (multiplier > 0) { health /= multiplier; }
        }
        // GetHealth :68315 reports script units, rounded and never below one.
        result = static_cast<std::int16_t>(std::clamp(std::round(health), 1.0f, 32767.0f));
        return true;
    }
    case 52:
        result = static_cast<std::int16_t>(std::round(combat.pendingHit.damage));
        return true;
    case 53:
        Damage(static_cast<float>(args[0]));
        return true;
    case 55:
        combat.targetType = args[0];
        combat.targetAlive = false;
        combat.targetId = 0;
        return true;
    case 54: {
        // This resolves the LEVEL script's resource table, not this enemy's.
        // Keep the dependency explicit in an empty arena; inventing index 0's
        // resource would spawn the wrong object (original :72587).
        CLevel *level = GetLevelContext();
        if (level == nullptr) {
            combat.deferredMechanisms |= 2;
            return true;
        }
        float z = 0;
        if (!level->GetResource(args[0], action.resource) ||
            !GetNodeLocationChunk(combat.variables[14], args[1], action.x, action.y, z)) { return false; }
        // ARM 0x403b4/0x40444: truncate map coordinates, optional ID, no forced slot.
        action.kind = Action::Kind::SpawnEnemy;
        action.x = std::trunc(action.x);
        action.y = std::trunc(action.y);
        action.slot = -1;
        if (argumentCount == 3) { action.slot = args[2]; }
        break;
    }
    case 59:
        // Boss entry presentation and camera targeting are outside this pass.
        // Camera targeting is now implemented below; native 59's stored
        // parameters still need a verified consumer before reproducing them.
        // Confirmed storage at :72640; only the last argument is not Q8.
        combat.native59Parameters = {args[0] / 256.0f, args[1] / 256.0f,
            args[2] / 256.0f, static_cast<float>(args[3])};
        return true;
    case 68:
        // SetCameraTarget :68799 runs synchronously inside the spawn Flow;
        // queueing this after LEVEL's next camera call would change its order.
        if (GetLevelContext() != nullptr) {
            GetLevelContext()->FocusCameraOnEnemy(combat.x, combat.y);
        } else {
            combat.deferredMechanisms |= 1;
        }
        return true;
    case 62:
        // SpawnItem is the original kill reward, deliberately excluded.
        // Correction: :68644 creates a CPickup at the enemy's location;
        // its collection script grants the reward later, independently of XP.
        action.kind = CEnemy::Action::Kind::SpawnPickup;
        action.resource = ScriptResource(args[0]);
        break;
    case 57:
        combat.eventTimer = 1000 * args[0];
        return true;
    case 58:
        if (!combat.dead) {
            combat.health = 0;
            combat.dead = true;
            ++combat.deathCount;
        }
        return true;
    case 61:
        combat.scaleFactor = args[0] / 256.0f;
        return true;
    case 65:
        action.kind = CEnemy::Action::Kind::RemoveBullet;
        result = 1;
        break;
    case 66: {
        int amplitude = 2;
        if (argumentCount == 2 && args[1] <= 0) { amplitude = 0; }
        if (argumentCount == 1 || argumentCount == 2) {
            stun.SetStunned(1000 * args[0] / 256, 50, amplitude);
        }
        return true;
    }
    case 67:
        stun.ClearStunned();
        return true;
    case 69:
        // The scene resolves the animated node's direction as well as position.
        QueueBullet(ScriptResource(args[0]), action.part, args[1], combat.facing);
        if (!combat.actions.empty()) {
            combat.actions.back().slot = 1;
        }
        return true;
    case 70:
        combat.turret = true;
        return true;
    case 71:
        // Owner turret-availability UI; no inventory in Arena.
        // The original offline owner index is -1, selecting the local player.
        action.kind = CEnemy::Action::Kind::TurretActive;
        action.slot = args[0] != 0;
        break;
    default:
        return false;
    }
    combat.actions.push_back(action);
    return true;
}
