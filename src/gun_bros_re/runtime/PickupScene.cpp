/** @file PickupScene.cpp
 * @brief Keep templates/animation clocks stable; collect each instance once.
 */
#include "runtime/PickupScene.h"
#include "runtime/CombatScene.h"
#include "gun_bros/WeaponEffects.h"
#include <cstdio>

PickupScene::PickupScene(CResTOCManager &toc, PackTables &tables, const CShaderProgram &program,
    CProfileManager *profile) : m_toc(toc), m_tables(tables), m_program(program), m_profile(profile) {}

bool PickupScene::Init() {
    if (!m_batch.Create(m_program) || !LoadPickupCatalog(m_toc, m_tables, m_catalog)) { return false; }
    for (const PickupEntry &entry : m_catalog) {
        const auto &ref = entry.data.sprite;
        if (m_spritePacks.count(ref.packHash) == 0) {
            const int pack = m_toc.GetPackIndexFromHash(ref.packHash);
            if (pack < 0) { return false; }
            auto glu = std::make_unique<CSpriteGlu>();
            if (!glu->Init(*m_toc.GetPack(pack))) { return false; }
            m_spritePacks[ref.packHash] = std::move(glu);
        }
        CSpriteGlu &glu = *m_spritePacks[ref.packHash];
        const CSpriteGluArchetype *archetype = glu.GetArchetype(ref.archetype);
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

void PickupScene::Reset() {
    if (m_effects != nullptr) {
        for (const auto &instance : m_instances) { m_effects->StopEffect(instance->effectHandle); }
    }
    m_instances.clear();
    collections.clear();
    spawned = 0;
    collected = 0;
    failures = 0;
}

bool PickupScene::Spawn(const GameObjectRef &ref, float x, float y, int objectId) {
    for (const auto &visual : m_visuals) {
        const auto &entry = *visual->entry;
        if (entry.ref.packHash != ref.packHash || entry.ref.localIndex != ref.localIndex) { continue; }
        auto instance = std::make_unique<Instance>();
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

void PickupScene::GrantStoreItem(const GameObjectRef &ref) {
    if (m_profile == nullptr) { return; }
    std::vector<std::uint8_t> payload;
    if (!m_tables.ReadSectionResource(ref.packHash, GameSection::StoreItem, ref.localIndex, payload)) { ++failures; return; }
    CStoreItem item;
    CArrayInputStream stream(payload);
    if (!item.Init(stream)) { ++failures; return; }
    // CollectItem uses AcquireItem(..., free=true). No currency is deducted.
    for (const GameObjectTypeRef &object : item.objects) {
        if (object.type == 17) { m_profile->AddPowerup(object.object, 1); }
        else if (object.type == 2 || object.type == 6) { m_profile->Grant(object.type, object.object); }
        else { ++failures; std::printf("[pickup] unsupported store object type=%u\n", object.type); }
    }
}

void PickupScene::UpdateEffects(int deltaMs, WeaponEffects &effects) {
    m_effects = &effects;
    for (const auto &instance : m_instances) {
        instance->animation.Update(static_cast<std::uint16_t>(deltaMs));
        if (instance->effectStarted) { continue; }
        instance->effectStarted = true;
        const GameObjectRef &resource = instance->visual->entry->data.particleEffect;
        if (resource.IsNull()) { continue; }
        // CPickup::Spawn anchors the looping effect one world unit above it.
        instance->effectHandle = effects.StartPersistentEffect(resource, instance->x, instance->y - 1);
        if (instance->effectHandle == 0) { ++failures; }
    }
}

void PickupScene::Update(int deltaMs, CombatScene &scene, WeaponEffects &effects) {
    UpdateEffects(deltaMs, effects);
    collections.clear();
    for (std::size_t index = 0; index < m_instances.size();) {
        Instance &instance = *m_instances[index];
        if (!scene.TouchesPickup(instance.x, instance.y)) { ++index; continue; }
        if (instance.pickup.Collect()) {
            effects.StopEffect(instance.effectHandle);
            ++collected;
            for (const PickupAction &action : instance.pickup.TakeActions()) {
                if (action.kind == PickupAction::Kind::Xplodium) { scene.AddXplodium(action.amount); }
                else if (action.kind == PickupAction::Kind::Experience) { scene.AddExperience(action.amount); }
                else if (action.kind == PickupAction::Kind::Health) { scene.AddHealth(action.amount); }
                else if (action.kind == PickupAction::Kind::StoreItem) { GrantStoreItem(action.resource); }
                else if (action.kind == PickupAction::Kind::Sound) {
                    GunCue cue;
                    cue.kind = GunCue::Kind::Sound;
                    cue.resource = action.resource;
                    effects.Emit(cue, instance.x, instance.y, 0, 0);
                }
            }
            failures += instance.pickup.GetUnsupportedCount();
            collections.push_back({instance.visual->entry->ref, instance.objectId});
            std::printf("[pickup] collected %s id=%d\n", instance.visual->entry->owner.c_str(), instance.objectId);
        }
        m_instances.erase(m_instances.begin() + index);
    }
}

void PickupScene::Draw(const float *mvp, float scale) {
    m_batch.Begin();
    for (const auto &instance : m_instances) {
        const auto &quads = instance->visual->frames[instance->animation.GetStep()];
        for (const SpriteQuad &quad : quads) {
            m_batch.AddTransformedQuad(*quad.page, instance->x + quad.offsetX * scale,
                instance->y + quad.offsetY * scale, quad.Width() * scale, quad.Height() * scale,
                quad.source, quad.flipHorizontal, quad.flipVertical, quad.blend, 0, 0, 1, 1, 0, 1, quad.rotateTexture);
        }
    }
    m_batch.Upload();
    m_batch.Draw(m_program, mvp);
}
