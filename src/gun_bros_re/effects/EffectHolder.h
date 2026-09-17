#pragma once
#include <cstddef>
#include <cstdint>
class ZSpriteRenderer;
class ZEffectColors;
struct ZEffectProjection;

/** Original EffectHolder interface (:295136); hosts supply their live anchor. */
class EffectHolder {
public:
    struct Anchor {
        float x = 0, y = 0, z = 0, angle = 0;
        bool alive = true;
    };
    virtual ~EffectHolder() = default;
    virtual void Update(const Anchor &anchor, int deltaMs, std::uint32_t &randomState) = 0;
    virtual void Draw(ZSpriteRenderer &sprites, ZEffectColors &colors, const ZEffectProjection &projection) const = 0;
    virtual void Stop() = 0;
    virtual void StopInstant() = 0;
    virtual bool IsDone() const = 0;
    virtual std::size_t GetParticleCount() const { return 0; }
    virtual bool IsParticle() const { return false; }
    virtual bool IsRibbon() const { return false; }
};
