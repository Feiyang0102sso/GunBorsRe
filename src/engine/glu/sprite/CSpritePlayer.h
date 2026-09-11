/**
 * @file CSpritePlayer.h
 * @brief Keeps one animation's playback position: which step, for how long.
 *
 * Port of CSpritePlayer (src/spriteGlu3/spritePlayer.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:58838 (Update), :58766 (AdvanceFrame),
 *            :58665 (SetFrame), :58861 (SetAnimation), :58340 (constructor)
 *
 * The original player owns the whole draw path -- it holds the archetype, walks
 * to the frame, and blits it. This one is only the clock. Everything downstream
 * of "which step are we on" is already done by CSpriteIterator, and callers
 * expand their steps once at load rather than per frame, so a player that also
 * carried the archetype would just be a pointer nobody follows.
 *
 * What it therefore needs is one number per step: how long that step lasts.
 * That is `stepDurationsMs`, which the caller owns and must outlive the player.
 *
 * The one piece of behaviour worth having copied exactly is the time carry in
 * EnterStep. A step does not restart the clock at its full duration; it starts
 * at whatever the previous step overshot by, so a slow frame does not stretch
 * the animation. The `duration / 2` floor caps how much a single very slow
 * frame can skip.
 */

#ifndef GUN_BROS_RE_SPRITE_GLU_CSPRITEPLAYER_H
#define GUN_BROS_RE_SPRITE_GLU_CSPRITEPLAYER_H

#include <cstdint>
#include <vector>

/**
 * A playhead over one animation's steps.
 *
 * Constructed looping and forward, which is what CSpritePlayer's constructor
 * sets and what every prop keeps -- scenery animations run until the map is
 * unloaded. The two flags exist because the original has them and because
 * anything that dies on screen will need them.
 */
class CSpritePlayer {
public:
    CSpritePlayer();

    /**
     * Point the player at an animation and rewind it.
     *
     * @param stepDurationsMs One duration per step, owned by the caller. Null
     *                        or empty parks the player: Update does nothing.
     */
    void SetAnimation(const std::vector<std::uint16_t> *stepDurationsMs);

    /** Jump to a step, carrying time the way advancing to it would. */
    void SetStep(std::uint32_t step);

    /**
     * Advance the clock, stepping the animation on when the step runs out.
     *
     * At most one step per call, as the original does. A frame long enough to
     * cover several steps therefore only shows the next one, but the carry in
     * EnterStep means the ones after it come round immediately rather than
     * being stretched.
     */
    void Update(std::uint16_t deltaMs);

    std::uint32_t GetStep() const { return m_stepIndex; }

    /** Whether the last Update walked off the end of the animation. */
    bool HasFinished() const { return m_finished; }

    void SetLooping(bool looping) { m_looping = looping; }
    void SetReversed(bool reversed) { m_reversed = reversed; }

private:
    /** Move to the next step in the play direction, looping or stopping. */
    void AdvanceStep();

    /** Make `step` current and set the clock for it, carrying any overshoot. */
    void EnterStep(std::uint32_t step);

    std::uint32_t StepCount() const;

    const std::vector<std::uint16_t> *m_stepDurationsMs;
    std::uint32_t m_stepIndex;

    /**
     * Milliseconds left on the current step; goes negative when a frame
     * overruns it, and EnterStep pays that back.
     *
     * The original is an __int16. This is wider only to keep the arithmetic
     * obviously in range -- durations are tens to a few thousand milliseconds,
     * nowhere near either type's limit, so the two behave identically.
     */
    std::int32_t m_remainingMs;

    bool m_looping;
    bool m_reversed;
    bool m_finished;
};

#endif  // GUN_BROS_RE_SPRITE_GLU_CSPRITEPLAYER_H
