#pragma once
#include "gun_bros_re/effects/EffectHolder.h"
#include <array>
#include <functional>
#include <memory>

/** CBullet's four attached effects (:63810), separate from map effect slots. */
class EffectContainer {
public:
    using Handle = std::uint64_t;
    using Anchor = std::function<EffectHolder::Anchor()>;
    Handle Attach(std::unique_ptr<EffectHolder> holder, Anchor anchor);
    EffectHolder *Get(Handle handle);
    void Update(int deltaMs, std::uint32_t &randomState);
    void Draw(ZSpriteRenderer &sprites, ZEffectColors &colors, const ZEffectProjection &projection, bool ribbons) const;
    void Stop();
    void Clear();
    bool IsDone() const;
    std::size_t GetParticleCount() const;
    std::size_t GetEffectCount() const;
    std::size_t GetRibbonCount() const;
private:
    struct Entry {
        std::unique_ptr<EffectHolder> holder;
        Anchor anchor;
        Handle handle = 0;
    };
    std::array<Entry, 4> m_entries;
    Handle m_nextHandle = 1;
};
