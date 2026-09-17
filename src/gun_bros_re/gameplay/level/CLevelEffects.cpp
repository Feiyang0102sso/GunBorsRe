/** @file CLevelEffects.cpp
 * @brief CLevel object scheduling and original gameplay cue dispatch.
 */
#define NOMINMAX
#include "gun_bros_re/effects/CParticleEffectPlayer.h"
#include "gun_bros_re/effects/CParticleSystem.h"
#include "gun_bros_re/effects/ZParticleResources.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/ZCombatAudio.h"
#include "gun_bros_re/effects/CEffectLayer.h"
#include "engine/core/ZMatrix4d.h"
#include "gun_bros_re/gameplay/CBullet.h"
#include "gun_bros_re/gameplay/ZBulletResources.h"
#include "gun_bros_re/effects/ZEffectColors.h"
#include "gun_bros_re/gameplay/ZProjectileGeometry.h"
#include "gun_bros_re/effects/CParticleEffect.h"
#include "engine/glu/sprite/ZSpriteRenderer.h"
#include "engine/graphics/ZEffectProjection.h"
#include <algorithm>
#include <cmath>

using ProjectileGeometry::ProjectMuzzle;
using ProjectileGeometry::SegmentFraction;

namespace {
constexpr float kRadians = 3.14159265f / 180.0f;
constexpr std::uint32_t kBeamFlag = 0x100;
}
float CLevel::RandomProjectile(float minimum, float maximum) {
    m_effectRandom = m_effectRandom * 1664525u + 1013904223u;
    return minimum + (maximum - minimum) * static_cast<float>(m_effectRandom >> 8) / 16777215.0f;
}

CParticleEffectPlayer *CLevel::StartParticleEffect(const GameObjectRef &ref,
    float x, float y, float z, float angle) {
    const auto *data = m_particleResources->Get(ref);
    if (data == nullptr) { return nullptr; }
    auto *player = m_mapParticles->AddEffect(*data, x, y);
    if (player != nullptr) { player->SetPosition(x, y, z, angle); }
    return player;
}

void CLevel::EmitBulletCue(const ZGunCue &cue, float x, float y, float z, float direction, CBullet *owner) {
    if (owner != nullptr && (cue.kind == ZGunCue::Kind::RibbonTrail || cue.kind == ZGunCue::Kind::RibbonColor)) {
        owner->ApplyRibbonCue(cue);
        return;
    }
    if (m_projectileWorld != nullptr && owner != nullptr &&
        (cue.kind == ZGunCue::Kind::Splash || cue.kind == ZGunCue::Kind::SpawnEnemy)) {
        ZCombatHit hit;
        hit.projectile = owner->id;
        hit.owner = owner->owner;
        hit.weapon = owner->weapon;
        hit.weaponSlot = owner->weaponSlot;
        hit.bullet = owner->source.resource;
        hit.critical = owner->critical;
        hit.weaponMasteryLimit = owner->weaponMasteryLimit;
        hit.ownerType = owner->ownerType;
        hit.flags = owner->flags;
        hit.x = x;
        hit.y = y;
        hit.direction = direction;
        // Original splash natives use their authored damage and owner
        // armor/level multiplier, independently of the bullet's base damage.
        // CBullet::FunctionResolver :61169/:61242/:61379 uses native
        // arguments directly; Configure's mastery roll scales direct hits.
        hit.damage = cue.damage * m_projectileWorld->GetDamageMultiplier(owner->owner, owner->damageMultiplier);
        hit.percentDamage = cue.percentDamage;
        hit.spawnObjectId = cue.spawnObjectId;
        hit.forceSpawn = cue.forceSpawn;
        if (cue.kind == ZGunCue::Kind::Splash) {
            m_projectileWorld->Splash(hit, cue.radius, cue.cone, cue.force, cue.forceMs);
        } else { m_projectileWorld->SpawnFromProjectile(cue.resource, hit); }
        return;
    }
    if (cue.kind == ZGunCue::Kind::StopTrail) { if (owner != nullptr) { owner->StopTrail(); } return; }
    if (cue.kind == ZGunCue::Kind::Effect || cue.kind == ZGunCue::Kind::Trail) {
        float angle = 0;
        if (cue.alignEffect) { angle = direction + 90.0f; }
        const auto *data = m_particleResources->Get(cue.resource);
        if (data == nullptr) { return; }
        if (owner != nullptr && (cue.kind == ZGunCue::Kind::Trail || owner->beam)) {
            owner->AttachParticleEffect(*data, m_effectLayerPool, cue.kind == ZGunCue::Kind::Trail, cue.alignEffect);
        } else if (owner != nullptr) {
            m_effectLayer.AddParticleEffect(*data, x, y, z, angle);
        } else { StartParticleEffect(cue.resource, x, y, z, angle); }
    } else if (cue.kind == ZGunCue::Kind::Sound || cue.kind == ZGunCue::Kind::LoopSound ||
               cue.kind == ZGunCue::Kind::StopSound) {
        ZCombatId soundOwner = kPlayerCombatId;
        if (owner != nullptr) { soundOwner = owner->owner; }
        m_combatAudio->PlayCue(cue, soundOwner);
    }
}

// CParticle::Spawn selects an animation once using RandomBit.
void CLevel::AdvanceParticles(int deltaMs) {
    if (deltaMs <= 0) { return; }
    for (auto &entry : m_brotherParticles) {
        const auto particles = entry.second.lock();
        if (particles) { particles->Update(deltaMs, m_effectRandom); }
    }
    m_mapParticles->Update(deltaMs, m_effectRandom);
    m_effectLayer.Update(deltaMs, m_effectRandom);
}

CLevel::CLevel(CResTOCManager &toc, ZPackTables &tables, const ZShaderProgram &program,
    std::shared_ptr<CParticlePool> particlePool, std::shared_ptr<CParticleSystem> mapParticles) : CLevel() {
    m_tables = &tables;
    m_program = &program;
    m_effectSprites = std::make_unique<ZSpriteRenderer>(toc, program);
    m_combatAudio = std::make_unique<ZCombatAudio>(tables);
    m_bulletResources = std::make_unique<ZBulletResources>(tables, program);
    m_particleResources = std::make_unique<ZParticleResources>(tables);
    m_mapParticles = std::move(mapParticles);
    if (!m_mapParticles) { m_mapParticles = std::make_shared<CParticleSystem>(); }
    // CMap allocates 200 slots (:91849). Menus/powerups supply
    // their shared owner pool explicitly instead of allocating per effect.
    if (!particlePool) { particlePool = std::make_shared<CParticlePool>(200); }
    m_effectLayerPool = std::move(particlePool);
    m_effectLayer.SetPool(m_effectLayerPool);
}
CLevel::~CLevel() {
    // Map storage may outlive this level. Remove callbacks into its actor world
    // before actor/resource members are destroyed.
    if (m_mapParticles) { Clear(); }
}

std::vector<ZWeaponProjectileState> CLevel::GetProjectileStates() const {
    std::vector<ZWeaponProjectileState> result;
    for (const auto &shot : m_bullets) {
        if (shot->removed) { continue; }
        result.push_back({shot->source.resource, shot->owner, shot->beam, shot->x, shot->y,
            shot->direction, shot->length, shot->animation, shot->ageMs});
        auto &state = result.back();
        state.beamSourceAnimation = shot->beamSourceAnimation;
        state.beamEndAnimation = shot->beamEndAnimation;
        if (!shot->beam) { state.collisionRadius = shot->visual->data.GetRadius(); }
        state.collisionEnabled = shot->HasActiveCollision() && !shot->removed;
    }
    return result;
}

void CLevel::SetCombatWorld(ZProjectileWorld *world) { m_projectileWorld = world; }

std::uint64_t CLevel::StartPersistentEffect(const GameObjectRef &resource, float x, float y, bool loop) {
    auto *player = StartParticleEffect(resource, x, y, 0, 0);
    if (player == nullptr) { return 0; }
    // CPickup::Spawn :99893 explicitly enables looping after AddEffect.
    player->SetLooping(loop);
    return m_mapParticles->GetHandle(*player);
}

void CLevel::StopEffect(std::uint64_t handle) {
    auto *player = m_mapParticles->Get(handle);
    if (player != nullptr) { player->Stop(); player->SetAnchor({}); }
}

void CLevel::StopSpawning(std::uint64_t handle) {
    auto *player = m_mapParticles->Get(handle);
    if (player != nullptr) { player->StopSpawning(); }
}

void CLevel::AdvanceAmbientEffects(int deltaMs) {
    BeginAudioFrame();
    AdvanceParticles(deltaMs);
    m_combatAudio->Update();
}

void CLevel::BeginAudioFrame() { m_combatAudio->BeginFrame(); }

unsigned CLevel::GetVoiceCount() const { return m_combatAudio->GetVoiceCount(); }

void CLevel::SetViewBounds(float centerX, float centerY, float width, float height) {
    m_projectileView.enabled = width > 0 && height > 0;
    m_projectileView.left = centerX - width * 0.5f;
    m_projectileView.top = centerY - height * 0.5f;
    m_projectileView.width = width;
    m_projectileView.height = height;
}

ZCombatId CLevel::SpawnProjectile(const GameObjectRef &resource, float x, float y,
    float z, float direction, float speed, ZCombatId owner, int ownerType, int part, int node) {
    ZBulletVisual *visual = m_bulletResources->Get(resource);
    if (visual == nullptr) { return 0; }
    std::unique_ptr<CBullet> shot(new CBullet());
    shot->id = m_nextProjectile++;
    shot->owner = owner;
    shot->ownerType = ownerType;
    if (m_projectileWorld != nullptr) { shot->damageMultiplier = m_projectileWorld->GetDamageMultiplier(owner); }
    if (m_projectileWorld != nullptr) { shot->powerupMultiplier = m_projectileWorld->GetProjectilePowerupMultiplier(owner); }
    shot->part = part;
    shot->source.node = node;
    shot->source.resource = resource;
    shot->visual = visual;
    shot->x = x;
    shot->y = y;
    shot->z = z;
    shot->direction = direction;
    shot->speed = speed;
    shot->beam = (visual->data.GetFlags() & kBeamFlag) != 0;
    if (m_projectileWorld != nullptr) { shot->SetLevelContext(m_projectileWorld->GetScriptLevel()); }
    shot->Bind(visual->data, false);
    const ZCombatId id = shot->id;
    for (const ZGunCue &cue : shot->TakeCues()) {
        EmitBulletCue(cue, x, y, z, direction, shot.get());
    }
    m_bullets.push_back(std::move(shot));
    ++m_shotsFired;
    return id;
}

void CLevel::ResolveHit(ZCombatId projectile, ZHitResult result) {
    for (auto &shot : m_bullets) {
        if (shot->id == projectile && shot->pendingHit) {
            shot->pendingHit = false;
            shot->OnCollision(result);
            return;
        }
    }
}

bool CLevel::RemoveOldestProjectile(ZCombatId owner) {
    for (auto &shot : m_bullets) {
        if (shot->owner == owner && !shot->removed) {
            shot->ForceRemoval();
            return true;
        }
    }
    return false;
}

void CLevel::RetireOwner(ZCombatId owner) {
    m_combatAudio->RetireOwner(owner);
    m_mapParticles->RetireOwner(owner);
    const auto entry = m_brotherParticles.find(owner);
    if (entry != m_brotherParticles.end()) {
        const auto particles = entry->second.lock();
        if (particles) { particles->Stop(); }
    }
    // The id can come back on a new actor; do not let a stale entry keep its
    // loop silent.
    for (auto &shot : m_bullets) {
        if (shot->owner == owner && shot->beam) {
            shot->removed = true;
            shot->StopAttachedEffects();
        }
    }
}

void CLevel::PlayMoveSound(const GameObjectRef &sound) {
    m_combatAudio->PlayWav(sound.packHash, sound.localIndex, false, kPlayerCombatId, true);
}

void CLevel::Emit(const ZGunCue &cue, float x, float y, float z, float direction,
    ZCombatId actor, int slot, int part, int node) {
    if (cue.kind == ZGunCue::Kind::Sound || cue.kind == ZGunCue::Kind::LoopSound || cue.kind == ZGunCue::Kind::StopSound) {
        m_combatAudio->PlayCue(cue, actor);
        return;
    }
    if (cue.brotherPowerup) {
        const auto entry = m_brotherParticles.find(actor);
        if (entry == m_brotherParticles.end()) { return; }
        const auto particles = entry->second.lock();
        if (!particles) { return; }
        const CParticleEffect *data = nullptr;
        if (cue.kind == ZGunCue::Kind::Trail) { data = m_particleResources->Get(cue.resource); }
        particles->x = x; particles->y = y;
        particles->Apply(cue, data);
        return;
    }
    if (actor != 0 && (cue.kind == ZGunCue::Kind::Trail || cue.kind == ZGunCue::Kind::StopTrail)) {
        m_mapParticles->StopLinked(actor, slot, cue.stopParticlesImmediately);
        if (cue.kind == ZGunCue::Kind::StopTrail) { return; }
        float initialAngle = 0;
        if (cue.alignEffect) { initialAngle = direction + 90; }
        auto *player = StartParticleEffect(cue.resource, x, y, z, initialAngle);
        if (player == nullptr) { return; }
        player->SetScale(cue.effectScale);
        player->SetZOrderGroup(cue.effectGroup);
        player->SetLooping(cue.loopParticles);
        m_mapParticles->SetOwner(m_mapParticles->GetHandle(*player), actor, slot);
        if (m_projectileWorld != nullptr) {
            auto *world = m_projectileWorld;
            const bool actorAnchor = cue.anchorToActor;
            const bool enemyAnchor = cue.linkedEnemyEffect;
            const bool align = cue.alignEffect;
            player->SetAnchor([world, actor, part, node, actorAnchor, enemyAnchor, align](float &px, float &py, float &pz, float &angle) {
                if (actorAnchor) { return world->ParticleAnchor(actor, px, py, pz, angle); }
                if (enemyAnchor) {
                    if (!world->LinkedParticleAnchor(actor, node, px, py, pz, angle)) { return false; }
                    if (!align) { angle = 0; }
                    return true;
                }
                float direction = 0;
                if (!world->Anchor(actor, part, node, px, py, pz, direction)) { return false; }
                if (align) { angle = direction + 90; }
                return true;
            });
        }
        return;
    }
    if (cue.kind == ZGunCue::Kind::Effect) {
        float angle = 0;
        if (cue.alignEffect) { angle = direction + 90; }
        auto *player = StartParticleEffect(cue.resource, x, y, z, angle);
        if (player == nullptr) { return; }
        player->SetScale(cue.effectScale);
        player->SetZOrderGroup(cue.effectGroup);
        // Keep attribution separate from the anchor: death bursts stay in place.
        // Correction: native 11 follows the brother until retirement; emitted
        // world-space particles remain where they were born (:138969/134152).
        m_mapParticles->SetOwner(m_mapParticles->GetHandle(*player), actor);
        if (cue.anchorToActor && m_projectileWorld != nullptr) {
            auto *world = m_projectileWorld;
            player->SetAnchor([world, actor](float &px, float &py, float &pz, float &rotation) {
                return world->ParticleAnchor(actor, px, py, pz, rotation);
            });
        }
        return;
    }
    EmitBulletCue(cue, x, y, z, direction);
}

bool CLevel::HasActorBurst(ZCombatId actor) const { return m_mapParticles->HasActorBurst(actor); }

void CLevel::Clear() {
    m_bullets.clear();
    for (auto &entry : m_brotherParticles) {
        const auto particles = entry.second.lock();
        if (particles) { particles->Stop(); }
    }
    m_brotherParticles.clear();
    m_mapParticles->Clear();
    m_effectLayer.Clear();

    m_combatAudio->Clear();
    m_combatAudio->BeginFrame();
}

void CLevel::SetPaused(bool paused) { m_combatAudio->SetPaused(paused); }
std::size_t CLevel::GetBulletCount() const {
    std::size_t count = 0;
    for (const auto &shot : m_bullets) { if (!shot->removed) { ++count; } }
    return count;
}
std::size_t CLevel::GetRibbonCount() const {
    std::size_t count = 0;
    for (const auto &shot : m_bullets) { count += shot->effects.GetRibbonCount(); }
    return count;
}
std::size_t CLevel::GetDrawnBeamQuadCount() const { return m_drawnBeamQuads; }
std::size_t CLevel::GetDrawnLightningQuadCount() const { return m_drawnLightningQuads; }
std::size_t CLevel::GetParticleCount() const {
    std::size_t count = m_effectLayer.GetParticleCount();
    for (const auto &shot : m_bullets) { count += shot->effects.GetParticleCount(); }
    count += m_mapParticles->GetParticleCount();
    for (const auto &entry : m_brotherParticles) {
        const auto particles = entry.second.lock();
        if (particles) { count += particles->GetParticleCount(); }
    }
    return count;
}
std::size_t CLevel::GetEffectCount() const {
    std::size_t count = m_effectLayer.GetEffectCount();
    for (const auto &shot : m_bullets) { count += shot->effects.GetEffectCount(); }
    count += m_mapParticles->GetEffectCount();
    for (const auto &entry : m_brotherParticles) {
        const auto particles = entry.second.lock();
        if (particles) { count += particles->GetEffectCount(); }
    }
    return count;
}
std::size_t CLevel::GetTrailCount() const {
    std::size_t count = 0;
    for (const auto &shot : m_bullets) { count += shot->effects.GetEffectCount(); }
    return count;
}
std::size_t CLevel::GetShotCount() const { return m_shotsFired; }
std::size_t CLevel::GetSoundCueCount() const { return m_combatAudio->GetSoundCueCount(); }

void CLevel::EmitBrother(ZPlayerModel &player, const float *modelToScene, float facingDegrees,
    ZCombatId owner, const ZWeaponCollision *collision) {
    if (!player.weapon) { return; }
    const auto strengthening = player.weapon->brother.GetPowerupParticles();
    strengthening->x = modelToScene[3]; strengthening->y = modelToScene[7];
    m_brotherParticles[owner] = strengthening;
    for (const ZGunCue &cue : player.weapon->brother.TakeCues()) {
        if (cue.kind == ZGunCue::Kind::Grenade) {
            if (!player.weapon->brother.CanThrowGrenade(cue.hand)) { continue; }
            ZMeshBoneTransform origin{};
            // GetGunNodeLocation(1) uses torso node 2, independently of the gun.
            if (!player.weapon->brother.GetTorso().GetAnimation().GetNodeAt(2, origin)) { continue; }
            const float x = modelToScene[0] * origin.posX + modelToScene[1] * origin.posY + modelToScene[2] * origin.posZ + modelToScene[3];
            const float y = modelToScene[4] * origin.posX + modelToScene[5] * origin.posY + modelToScene[6] * origin.posZ + modelToScene[7];
            const float z = modelToScene[8] * origin.posX + modelToScene[9] * origin.posY + modelToScene[10] * origin.posZ + modelToScene[11];
            if (SpawnProjectile(cue.resource, x, y, z, facingDegrees - 90, cue.speed, owner, 0) != 0) {
                player.weapon->brother.OnGrenadeThrown(cue.hand);
            }
            continue;
        }
        if (cue.kind == ZGunCue::Kind::Splash && m_projectileWorld != nullptr) {
            ZCombatHit hit;
            hit.owner = owner;
            hit.ownerType = 0;
            hit.damage = cue.damage;
            hit.percentDamage = cue.percentDamage;
            hit.x = modelToScene[3];
            hit.y = modelToScene[7];
            m_projectileWorld->Splash(hit, cue.radius, cue.cone, cue.force, cue.forceMs);
            continue;
        }
        Emit(cue, modelToScene[3], modelToScene[7], 0, facingDegrees - 90, owner, cue.hand, -1, -1);
    }
    for (const ZMoveSoundRef &sound : player.weapon->brother.GetTorso().TakeSounds()) {
        m_combatAudio->PlayWav(sound.packHash, sound.localIndex, false, owner);
    }
    for (const ZMoveSoundRef &sound : player.weapon->brother.GetLegs().TakeSounds()) {
        m_combatAudio->PlayWav(sound.packHash, sound.localIndex, false, owner);
    }
    const float direction = facingDegrees - 90.0f;
    // CLevel::UpdateNormal iterates a growing object list: bullets spawned
    // by a player are advanced before their first draw in the same tick.
    for (const ZGunCue &cue : player.ActiveWeapon().gun.TakeCues()) {
        if (cue.kind == ZGunCue::Kind::Sound || cue.kind == ZGunCue::Kind::LoopSound || cue.kind == ZGunCue::Kind::StopSound) {
            m_combatAudio->PlayCue(cue, owner);
            continue;
        }
        int copies = 1;
        if (cue.hand == 2) { copies = 2; }
        for (int copy = 0; copy < copies; ++copy) {
            int hand = cue.hand;
            if (copies == 2) { hand = copy; }
            float x = 0, y = 0, z = 0;
            if (!ProjectMuzzle(player, modelToScene, hand, cue.node, x, y, z)) { continue; }
            if (cue.kind != ZGunCue::Kind::Bullet) { EmitBulletCue(cue, x, y, z, direction); continue; }
            ZBulletVisual *visual = m_bulletResources->Get(cue.resource);
            if (visual == nullptr) { continue; }
            std::unique_ptr<CBullet> shot(new CBullet());
            shot->id = m_nextProjectile++;
            shot->owner = owner;
            shot->weapon = player.gunResource;
            shot->weaponSlot = player.gunSlot;
            shot->followsMuzzle = true;
            shot->weaponMasteryLimit = player.ActiveWeapon().data.GetMasteryLimit();
            float masteryRoll = 1;
            if (player.ActiveWeapon().gun.GetMasteryLevel() > 0) { masteryRoll = RandomProjectile(0, 1); }
            shot->masteryDamageMultiplier = player.ActiveWeapon().gun.GetMasteryDamageMultiplier(masteryRoll, &shot->critical);
            if (m_projectileWorld != nullptr) { shot->powerupMultiplier = m_projectileWorld->GetProjectilePowerupMultiplier(owner); }
            shot->part = hand;
            shot->visual = visual;
            shot->source = cue;
            shot->source.hand = hand;
            shot->x = x; shot->y = y; shot->z = z;
            shot->direction = direction + RandomProjectile(cue.minimumAngle, cue.maximumAngle);
            shot->speed = cue.speed;
            shot->beam = (visual->data.GetFlags() & kBeamFlag) != 0;
            if (m_projectileWorld != nullptr) { shot->SetLevelContext(m_projectileWorld->GetScriptLevel()); }
            shot->Bind(visual->data, cue.alternate);
            player.ActiveWeapon().gun.AddBullet(*shot);
            // CBullet::Fire :62212-62243 tests owner -> muzzle before movement.
            // A zero-speed mine can already be beyond the terrain at birth.
            if (collision != nullptr) {
                const CCollisionData *birthCollision = &collision->walls;
                if ((shot->flags & 0x20) != 0) { birthCollision = &collision->terrain; }
                SegmentFraction(modelToScene[3], modelToScene[7], x - modelToScene[3], y - modelToScene[7],
                    birthCollision, &shot->spawnNormalX, &shot->spawnNormalY, &shot->spawnCollision);
            }
            if (shot->beam) {
                const float beamLength = static_cast<float>(shot->maximumBeamLength);
                const float dx = std::cos(shot->direction * kRadians) * beamLength;
                const float dy = std::sin(shot->direction * kRadians) * beamLength;
                const CCollisionData *walls = nullptr;
                if (collision != nullptr) {
                    walls = &collision->walls;
                    if ((visual->data.GetFlags() & 0x20) != 0) { walls = &collision->terrain; }
                }
                shot->length = beamLength * SegmentFraction(x, y, dx, dy, walls);
            }
            for (const ZGunCue &spawnCue : shot->TakeCues()) { EmitBulletCue(spawnCue, x, y, z, shot->direction, shot.get()); }
            m_bullets.push_back(std::move(shot));
            ++m_shotsFired;
        }
    }
}

void CLevel::Update(ZPlayerModel &player, const float *modelToScene, float facingDegrees,
    int deltaMs, const ZWeaponCollision *collision) {
    // Combat time, for the move-sound window in PlayWav. Advanced before the
    // early exit so a scene without a player still ages its cues.
    m_combatAudio->AdvanceClock(deltaMs);
    if (!player.weapon || deltaMs <= 0) { return; }
    if (m_projectileWorld == nullptr) { BeginAudioFrame(); }
    m_effectPlayerX = modelToScene[3];
    m_effectPlayerY = modelToScene[7];
    EmitBrother(player, modelToScene, facingDegrees, kPlayerCombatId, collision);
    const float direction = facingDegrees - 90;
    for (auto &shot : m_bullets) {
        if (!shot->removed) { shot->UpdateProjectile(m_projectileWorld, *m_effectSprites, m_projectileView, player, modelToScene,
            facingDegrees, deltaMs, collision, m_effectRandom); }
        for (const ZGunCue &cue : shot->TakeCues()) {
            EmitBulletCue(cue, shot->x, shot->y, shot->z, shot->direction, shot.get());
        }
    }
    for (auto &shot : m_bullets) {
        shot->UpdateAttachedEffects(deltaMs, m_effectRandom);
    }
    std::size_t i = 0;
    while (i < m_bullets.size()) {
        if (m_bullets[i]->removed && m_bullets[i]->effects.IsDone()) {
            m_bullets.erase(m_bullets.begin() + i);
        }
        else { ++i; }
    }
    AdvanceParticles(deltaMs);
    m_combatAudio->Update();
}

std::vector<CParticleSystem::RenderItem> CLevel::GetMapParticleItems() const {
    return m_mapParticles->GetRenderItems();
}

void CLevel::DrawMapParticle(const CParticleSystem::RenderItem &item, const float *sceneMvp) {
    m_effectSprites->Begin();
    item.player->QueueParticle(item.index, *m_effectSprites);
    m_effectSprites->Draw(sceneMvp);
}

void CLevel::Draw(const float *sceneMvp, const float *previewProjection, float meshCameraScale,
    ZWeaponDrawPass pass, bool mapParticlesInQueue) {
    const ZEffectProjection projection(previewProjection);
    glDisable(GL_DEPTH_TEST);
    m_effectSprites->Batch().Begin();
    m_drawnBeamQuads = 0;
    m_drawnLightningQuads = 0;
    for (const auto &shot : m_bullets) {
        const bool behindPlayer = std::hypot(shot->x - m_effectPlayerX, shot->y - m_effectPlayerY) < 100 || shot->y + 10 < m_effectPlayerY;
        if (pass == ZWeaponDrawPass::BehindPlayer && !behindPlayer) { continue; }
        if (pass == ZWeaponDrawPass::InFrontOfPlayer && behindPlayer) { continue; }
        shot->effects.Draw(*m_effectSprites, m_effectColors, projection, true);
    }
    for (auto &shot : m_bullets) {
        // CBullet::GetZOrder (:60280) puts a player's bullet behind its
        // shooter while within 100 world units. Otherwise use world Y + 10.
        // This hides the backward half of long tracers inside the gun mesh.
        const float distance = std::hypot(shot->x - m_effectPlayerX, shot->y - m_effectPlayerY);
        const bool behindPlayer = distance < 100.0f || shot->y + 10.0f < m_effectPlayerY;
        if (pass == ZWeaponDrawPass::BehindPlayer && !behindPlayer) { continue; }
        if (pass == ZWeaponDrawPass::InFrontOfPlayer && behindPlayer) { continue; }
        if (shot->removed) { continue; }
        shot->DrawProjectile(*m_effectSprites, m_effectColors, *m_program, sceneMvp, projection,
            meshCameraScale, m_drawnBeamQuads, m_drawnLightningQuads);
    }
    if (pass == ZWeaponDrawPass::BehindPlayer) {
        m_effectSprites->Batch().Upload();
        m_effectSprites->Batch().Draw(*m_program, sceneMvp);
        return;
    }
    for (const auto &shot : m_bullets) { shot->effects.Draw(*m_effectSprites, m_effectColors, projection, false); }
    m_effectLayer.Draw(*m_effectSprites, m_effectColors, projection);
    if (!mapParticlesInQueue) { m_mapParticles->QueueParticles(*m_effectSprites, previewProjection); }
    for (const auto &entry : m_brotherParticles) {
        const auto particles = entry.second.lock();
        if (particles) { particles->Draw(*m_effectSprites, previewProjection); }
    }
    m_effectSprites->Batch().Upload();
    m_effectSprites->Batch().Draw(*m_program, sceneMvp);
}
