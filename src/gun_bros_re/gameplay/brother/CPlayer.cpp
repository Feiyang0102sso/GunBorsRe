#define NOMINMAX
#include "gun_bros_re/gameplay/brother/CPlayer.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/gameplay/collision/CCollisionData.h"
#include "gun_bros_re/gameplay/enemy/CLevelObjectPool.h"
#include "gun_bros_re/gameplay/map/CMap.h"
#include "gun_bros_re/gameplay/collision/Collision.h"
#include "gun_bros_re/gameplay/brother/CBrotherAI.h"
#include "gun_bros_re/data/CFriendPowerManager.h"
#include <algorithm>
#include <cmath>

// Existing desktop normalized-stick speed; native analog acceleration remains
// different (CPlayer::UpdateMovement :101384). Preserve it during this refactor.
constexpr float kDesktopPlayerSpeed = 220;

CPlayer::CPlayer(CBrother &model, CBrother::Vitals &vitals)
    : m_model(&model), m_vitals(&vitals) {}

void CPlayer::BindActor(CBrother &model, CBrother::Vitals &vitals) {
    m_model = &model;
    m_vitals = &vitals;
}

void CPlayer::BindProgress(CPlayerProgress *progress) {
    m_progress = progress;
    if (progress != nullptr && m_vitals != nullptr) { m_vitals->maximum = progress->GetHealth(); }
}

bool CPlayer::AddExperience(unsigned amount, bool updateHealth) {
    if (m_progress == nullptr) { return false; }
    if (m_vitals == nullptr) { return false; }
    const float fraction = m_vitals->health / m_vitals->maximum;
    const bool leveled = m_progress->AddExperience(amount);
    // CPlayer::AddExperience :101185 preserves the current health fraction.
    if (leveled && updateHealth) {
        m_vitals->maximum = m_progress->GetHealth();
        m_vitals->health = m_vitals->maximum * fraction;
    }
    return leveled;
}

std::uint64_t CPlayer::GetExperience() const {
    if (m_progress == nullptr) { return 0; }
    return m_progress->GetExperience();
}

std::uint64_t CPlayer::AddXplodium(unsigned amount, unsigned percent) {
    // CPlayer::AddXplodium :101116 keeps hundredths between individual grants.
    // Rounding every small pickup separately loses the later-wave bonus.
    const std::uint64_t scaled = static_cast<std::uint64_t>(amount) * percent + m_xplodiumRemainder;
    const std::uint64_t earned = scaled / 100;
    m_xplodium += earned;
    m_xplodiumRemainder = static_cast<unsigned>(scaled % 100);
    return earned;
}

void CPlayer::AddHealth(unsigned amount) {
    if (m_vitals != nullptr && !m_vitals->dead) {
        m_vitals->health = std::min(m_vitals->maximum, m_vitals->health + amount);
    }
}

void CPlayer::BindLevel(CMap &map, const CCollisionData &collision,
    CLevelObjectPool &objects, float collisionRadius) {
    m_map = &map;
    m_collision = &collision;
    m_objects = &objects;
    m_collisionRadius = collisionRadius;
}

void CPlayer::BeginMovement() {
    previousX = x;
    previousY = y;
}

bool CPlayer::UpdateMovement(int deltaMs, float moveX, float moveY) {
    if (m_model == nullptr || m_vitals == nullptr || m_vitals->dead || m_vitals->stunMs != 0) { return false; }
    const float length = std::hypot(moveX, moveY);
    if (length > 0 && m_model->CanMove()) {
        // Native modifier order: CPlayer::UpdateMovement :101384; PLAYER BT
        // supplies the brother template, equipment supplies armor/gun values.
        const float speed = kDesktopPlayerSpeed * m_model->GetArmorMultiplier(2) *
            CFriendPowerManager::Multiplier(m_model->friendCount, 2) * m_model->GetFrenzyMultiplier(2) *
            m_model->ActiveWeapon().GetMasterySpeedMod() * 0.01f;
        x += moveX / length * speed * deltaMs * 0.001f;
        y += moveY / length * speed * deltaMs * 0.001f;
    }
    return length > 0;
}

void CPlayer::UpdateShooting(int deltaMs, bool moving, bool shoot, ZBrotherAIWorld &world) {
    if (m_model == nullptr || m_vitals == nullptr || m_vitals->dead || m_vitals->stunMs != 0) { return; }
    // CPlayer::UpdateShooting :101312 only targets while the fire stick is
    // active. Desktop mouse-held fire supplies that intent; idle never fires.
    if (m_model->IsAutoFire()) {
        if (shoot) { shoot = m_autoAim.Update(deltaMs, x, y, facing, world); }
        else { m_autoAim.ClearTarget(facing); }
    } else if (m_autoAim.GetTarget() != 0) { m_autoAim.ClearTarget(facing); }
    m_model->SetInput(moving, shoot);
}

void CPlayer::ApplyKnockback(int deltaMs, float seconds) {
    if (forceMs > 0 && m_vitals != nullptr && !m_vitals->dead) {
        x += forceX * seconds;
        y += forceY * seconds;
        forceMs = std::max(0, forceMs - deltaMs);
    }
}

void CPlayer::Move() {
    if (m_map == nullptr || m_collision == nullptr || m_objects == nullptr) { return; }
    const CLayerCamera::Rectangle bounds = m_map->GetCameraExtent();
    // CPlayer::Move :100691-100862: bounds, enemy bodies, then map/prop edges.
    x = std::clamp(x, bounds.x + m_collisionRadius,
        bounds.x + bounds.width - m_collisionRadius);
    y = std::clamp(y, bounds.y + m_collisionRadius,
        bounds.y + bounds.height - m_collisionRadius);
    if (m_model == nullptr) { return; }
    const CBrother &player = (*m_model);
    if (!player.CanPassEnemies()) {
        for (const auto &actor : m_objects->GetEnemies()) {
            const CEnemy &enemy = *actor;
            if (!enemy.CanCollideWithPlayer()) { continue; }
            const CEnemy::CombatState &state = enemy.combat;
            float offsetX, offsetY;
            enemy.GetRotationOffset(actor->data->gameScale, offsetX, offsetY);
            const ZCollisionPoint enemyPrevious(state.previousX + offsetX, state.previousY + offsetY);
            const ZCollisionPoint enemyCurrent(state.x + offsetX, state.y + offsetY);
            const float moveX = x - previousX, moveY = y - previousY;
            const float towardEnemy = moveX * (enemyPrevious.x - x) + moveY * (enemyPrevious.y - y);
            if (towardEnemy <= 0) { continue; }
            float fraction = 0;
            // Native 0.8 is a body allowance, not a resource radius or model scale.
            if (Collision::CircleCircle({previousX, previousY}, {x, y}, player.GetRadius(),
                enemyPrevious, enemyCurrent, enemy.GetPart(0).radius * 0.8f, fraction)) {
                x = previousX + moveX * fraction;
                y = previousY + moveY * fraction;
            }
        }
    }
    const ZCollisionPoint position = m_collision->ResolveCircleMovement(
        ZCollisionPoint(previousX, previousY),
        ZCollisionPoint(x - previousX, y - previousY), m_collisionRadius);
    x = position.x;
    y = position.y;
}

void CPlayer::Update(int deltaMs, float moveX, float moveY, bool shoot,
    ZBrotherAIWorld &world, bool moveActor) {
    BeginMovement();
    const bool moving = UpdateMovement(deltaMs, moveX, moveY);
    UpdateShooting(deltaMs, moving, shoot, world);
    const float forceSeconds = m_model->GetKnockbackStepSeconds(deltaMs);
    m_model->Update(deltaMs);
    ApplyKnockback(deltaMs, forceSeconds);
    if (moveActor) { Move(); }
}

unsigned CPlayer::CollectItem(ZPackTables &tables, const GameObjectRef &ref, CProfileManager *profile) {
    if (profile == nullptr) { return 0; }
    unsigned failures = 0;
    std::vector<std::uint8_t> payload;
    if (!tables.ReadSectionResource(ref.packHash, ZGameSection::StoreItem, ref.localIndex, payload)) { return 1; }
    CStoreItem item;
    CArrayInputStream stream(payload);
    if (!item.Init(stream)) { return 1; }
    // CollectItem uses AcquireItem(..., free=true). No currency is deducted.
    // Preserve desktop Grant/AddPowerup rules; full store award policy is not restored here.
    for (const GameObjectTypeRef &object : item.objects) {
        if (object.type == 17) { profile->AddPowerup(object.object, 1); }
        else if (object.type == 2 || object.type == 6) { profile->Grant(object.type, object.object); }
        else { ++failures; std::printf("[pickup] unsupported store object type=%u\n", object.type); }
    }
    return failures;
}
