/** Original gameplay contracts independent of rendering and platform input. */
#include "engine/core/CRandGen.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/multiplayer/CMPMatch.h"
#include <cmath>
#include <cstdio>

namespace {
class SeekingWorld final : public CBullet::World {
public:
    unsigned queries = 0;
    Collision::ObjectId selected = 20;
    bool firstAlive = true;
    float targetX = 0;
    float targetY = 100;
    Collision::Trace Trace(const Collision::Hit &, float, float, float, float, float,
        const std::vector<Collision::ObjectId> &) override { return {}; }
    Collision::HitResult ApplyHit(Collision::ObjectId, const Collision::Hit &) override { return Collision::HitResult::Ignored; }
    void Splash(const Collision::Hit &, float, float, float, int) override {}
    void SpawnFromProjectile(const GameObjectRef &, const Collision::Hit &) override {}
    Collision::ObjectId FindSeekTarget(const Collision::Hit &, float) override { ++queries; return selected; }
    bool Anchor(Collision::ObjectId id, int, int, float &x, float &y, float &, float &) override {
        if (id == 20 && !firstAlive) { return false; }
        x = targetX; y = targetY;
        return id == 20 || id == 21;
    }
};
}

int RunGameplayOwnershipCheck() {
    unsigned failures = 0;
    CRandGen random(5489);
    // Published MT19937 sequence, also reproduced by original Generate :370253.
    const std::uint32_t sequence[] = {3499211612u, 581869302u, 3890346734u, 3586334585u, 545404204u};
    for (const auto expected : sequence) { if (random.Generate() != expected) { ++failures; } }
    random.Seed(5489);
    if (random.Integer(4, 4) != 4 || random.Generate() != sequence[0]) { ++failures; }

    CMPMatch match;
    CLevel level;
    CBullet first, second;
    first.SetLevelContext(&level);
    second.SetLevelContext(&level);
    level.SetScriptRandomSeed(5489);
    const std::int16_t bounds[] = {0, 100};
    if (first.ResolveNativeFunction(0x0401, bounds, 2) != sequence[0] % 101 ||
        second.ResolveNativeFunction(0x0402, bounds, 2) != sequence[1] % 101) { ++failures; }
    if (&first.GetRandom() != &second.GetRandom()) { ++failures; }
    level.SetScriptRandomSeed(5489);
    if (*first.ResolveNativeVariable(0x0400) != (sequence[0] % 1001 >= 500)) { ++failures; }
    level.SetCooperative(true);
    level.SetMatch(&match);
    if (*first.ResolveNativeVariable(0x0404) != 1 || *second.ResolveNativeVariable(0x0405) != 1) { ++failures; }

    CBullet ordering;
    ordering.x = 10; ordering.y = 40;
    ordering.ownerType = 0;
    if (ordering.GetZOrder(10, 60, true) != 59 || ordering.GetZOrder(110, 40, true) != 50) { ++failures; }
    ordering.ownerType = 2;
    if (ordering.GetZOrder(10, 70, true) != 69) { ++failures; }
    ordering.ownerType = 1;
    if (ordering.GetZOrder(10, 70, true) != 50) { ++failures; }

    SeekingWorld world;
    CGun::Template gunData;
    CGun gun;
    gun.Bind(gunData, nullptr);
    const std::int16_t seekAngle[] = {180};
    gun.FunctionResolver(14, seekAngle, 1);
    CBullet bullet;
    bullet.seekingTurnRate = 90;
    bullet.speed = 100;
    gun.AddBullet(bullet);
    bullet.InitializeSeeking(&world);
    bullet.UpdateSeeking(&world, 100);
    if (std::abs(bullet.direction - 9) > 0.001f || world.queries != 1 || bullet.seekingTarget != 20) { ++failures; }
    world.selected = 21;
    world.targetY = 10000;
    bullet.UpdateSeeking(&world, 100);
    if (std::abs(bullet.direction - 18) > 0.001f || world.queries != 1 || bullet.seekingTarget != 20) { ++failures; }
    world.firstAlive = false;
    bullet.UpdateSeeking(&world, 100);
    if (bullet.seekingTarget != Collision::NoObject || world.queries != 1) { ++failures; }
    bullet.flags |= 0x2000;
    bullet.UpdateSeeking(&world, 100);
    if (bullet.seekingTarget != 21 || world.queries != 2 || std::abs(bullet.direction - 27) > 0.001f) { ++failures; }
    const float before = bullet.direction;
    bullet.UpdateSeeking(&world, 8);
    if (bullet.direction != before) { ++failures; }
    std::printf("[gameplay-ownership-check] shared-rng=MT19937 order-cases=4 seek-queries=%u failures=%u\n", world.queries, failures);
    if (failures != 0) { return 1; }
    return 0;
}
