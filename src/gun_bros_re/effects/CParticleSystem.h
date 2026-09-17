#pragma once
#include "gun_bros_re/effects/CParticleEffectPlayer.h"
#include <array>

/** CMap's transient particle players (:133841-133966).
 * Effect slots and particle slots are independent original limits.
 * UI, CEffectLayer and powerup-owned players do not use this array.
 */
class CParticleSystem {
public:
    static constexpr std::size_t kEffectSlots = 20;
    static constexpr std::size_t kParticleSlots = 200;
    using Handle = std::uint64_t;
    struct RenderItem {
        const CParticleEffectPlayer *player = nullptr;
        std::size_t index = 0;
        int group = 3;
        int y = 0;
    };

    CParticleSystem();
    CParticleEffectPlayer *AddEffect(const CParticleEffect &effect, float x, float y);
    /** Host identity prevents a retired handle from affecting a reused slot. */
    Handle GetHandle(const CParticleEffectPlayer &player) const;
    CParticleEffectPlayer *Get(Handle handle);
    const CParticleEffectPlayer *Get(Handle handle) const;
    void SetOwner(Handle handle, std::uint64_t actor, int linkedSlot = -1);
    void StopLinked(std::uint64_t actor, int linkedSlot, bool immediately);
    void RetireOwner(std::uint64_t actor);
    bool HasActorBurst(std::uint64_t actor) const;
    void Update(int deltaMs, std::uint32_t &randomState);
    void QueueParticles(ZSpriteRenderer &renderer, const float *projection = nullptr) const;
    std::vector<RenderItem> GetRenderItems() const;
    void Clear();
    std::size_t GetEffectCount() const;
    std::size_t GetParticleCount() const;
    const std::shared_ptr<CParticlePool> &GetPool() const { return m_pool; }
private:
    struct Slot {
        CParticleEffectPlayer player;
        Handle handle = 0;
        std::uint64_t actor = 0;
        int linkedSlot = -1;
    };
    std::shared_ptr<CParticlePool> m_pool;
    std::array<Slot, kEffectSlots> m_slots;
    Handle m_nextHandle = 1;
};
