/** Windows PvP input policy; all attacks execute the original CBrother/Flow. */
#pragma once
#include "gun_bros_re/gameplay/CBrotherAI.h"
#include "gun_bros_re/gameplay/CCollisionData.h"
#include "gun_bros_re/gameplay/CMPMatch.h"
#include "gun_bros_re/data/WeaponCatalog.h"
#include "gun_bros_re/data/StoreCatalog.h"
#include "gun_bros_re/data/CProfileManager.h"

class DeathmatchBot final : public CBrotherAI {
public:
    enum class Tactic { Search, Fight, Supply, Cover, Dead };
    void Configure(unsigned seed, const WeaponEntry &first, const WeaponEntry &second);
    static std::array<unsigned, 2> ChooseLoadout(const CMPMatch::Entry &match, const std::vector<WeaponEntry> &weapons, unsigned seed);
    static const StoreEntry *ChoosePurchase(const std::vector<StoreEntry> &store, const CProfileManager &profile, unsigned level, const CMPMatch::Life &life);
    void Reset(float x, float y, float facing) override;
    void Update(int deltaMs, CBrother &brother, IBrotherAIWorld &world, float playerX, float playerY, float speedMultiplier) override;
    CombatId GetTarget() const override { return m_target; }
    bool IsMoving() const override { return m_moving; }
    unsigned GetTargetCount() const override { return m_sightings; }
    Tactic GetTactic() const { return m_tactic; }
    void PrintNavigation() const;
    bool WantsGrenade() const { return m_tactic == Tactic::Fight && m_visible && m_distance > 100 && m_distance < 300; }
    bool WantsHealth() const { return vitals.health < vitals.maximum * 0.65f; }
    bool WantsShop() const { return m_ageMs > 12000 && m_shopDelayMs == 0 && (!m_visible || m_tactic == Tactic::Cover); }
    void OnShopAttempt() { m_shopDelayMs = 15000; }
private:
    Tactic m_tactic = Tactic::Search;
    CombatId m_target = 0;
    bool m_visible = false, m_moving = false;
    unsigned m_sightings = 0, m_ageMs = 0;
    int m_decisionMs = 0, m_reactionMs = 0, m_swapMs = 0, m_memoryMs = 0, m_shopDelayMs = 0;
    float m_lastX = 0, m_lastY = 0, m_goalX = 0, m_goalY = 0, m_moveX = 0, m_moveY = 0;
    float m_distance = 0, m_aimError = 0, m_strafe = 1;
    bool m_navigating = false;
    float m_waypointX = 0, m_waypointY = 0;
    float m_ranges[2]{230, 230};
    std::vector<CollisionPoint> m_route;
    float m_routeGoalX = 0, m_routeGoalY = 0;
    int m_routeMs = 0;
    std::mt19937 m_random;
};
