#pragma once
#include <vector>

namespace MenuDetail {
struct ZMenuTransitionTrace {
    bool active = false;
    unsigned time = 0, starts = 0;
    struct Frame {
        unsigned page, headerTime, wipeTime;
        bool navigationReady, refineryExitPending, wipeActive;
    };
    std::vector<Frame> frames;
};

// Canvas-space tap plus optional deterministic clock/presentation increments.
struct ZMenuInputFrame { float x; float y; unsigned advanceMs = 0; unsigned renderDelayMs = 0; };
}
