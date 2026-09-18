#pragma once
/** Immutable BIG sprite frames for CProp::Bind/Draw; prop.cpp :124863/:124746.
 * This cache is a desktop storage detail, not an original resource format.
 */
#include "gun_bros_re/gameplay/map/CProp.h"
#include "engine/glu/sprite/CSpriteIterator.h"
struct CProp::Animation {
    // One quad list per animation step, so playback is a subscript rather than
    // a walk back down the sprite tree. A template's steps run to a couple of
    // dozen at most, and expanding them all costs a fraction of the archive
    // read that got us the template in the first place.
    std::vector<std::vector<ZSpriteQuad>> quadsByStep;

    // The same steps' durations, which is all a CSpritePlayer needs.
    std::vector<std::uint16_t> stepDurationsMs;
};

/** Shared template and expanded animations; CProp owns all mutable playheads. */
class CMap;
class CResTOCManager;

struct CProp::Resources {
    bool Load(CResTOCManager &toc, CMap &map, std::uint32_t packHash, std::uint8_t localIndex);
    Template data;
    GameObjectRef resource;
    std::vector<Animation> animations;
    std::vector<std::vector<std::uint16_t>> durations;
    std::uint32_t skippedParts = 0;
    std::uint32_t unsupportedTransforms = 0;
};

namespace MapDetail {
void ExpandSlot(CSpriteIterator &iterator, const ZSpriteArchetype &archetype,
                std::uint8_t animationIndex, CProp::Animation &out);
bool PropAnimates(const CProp::Resources &sprite);
void StartPropPlayers(CProp &prop, std::size_t propOrdinal);
bool PropHasCollision(const CProp &prop);
bool PropSpawnOrder(const CProp &left, const CProp &right);
}
