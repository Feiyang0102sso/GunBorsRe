#pragma once
#include <cstdint>

/** Optional presentation pass, owned by the window and destroyed before its GL context. */
class IWindowOverlay {
public:
    virtual ~IWindowOverlay() = default;
    virtual void Tick(std::uint64_t now) = 0;
    virtual void Draw(int width, int height) = 0;
};
