#include "gun_bros_re/gameplay/CTargetingController.h"
#include "gun_bros_re/gameplay/brother/CBrotherAI.h"
#include <cmath>
#include <cstdio>
namespace {
class TargetingProbeWorld : public ZBrotherAIWorld {
public:
    float targetX = 100;
    bool available = true;
    ZCombatId FindBrotherTarget(float x, float y, float radius) override {
        if (available && std::hypot(targetX - x, y) < radius) { return 42; }
        return 0;
    }
    bool GetBrotherTarget(ZCombatId id, float &x, float &y) override {
        if (!available || id != 42) { return false; }
        x = targetX;
        y = 0;
        return true;
    }
    bool GetBrotherWaypoint(float, float, float, float, float &, float &) override { return false; }
    void ResolveBrotherForce(float, float, float &, float &) override {}
};
}
unsigned CheckTargetingController() {
    unsigned failures = 0;
    CTargetingController targeting;
    TargetingProbeWorld world;
    float facing = 270;
    bool shooting = false;
    for (int elapsed = 0; elapsed < 1600; elapsed += 16) { shooting = targeting.Update(16, 0, 0, facing, world); }
    if (!shooting || targeting.GetTarget() != 42 || std::abs(facing - 90) > 5.1f) { ++failures; }
    world.targetX = 210;
    targeting.Update(16, 0, 0, facing, world);
    if (targeting.GetTarget() != 42) { ++failures; }
    world.targetX = 221;
    targeting.Update(16, 0, 0, facing, world);
    if (targeting.GetTarget() != 0) { ++failures; }
    for (int elapsed = 0; elapsed < 800; elapsed += 16) { shooting = targeting.Update(16, 0, 0, facing, world); }
    if (shooting || targeting.GetTarget() != 0) { ++failures; }
    world.targetX = 100;
    for (int elapsed = 0; elapsed < 800; elapsed += 16) { targeting.Update(16, 0, 0, facing, world); }
    world.available = false;
    targeting.Update(16, 0, 0, facing, world);
    if (targeting.GetTarget() != 0) { ++failures; }
    std::printf("[targeting-check] acquire/retain/release/removed failures=%u\n", failures);
    return failures;
}
