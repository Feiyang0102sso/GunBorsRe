/** Original generic prompt playback; rendered by the host Movie adapter. */
#pragma once
#include "engine/glu/movie/CMovie.h"
#include <algorithm>

class CMenuPopupPrompt {
public:
    enum class State { Closed, Opening, Ready, Shrinking, Closing };

    /** BindContent :207403 computes a target within chapter 1 from the actual
     * font/image content height and the two authored region heights. */
    bool Bind(const CMovie &movie, float minimumHeight, float maximumHeight, float contentHeight) {
        unsigned end = 0;
        if (!movie.GetChapterRange(1, expansionStart, end) ||
            !movie.GetChapterRange(3, closingStart, closingEnd) || maximumHeight <= minimumHeight) { return false; }
        const float fraction = std::min(1.0f, (contentHeight - minimumHeight) / (maximumHeight - minimumHeight));
        const int expansion = std::max(1, static_cast<int>((end - expansionStart) * fraction));
        target = expansionStart + static_cast<unsigned>(expansion);
        time = 0;
        contentAlpha = 0;
        state = State::Opening;
        return true;
    }

    /** Show/Update/Hide :207146-207330; no fixed host popup duration. */
    void Update(unsigned deltaMs) {
        if (state == State::Opening) {
            time += std::min(deltaMs, target - time);
            if (time > expansionStart) {
                contentAlpha = std::min(1.0f, contentAlpha + static_cast<float>(deltaMs) / (target - expansionStart));
            }
            if (time == target) { state = State::Ready; }
        } else if (state == State::Shrinking) {
            contentAlpha = std::max(0.0f, contentAlpha - static_cast<float>(deltaMs) / (target - expansionStart));
            if (deltaMs > time - expansionStart) {
                time = closingStart;
                state = State::Closing;
            } else {
                time -= deltaMs;
            }
        } else if (state == State::Closing) {
            if (deltaMs > closingEnd - time) {
                time = closingEnd;
                state = State::Closed;
            } else {
                time += deltaMs;
            }
        }
    }

    void Hide() {
        if (state == State::Opening || state == State::Ready) {
            time = std::max(time, expansionStart);
            state = State::Shrinking;
        }
    }
    bool IsActive() const { return state != State::Closed; }
    bool IsReady() const { return state == State::Ready; }
    unsigned MovieTime() const { return time; }
    unsigned TargetTime() const { return target; }
    float ContentAlpha() const { return contentAlpha; }

private:
    State state = State::Closed;
    unsigned time = 0, target = 0;
    unsigned expansionStart = 0, closingStart = 0, closingEnd = 0;
    float contentAlpha = 0;
};
