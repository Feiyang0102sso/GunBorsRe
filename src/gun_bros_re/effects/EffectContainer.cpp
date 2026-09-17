#include "gun_bros_re/effects/EffectContainer.h"

EffectContainer::Handle EffectContainer::Attach(std::unique_ptr<EffectHolder> holder, Anchor anchor) {
    for (auto &entry : m_entries) {
        if (entry.holder) { continue; }
        entry.holder = std::move(holder);
        entry.anchor = std::move(anchor);
        entry.handle = m_nextHandle++;
        return entry.handle;
    }
    return 0; // Attach :294706 returns failure; it never replaces an older effect.
}

EffectHolder *EffectContainer::Get(Handle handle) {
    if (handle == 0) { return nullptr; }
    for (auto &entry : m_entries) {
        if (entry.handle == handle) { return entry.holder.get(); }
    }
    return nullptr;
}

void EffectContainer::Update(int deltaMs, std::uint32_t &randomState) {
    for (auto &entry : m_entries) {
        if (!entry.holder) { continue; }
        // Original Update :294655 releases previously completed slots first.
        if (entry.holder->IsDone()) { entry = {}; }
        else { entry.holder->Update(entry.anchor(), deltaMs, randomState); }
    }
}

void EffectContainer::Draw(ZSpriteRenderer &sprites, ZEffectColors &colors,
    const ZEffectProjection &projection, bool ribbons) const {
    for (const auto &entry : m_entries) {
        if (!entry.holder || entry.holder->IsDone()) { continue; }
        if (entry.holder->IsRibbon() == ribbons) { entry.holder->Draw(sprites, colors, projection); }
    }
}

void EffectContainer::Stop() {
    for (auto &entry : m_entries) {
        if (entry.holder) { entry.holder->Stop(); }
    }
}

void EffectContainer::Clear() {
    for (auto &entry : m_entries) { entry = {}; }
}

bool EffectContainer::IsDone() const {
    for (const auto &entry : m_entries) {
        if (entry.holder && !entry.holder->IsDone()) { return false; }
    }
    return true;
}

std::size_t EffectContainer::GetParticleCount() const {
    std::size_t count = 0;
    for (const auto &entry : m_entries) {
        if (entry.holder) { count += entry.holder->GetParticleCount(); }
    }
    return count;
}

std::size_t EffectContainer::GetEffectCount() const {
    std::size_t count = 0;
    for (const auto &entry : m_entries) {
        if (entry.holder && entry.holder->IsParticle() && !entry.holder->IsDone()) { ++count; }
    }
    return count;
}

std::size_t EffectContainer::GetRibbonCount() const {
    std::size_t count = 0;
    for (const auto &entry : m_entries) {
        if (entry.holder && entry.holder->IsRibbon() && !entry.holder->IsDone()) { ++count; }
    }
    return count;
}
