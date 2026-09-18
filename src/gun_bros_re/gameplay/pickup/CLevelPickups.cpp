/** CLevel pickup scheduling and notifications (:118288), with desktop peer reward routing. */
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/CMPMatch.h"
#include <cstdio>

bool CLevel::InitPickups(CResTOCManager &toc, ZPackTables &tables,
    const ZShaderProgram &program, CProfileManager *profile) {
    ResetPickups();
    m_pickupBatch.reset();
    m_pickupTemplates.clear();
    m_pickupSpritePacks.clear();
    m_pickupProfile = profile;
    unsigned templateCount = 0;
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const auto *pack = toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Pickup);
        if (count == 0) { continue; }
        auto &templates = m_pickupTemplates[pack->GetPackHash()];
        templates.resize(count);
        for (unsigned index = 0; index < count; ++index) {
            GameObjectRef resource;
            resource.packHash = pack->GetPackHash();
            resource.localIndex = static_cast<std::uint8_t>(index);
            if (!templates[index].Load(tables, resource)) { return false; }
        }
        templateCount += count;
    }
    std::printf("[pickup] templates=%u\n", templateCount);
    if (templateCount == 0) { return false; }
    // Keep templates and expanded animation data stable for live CPickup objects.
    // CSpriteGlu shares each pack's textures; players own their expanded frames.
    for (const auto &pack : m_pickupTemplates) {
        for (const auto &data : pack.second) {
            const auto &ref = data.sprite;
            if (m_pickupSpritePacks.count(ref.packHash) == 0) {
                const int pack = toc.GetPackIndexFromHash(ref.packHash);
                if (pack < 0) { return false; }
                auto glu = std::make_unique<CSpriteGlu>();
                if (!glu->Init(*toc.GetPack(pack))) { return false; }
                m_pickupSpritePacks[ref.packHash] = std::move(glu);
            }
            // Preserve eager validation of every authored animation before gameplay.
            CSpritePlayer sprite;
            if (!sprite.Init(*m_pickupSpritePacks.at(ref.packHash), ref.archetype, ref.animation)) { return false; }
        }
    }
    auto batch = std::make_unique<ZQuadBatch>();
    if (!batch->Create(program)) { return false; }
    m_pickupBatch = std::move(batch);
    return true;
}

void CLevel::ResetPickups() {
    m_objects.ClearPickups();
    m_pickupCollections.clear();
    m_pickupSpawned = 0;
    m_pickupCollected = 0;
    m_pickupFailures = 0;
}

bool CLevel::SpawnPickupAt(const GameObjectRef &resource, float x, float y, int objectId) {
    if (m_pickupBatch == nullptr) { return false; }
    const auto pack = m_pickupTemplates.find(resource.packHash);
    if (pack == m_pickupTemplates.end() || resource.localIndex >= pack->second.size()) {
        ++m_pickupFailures;
        std::printf("[pickup] missing %08x:%u\n", resource.packHash, resource.localIndex);
        return false;
    }
    const auto &data = pack->second[resource.localIndex];
    const auto &effectRef = data.particleEffect;
    const CParticleEffect *effect = nullptr;
    if (!effectRef.IsNull()) {
        effect = m_particleResources->Get(effectRef);
        if (effect == nullptr) { ++m_pickupFailures; return false; }
    }
    auto *pickup = m_objects.GetPickup();
    if (pickup == nullptr) { return false; }
    if (!pickup->Bind(data, resource, *m_pickupSpritePacks.at(data.sprite.packHash), m_mapParticles, effect)) {
        m_objects.ReleasePickup(m_objects.GetPickups().size() - 1);
        ++m_pickupFailures;
        std::printf("[pickup] invalid sprite %08x:%u\n", resource.packHash, resource.localIndex);
        return false;
    }
    pickup->SetLevelContext(this);
    pickup->Spawn(x, y, objectId, ++m_pickupSpawned);
    SetIndicator(objectId, 1, (1ULL << 32) | pickup->GetSerial());
    std::printf("[pickup] spawned %08x:%u id=%d at=%.1f,%.1f\n", resource.packHash, resource.localIndex, objectId, x, y);
    return true;
}

void CLevel::UpdatePickupAnimations(int deltaMs) {
    for (const auto &pickup : m_objects.GetPickups()) { pickup->Update(deltaMs); }
}

void CLevel::ApplyPickupActions(CPickup &pickup, unsigned peer) {
    for (const ZPickupAction &action : pickup.TakeActions()) {
        if (action.kind == ZPickupAction::Kind::Xplodium) {
            if (peer != 0) { AddPeerXplodium(action.amount); } else { AddXplodium(action.amount); }
        } else if (action.kind == ZPickupAction::Kind::Experience) {
            if (peer != 0) { AddPeerExperience(action.amount); } else { AddExperience(action.amount); }
        } else if (action.kind == ZPickupAction::Kind::Health) {
            if (peer != 0) {
                auto *vitals = GetBrotherVitals();
                vitals->health = std::min(vitals->maximum, vitals->health + action.amount);
            } else { AddHealth(action.amount); }
        } else if (action.kind == ZPickupAction::Kind::StoreItem) {
            CProfileManager *profile = m_pickupProfile;
            if (peer != 0) { profile = m_peerProfile; }
            m_pickupFailures += m_actor.CollectItem(*m_tables, action.resource, profile);
        }
        else if (action.kind == ZPickupAction::Kind::Sound) {
            ZGunCue cue;
            cue.kind = ZGunCue::Kind::Sound;
            cue.resource = action.resource;
            Emit(cue, pickup.GetX(), pickup.GetY(), 0, 0);
        }
    }
    m_pickupFailures += pickup.GetUnsupportedCount();
}

void CLevel::UpdatePickups(int deltaMs) {
    UpdatePickupAnimations(deltaMs);
    m_pickupCollections.clear();
    const auto &pickups = m_objects.GetPickups();
    for (std::size_t index = 0; index < pickups.size();) {
        CPickup &pickup = *pickups[index];
        const bool playerTouch = TouchesPickup(pickup.GetX(), pickup.GetY());
        bool peerTouch = BrotherTouchesPickup(pickup.GetX(), pickup.GetY());
        if (playerTouch && peerTouch) {
            peerTouch = IsDeathmatch() && BrotherIsCloser(pickup.GetX(), pickup.GetY());
        }
        if (!playerTouch && !peerTouch) { ++index; continue; }
        if (pickup.Collect()) {
            ++m_pickupCollected;
            unsigned peer = 0;
            if (peerTouch) { peer = 1; }
            ApplyPickupActions(pickup, peer);
            m_pickupCollections.push_back({pickup.GetResource(), pickup.GetID(), peer});
            const auto &resource = pickup.GetResource();
            std::printf("[pickup] collected %08x:%u id=%d\n", resource.packHash, resource.localIndex, pickup.GetID());
        }
        m_objects.ReleasePickup(index);
    }
    // Keep the previous frame order: all collection rewards precede LEVEL callbacks.
    for (const auto &pickup : m_pickupCollections) {
        if (m_match != nullptr && pickup.objectId >= CMPMatch::PickupIdBase &&
            !CollectMatchWeapon(pickup.peer, pickup.objectId - CMPMatch::PickupIdBase)) {
            RecordInvalidSpawn();
        }
        OnPickupCollected(pickup.objectId, pickup.resource);
    }
}

void CLevel::DrawPickups(const float *matrix, float scale) {
    if (m_pickupBatch == nullptr) { return; }
    m_pickupBatch->Begin();
    for (const auto &pickup : m_objects.GetPickups()) { pickup->Draw(*m_pickupBatch, scale); }
    m_pickupBatch->Upload();
    m_pickupBatch->Draw(*m_program, matrix);
}

bool CLevel::GetPickupPosition(int objectId, float &x, float &y) const {
    for (const auto &pickup : m_objects.GetPickups()) {
        if (pickup->GetID() == objectId) { x = pickup->GetX(); y = pickup->GetY(); return true; }
    }
    return false;
}

bool CLevel::GetPickupIndicatorTarget(unsigned serial, float &x, float &y) const {
    for (const auto &pickup : m_objects.GetPickups()) {
        if (pickup->GetSerial() == serial) { x = pickup->GetX(); y = pickup->GetY(); return true; }
    }
    return false;
}

bool CLevel::FindNearestPickup(float x, float y, float &goalX, float &goalY) const {
    float nearest = 1000000000.0f;
    bool found = false;
    for (const auto &pickup : m_objects.GetPickups()) {
        const float dx = pickup->GetX() - x, dy = pickup->GetY() - y;
        const float distance = dx * dx + dy * dy;
        if (distance < nearest) { nearest = distance; goalX = pickup->GetX(); goalY = pickup->GetY(); found = true; }
    }
    return found;
}
