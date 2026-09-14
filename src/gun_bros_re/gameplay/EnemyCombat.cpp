/** @file EnemyCombat.cpp
 * @brief Enemy script behaviour, timers and damage in world coordinates.
 * Reference: CEnemy::Spawn :73239, Update :67720, resolver :71692.
 */
#include "gun_bros_re/gameplay/CEnemy.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kRadians = 3.14159265f / 180.0f;
constexpr float kScriptSecondsToMs = 1000.0f / 256.0f;

float NormalizeAngle(float angle) {
    angle = std::fmod(angle, 360.0f);
    if (angle < 0) {
        angle += 360;
    }
    return angle;
}

float AngleDifference(float from, float to) {
    float difference = NormalizeAngle(to) - NormalizeAngle(from);
    if (difference > 180) {
        difference -= 360;
    }
    if (difference < -180) {
        difference += 360;
    }
    return difference;
}
}

void CEnemy::ConfigureTemplate(float radius, bool targetable,
    const GameObjectRef &bullet, const CCollisionData &collision) {
    m_parts[0].radius = radius;
    combat.targetable = targetable;
    combat.bullet = bullet;
    combat.collision = collision;
}

void CEnemy::SetTarget(CombatId id, float x, float y, bool alive) {
    combat.targetId = id;
    combat.targetX = x;
    combat.targetY = y;
    combat.targetAlive = alive;
}

float CEnemy::TargetAngle() const {
    return NormalizeAngle(std::atan2(combat.targetX - combat.x,
        combat.y - combat.targetY) / kRadians);
}

float CEnemy::Random(float minimum, float maximum) {
    combat.randomState = combat.randomState * 1664525u + 1013904223u;
    return minimum + (maximum - minimum) *
        static_cast<float>(combat.randomState >> 8) / 16777216.0f;
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
    EnemyAction action;
    action.kind = EnemyAction::Kind::Bullet;
    action.resource = resource;
    action.part = part;
    action.node = node;
    action.direction = direction;
    action.speed = static_cast<float>(combat.variables[6]);
    combat.actions.push_back(action);
}

bool CEnemy::CanReceiveProjectile(int ownerType, CombatId owner) const {
    if (combat.dead || combat.removed || combat.health <= 0 || owner == combat.id) {
        return false;
    }
    // Both circle and complex collision use these exact comparisons (:72862).
    const int allegiance = combat.variables[16];
    if (ownerType == 1 && allegiance == 0) {
        return false;
    }
    if ((ownerType == 0 || ownerType == 2) && allegiance == 1) {
        return false;
    }
    return true;
}

HitResult CEnemy::ReceiveHit(const CombatHit &hit) {
    if (!CanReceiveProjectile(hit.ownerType, hit.owner)) {
        return HitResult::Ignored;
    }
    combat.pendingHit = hit;
    combat.collisionPending = true;
    combat.collisionResult = HitResult::Pending;
    combat.variables[2] = static_cast<std::int16_t>(hit.part);
    combat.variables[3] = 256;
    combat.variables[4] = 0;
    if (hit.splash) { combat.variables[4] = 1; }
    combat.variables[5] = static_cast<std::int16_t>(NormalizeAngle(
        std::atan2(hit.x - combat.x, combat.y - hit.y) / kRadians));
    combat.variables[7] = static_cast<std::int16_t>(hit.edge);
    combat.variables[9] = static_cast<std::int16_t>(NormalizeAngle(
        combat.variables[5] - combat.facing));
    combat.variables[19] = 0;
    // Scripts can reject frontal hits or modify the damage multiplier before
    // native 13 commits the collision. A direct health subtraction skips this.
    // HandleCollision :71526-71534 returns before the hit event for a grenade
    // (IsGrenade :60370, collision flag bit 4). Its later OnSplashDamage :67945
    // still triggers the event. Otherwise each fuse contact can strip armor.
    if (!hit.splash && (hit.flags & (1u << 4)) != 0) {
        return combat.collisionResult;
    }
    TriggerEvent(2);
    return combat.collisionResult;
}

void CEnemy::ResolvePendingHit(bool apply) {
    if (!combat.collisionPending) {
        return;
    }
    combat.collisionPending = false;
    if (apply) {
        const int part = combat.variables[2];
        if (part >= 0 && static_cast<std::uint32_t>(part) < m_partCount) {
            m_parts[part].hitFlash = 1;
        }
        Damage(combat.pendingHit.damage * static_cast<float>(combat.variables[3]) / 256.0f);
        combat.collisionResult = HitResult::Hit;
        if (combat.dead) {
            combat.collisionResult = HitResult::Killed;
        }
    } else {
        combat.collisionResult = HitResult::Ignored;
    }
    EnemyAction action;
    action.kind = EnemyAction::Kind::CollisionResolved;
    action.projectile = combat.pendingHit.projectile;
    action.owner = combat.pendingHit.owner;
    action.resource = combat.pendingHit.weapon;
    action.slot = static_cast<int>(combat.pendingHit.weaponSlot);
    action.result = combat.collisionResult;
    combat.actions.push_back(action);
}

void CEnemy::Damage(float amount) {
    if (combat.dead || combat.removed || combat.health <= 0 || amount <= 0) {
        return;
    }
    combat.lastDamage = std::min(combat.health, amount);
    combat.totalDamage += combat.lastDamage;
    combat.health = std::max(0.0f, combat.health - amount);
    combat.hitFlash = 1;
    combat.healthBarFlashMs = 1000; // CEnemy::Damage :71563.
    ++combat.hitCount;
    if (combat.health == 0) {
        combat.dead = true;
        ++combat.deathCount;
        combat.autoFireInterval = 0;
        // Export 1 selects the death sequence and its eventual removal call.
        m_interpreter.CallExportFunction(1);
    }
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

std::vector<EnemyAction> CEnemy::TakeActions() {
    std::vector<EnemyAction> actions;
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

void CEnemy::SetBehaviour(const std::int16_t *arguments, int count) {
    if (count == 0) {
        return;
    }
    const int mode = arguments[0];
    combat.behaviour = mode;
    if (mode == 0 && count >= 2) {
        float maximum = static_cast<float>(arguments[1]);
        if (count >= 3) {
            maximum = static_cast<float>(arguments[2]);
        }
        // SetBehaviour's argument is arrival distance, NOT movement speed.
        // UpdatePathFinder reads speed from script variable 0 (:70077).
        combat.arrivalDistance = Random(static_cast<float>(arguments[1]), maximum);
        combat.arrived = false;
    } else if (mode == 2) {
        m_linkPathFinder.Init(m_path, combat.x, combat.y);
        combat.arrived = false;
    } else if ((mode == 3 && count >= 2) || (mode == 5 && count >= 3)) {
        float angle = combat.facing;
        float distance = static_cast<float>(arguments[1]);
        if (mode == 5) {
            angle += arguments[1];
            distance = static_cast<float>(arguments[2]);
        }
        // With no navigation obstacles Arena has an unobstructed ray.
        distance = std::max(0.1f, distance);
        combat.destinationX = combat.x + std::sin(angle * kRadians) * distance;
        combat.destinationY = combat.y - std::cos(angle * kRadians) * distance;
        combat.behaviour = 3;
        combat.movementTimer = -1;
        if (mode == 5 && count >= 4 && combat.variables[0] > 0) {
            const int move = arguments[3];
            if (m_moveSet && move >= 0 && static_cast<std::size_t>(move) < m_moveSet->GetMoves().size()) {
                CMoveSetMeshController clock;
                clock.SetMoveSet(m_moveSet, m_configMeshes);
                clock.SetMove(move);
                float speed = m_moveSet->GetMoves()[move].speed;
                if (speed <= 0) { speed = 1; }
                const int duration = static_cast<int>(clock.GetAnimation().GetRangeDurationMs() / speed);
                combat.movementTimer = std::max(0, static_cast<int>(distance * 1000 /
                    combat.variables[0]) - duration);
            }
        }
    } else if (mode == 4) {
        combat.variables[1] = 0;
    } else if (mode == 6 && count >= 4) {
        combat.rotationStart = combat.facing;
        combat.rotationEnd = static_cast<float>(arguments[1]);
        combat.rotationDuration = 0;
        if (arguments[2] != 0) {
            combat.rotationDuration = std::abs(AngleDifference(combat.facing,
                combat.rotationEnd) / arguments[2]) * 1000;
        }
        combat.rotationTime = 0;
        combat.rotationSmooth = arguments[3] != 0;
        combat.variables[1] = 4;
    }
}

void CEnemy::UpdatePathFinder(float waypointX, float waypointY, float seconds) {
    if (seconds <= 0) { return; }
    const float dx = waypointX - combat.x;
    const float dy = waypointY - combat.y;
    const float distance = std::hypot(dx, dy);
    const float speed = std::max(0.0f, static_cast<float>(combat.variables[0]));
    float velocityX = 0;
    float velocityY = 0;
    if (distance > 0) {
        const float pathSpeed = std::min(speed, distance / seconds);
        velocityX = dx * (pathSpeed / distance);
        velocityY = dy * (pathSpeed / distance);
    }
    // CEnemy::UpdatePathFinder :70068-70164; ARM 0x3d0c8-0x3d1bc.
    // The loaded angle constants are +1 and pi. For finite vectors the
    // clamped acos minus pi/2 is <= pi/2, selecting vector addition.
    velocityX += combat.flockX;
    velocityY += combat.flockY;
    const float combinedSpeed = std::hypot(velocityX, velocityY);
    if (combinedSpeed > speed) {
        velocityX *= speed / combinedSpeed;
        velocityY *= speed / combinedSpeed;
    }
    combat.x += velocityX * seconds;
    combat.y += velocityY * seconds;
}

bool CEnemy::CanCollideWithPlayer() const {
    // Enabled/removed represent membership in the original level's enemy list.
    // Targeting type, visible parts and complex bullet edges do not filter bodies.
    return combat.enabled && !combat.removed && combat.health != 0 && combat.variables[16] != 1;
}

void CEnemy::UpdateCombatBeforeAnimation(int deltaMs) {
    combat.healthBarFlashMs = std::max(0, combat.healthBarFlashMs - deltaMs);
    combat.previousX = combat.x;
    combat.previousY = combat.y;
    combat.hitFlash = std::max(0.0f, combat.hitFlash - deltaMs * 0.004f);
    if (combat.removed) {
        return;
    }
    const float seconds = deltaMs * 0.001f;
    if (!combat.dead && combat.autoFireInterval > 0) {
        combat.autoFireTimer -= deltaMs;
        if (combat.autoFireTimer <= 0 && combat.targetAlive) {
            combat.autoFireTimer = combat.autoFireInterval;
            QueueBullet(ScriptResource(combat.autoFireResource), combat.variables[14],
                0, TargetAngle());
        }
    }
    // Death scripts may deliberately choose a movement/rotation behaviour.
    if (combat.behaviour == 0 && combat.targetAlive && !combat.dead) {
        float dx = combat.targetX - combat.x;
        float dy = combat.targetY - combat.y;
        const float distance = std::hypot(dx, dy);
        if (distance > combat.arrivalDistance && distance > 0) {
            combat.arrived = false;
            float waypointX = combat.targetX;
            float waypointY = combat.targetY;
            if (combat.hasNavigationTarget) {
                waypointX = combat.navigationX;
                waypointY = combat.navigationY;
            }
            UpdatePathFinder(waypointX, waypointY, seconds);
        } else if (!combat.arrived) {
            combat.arrived = true;
            TriggerEvent(0);
        }
    } else if (combat.behaviour == 2) {
        // CEnemy::UpdatePathFinder :70031 advances links before movement and
        // emits event 0 when the once-mode route is exhausted.
        m_linkPathFinder.Update(combat.x, combat.y);
        const auto *node = m_linkPathFinder.GetDestination();
        if (m_linkPathFinder.IsDone()) {
            if (!combat.arrived) { combat.arrived = true; TriggerEvent(0); }
        } else if (node != nullptr) {
            UpdatePathFinder(node->x, node->y, seconds);
        }
    } else if (combat.behaviour == 1 || combat.behaviour == 3) {
        const float dx = combat.destinationX - combat.x;
        const float dy = combat.destinationY - combat.y;
        const float distance = std::hypot(dx, dy);
        const float travel = std::max(0.0f, combat.variables[0] * seconds);
        if (distance > 0) {
            if (travel >= distance) {
                combat.x = combat.destinationX;
                combat.y = combat.destinationY;
                combat.behaviour = 7;
                TriggerEvent(0);
            } else {
                combat.x += dx / distance * travel;
                combat.y += dy / distance * travel;
            }
        }
        if (combat.movementTimer >= 0) {
            combat.movementTimer -= deltaMs;
            if (combat.movementTimer <= 0) {
                combat.movementTimer = -1;
                TriggerEvent(5);
            }
        }
    } else if (combat.behaviour == 6) {
        combat.rotationTime += deltaMs;
        float fraction = 1;
        if (combat.rotationDuration > 0) {
            fraction = std::min(1.0f, combat.rotationTime / combat.rotationDuration);
        }
        float interpolation = fraction;
        if (combat.rotationSmooth) {
            interpolation = (1 - std::cos(fraction * 3.14159265f)) * 0.5f;
        }
        combat.facing = NormalizeAngle(combat.rotationStart +
            AngleDifference(combat.rotationStart, combat.rotationEnd) * interpolation);
        if (fraction >= 1) {
            combat.behaviour = 7;
            TriggerEvent(0);
        }
    }
    if (combat.targetAlive && (combat.variables[1] == 0 || combat.variables[1] == 2)) {
        const float difference = AngleDifference(combat.facing, TargetAngle());
        float turn = difference;
        if (combat.variables[10] > 0) {
            const float limit = combat.variables[10] * seconds;
            turn = std::clamp(difference, -limit, limit);
        }
        combat.facing = NormalizeAngle(combat.facing + turn);
        if (combat.behaviour == 4 && std::abs(difference - turn) < 0.1f) {
            combat.behaviour = 7;
            TriggerEvent(0);
        }
    } else if (combat.variables[1] == 1 &&
        (combat.x != combat.previousX || combat.y != combat.previousY)) {
        combat.facing = NormalizeAngle(std::atan2(combat.x - combat.previousX,
            combat.previousY - combat.y) / kRadians);
    }
}

void CEnemy::UpdateCombatAfterAnimation(int deltaMs) {
    if (combat.removed) {
        return;
    }
    UpdateCombatTimers(deltaMs);
    if (!combat.dead && combat.triggerDistance > 0 && combat.targetAlive) {
        const float distance = std::hypot(combat.targetX - combat.x, combat.targetY - combat.y);
        if ((combat.triggerInside && distance < combat.triggerDistance) ||
            (!combat.triggerInside && distance > combat.triggerDistance)) {
            TriggerEvent(1);
        }
    }
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
    EnemyAction action;
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
        action.kind = EnemyAction::Kind::Effect;
        if (function == 9) {
            action.kind = EnemyAction::Kind::Sound;
        } else if (function == 27) {
            action.kind = EnemyAction::Kind::LoopSound;
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
        return true;
    case 24:
        // Link-path mode has no effect without a path layer.
        m_linkPathFinder.SetMode(args[0] == 1, combat.x, combat.y);
        return true;
    case 26:
        action.kind = EnemyAction::Kind::Broadcast;
        action.slot = args[0];
        break;
    case 28:
        action.kind = EnemyAction::Kind::StopSound;
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
        action.kind = EnemyAction::Kind::LinkedEffect;
        action.slot = args[0];
        action.resource = ScriptResource(args[2]);
        action.node = args[1];
        break;
    case 32:
        action.kind = EnemyAction::Kind::StopEffect;
        action.slot = args[0];
        break;
    case 33:
    case 34:
        action.kind = EnemyAction::Kind::Splash;
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
        action.kind = EnemyAction::Kind::Effect;
        action.resource = ScriptResource(args[0]);
        action.node = args[1];
        break;
    case 37:
        if (args[0] >= 0 && static_cast<std::uint32_t>(args[0]) < m_partCount) {
            // Offset 168 selects the rotated or unrotated drawing pass; it
            // never hides a part (:67674). Turret bases need the latter.
            m_parts[args[0]].followsFacing = args[1] != 0;
        }
        return true;
    case 38:
        action.kind = EnemyAction::Kind::Stun;
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
        action.kind = EnemyAction::Kind::Shake;
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
        action.kind = EnemyAction::Kind::LevelEvent;
        action.slot = 1;
        break;
    case 47:
        // Level objectives/teleport notifications have no Arena owner.
        // World sessions consume the authored callback after the actor update.
        action.kind = EnemyAction::Kind::Teleported;
        break;
    case 56:
        // CEnemy native 0x38 forwards its argument to CLevel::HandleEvent.
        action.kind = EnemyAction::Kind::LevelEvent;
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
    case 54:
        // This resolves the LEVEL script's resource table, not this enemy's.
        // Keep the dependency explicit in an empty arena; inventing index 0's
        // resource would spawn the wrong object (original :72587).
        combat.deferredMechanisms |= 2;
        return true;
    case 59:
        // Boss entry presentation and camera targeting are outside this pass.
        // Camera targeting is now implemented below; native 59's stored
        // parameters still need a verified consumer before reproducing them.
        combat.deferredMechanisms |= 1;
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
        action.kind = EnemyAction::Kind::SpawnPickup;
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
        action.kind = EnemyAction::Kind::RemoveBullet;
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
        action.kind = EnemyAction::Kind::TurretActive;
        action.slot = args[0] != 0;
        break;
    default:
        return false;
    }
    combat.actions.push_back(action);
    return true;
}
