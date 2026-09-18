#include "gun_bros_re/gameplay/map/CMapResources.h"
#include "engine/resources/CResTOCManager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace MapDetail {

/**
 * Expand every step of one animation into the quads that step draws.
 *
 * An unused slot -- animation 255, which is how a prop says it has no
 * foreground or no main sprite -- comes back empty, and so does one whose
 * animation is out of range. Neither is an error: most templates fill one slot
 * of the three, and a player pointed at an empty slot simply never ticks.
 */
void ExpandSlot(CSpriteIterator &iterator, const ZSpriteArchetype &archetype,
                std::uint8_t animationIndex, CProp::Animation &out) {
    if (animationIndex == kNoSpriteGluIndex) {
        return;
    }
    if (animationIndex >= archetype.GetAnimationCount()) {
        return;
    }

    const ZSpriteAnimation &animation = archetype.GetAnimation(animationIndex);
    const std::size_t stepCount = animation.steps.size();

    out.quadsByStep.resize(stepCount);
    out.stepDurationsMs.resize(stepCount);

    for (std::size_t step = 0; step < stepCount; ++step) {
        out.stepDurationsMs[step] = animation.steps[step].durationMs;
        iterator.Expand(animationIndex, static_cast<std::uint32_t>(step),
                        out.quadsByStep[step]);
    }
}

/**
 * Whether any of a template's slots has more than one step to play.
 *
 * Most scenery is a single step and stands perfectly still, which is correct
 * and also indistinguishable from a broken clock. Counting the ones that can
 * move is what tells the two apart without staring at the window.
 */
bool PropAnimates(const CProp::Resources &resources) {
    for (const auto &animation : resources.animations) {
        if (animation.stepDurationsMs.size() > 1) { return true; }
    }
    return false;
}

/** Whether a slot draws anything on its first step. */

/** Whether a PROP reference names one of the two PvP barricade orientations. */

/** Build the known visual states named by the three original prop scripts. */

/**
 * Expand one prop template into the quads its three slots draw.
 *
 * This is CProp::Bind's job: it resolves the sprite reference to an archetype
 * and points three CSpritePlayers at three animations of it. The z-order group
 * falls out of which of the three ended up with an animation, exactly as
 * CProp::GetZOrderGroup computes it.
 */
} // namespace MapDetail

using namespace MapDetail;
bool CProp::Resources::Load(CResTOCManager &tocManager, CMap &loaded,
    std::uint32_t propPackHash, std::uint8_t localIndex) {
    Resources &out = *this;
    std::vector<std::uint8_t> payload;
    if (!ReadSectionResource(tocManager, loaded, propPackHash, ZGameSection::Prop,
                             localIndex, payload)) {
        return false;
    }

    CArrayInputStream stream(payload);
    CProp::Template propTemplate;
    if (!propTemplate.Init(stream)) {
        return false;
    }
    out.data = propTemplate;
    out.resource.packHash = propPackHash;
    out.resource.localIndex = localIndex;

    // The sprite lives in whichever pack the reference names, which need not
    // be the one the template came from.
    const CGameSpriteGluRef &spriteRef = propTemplate.GetSpriteRef();
    const int gluPackIndex = tocManager.GetPackIndexFromHash(spriteRef.packHash);
    CMap::Resources::Pack *gluPack = GetPackResources(tocManager, loaded, gluPackIndex);
    if (gluPack == nullptr || !gluPack->spriteGluReady) {
        return false;
    }

    const ZSpriteArchetype *archetype =
        gluPack->spriteGlu.GetArchetype(spriteRef.archetype);
    if (archetype == nullptr) {
        return false;
    }

    CSpriteIterator iterator(gluPack->spriteGlu, *archetype);
    {
        out.animations.resize(archetype->GetAnimationCount());
        out.durations.resize(archetype->GetAnimationCount());
        for (unsigned animation = 0; animation < archetype->GetAnimationCount(); ++animation) {
            if (!propTemplate.GetScript().IsPresent() && animation != propTemplate.GetMainAnimation() &&
                animation != propTemplate.GetBackgroundAnimation() && animation != propTemplate.GetForegroundAnimation()) { continue; }
            ExpandSlot(iterator, *archetype, static_cast<std::uint8_t>(animation), out.animations[animation]);
            out.durations[animation] = out.animations[animation].stepDurationsMs;
        }
    }
    out.skippedParts = iterator.GetSkippedPartCount();
    out.unsupportedTransforms = iterator.GetUnsupportedTransformCount();

    return true;
}

namespace MapDetail {

/**
 * A repeatable stand-in for the Utility::Random in CProp::Bind.
 *
 * Bind starts the main slot on a random step so a field of identical rocks
 * does not pulse in unison. A real random would cost --screenshot its one
 * useful property, that two runs produce the same image, so this hashes the
 * prop's place in the draw order instead: scattered between neighbours, and
 * the same on every run.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:124916
 *
 * The multiplier is Knuth's, and the shift drops the low bits, which move too
 * regularly between consecutive ordinals to scatter anything.
 */

/** Background slot selected by a prop's current cover state. */

/**
 * Point one placed prop's three players at their slots, as CProp::Bind does.
 *
 * All three loop forwards, which is CSpritePlayer's constructed state and
 * which Bind never changes. Only the main slot starts part-way in; the other
 * two begin at step 0, so a prop's foreground and background stay in step with
 * each other however its body is phased.
 */
void StartPropPlayers(CProp &prop, std::size_t propOrdinal) {
    // Stable research seed; Bind uses the original inclusive random-frame operation.
    prop.SetRandomSeed(static_cast<std::uint32_t>(propOrdinal + 1));
    prop.BindResources();
}

/** Preserve authored object indices before binding any Flow self pointer. */
bool PropSpawnOrder(const CProp &left, const CProp &right) {
    // Instances are bound after storage is stable; authored object order is retained.
    if (left.objectLayer != right.objectLayer) { return left.objectLayer < right.objectLayer; }
    return left.objectId < right.objectId;
}

}
