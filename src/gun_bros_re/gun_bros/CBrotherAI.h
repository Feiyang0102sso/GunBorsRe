/** @file CBrotherAI.h
 * @brief Original brother follow/target policy, with a desktop world boundary.
 */
#ifndef GUN_BROS_RE_CBROTHERAI_H
#define GUN_BROS_RE_CBROTHERAI_H
#include "gun_bros/CBrother.h"
#include <random>

class IBrotherAIWorld {
public:
    virtual ~IBrotherAIWorld() = default;
    virtual CombatId FindBrotherTarget(float x, float y, float radius) = 0;
    virtual bool GetBrotherTarget(CombatId id, float &x, float &y) = 0;
    virtual bool GetBrotherWaypoint(float x, float y, float targetX, float targetY,
        float &waypointX, float &waypointY) = 0;
    virtual void ResolveBrotherForce(float previousX, float previousY, float &x, float &y) = 0;
};

/** CBrotherAI::UpdateMovement :139560 and CTargetingController :223185.
 * Script/mesh storage remains in the shared CBrother. Movement follows the
 * player, independently of the enemy selected for shooting.
 */
class CBrotherAI {
public:
    void Reset(float startX, float startY);
    void SetForce(float x, float y, int durationMs);
    void Update(int deltaMs, CBrother &brother, IBrotherAIWorld &world,
        float playerX, float playerY, float speedMultiplier);
    float x = 0;
    float y = 0;
    float previousX = 0;
    float previousY = 0;
    float facing = 0;
    PlayerVitals vitals;
    CombatId GetTarget() const { return m_target; }
    bool IsMoving() const { return m_moving; }
    unsigned GetTargetCount() const { return m_targetCount; }
private:
    int Random(int minimum, int maximum);
    void UpdateTarget(int deltaMs, IBrotherAIWorld &world, bool &shooting);
    std::mt19937 m_random{0xB6400};
    CombatId m_target = 0;
    int m_findDelay = 0;
    int m_fireDelay = 0;
    float m_aimError = 0;
    float m_desiredAimError = 0;
    bool m_moving = false;
    unsigned m_targetCount = 0;
    float m_forceX = 0;
    float m_forceY = 0;
    int m_forceMs = 0;
};
#endif
