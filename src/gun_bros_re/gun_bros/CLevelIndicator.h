/** @file CLevelIndicator.h
 * @brief Original offscreen target marker and its 200 ms cosine fade.
 */
#ifndef GUN_BROS_RE_CLEVELINDICATOR_H
#define GUN_BROS_RE_CLEVELINDICATOR_H
#include <cmath>
#include <cstdint>

struct CLevelIndicator {
    int objectId = -1;
    std::uint64_t targetKey = 0; // Stable host handle; script IDs may be reused.
    unsigned type = 0;
    float x = 0, y = 0;
    unsigned elapsedMs = 0;
    int fade = -1;

    // CLevelIndicator::FadeOut :191302 / Update :191533 uses 1000 units
    // decremented by five per millisecond, not a one-second fade.
    void FadeOut() { if (fade < 0) { fade = 1000; } }
    void Update(int deltaMs) {
        elapsedMs += deltaMs;
        if (fade > 0) {
            fade -= deltaMs * 5;
            if (fade < 0) { fade = 0; }
        }
    }
    float Alpha() const {
        if (fade < 0) { return 1; }
        return (1 - std::cos(3.14159265f * fade / 1000)) * 0.5f;
    }
    bool IsDone() const { return fade == 0; }
};
#endif
