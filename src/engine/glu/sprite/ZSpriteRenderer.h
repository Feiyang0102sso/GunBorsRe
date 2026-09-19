#pragma once
/** Shared BIG Sprite expansion and Windows quad submission. No gameplay state.
 * Uses CSpriteIterator (original Draw :59035), sprite_archetype.bt.
 */
#include "engine/glu/sprite/CSpriteIterator.h"
#include "engine/resources/CResTOCManager.h"
#include <map>
#include <memory>

struct ZVisualAnimation {
    std::vector<std::vector<ZSpriteQuad>> frames;
    std::vector<int> endMs;
    int durationMs = 0;
};

std::size_t AnimationFrame(const ZVisualAnimation &animation, float ageMs);
void FrameBounds(const ZVisualAnimation &animation, float ageMs, float &top, float &bottom);
void FrameBoundsAt(const ZVisualAnimation &animation, std::size_t frame, float &top, float &bottom);

class ZSpriteRenderer {
public:
    ZSpriteRenderer(CResTOCManager &manager, const ZShaderProgram &shader);
    ZVisualAnimation &Animation(std::uint32_t packHash, int archetype, int animation);
    void AddSprite(ZVisualAnimation &animation, float ageMs, float x, float y,
        float scaleX, float scaleY, float angle, float alpha);
    void AddFrame(const ZVisualAnimation &animation, std::size_t frame, float x, float y,
        float scaleX, float scaleY, float angle, float alpha);
    void Begin();
    void Draw(const float *matrix);
    ZQuadBatch &Batch() { return batch; }
private:
    CResTOCManager &toc;
    const ZShaderProgram &program;
    ZQuadBatch batch;
    std::map<std::uint32_t, std::unique_ptr<CSpriteGlu>> spritePacks;
    std::map<std::uint64_t, ZVisualAnimation> animations;
};
