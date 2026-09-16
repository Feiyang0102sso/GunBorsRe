/** Explicit tactical states. Constants tune host decisions, never gun damage. */
#define NOMINMAX
#include "gun_bros_re/gameplay/ZDeathmatchBot.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/gameplay/ZPowerupScene.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "engine/core/CStringToKey.h"

namespace {
constexpr float Radians = 3.14159265f / 180;
constexpr int DecisionMs = 220, ReactionMs = 240, MemoryMs = 3500;
constexpr int PowerupDecisionMs = 500; // Host input cadence; BIG owns item cooldowns.
constexpr float InputSpeed = 220; // CPlayer movement adapter, shared with CombatScene.
float PreferredRange(const ZWeaponEntry &weapon) {
    // Tactical preferences derived from the resource's original category.
    if (weapon.category == 2) { return 150; }
    if (weapon.category == 3 || weapon.category == 4) { return 300; }
    return 230;
}
}
const ZStoreEntry *ZDeathmatchBot::ChoosePurchase(const std::vector<ZStoreEntry> &store, const CProfileManager &profile, unsigned level, const CMPMatch::Life &life) {
    if (life.dead) { return nullptr; }
    GameObjectRef ref; ref.packHash = CStringToKey("pack5");
    unsigned healthCount = life.healthPacks;
    for (unsigned index : {1u, 8u, 9u}) { ref.localIndex = static_cast<std::uint8_t>(index); healthCount += profile.GetPowerupCount(ref); }
    ref.localIndex = 13;
    const unsigned grenadeCount = life.grenades + profile.GetPowerupCount(ref);
    // Buy only charges this life can still consume. Health variants share one budget.
    for (unsigned index : {9u, 8u, 1u, 13u}) {
        if (index == 13 && grenadeCount >= 2) { continue; }
        if (index != 13 && healthCount >= 2) { continue; }
        for (const auto &entry : store) {
            const auto &item = entry.data;
            if (item.objects.size() != 1 || item.objects[0].type != 17 || item.requiredLevel > level) { continue; }
            const auto &object = item.objects[0].object;
            if (object.packHash != ref.packHash || object.localIndex != index) { continue; }
            if (item.commonPrice != 0) {
                if (profile.coins < item.commonPrice) { continue; }
            } else if (item.rarePrice == 0 || profile.warbucks < item.rarePrice) { continue; }
            return &entry;
        }
    }
    return nullptr;
}
void ZDeathmatchBot::PrintNavigation() const {
    std::printf("[deathmatch-bot] tactic=%u goal=%.1f,%.1f route=%zu movement=%.2f,%.2f\n", static_cast<unsigned>(m_tactic), m_goalX, m_goalY, m_route.size(), m_moveX, m_moveY);
    if (!m_route.empty()) { std::printf("[deathmatch-bot] next=%.1f,%.1f\n", m_route.front().x, m_route.front().y); }
}
std::array<unsigned, 2> ZDeathmatchBot::ChooseLoadout(const CMPMatch::Entry &match, const std::vector<ZWeaponEntry> &weapons, unsigned seed) {
    std::mt19937 random(seed);
    const unsigned first = std::uniform_int_distribution<unsigned>(0, static_cast<unsigned>(match.guns.size() - 1))(random);
    int firstCategory = -1;
    for (const auto &weapon : weapons) {
        if (weapon.packHash == match.guns[first].packHash && weapon.ordinal == match.guns[first].localIndex) { firstCategory = weapon.category; }
    }
    unsigned second = (first + 1) % static_cast<unsigned>(match.guns.size());
    for (unsigned offset = 1; offset < match.guns.size(); ++offset) {
        const unsigned index = (first + offset) % static_cast<unsigned>(match.guns.size());
        for (const auto &weapon : weapons) {
            if (weapon.packHash == match.guns[index].packHash && weapon.ordinal == match.guns[index].localIndex && weapon.category != firstCategory) { return {first, index}; }
        }
    }
    return {first, second};
}
void ZDeathmatchBot::Configure(unsigned seed, const ZWeaponEntry &first, const ZWeaponEntry &second, CMPMatch::BotLevel level) {
    m_level = level;
    m_random.seed(seed);
    m_ranges[0] = PreferredRange(first); m_ranges[1] = PreferredRange(second);
}
bool ZDeathmatchBot::TakePowerupRequest() {
    if (m_level == CMPMatch::BotLevel::Easy || vitals.dead || m_powerupDecisionMs > 0 || (!m_visible && !WantsHealth())) { return false; }
    m_powerupDecisionMs = PowerupDecisionMs;
    return true;
}

void ZDeathmatchBot::UsePowerups(ZPowerupScene &powerups) {
    if (m_level != CMPMatch::BotLevel::Easy) {
        if (!TakePowerupRequest()) { return; }
        if (WantsHealth() && powerups.UseMatchConsumable(false)) { return; }
        if (m_level == CMPMatch::BotLevel::Hard) { powerups.UseAny(); }
        // Normal retains Easy's standard grenade selection and tactical range.
        else if (WantsGrenade()) { powerups.UseMatchConsumable(true); }
        return;
    }
    if (WantsHealth()) { powerups.UseMatchConsumable(false); }
    if (WantsGrenade()) { powerups.UseMatchConsumable(true); }
}

void ZDeathmatchBot::Reset(float startX, float startY, float startFacing) {
    CBrotherAI::Reset(startX, startY, startFacing);
    m_target = 0; m_visible = false; m_moving = false;
    m_ageMs = 0; m_decisionMs = 0; m_memoryMs = 0; m_shopDelayMs = 0; m_swapMs = 0;
    m_goalX = startX; m_goalY = startY; m_moveX = 0; m_moveY = 0;
    m_tactic = Tactic::Search;
    m_powerupDecisionMs = 0;
    m_route.clear(); m_routeMs = 0;
}
void ZDeathmatchBot::Update(int deltaMs, CBrother &brother, ZBrotherAIWorld &world, float, float, float speedMultiplier) {
    auto &scene = static_cast<CLevel &>(world);
    if (deltaMs <= 0) { return; }
    UpdateForce(deltaMs, brother, world);
    m_moving = false;
    if (vitals.dead || vitals.stunMs > 0) { m_tactic = Tactic::Dead; brother.SetInput(false, false); return; }
    m_ageMs += deltaMs;
    m_powerupDecisionMs = std::max(0, m_powerupDecisionMs - deltaMs);
    m_routeMs -= deltaMs;
    m_decisionMs -= deltaMs; m_reactionMs -= deltaMs; m_swapMs -= deltaMs;
    m_memoryMs -= deltaMs; m_shopDelayMs = std::max(0, m_shopDelayMs - deltaMs);
    float targetX = 0, targetY = 0;
    const bool visible = scene.GetBrotherTarget(kPlayerCombatId, targetX, targetY) &&
        std::hypot(targetX - x, targetY - y) <= 700 && scene.HasLineOfFire(x, y, targetX, targetY);
    if (visible) {
        if (!m_visible) {
            if (m_memoryMs <= 0) { m_reactionMs = ReactionMs; }
            ++m_sightings;
        }
        m_lastX = targetX; m_lastY = targetY; m_memoryMs = MemoryMs;
    }
    m_visible = visible;
    m_target = 0;
    if (visible) { m_target = kPlayerCombatId; }
    m_distance = std::hypot(m_lastX - x, m_lastY - y);
    if (m_decisionMs <= 0) {
        m_decisionMs = DecisionMs;
        m_aimError = std::uniform_real_distribution<float>(-5, 5)(m_random);
        if (std::uniform_int_distribution<int>(0, 8)(m_random) == 0) { m_strafe = -m_strafe; }
        m_tactic = Tactic::Search;
        if (visible) { m_tactic = Tactic::Fight; m_goalX = m_lastX; m_goalY = m_lastY; }
        else if (m_memoryMs > 0) { m_goalX = m_lastX; m_goalY = m_lastY; }
        else if (std::hypot(m_goalX - x, m_goalY - y) < 60 || (m_moveX == 0 && m_moveY == 0)) {
            scene.FindMatchDestination(x, y, false, 0, 0, m_goalX, m_goalY, m_random());
        }
        float supplyX = 0, supplyY = 0;
        if (scene.FindMatchSupply(x, y, supplyX, supplyY) && (m_memoryMs <= 0 || std::hypot(supplyX - x, supplyY - y) < m_distance * 0.65f)) {
            m_tactic = Tactic::Supply; m_goalX = supplyX; m_goalY = supplyY;
        }
        if (visible && WantsHealth() && scene.FindMatchDestination(x, y, true, m_lastX, m_lastY, supplyX, supplyY)) {
            m_tactic = Tactic::Cover; m_goalX = supplyX; m_goalY = supplyY;
        }
        const unsigned slot = scene.GetBrotherWeaponSlot();
        if (visible && m_swapMs <= 0 && std::abs(m_distance - m_ranges[1 - slot]) + 45 < std::abs(m_distance - m_ranges[slot])) {
            m_weaponSwapRequested = true; m_swapMs = 2500;
        }
        float goalX = m_goalX, goalY = m_goalY;
        bool navigating = m_tactic != Tactic::Fight;
        if (visible && m_distance > m_ranges[slot] * 1.2f && !world.CanBrotherWalk(x, y, m_lastX, m_lastY)) { navigating = true; }
        if (navigating) {
            m_decisionMs = 50;
            if (m_routeMs <= 0 || std::hypot(m_goalX - m_routeGoalX, m_goalY - m_routeGoalY) > 80) {
                scene.FindMatchRoute(x, y, m_goalX, m_goalY, m_route);
                m_routeGoalX = m_goalX; m_routeGoalY = m_goalY; m_routeMs = 5000;
            }
            while (m_route.size() > 1 && world.CanBrotherWalk(x, y, m_route[1].x, m_route[1].y)) { m_route.erase(m_route.begin()); }
            if (!m_route.empty()) { goalX = m_route.front().x; goalY = m_route.front().y; }
            else { world.GetBrotherWaypoint(x, y, m_goalX, m_goalY, goalX, goalY); }
        }
        float best = std::numeric_limits<float>::max();
        m_moveX = 0; m_moveY = 0;
        m_navigating = navigating; m_waypointX = goalX; m_waypointY = goalY;
        float probe = InputSpeed * speedMultiplier * m_decisionMs / 1000.0f;
        const float waypointDistance = std::hypot(goalX - x, goalY - y);
        if (navigating) { probe = std::min(probe, waypointDistance); }
        for (unsigned direction = 0; direction <= 16; ++direction) {
            const float angle = direction * 22.5f * Radians;
            float dx = std::cos(angle), dy = std::sin(angle);
            if (direction == 16) {
                dx = 0; dy = 0;
                if (navigating && waypointDistance > 0) { dx = (goalX - x) / waypointDistance; dy = (goalY - y) / waypointDistance; }
            }
            const float nextX = x + dx * probe, nextY = y + dy * probe;
            const float shortProbe = std::min(5.0f, probe);
            if (!world.CanBrotherWalk(x, y, x + dx * shortProbe, y + dy * shortProbe)) { continue; }
            float score = std::hypot(goalX - nextX, goalY - nextY);
            if (!navigating) {
                score = std::abs(std::hypot(m_lastX - nextX, m_lastY - nextY) - m_ranges[slot]);
                const float cross = dx * (m_lastY - y) - dy * (m_lastX - x);
                score -= m_strafe * cross / std::max(1.0f, m_distance) * 35;
                // Keep a firing lane instead of circling back behind the same cover.
                if (!scene.HasLineOfFire(nextX, nextY, m_lastX, m_lastY)) { score += 200; }
            }
            if (!world.CanBrotherWalk(x, y, nextX, nextY)) { score += 250; }
            if (score < best) { best = score; m_moveX = dx; m_moveY = dy; }
        }
    }
    if (brother.CanMove()) {
        float step = InputSpeed * speedMultiplier * deltaMs / 1000.0f;
        if (m_navigating) { step = std::min(step, std::hypot(m_waypointX - x, m_waypointY - y)); }
        const float oldX = x, oldY = y;
        x += m_moveX * step; y += m_moveY * step;
        world.ResolveBrotherForce(oldX, oldY, x, y);
        m_moving = std::hypot(x - oldX, y - oldY) > 0.01f;
        if (!m_moving) { m_decisionMs = 0; }
    }
    const bool fire = m_shootingAllowed && visible && m_reactionMs <= 0 && brother.CanShoot() && !brother.HasGrenadeRequest(0);
    if (visible) { facing = std::atan2(m_lastY - y, m_lastX - x) / Radians + 90 + m_aimError; }
    else if (m_moving) { facing = std::atan2(m_moveY, m_moveX) / Radians + 90; }
    brother.SetInput(m_moving, fire);
}
