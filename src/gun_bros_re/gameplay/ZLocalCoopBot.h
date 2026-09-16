/** Local peer input policy. This is host AI, not an original remote player. */
#pragma once
#include "gun_bros_re/gameplay/CBrotherAI.h"

class ZLocalCoopBot final : public CBrotherAI {
public:
    void Reset(float startX, float startY, float startFacing) override;
    void Update(int deltaMs, CBrother &brother, ZBrotherAIWorld &world,
        float playerX, float playerY, float speedMultiplier) override;
    ZCombatId GetTarget() const override { return m_target; }
    bool IsMoving() const override { return m_moving; }
    unsigned GetTargetCount() const override { return m_targetCount; }
    // Menu decisions advance only while the session accepts peer input.
    void AdvanceActions(unsigned deltaMs);
    bool TakePowerupRequest();
    bool TakeShopRequest();
    unsigned ShopSelection(unsigned elapsedMs) const;
    bool ShouldBuyShopItem(unsigned elapsedMs, unsigned ownedCount);
private:
    ZCombatId m_target = 0;
    int m_reactionMs = 0;
    bool m_moving = false;
    unsigned m_targetCount = 0;
    int m_swapMs = 0, m_roamMs = 0;
    unsigned m_powerupMs = 0, m_shopMs = 0;
    unsigned m_lastShopAttempt = UINT32_MAX;
    float m_roamX = 0, m_roamY = 0;
    int m_steeringMs = 0;
    float m_moveX = 0, m_moveY = 0;
    bool m_rescuing = false;
    std::mt19937 m_random{0xB07};
};
