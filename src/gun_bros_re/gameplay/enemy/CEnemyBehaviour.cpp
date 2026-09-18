/** @file CEnemyBehaviour.cpp
 * Original: src/gunbros/enemy.cpp SetBehaviour :69707, Update :67732,
 * Damage :71552, ResolveFunctionLocally :71692. One CEnemy, split implementation.
 */
#include "gun_bros_re/gameplay/enemy/CEnemy.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kRadians = 3.14159265f / 180.0f;
}

float CEnemy::NormalizeAngle(float angle) {
    angle = std::fmod(angle, 360.0f);
    if (angle < 0) {
        angle += 360;
    }
    return angle;
}

float CEnemy::AngleDifference(float from, float to) {
    float difference = NormalizeAngle(to) - NormalizeAngle(from);
    if (difference > 180) {
        difference -= 360;
    }
    if (difference < -180) {
        difference += 360;
    }
    return difference;
}

void CEnemy::SetTarget(ZCombatId id, float x, float y, bool alive) {
    combat.targetId = id;
    combat.targetX = x;
    combat.targetY = y;
    combat.targetAlive = alive;
}

void CEnemy::UpdateNavigation(const CLayerPathMesh &path, const std::vector<float> &distances) {
    m_meshPathFinder.Update(path, distances, combat.x, combat.y, combat.targetX, combat.targetY);
    m_meshPathFinder.GetDestination(combat.navigationX, combat.navigationY);
    combat.hasNavigationTarget = true;
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
        // World SetBehaviourMoveAngle :69625 first clips against CastRay and
        // subtracts the original part-0 radius. ARM 0x3c908 maps negative input to 1.
        if (distance < 0) { distance = 1; }
        const float directionX = std::sin(angle * kRadians);
        const float directionY = -std::cos(angle * kRadians);
        if (GetLevelContext() != nullptr) {
            const auto *path = dynamic_cast<const CLayerPathMesh *>(GetLevelContext()->GetNavigationPath());
            if (path != nullptr) {
                const float boundary = path->CastRay(combat.x, combat.y, directionX, directionY);
                if (distance + m_parts[0].radius > boundary) {
                    distance = boundary - m_parts[0].radius;
                }
            }
        }
        distance = std::max(0.1f, distance);
        combat.destinationX = combat.x + directionX * distance;
        combat.destinationY = combat.y + directionY * distance;
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
