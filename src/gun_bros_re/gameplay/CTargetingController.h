/** @file CTargetingController.h
 * @brief Original type-2 enemy targeting used by the player's Auto Aim item.
 */
#ifndef GUN_BROS_RE_CTARGETINGCONTROLLER_H
#define GUN_BROS_RE_CTARGETINGCONTROLLER_H
#include "gun_bros_re/gameplay/ZCombatTypes.h"
#include <random>

class ZBrotherAIWorld;

class CTargetingController {
public:
    void Reset();
    void ClearTarget(float facing);
    bool Update(int deltaMs, float x, float y, float &facing, ZBrotherAIWorld &world);
    ZCombatId GetTarget() const { return m_target; }
private:
    int Random(int minimum, int maximum);
    std::mt19937 m_random{0xA170}; // Reproducible host RNG, not the original global RNG sequence.
    ZCombatId m_target = 0;
    int m_findDelay = 0;
    int m_fireDelay = 0;
    float m_aimError = 0;
    float m_desiredAimError = 0;
    float m_lastFacing = 0;
};

unsigned CheckTargetingController();

#endif
