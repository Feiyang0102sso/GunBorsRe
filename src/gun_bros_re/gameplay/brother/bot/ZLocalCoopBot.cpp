#include "gun_bros_re/gameplay/brother/bot/ZLocalPVPBot.h"
#include "gun_bros_re/ui/CPowerUpSelector.h"
#include "gun_bros_re/gameplay/CBullet.h"
/** Test peer input policy. Shared CBrother/CGun/Flow still execute all actions.
 * This policy is a Windows replacement for player input, not original game AI.
 */
#include "gun_bros_re/gameplay/brother/bot/ZLocalCoopBot.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kRadians = 3.14159265f / 180;
constexpr float kSpeed = 220; // CPlayer movement speed, same as the local input host.
constexpr float kSearchRadius = 700;
constexpr float kPreferredRange = 230;
constexpr float kLookAhead = 100;
constexpr int kReactionMs = 180;
constexpr int kSwapMinimumMs = 3000, kSwapMaximumMs = 6000;
constexpr int kRoamMs = 2500;
constexpr int kSteeringMs = 180; // Host policy: retain an input briefly instead of frame-wise zigzagging.
constexpr float kRangeTolerance = 40;
}

void ZLocalCoopBot::Reset(float startX, float startY, float startFacing) {
    CBrotherAI::Reset(startX, startY, startFacing);
    m_target = 0;
    m_reactionMs = 0;
    m_moving = false;
    m_targetCount = 0;
    m_swapMs = kSwapMinimumMs;
    m_roamMs = 0;
    m_roamX = x;
    m_roamY = y;
    m_powerupMs = 0;
    m_shopMs = 0;
    m_lastShopAttempt = UINT32_MAX;
    m_steeringMs = 0;
    m_moveX = m_moveY = 0;
    m_rescuing = false;
    m_random.seed(0xB07);
}

void ZLocalCoopBot::AdvanceActions(unsigned deltaMs) {
    m_powerupMs += deltaMs;
    m_shopMs += deltaMs;
}
bool ZLocalCoopBot::TakePowerupRequest() {
    if (m_powerupMs < 9000) { return false; }
    m_powerupMs = 0;
    return true;
}
bool ZLocalCoopBot::TakeShopRequest() {
    if (m_shopMs < 45000) { return false; }
    m_shopMs = 0;
    return true;
}

unsigned ZLocalCoopBot::ShopSelection(unsigned elapsedMs) const { return elapsedMs / 850; }

bool ZLocalCoopBot::ShouldBuyShopItem(unsigned elapsedMs, unsigned ownedCount) {
    if (elapsedMs < 400) { m_lastShopAttempt = UINT32_MAX; return false; }
    const unsigned selection = ShopSelection(elapsedMs);
    if (elapsedMs % 850 < 400 || selection == m_lastShopAttempt) { return false; }
    m_lastShopAttempt = selection;
    return ownedCount == 0;
}

void ZLocalCoopBot::Update(int deltaMs, CBrother &brother, ZBrotherAIWorld &world,
    float playerX, float playerY, float speedMultiplier) {
    if (deltaMs <= 0) { return; }
    UpdateForce(deltaMs, brother, world);
    m_moving = false;
    if (vitals.dead || vitals.stunMs > 0) { brother.SetInput(false, false); return; }
    m_swapMs -= deltaMs;
    if (m_swapMs <= 0 && m_shootingAllowed) {
        m_weaponSwapRequested = true;
        m_swapMs = std::uniform_int_distribution<int>(kSwapMinimumMs, kSwapMaximumMs)(m_random);
    }
    float targetX = 0, targetY = 0;
    if (m_target != 0 && (!world.GetBrotherTarget(m_target, targetX, targetY) ||
        std::hypot(targetX - x, targetY - y) > kSearchRadius)) { m_target = 0; }
    if (m_target == 0 && m_shootingAllowed) {
        m_target = world.FindBrotherTarget(x, y, kSearchRadius);
        if (m_target != 0) {
            world.GetBrotherTarget(m_target, targetX, targetY);
            m_reactionMs = kReactionMs;
            ++m_targetCount;
        }
    }
    m_reactionMs = std::max(0, m_reactionMs - deltaMs);
    m_roamMs -= deltaMs;
    if (m_roamMs <= 0) {
        const float angle = std::uniform_real_distribution<float>(0, 360)(m_random) * kRadians;
        m_roamX = x + std::cos(angle) * kPreferredRange;
        m_roamY = y + std::sin(angle) * kPreferredRange;
        m_roamMs = kRoamMs;
    }
    // A living player is never a movement destination. Approach only to revive.
    const bool rescue = world.IsPlayerDown();
    m_steeringMs -= deltaMs;
    if (rescue != m_rescuing) { m_steeringMs = 0; }
    m_rescuing = rescue;
    const float movementStep = kSpeed * speedMultiplier * deltaMs * 0.001f;
    if (!world.CanBrotherWalk(x, y, x + m_moveX * movementStep, y + m_moveY * movementStep)) { m_steeringMs = 0; }
    float goalX = m_roamX, goalY = m_roamY;
    if (m_target != 0) { goalX = targetX; goalY = targetY; }
    if (rescue) { goalX = playerX; goalY = playerY; }
    float waypointX = goalX, waypointY = goalY;
    if (rescue && world.GetBrotherWaypoint(x, y, goalX, goalY, waypointX, waypointY)) {
        goalX = waypointX; goalY = waypointY;
    }
    float moveX = m_moveX, moveY = m_moveY;
    if (m_steeringMs <= 0 || rescue) {
        const auto threats = world.GetBrotherThreats();
        float bestScore = std::numeric_limits<float>::max();
        moveX = 0; moveY = 0;
        // Score stationary plus sixteen walkable directions against every nearby
        // enemy. A close crowd outweighs the firing-range or rescue preference.
        for (unsigned candidate = 0; candidate <= 16; ++candidate) {
            float dx = 0, dy = 0;
            if (candidate != 0) {
                const float angle = (candidate - 1) * 22.5f * kRadians;
                dx = std::cos(angle); dy = std::sin(angle);
            }
            const float nextX = x + dx * kLookAhead, nextY = y + dy * kLookAhead;
            // Narrow passages can reject every 100-unit probe despite having a
            // valid short step. Require the actual step, then penalize blocked lookahead.
            const float step = kSpeed * speedMultiplier * deltaMs * 0.001f;
            if (!world.CanBrotherWalk(x, y, x + dx * step, y + dy * step)) { continue; }
            float preferred = 0;
            if (m_target != 0 && !rescue) { preferred = kPreferredRange; }
            if (rescue && goalX == playerX && goalY == playerY) { preferred = 70; }
            float score = std::abs(std::hypot(goalX - nextX, goalY - nextY) - preferred);
            // A safe firing band permits standing still. Exact-distance scoring
            // alternated left/right probes while pursuing an already stationary target.
            if (!rescue) { score = std::max(0.0f, score - kRangeTolerance); }
            if (rescue) {
                // A navigation portal must be crossed. Scoring a 100-unit probe
                // with a 70-unit stand-off made the peer stop before that portal.
                score = std::abs(std::hypot(goalX - x - dx * step, goalY - y - dy * step) - preferred);
            } else if (!world.CanBrotherWalk(x, y, nextX, nextY)) { score += 250; }
            for (const auto &threat : threats) {
                const float clearance = std::max(1.0f, std::hypot(threat.x - nextX, threat.y - nextY) - threat.radius);
                if (clearance < 180) { score += 120000 / (clearance * clearance); }
            }
            if (score < bestScore) { bestScore = score; moveX = dx; moveY = dy; }
        }
        m_moveX = moveX; m_moveY = moveY;
        m_steeringMs = kSteeringMs;
    }
    if (brother.CanMove() && (moveX != 0 || moveY != 0)) {
        const float step = kSpeed * speedMultiplier * deltaMs * 0.001f;
        const float oldX = x, oldY = y;
        x += moveX * step; y += moveY * step;
        world.ResolveBrotherForce(oldX, oldY, x, y);
        m_moving = x != oldX || y != oldY;
    }
    const bool shooting = m_shootingAllowed && brother.CanShoot() && m_target != 0 && m_reactionMs == 0;
    if (shooting) { facing = std::atan2(targetY - y, targetX - x) / kRadians + 90; }
    else if (m_moving) { facing = std::atan2(y - previousY, x - previousX) / kRadians + 90; }
    brother.SetInput(m_moving, shooting);
}
bool ZLocalCoopBot::UseAnyPowerup(CPowerUpSelector &selector, std::uint32_t &choice, bool grantTestCharge) {
    if (selector.m_resources.m_powerups.empty()) { return false; }
    // Input policy only: every attempt still enters the original CanUse/Use.
    choice = choice * 1664525u + 1013904223u;
    for (unsigned offset = 0; offset < selector.m_resources.m_powerups.size(); ++offset) {
        const unsigned index = (choice % selector.m_resources.m_powerups.size() + offset) % selector.m_resources.m_powerups.size();
        if (!selector.Select(index)) { continue; }
        if (!grantTestCharge && selector.m_level->IsLocalLive() && !CanUseSelectedPowerup(selector)) { continue; }
        const auto &ref = selector.m_resources.m_powerups[index].resource;
        bool granted = false;
        if (grantTestCharge && selector.m_owner == kBrotherCombatId && selector.m_level->HasLocalBot() && selector.GetCount() == 0) {
            selector.m_profile->AddPowerup(ref, 1); granted = true;
        }
        if (selector.UseSelected()) { return true; }
        // Health and other selector-only actions still pass their original exports.
        if (ZLocalPVPBot::HasUnlimitedInventory(selector) && selector.UseSelected(true)) { return true; }
        if (granted) { selector.m_profile->ConsumePowerup(ref); }
    }
    return false;
}

bool ZLocalCoopBot::CanUseSelectedPowerup(const CPowerUpSelector &selector) {
    const ZPowerupEntry *entry = selector.GetSelected();
    if (entry == nullptr || selector.m_vitals->dead || selector.m_level->IsRescuePending()) { return false; }
    bool airstrike = false, grenade = false;
    // POWERUP script resources: 253 denotes a Movie dependency. Retail
    // offensive Movies are air strikes; afterDeath is a separate action.
    // powerup_template.bt; CPowerup::FunctionResolver native 1 :188267.
    // This remains a local tactical heuristic, never a player availability gate.
    for (const auto &ref : entry->data.script.GetResources()) {
        if (ref.sectionOrType == 253 && entry->data.field112 == 0) { airstrike = true; }
        if (ref.sectionOrType != static_cast<unsigned>(ZGameSection::Bullet) - 1) { continue; }
        std::vector<std::uint8_t> bytes;
        if (!selector.m_resources.m_tables->ReadSectionResource(ref.packHash, ZGameSection::Bullet, ref.resourceId, bytes)) { return false; }
        CArrayInputStream input(bytes);
        CBullet::Template bullet;
        if (!bullet.Init(input) || input.Available() != 0) {
            std::printf("[powerup] invalid bot projectile resource=%08x:%u\n", ref.packHash, ref.resourceId);
            return false;
        }
        // CBullet::IsGrenade :60370 uses flag bit 4, not the powerup ID.
        if ((bullet.GetFlags() & 16) != 0) { grenade = true; }
    }
    if (!airstrike && !grenade) { return true; }
    unsigned alive = 0, nearby = 0;
    float x = 0, y = 0;
    selector.m_level->ActorPosition(selector.m_owner, x, y);
    for (const auto &actor : selector.m_level->GetEnemies()) {
        const auto &enemy = actor->combat;
        if (enemy.dead || enemy.removed || !enemy.enabled || enemy.health <= 0) { continue; }
        ++alive;
        const float dx = enemy.x - x, dy = enemy.y - y;
        if (dx * dx + dy * dy <= BotGrenadeRadius * BotGrenadeRadius) { ++nearby; }
    }
    if (airstrike && alive <= 10) { return false; }
    if (grenade && nearby < 2) { return false; }
    return true;
}
