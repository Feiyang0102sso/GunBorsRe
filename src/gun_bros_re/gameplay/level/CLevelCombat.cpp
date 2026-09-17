/** @file CLevelCombat.cpp
 * @brief CLevel active combat update and enemy action dispatch.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/CFlock.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
#include "gun_bros_re/debug/PerformanceProbe.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kRadians = 3.14159265f / 180;
constexpr int kCorpseLimitMs = 10000;
}

void CLevel::Actions(ZCombatEnemy &actor) {
    CEnemy &enemy = actor.model.enemy;
    const CEnemy::CombatState &state = enemy.combat;
    for (const CEnemy::Action &action : enemy.TakeActions()) {
        float x = state.x, y = state.y, z = 0, direction = state.facing - 90;
        // Death effects still use the final pose, even though the actor is no
        // longer a valid continuous beam/effect anchor.
        if (!state.dead) { Anchor(state.id, action.part, action.node, x, y, z, direction); }
        int ownerType = 1;
        if (state.targetType == 2) { ownerType = 0; }
        if (action.kind == CEnemy::Action::Kind::LevelEvent) {
            QueueLevelEvent(static_cast<std::uint8_t>(action.slot));
        } else if (action.kind == CEnemy::Action::Kind::Teleported) {
            QueueEnemyTeleport(actor.objectId, state.templateRef);
        } else if (action.kind == CEnemy::Action::Kind::Shake) {
            if (m_map != nullptr) { m_map->GetCamera().Shake(action.durationMs); }
        } else if (action.kind == CEnemy::Action::Kind::TurretActive) {
            // CEnemy native 71 :72744 selects the local player when offline.
            ZPlayerModel *owner = m_playerModel;
            if (state.summoner == kBrotherCombatId && m_brotherModel != nullptr) { owner = m_brotherModel; }
            owner->weapon->brother.SetTurretIsActive(action.slot != 0);
            std::printf("[turret] actor=%llu active=%d\n", static_cast<unsigned long long>(state.id), action.slot != 0);
        } else if (action.kind == CEnemy::Action::Kind::SpawnPickup) {
            QueuePickupSpawn(action.resource, x, y);
        } else if (action.kind == CEnemy::Action::Kind::Bullet) {
            if (action.slot != 1) { direction = action.direction - 90; }
            SpawnProjectile(action.resource, x, y, z, direction,
                action.speed, state.id, ownerType, action.part, action.node);
        } else if (action.kind == CEnemy::Action::Kind::Stun) {
            if (ownerType == 1 && std::hypot(m_actor.x - x, m_actor.y - y) < action.radius) {
                m_playerModel->weapon->brother.Stun(action.durationMs);
            }
            if (ownerType == 1 && m_brother != nullptr && std::hypot(m_brother->x - x, m_brother->y - y) < action.radius) {
                m_brotherModel->weapon->brother.Stun(action.durationMs);
            }
        } else if (action.kind == CEnemy::Action::Kind::CollisionResolved) {
            // Record assistance only after the enemy Flow accepts the collision.
            if (m_localLive && (action.result == ZHitResult::Hit || action.result == ZHitResult::Killed) &&
                !action.resource.IsNull() && action.slot >= 0 && action.slot < 2) {
                if (action.owner == kPlayerCombatId) { actor.assistMask[0] |= 1u << action.slot; }
                if (action.owner == kBrotherCombatId) { actor.assistMask[1] |= 1u << action.slot; }
            }
            ResolveHit(action.projectile, action.result);
        } else if (action.kind == CEnemy::Action::Kind::RemoveBullet) {
            RemoveOldestProjectile(state.id);
        } else if (action.kind == CEnemy::Action::Kind::Broadcast) {
            for (auto &other : m_objects.GetEnemies()) {
                if (other.get() != &actor && !other->model.enemy.combat.dead) {
                    other->model.enemy.TriggerEvent(static_cast<std::uint8_t>(action.slot));
                }
            }
        } else if (action.kind == CEnemy::Action::Kind::Splash || action.kind == CEnemy::Action::Kind::SpawnEnemy) {
            ZCombatHit hit;
            hit.owner = state.id;
            hit.ownerType = ownerType;
            hit.x = x; hit.y = y; hit.direction = direction;
            hit.damage = action.damage * GetDamageMultiplier(state.id);
            if (action.kind == CEnemy::Action::Kind::Splash) { Splash(hit, action.radius, 360, action.force, action.durationMs); }
            else { SpawnFromProjectile(action.resource, hit); }
        } else {
            ZGunCue cue;
            cue.resource = action.resource;
            cue.effectGroup = action.effectGroup;
            cue.effectScale = action.effectScale;
            cue.alignEffect = action.alignEffect;
            cue.linkedEnemyEffect = action.kind == CEnemy::Action::Kind::LinkedEffect;
            cue.kind = ZGunCue::Kind::Effect;
            if (action.kind == CEnemy::Action::Kind::Sound) { cue.kind = ZGunCue::Kind::Sound; }
            if (action.kind == CEnemy::Action::Kind::LoopSound) { cue.kind = ZGunCue::Kind::LoopSound; }
            if (action.kind == CEnemy::Action::Kind::StopSound) { cue.kind = ZGunCue::Kind::StopSound; }
            if (action.kind == CEnemy::Action::Kind::LinkedEffect) { cue.kind = ZGunCue::Kind::Trail; }
            if (action.kind == CEnemy::Action::Kind::StopEffect) { cue.kind = ZGunCue::Kind::StopTrail; }
            if (action.kind == CEnemy::Action::Kind::Shake || action.kind == CEnemy::Action::Kind::Reward) { continue; }
            Emit(cue, x, y, z, direction, state.id, action.slot, action.part, action.node);
        }
    }
}

void CLevel::Update(int deltaMs, float moveX, float moveY, bool shoot) {
    // Equipment changes create a new script host; reconnect before input.
    m_playerModel->weapon->brother.SetLevelContext(GetScriptLevel());
    if (deltaMs <= 0) { return; }
    if (IsDeathmatch() && (m_matchShopping[0] || IsMatchSpawnPending(0))) { moveX = 0; moveY = 0; shoot = false; }
    BeginAudioFrame();
    UpdateExperienceTexts(deltaMs);
    BeginCombatFrame();
    m_actor.Update(deltaMs, moveX, moveY, shoot, *this, !IsMatchSpawnPending(0));
    if (IsDeathmatch() && m_playerModel->weapon->brother.TakeWeaponSwap() &&
        !FinishMatchWeaponSwap(0)) {
        m_objects.RecordInvalidSpawn();
    }
    if (m_brotherModel != nullptr) {
        m_brotherModel->weapon->brother.SetLevelContext(GetScriptLevel());
        PerformanceProbe::Scope timing(PerformanceProbe::counters.brotherMs);
        m_brother->SetShootingAllowed(CanBrotherShoot());
        // Retail DM disables the cooperative AI. This peer supplies player input.
        if (IsDeathmatch()) { m_brother->SetShootingAllowed(CanPlayerShoot()); }
        float speedMultiplier = PlayerArmorMultiplier(*m_brotherModel, 2) * CFriendPowerManager::Multiplier(m_brotherModel->friendCount, 2) * m_brotherModel->weapon->brother.GetFrenzyMultiplier(2);
        // Live substitutes player input, so CPlayer::UpdateMovement :101437
        // also applies the equipped gun's native mastery movement modifier.
        if (m_localLive || IsDeathmatch()) { speedMultiplier *= m_brotherModel->ActiveWeapon().gun.GetMasterySpeedMod() * 0.01f; }
        if (IsDeathmatch() && (m_matchShopping[1] || IsMatchSpawnPending(1))) { m_brotherModel->weapon->brother.SetInput(false, false); }
        else { m_brother->Update(deltaMs, m_brotherModel->weapon->brother, *this, m_actor.x, m_actor.y, speedMultiplier); }
        if (m_brother->TakeWeaponSwapRequest()) { RequestBrotherWeaponSwap(); }
        AdvancePlayer(*m_brotherModel, deltaMs);
        if (m_brotherModel->weapon->brother.TakeWeaponSwap() && !SwapBrotherWeapon()) {
            m_objects.RecordInvalidSpawn();
        }
    }
    UpdateLocalRevive(deltaMs);
    // CLevel::Update :121318 refreshes CFlock before object movement.
    // AddObject/RemoveObject maintain membership, including unremoved corpses.
    m_flockEnemies.clear();
    for (auto &actor : m_objects.GetEnemies()) {
        auto &state = actor->model.enemy.combat;
        if (state.enabled && !state.removed) { m_flockEnemies.push_back(&state); }
    }
    {
        PerformanceProbe::Scope timing(PerformanceProbe::counters.flockMs);
        if (PerformanceProbe::disableFlock) {
            for (auto *state : m_flockEnemies) { state->flockX = 0; state->flockY = 0; }
        } else
        { CFlock::RefreshFlock(m_flockEnemies); }
    }
    for (auto &actor : m_objects.GetEnemies()) {
        CEnemy &enemy = actor->model.enemy;
        CEnemy::CombatState &state = enemy.combat;
        if (!state.enabled || state.removed) { continue; }
        PerformanceProbe::Scope timing(PerformanceProbe::counters.enemyMs);
        int enemyDeltaMs = deltaMs;
        // TransformObjectElapseMS :114279 leaves dead actors and player shots
        // at normal speed; live enemies use the script's Q8 time multiplier.
        if (!state.dead) {
            enemyDeltaMs = std::max(1, static_cast<int>(std::lround(deltaMs * GetObjectTimeScale())));
        }
        SelectTarget(*actor);
        UpdateNavigation(*actor, enemyDeltaMs);
        enemy.Update(enemyDeltaMs);
        for (std::uint32_t part = 0; part < enemy.GetPartCount(); ++part) {
            for (const ZMoveSoundRef &sound : enemy.GetPart(part).controller.TakeSounds()) {
                GameObjectRef resource;
                resource.packHash = sound.packHash;
                resource.localIndex = sound.localIndex;
                PlayMoveSound(resource);
            }
        }
        ResolveMovement(state.previousX, state.previousY, state.x, state.y,
            enemy.GetPart(0).radius * m_cameraScale, false);
        actor->contactTimer = std::max(0, actor->contactTimer - enemyDeltaMs);
        actor->brotherContactTimer = std::max(0, actor->brotherContactTimer - enemyDeltaMs);
        // TestCollisions :73178-73194 uses both location histories and the full
        // 22-unit brother radius, not the smaller map-wall resolution radius.
        float contactFraction = 0;
        if (m_brother != nullptr && !m_brother->vitals.dead && !state.dead &&
            state.variables[16] != 1 && state.targetType != 2 && state.variables[12] > 0 &&
            state.variables[13] > 0 && actor->brotherContactTimer == 0 &&
            CombatGeometry::CircleCircle({m_brother->previousX, m_brother->previousY},
                {m_brother->x, m_brother->y}, m_brotherModel->weapon->brother.GetRadius(),
                {state.previousX, state.previousY}, {state.x, state.y}, enemy.GetPart(0).radius, contactFraction)) {
            ZCombatHit contact;
            contact.owner = state.id;
            contact.ownerType = 1;
            contact.damage = state.variables[17] * GetDamageMultiplier(state.id);
            ApplyHit(kBrotherCombatId, contact);
            const float angle = (state.facing - 90) * kRadians;
            ApplyBrotherForce(kBrotherCombatId, std::cos(angle) * state.variables[12],
                std::sin(angle) * state.variables[12], state.variables[13]);
            actor->brotherContactTimer = state.variables[13];
            enemy.TriggerEvent(8);
        }
        if (!state.dead && state.variables[16] != 1 && state.targetType != 2 && !m_vitals->dead &&
            state.variables[12] > 0 && state.variables[13] > 0 &&
            actor->contactTimer == 0 && CombatGeometry::CircleCircle({m_actor.previousX, m_actor.previousY},
                {m_actor.x, m_actor.y}, m_playerModel->weapon->brother.GetRadius(),
                {state.previousX, state.previousY}, {state.x, state.y}, enemy.GetPart(0).radius, contactFraction)) {
            if (state.variables[17] > 0) {
                ZCombatHit contact;
                contact.owner = state.id;
                contact.ownerType = 1;
                contact.damage = state.variables[17] * GetDamageMultiplier(state.id);
                ApplyHit(kPlayerCombatId, contact);
            }
            const float angle = (state.facing - 90) * kRadians;
            ApplyBrotherForce(kPlayerCombatId, std::cos(angle) * state.variables[12],
                std::sin(angle) * state.variables[12], state.variables[13]);
            actor->contactTimer = state.variables[13];
            enemy.TriggerEvent(8);
        }
        Actions(*actor);
    }
    float matrix[16];
    if (m_brotherModel != nullptr) {
        BrotherMatrix(matrix);
        EmitBrother(*m_brotherModel, matrix, m_brother->facing, kBrotherCombatId, m_weaponCollision);
    }
    PlayerMatrix(matrix);
    {
        PerformanceProbe::Scope timing(PerformanceProbe::counters.effectsMs);
        Update(*m_playerModel, matrix, m_actor.facing, deltaMs, m_weaponCollision);
    }
    for (auto &actor : m_objects.GetEnemies()) {
        Actions(*actor);
        CEnemy::CombatState &state = actor->model.enemy.combat;
        if (state.dead) {
            actor->corpseMs += deltaMs;
            if (actor->corpseMs > kCorpseLimitMs) { state.removed = true; }
        }
    }
    FinishSpawns();
    // Accumulate completed actor statistics before erasing removed instances.
    auto &enemyObjects = m_objects.GetEnemies();
    for (std::size_t i = 0; i < enemyObjects.size();) {
        CEnemy::CombatState &state = enemyObjects[i]->model.enemy.combat;
        if (state.dead && !enemyObjects[i]->deathReported) {
            RewardEnemy(*enemyObjects[i]);
            GameObjectRef enemy;
            enemy.packHash = enemyObjects[i]->data->packHash;
            enemy.localIndex = static_cast<std::uint8_t>(enemyObjects[i]->data->ordinal);
            OnEnemyKilled(enemyObjects[i]->objectId, enemy);
            enemyObjects[i]->deathReported = true;
        }
        if (state.hitFlash > 0) { lastDamage = state.lastDamage; }
        if (state.removed) {
            RetireOwner(state.id);
            kills += state.deathCount;
            hits += state.hitCount;
            damageDealt += state.totalDamage;
            m_objects.ReleaseEnemy(i);
        } else { ++i; }
    }
}

bool CLevel::IsTeamDeathComplete() const {
    if (IsDeathmatch()) { return false; }
    if (!m_vitals->dead || !m_vitals->deathAnimationComplete) { return false; }
    if (!m_localLive || m_brother == nullptr) { return true; }
    if (NeedsDeathChoice(0) || NeedsDeathChoice(1)) { return false; }
    return m_brother->vitals.dead && m_brother->vitals.deathAnimationComplete;
}

bool CLevel::NeedsDeathChoice(unsigned peer) const {
    if (!m_localLive || peer > 1 || !m_afterDeathAvailable[peer] || m_brother == nullptr) { return false; }
    const ZPlayerVitals *vitals = m_vitals;
    if (peer == 1) { vitals = &m_brother->vitals; }
    return vitals->dead && vitals->deaths > m_deathChoiceHandled[peer];
}

void CLevel::FinishDeathChoice(unsigned peer) {
    if (peer == 0) { m_deathChoiceHandled[0] = m_vitals->deaths; }
    if (peer == 1 && m_brother != nullptr) { m_deathChoiceHandled[1] = m_brother->vitals.deaths; }
}

bool CLevel::ReviveActor(ZCombatId actor, unsigned reason) {
    if (actor == kPlayerCombatId) { return m_playerModel->weapon->brother.OnRevive(reason); }
    if (actor == kBrotherCombatId && m_brotherModel != nullptr) { return m_brotherModel->weapon->brother.OnRevive(reason); }
    return false;
}

bool CLevel::KillTestBot() {
    if (!HasLocalBot()) { return false; }
    m_localBotReviveRequested = false;
    return m_brotherModel->weapon->brother.StartDeath();
}

bool CLevel::ReviveTestBot() {
    if (IsDeathmatch()) { return false; } // Match respawn owns the life boundary.
    if (!HasLocalBot() || !m_brother->vitals.dead) { return false; }
    m_localBotReviveRequested = true;
    return true;
}

void CLevel::ActorPosition(ZCombatId actor, float &x, float &y) const {
    x = m_actor.x; y = m_actor.y;
    if (actor == kBrotherCombatId && m_brother != nullptr) { x = m_brother->x; y = m_brother->y; }
}

std::vector<ZBrotherAIWorld::Threat> CLevel::GetBrotherThreats() const {
    std::vector<ZBrotherAIWorld::Threat> result;
    for (const auto &actor : m_objects.GetEnemies()) {
        const auto &enemy = actor->model.enemy;
        if (!enemy.combat.enabled || !enemy.combat.targetable) { continue; }
        result.push_back({enemy.combat.x, enemy.combat.y, enemy.GetPart(0).radius + m_playerRadius});
    }
    return result;
}

void CLevel::UpdateLocalRevive(int deltaMs) {
    if (IsDeathmatch()) { return; }
    if (m_localLive && m_brother != nullptr) {
        const ZPlayerVitals *vitals[] = {m_vitals, &m_brother->vitals};
        for (unsigned peer = 0; peer < 2; ++peer) {
            auto &stats = m_multiplayer[peer];
            const unsigned deaths = vitals[peer]->deaths;
            stats.wave.deaths += deaths - stats.total.deaths;
            stats.total.deaths = deaths;
            if (vitals[peer]->hits != stats.streakHits) { stats.streak = 0; stats.streakHits = vitals[peer]->hits; }
        }
    }
    if (m_localBotReviveRequested && HasLocalBot() && m_brother->vitals.deathAnimationComplete) {
        if (m_brotherModel->weapon->brother.OnRevive()) { m_localBotReviveRequested = false; }
    }
    if (!m_localLive || m_brother == nullptr || m_brotherModel == nullptr) { return; }
    ZCombatId target = 0;
    CBrother *actor = nullptr;
    if (m_vitals->dead && m_vitals->deathAnimationComplete && !m_brother->vitals.dead) {
        target = kPlayerCombatId;
        actor = &m_playerModel->weapon->brother;
    } else if (m_brother->vitals.dead && m_brother->vitals.deathAnimationComplete && !m_vitals->dead) {
        target = kBrotherCombatId;
        actor = &m_brotherModel->weapon->brother;
    }
    if (target != m_reviveTarget) { m_reviveTarget = target; m_reviveProgress = 0; }
    // CPlayer::Update :100377 chooses PLAYER script resource 2 outside
    // rescue range and resource 3 inside it. This belongs to multiplayer,
    // independently of whether the other player's input comes from a bot.
    unsigned effectState = 0;
    const bool inRange = std::hypot(m_actor.x - m_brother->x, m_actor.y - m_brother->y) < 125;
    if (actor != nullptr) { effectState = 1; if (inRange) { effectState = 2; } }
    if (effectState != m_reviveEffectState || target != m_reviveEffectTarget) {
        StopEffect(m_reviveEffectHandle);
        m_reviveEffectHandle = 0;
        m_reviveEffectState = effectState;
        m_reviveEffectTarget = target;
        if (effectState != 0 && !m_reviveEffects[effectState - 1].IsNull()) {
            float x = 0, y = 0;
            ActorPosition(target, x, y);
            m_reviveEffectHandle = StartPersistentEffect(m_reviveEffects[effectState - 1], x, y, true);
        }
    }
    if (actor == nullptr) { return; }
    // CPlayer::Update :100405..100437: strict 125-unit radius, 0.0001/ms.
    // Leaving the radius retains progress; the original has no reset branch.
    if (!inRange) { return; }
    m_reviveProgress = std::min(1.0f, m_reviveProgress + deltaMs * 0.0001f);
    if (m_reviveProgress < 1 || !actor->OnRevive()) { return; }
    ++m_reviveCount;
    if (target == kBrotherCombatId) { AddExperience(10); } // SetRevivePercent :115366.
    else { AddPeerExperience(10); }
    m_reviveProgress = 0;
    m_reviveTarget = 0;
}
