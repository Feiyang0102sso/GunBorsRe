#include "gun_bros_re/effects/CParticleSystem.h"
#include <algorithm>

CParticleSystem::CParticleSystem() : m_pool(std::make_shared<CParticlePool>(kParticleSlots)) {}

CParticleEffectPlayer *CParticleSystem::AddEffect(const CParticleEffect &effect, float x, float y) {
    // AddEffect :133878 scans finished players in allocation order. No queue,
    // replacement or growth when all twenty are busy.
    for (auto &slot : m_slots) {
        if (!slot.player.IsDone()) { continue; }
        slot.player.Init(effect, m_pool);
        slot.player.SetWorldSpace(true);
        slot.player.SetLooping(false);
        slot.player.SetPosition(x, y, 0, 0);
        slot.handle = m_nextHandle++;
        slot.actor = 0;
        slot.linkedSlot = -1;
        return &slot.player;
    }
    return nullptr;
}

CParticleSystem::Handle CParticleSystem::GetHandle(const CParticleEffectPlayer &player) const {
    for (const auto &slot : m_slots) {
        if (&slot.player == &player) { return slot.handle; }
    }
    return 0;
}

CParticleEffectPlayer *CParticleSystem::Get(Handle handle) {
    if (handle == 0) { return nullptr; }
    for (auto &slot : m_slots) {
        if (slot.handle == handle) { return &slot.player; }
    }
    return nullptr;
}

const CParticleEffectPlayer *CParticleSystem::Get(Handle handle) const {
    if (handle == 0) { return nullptr; }
    for (const auto &slot : m_slots) {
        if (slot.handle == handle) { return &slot.player; }
    }
    return nullptr;
}

void CParticleSystem::Update(int deltaMs, std::uint32_t &randomState) {
    for (auto &slot : m_slots) { slot.player.Update(deltaMs, randomState); }
}

void CParticleSystem::QueueParticles(ZSpriteRenderer &renderer, const float *projection) const {
    auto items = GetRenderItems();
    // Standalone research has no map queue; preserve its same group/Y order.
    std::stable_sort(items.begin(), items.end(), [](const RenderItem &left, const RenderItem &right) {
        if (left.group != right.group) { return left.group < right.group; }
        return left.y < right.y;
    });
    for (const auto &item : items) { item.player->QueueParticle(item.index, renderer, projection); }
}

std::vector<CParticleSystem::RenderItem> CParticleSystem::GetRenderItems() const {
    std::vector<RenderItem> items;
    for (const auto &slot : m_slots) {
        for (std::size_t index = 0; index < slot.player.GetParticleCount(); ++index) {
            const auto &particle = slot.player.GetParticle(index);
            items.push_back({&slot.player, index, particle.zOrderGroup, static_cast<int>(particle.y)});
        }
    }
    return items;
}

void CParticleSystem::Clear() {
    for (auto &slot : m_slots) {
        slot.player.Stop();
        slot.player.SetAnchor({});
        slot.handle = 0;
        slot.actor = 0;
        slot.linkedSlot = -1;
    }
}

std::size_t CParticleSystem::GetEffectCount() const {
    std::size_t count = 0;
    for (const auto &slot : m_slots) {
        if (!slot.player.IsDone()) { ++count; }
    }
    return count;
}

std::size_t CParticleSystem::GetParticleCount() const {
    std::size_t count = 0;
    for (const auto &slot : m_slots) { count += slot.player.GetParticleCount(); }
    return count;
}

void CParticleSystem::SetOwner(Handle handle, std::uint64_t actor, int linkedSlot) {
    for (auto &slot : m_slots) {
        if (slot.handle != handle || handle == 0) { continue; }
        slot.actor = actor;
        slot.linkedSlot = linkedSlot;
        return;
    }
}

void CParticleSystem::StopLinked(std::uint64_t actor, int linkedSlot, bool immediately) {
    for (auto &slot : m_slots) {
        if (slot.actor != actor || slot.linkedSlot != linkedSlot || slot.player.IsDone()) { continue; }
        if (immediately) { slot.player.Stop(); }
        else { slot.player.StopSpawning(); }
        slot.player.SetAnchor({});
        slot.actor = 0;
    }
}

void CParticleSystem::RetireOwner(std::uint64_t actor) {
    for (auto &slot : m_slots) {
        if (slot.actor != actor || slot.player.IsDone()) { continue; }
        // Finite death bursts keep draining at the last valid position. Only
        // linked looping instances stop spawning when the actor goes away.
        slot.player.SetAnchor({});
        if (slot.linkedSlot >= 0) { slot.player.StopSpawning(); }
    }
}

bool CParticleSystem::HasActorBurst(std::uint64_t actor) const {
    for (const auto &slot : m_slots) {
        if (slot.actor == actor && slot.linkedSlot < 0 && !slot.player.IsDone()) { return true; }
    }
    return false;
}
