#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace MenuDetail {
/** Windows pointer adapter for CMenuMovieControl's continuous playback.
* Geometry and base speed come from the Movie. Integrate native damping at a
 * fixed 60 Hz reference so desktop frame rate does not change flick distance.
 */
struct ZMenuScrollMotion {
    float velocity = 0;
    std::uint64_t lastTick = 0, lastMotionTick = 0;
    bool captured = false;

    void Update(float &position, std::uint64_t clock, float delta, float wheel, bool held,
        bool pressedInside, bool enabled, float maximum, float stride, unsigned duration, bool elastic = false) {
        const float seconds = std::min(0.05f, static_cast<float>(clock - lastTick) / 1000);
        lastTick = clock;
        if (!enabled || (maximum <= 0 && !elastic)) { velocity = 0; captured = false; return; }
        const float baseSpeed = stride * 1000 / std::max(1u, duration);
        if (pressedInside) { captured = true; velocity = 0; }
        const float bound = std::clamp(position, 0.0f, maximum);
        const float extension = position - bound;
        if (elastic && extension != 0) {
            // CMenuMovieControl::DampenHyperExtension :140984 has a default
            // limit of three options. Input moving back toward the list is undamped.
            if (captured && held && delta * extension < 0) {
                delta *= std::max(0.0f, 1 - std::abs(extension) / (3 * stride));
            }
            if (!held) {
                // UpdatePlaybackSpeed :141207: return by remaining fraction +
                // whole options (capped at five) + 1/3, in chapter-time units.
                const float options = std::abs(extension) / stride;
                const float speed = std::min(5.0f, std::floor(options)) +
                    options - std::floor(options) + 1.0f / 3;
                const float distance = std::min(std::abs(extension), speed * baseSpeed * seconds);
                position -= std::copysign(distance, extension);
                velocity = 0;
                captured = false;
                return;
            }
        }
        if (captured && held) {
            position -= delta;
            if (delta != 0 && seconds > 0) {
                velocity = std::clamp(-delta / seconds, -5 * baseSpeed, 5 * baseSpeed);
                lastMotionTick = clock;
            } else if (clock - lastMotionTick > 80) { velocity = 0; }
        } else {
            captured = false;
            // CalculateBaseVelocity :141749 sets 250000 / chapter-ms;
            // UpdatePlaybackSpeed :141086 subtracts this * dt^2 / 2.
            const float deceleration = baseSpeed * (250000.0f / std::max(1u, duration)) / 120;
            const float speed = std::abs(velocity);
            const float travelTime = std::min(seconds, speed / deceleration);
            const float distance = speed * travelTime - deceleration * travelTime * travelTime / 2;
            if (velocity < 0) { position -= distance; }
            else { position += distance; }
            velocity = std::copysign(std::max(0.0f, speed - deceleration * seconds), velocity);
        }
        if (wheel != 0) { position -= wheel * stride; velocity = 0; }
        if (elastic) { position = std::clamp(position, -3 * stride, maximum + 3 * stride); }
        else { position = std::clamp(position, 0.0f, maximum); }
        if ((position == 0 && velocity < 0) || (position == maximum && velocity > 0)) { velocity = 0; }
    }
};

// Each page owns its binding state and playback cursor.
}
