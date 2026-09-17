/** @file ZPickupScene.cpp
 * @brief Keep templates/animation clocks stable; collect each instance once.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/ZPickupScene.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/ZWeaponEffects.h"
#include "gun_bros_re/gameplay/CParticlePool.h"
#include <cstdio>

ZPickupScene::ZPickupScene(CResTOCManager &toc, ZPackTables &tables, const ZShaderProgram &program,
    CProfileManager *profile, std::shared_ptr<CParticlePool> particlePool)
    : m_toc(toc), m_tables(tables), m_program(program), m_profile(profile) {
    // CPickup::Spawn :99889 uses CMap's CParticleSystem, not its effect-layer pool.
    if (!particlePool) { particlePool = std::make_shared<CParticlePool>(200); }
    m_particlePool = std::move(particlePool);
}

bool ZPickupScene::GetObjectPosition(int objectId, float &x, float &y) const {
    for (const auto &instance : m_instances) {
        if (instance->objectId == objectId) { x = instance->x; y = instance->y; return true; }
    }
    return false;
}

bool ZPickupScene::GetIndicatorTarget(unsigned serial, float &x, float &y) const {
    for (const auto &instance : m_instances) {
        if (instance->serial == serial) { x = instance->x; y = instance->y; return true; }
    }
    return false;
}

bool ZPickupScene::Init() {
    if (!m_batch.Create(m_program) || !LoadPickupCatalog(m_toc, m_tables, m_catalog)) { return false; }
    for (const ZPickupEntry &entry : m_catalog) {
        const auto &ref = entry.data.sprite;
        if (m_spritePacks.count(ref.packHash) == 0) {
            const int pack = m_toc.GetPackIndexFromHash(ref.packHash);
            if (pack < 0) { return false; }
            auto glu = std::make_unique<CSpriteGlu>();
            if (!glu->Init(*m_toc.GetPack(pack))) { return false; }
            m_spritePacks[ref.packHash] = std::move(glu);
        }
        CSpriteGlu &glu = *m_spritePacks[ref.packHash];
        const ZSpriteArchetype *archetype = glu.GetArchetype(ref.archetype);
        if (archetype == nullptr || ref.animation >= archetype->GetAnimationCount()) { return false; }
        auto visual = std::make_unique<Visual>();
        visual->entry = &entry;
        CSpriteIterator iterator(glu, *archetype);
        const auto &steps = archetype->GetAnimation(ref.animation).steps;
        visual->frames.resize(steps.size());
        for (unsigned index = 0; index < steps.size(); ++index) {
            visual->durations.push_back(steps[index].durationMs);
            if (!iterator.Expand(ref.animation, index, visual->frames[index])) { return false; }
        }
        if (steps.empty() || iterator.GetSkippedPartCount() != 0 || iterator.GetUnsupportedTransformCount() != 0) { return false; }
        m_visuals.push_back(std::move(visual));
    }
    return true;
}

void ZPickupScene::Reset() {
    if (m_effects != nullptr) {
        for (const auto &instance : m_instances) { m_effects->StopSpawning(instance->effectHandle); }
    }
    m_instances.clear();
    collections.clear();
    spawned = 0;
    collected = 0;
    failures = 0;
}

bool ZPickupScene::Spawn(const GameObjectRef &ref, float x, float y, int objectId) {
    for (const auto &visual : m_visuals) {
        const auto &entry = *visual->entry;
        if (entry.ref.packHash != ref.packHash || entry.ref.localIndex != ref.localIndex) { continue; }
        auto instance = std::make_unique<Instance>();
        instance->serial = spawned + 1;
        instance->visual = visual.get();
        instance->x = x;
        instance->y = y;
        instance->objectId = objectId;
        instance->pickup.Bind(entry.data);
        instance->animation.SetAnimation(&visual->durations);
        m_instances.push_back(std::move(instance));
        ++spawned;
        std::printf("[pickup] spawned %s id=%d at=%.1f,%.1f\n", entry.owner.c_str(), objectId, x, y);
        return true;
    }
    ++failures;
    std::printf("[pickup] missing %08x:%u\n", ref.packHash, ref.localIndex);
    return false;
}

void ZPickupScene::GrantStoreItem(const GameObjectRef &ref, CProfileManager *profile) {
    if (profile == nullptr) { return; }
    std::vector<std::uint8_t> payload;
    if (!m_tables.ReadSectionResource(ref.packHash, ZGameSection::StoreItem, ref.localIndex, payload)) { ++failures; return; }
    CStoreItem item;
    CArrayInputStream stream(payload);
    if (!item.Init(stream)) { ++failures; return; }
    // CollectItem uses AcquireItem(..., free=true). No currency is deducted.
    for (const GameObjectTypeRef &object : item.objects) {
        if (object.type == 17) { profile->AddPowerup(object.object, 1); }
        else if (object.type == 2 || object.type == 6) { profile->Grant(object.type, object.object); }
        else { ++failures; std::printf("[pickup] unsupported store object type=%u\n", object.type); }
    }
}

void ZPickupScene::UpdateEffects(int deltaMs, ZWeaponEffects &effects) {
    m_effects = &effects;
    for (const auto &instance : m_instances) {
        instance->animation.Update(static_cast<std::uint16_t>(deltaMs));
        if (instance->effectStarted) { continue; }
        instance->effectStarted = true;
        const GameObjectRef &resource = instance->visual->entry->data.particleEffect;
        if (resource.IsNull()) { continue; }
        // CPickup::Spawn anchors the looping effect one world unit above it.
        instance->effectHandle = effects.StartPersistentEffect(resource, instance->x, instance->y - 1, true, m_particlePool);
        if (instance->effectHandle == 0) { ++failures; }
    }
}

void ZPickupScene::Update(int deltaMs, CLevel &scene, ZWeaponEffects &effects) {
    UpdateEffects(deltaMs, effects);
    collections.clear();
    for (std::size_t index = 0; index < m_instances.size();) {
        Instance &instance = *m_instances[index];
        instance.pickup.SetLevelContext(&scene);
        const bool playerTouch = scene.TouchesPickup(instance.x, instance.y);
        bool peerTouch = scene.BrotherTouchesPickup(instance.x, instance.y);
        if (playerTouch && peerTouch) {
            peerTouch = scene.IsDeathmatch() && scene.BrotherIsCloser(instance.x, instance.y);
        }
        if (!playerTouch && !peerTouch) { ++index; continue; }
        if (instance.pickup.Collect()) {
            effects.StopSpawning(instance.effectHandle);
            ++collected;
            for (const ZPickupAction &action : instance.pickup.TakeActions()) {
                if (action.kind == ZPickupAction::Kind::Xplodium) {
                    if (peerTouch) { scene.AddPeerXplodium(action.amount); } else { scene.AddXplodium(action.amount); }
                } else if (action.kind == ZPickupAction::Kind::Experience) {
                    if (peerTouch) { scene.AddPeerExperience(action.amount); } else { scene.AddExperience(action.amount); }
                } else if (action.kind == ZPickupAction::Kind::Health) {
                    if (peerTouch) {
                        auto *vitals = scene.GetBrotherVitals();
                        vitals->health = std::min(vitals->maximum, vitals->health + action.amount);
                    } else { scene.AddHealth(action.amount); }
                } else if (action.kind == ZPickupAction::Kind::StoreItem) {
                    CProfileManager *profile = m_profile;
                    if (peerTouch) { profile = m_peerProfile; }
                    GrantStoreItem(action.resource, profile);
                }
                else if (action.kind == ZPickupAction::Kind::Sound) {
                    ZGunCue cue;
                    cue.kind = ZGunCue::Kind::Sound;
                    cue.resource = action.resource;
                    effects.Emit(cue, instance.x, instance.y, 0, 0);
                }
            }
            failures += instance.pickup.GetUnsupportedCount();
            unsigned peer = 0;
            if (peerTouch) { peer = 1; }
            collections.push_back({instance.visual->entry->ref, instance.objectId, peer});
            std::printf("[pickup] collected %s id=%d\n", instance.visual->entry->owner.c_str(), instance.objectId);
        }
        m_instances.erase(m_instances.begin() + index);
    }
}

bool ZPickupScene::FindNearest(float x, float y, float &goalX, float &goalY) const {
    float nearest = 1000000000.0f;
    bool found = false;
    for (const auto &item : m_instances) {
        const float dx = item->x - x, dy = item->y - y;
        const float distance = dx * dx + dy * dy;
        if (distance < nearest) { nearest = distance; goalX = item->x; goalY = item->y; found = true; }
    }
    return found;
}

void ZPickupScene::Draw(const float *mvp, float scale) {
    m_batch.Begin();
    for (const auto &instance : m_instances) {
        const auto &quads = instance->visual->frames[instance->animation.GetStep()];
        for (const ZSpriteQuad &quad : quads) {
            m_batch.AddTransformedQuad(*quad.page, instance->x + quad.offsetX * scale,
                instance->y + quad.offsetY * scale, quad.Width() * scale, quad.Height() * scale,
                quad.source, quad.flipHorizontal, quad.flipVertical, quad.blend, 0, 0, 1, 1, 0, 1, quad.rotateTexture);
        }
    }
    m_batch.Upload();
    m_batch.Draw(m_program, mvp);
}
