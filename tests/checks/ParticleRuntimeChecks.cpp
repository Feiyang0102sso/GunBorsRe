#include "ParticleRuntimeChecks.h"
#include "ParticlePlayerChecks.h"
#include "gun_bros_re/effects/CParticle.h"
#include <cmath>
#include <cstdio>

bool CheckParticleRuntime() {
    ZParticleEmitterTemplate emitter;
    emitter.animationMask = 1;
    std::uint32_t randomState = 123;
    CParticle particle;
    if (!particle.Spawn(emitter, randomState, 0, 0, 0) || !particle.IsDone() || particle.lifetimeMs != 0) {
        std::printf("[particle-check] empty interpolators acquired an invented lifetime\n");
        return false;
    }
    // A later key replaces a longer earlier segment. Lifetime uses LAST keys.
    emitter.interpolators[0] = {
        {false, 0, 2000, 2, 2, 4, 4},
        {true, 100, 100, 0, 0, 8, 8}
    };
    emitter.patternValues = {50, 80, 50, 80, 0, 0};
    emitter.accelerationX = 100;
    emitter.intervalMinimumSeconds = 0.3f;
    emitter.intervalMaximumSeconds = 0.4f;
    if (emitter.GetMaximumLifetimeMs() != 2000 || emitter.GetParticleCount() != 7) {
        std::printf("[particle-check] emitter allocation confused all keys with last keys\n");
        return false;
    }
    if (!particle.Spawn(emitter, randomState, 10, 20, 90) ||
        particle.x != 10 || particle.y != 20 || particle.ageMs != 0 ||
        particle.lifetimeMs != 200 || particle.Value(0) != 2 || particle.Value(5) != 1) {
        std::printf("[particle-check] spawn defaults, line pattern or lifetime mismatch\n");
        return false;
    }
    particle.Update(emitter, 100, randomState);
    if (particle.IsDone() || particle.Value(0) != 2 ||
        std::abs(particle.x - 10) > 0.001f || std::abs(particle.y - 21) > 0.001f) {
        std::printf("[particle-check] retained start or rotated acceleration mismatch\n");
        return false;
    }
    particle.Update(emitter, 50, randomState);
    if (particle.Value(0) != 5) { return false; }
    particle.Update(emitter, 50, randomState);
    if (!particle.IsDone() || particle.Value(0) != 8) { return false; }

    // Crossing several keys in one update selects the last crossed key.
    emitter.interpolators[0] = {
        {false, 0, 50, 1, 1, 1, 1},
        {false, 50, 50, 3, 3, 3, 3},
        {false, 100, 100, 7, 7, 9, 9}
    };
    if (!particle.Spawn(emitter, randomState, 0, 0, 0)) { return false; }
    particle.Update(emitter, 150, randomState);
    if (particle.Value(0) != 8) { return false; }

    emitter.interpolators[0] = {{false, 100, 100, 2, 2, 4, 4}};
    if (!particle.Spawn(emitter, randomState, 0, 0, 0) || particle.Value(0) != 1) { return false; }
    particle.Update(emitter, 90, randomState);
    if (particle.Value(0) != 1) { return false; }
    particle.Update(emitter, 10, randomState);
    if (particle.Value(0) != 2) { return false; }

    // Equal endpoint ranges still draw independently. The old shared random
    // fraction made this entire segment constant for every particle.
    emitter.interpolators[0] = {{false, 0, 100, 1, 10, 1, 10}};
    if (!particle.Spawn(emitter, randomState, 0, 0, 0)) { return false; }
    const float start = particle.Value(0);
    particle.Update(emitter, 50, randomState);
    if (particle.Value(0) == start) { return false; }
    std::printf("[particle-check] lifecycle, key transitions and local acceleration passed\n");
    return CheckParticlePlayers();
}
