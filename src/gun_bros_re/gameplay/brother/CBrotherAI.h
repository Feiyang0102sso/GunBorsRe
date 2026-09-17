/** @file CBrotherAI.h
 * @brief Original brother follow/target policy, with a desktop world boundary.
 */
#ifndef GUN_BROS_RE_CBROTHERAI_H
#define GUN_BROS_RE_CBROTHERAI_H
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include <random>

class ZBrotherAIWorld {
public:
    virtual ~ZBrotherAIWorld() = default;
    virtual bool IsPlayerDown() const { return false; }
    struct Threat { float x, y, radius; };
    virtual std::vector<Threat> GetBrotherThreats() const { return {}; }
    virtual bool CanBrotherWalk(float x, float y, float destinationX, float destinationY) const { return true; }
    virtual ZCombatId FindBrotherTarget(float x, float y, float radius) = 0;
    virtual bool GetBrotherTarget(ZCombatId id, float &x, float &y) = 0;
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
    virtual ~CBrotherAI() = default;
    /** startFacing is the map PLAYER object's spawn angle, as the player uses. */
    virtual void Reset(float startX, float startY, float startFacing);
    void SetForce(float x, float y, int durationMs);
    void SetShootingAllowed(bool allowed) { m_shootingAllowed = allowed; }
    virtual void Update(int deltaMs, CBrother &brother, ZBrotherAIWorld &world,
        float playerX, float playerY, float speedMultiplier);
    float x = 0;
    float y = 0;
    float previousX = 0;
    float previousY = 0;
    float facing = 0;
    ZPlayerVitals vitals;
    virtual ZCombatId GetTarget() const { return m_target; }
    virtual bool IsMoving() const { return m_moving; }
    virtual unsigned GetTargetCount() const { return m_targetCount; }
    bool TakeWeaponSwapRequest() { const bool requested = m_weaponSwapRequested; m_weaponSwapRequested = false; return requested; }
protected:
    void UpdateForce(int deltaMs, CBrother &brother, ZBrotherAIWorld &world);
    bool m_shootingAllowed = true;
    bool m_weaponSwapRequested = false;
private:
    int Random(int minimum, int maximum);
    void UpdateTarget(int deltaMs, ZBrotherAIWorld &world, bool &shooting);
    std::mt19937 m_random{0xB6400};
    ZCombatId m_target = 0;
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
