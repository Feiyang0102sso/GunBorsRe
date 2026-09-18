/**
 * @file CSpritePlayer.cpp
 * @brief Keeps one animation's playback position: which step, for how long.
 */

#include "engine/glu/sprite/CSpritePlayer.h"

CSpritePlayer::CSpritePlayer()
    : m_stepDurationsMs(nullptr),
      m_stepIndex(0),
      m_remainingMs(0),
      m_looping(true),
      m_reversed(false),
      m_finished(false) {}

std::uint32_t CSpritePlayer::StepCount() const {
    if (m_stepDurationsMs == nullptr) {
        return 0;
    }
    return static_cast<std::uint32_t>(m_stepDurationsMs->size());
}

void CSpritePlayer::SetAnimation(const std::vector<std::uint16_t> *stepDurationsMs) {
    m_drawData.reset();
    m_stepDurationsMs = stepDurationsMs;
    m_stepIndex = 0;
    m_finished = false;

    // Unlike EnterStep this takes the first step's full duration: there is no
    // previous step to have overshot.
    m_remainingMs = 0;
    if (StepCount() > 0) {
        m_remainingMs = (*m_stepDurationsMs)[0];
    }
}

void CSpritePlayer::SetStep(std::uint32_t step) {
    EnterStep(step);
}

void CSpritePlayer::Update(std::uint16_t deltaMs) {
    m_finished = false;
    if (StepCount() == 0) {
        return;
    }

    m_remainingMs -= deltaMs;
    if (m_remainingMs <= 0) {
        AdvanceStep();
    }
}

void CSpritePlayer::AdvanceStep() {
    const std::uint32_t stepCount = StepCount();
    const std::uint32_t currentStep = m_stepIndex;
    std::uint32_t nextStep = 0;

    if (m_reversed) {
        if (currentStep > 0) {
            nextStep = currentStep - 1;
        } else {
            m_finished = true;
            if (!m_looping) {
                // Parked on the first step. The clock stays overrun, so every
                // later Update comes back here and stops again.
                return;
            }
            nextStep = stepCount - 1;
        }
    } else {
        if (currentStep + 1 < stepCount) {
            nextStep = currentStep + 1;
        } else {
            m_finished = true;
            if (!m_looping) {
                return;
            }
            nextStep = 0;
        }
    }

    // A single-step animation loops onto itself. The original returns without
    // touching the clock, which is what keeps a still prop still.
    if (nextStep == currentStep) {
        return;
    }

    EnterStep(nextStep);
}

void CSpritePlayer::EnterStep(std::uint32_t step) {
    if (step >= StepCount()) {
        return;
    }

    m_stepIndex = step;

    // Carry the overshoot: the clock is already negative by however much the
    // last frame overran the previous step, and this step is that much short.
    const std::int32_t duration = (*m_stepDurationsMs)[step];
    std::int32_t startingTime = duration + m_remainingMs;
    if (m_remainingMs >= 0) {
        startingTime = duration;
    }

    // ...but never start a step with less than half of it left, so one very
    // slow frame cannot skip an animation forward without limit.
    const std::int32_t floorTime = duration / 2;
    if (startingTime < floorTime) {
        startingTime = floorTime;
    }
    m_remainingMs = startingTime;
}
