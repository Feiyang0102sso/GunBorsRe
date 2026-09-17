#include "ParticlePlayerChecks.h"
#include "gun_bros_re/gameplay/CParticleEffectPlayer.h"
#include <cstdio>
#include <cstring>

namespace {
void Word(std::vector<std::uint8_t> &bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}
void Scalar(std::vector<std::uint8_t> &bytes, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    Word(bytes, bits);
}
bool Expect(bool condition, const char *message) {
    if (!condition) { std::printf("[particle-player-check] FAIL %s\n", message); }
    return condition;
}

/** Minimal synthetic stream in entries/particle_effect.bt order; no game data override. */
bool Effect(CParticleEffect &effect, float start, float end, float interval, unsigned lifetime,
    unsigned emitterCount = 1) {
    std::vector<std::uint8_t> bytes;
    Word(bytes, 0);
    bytes.push_back(static_cast<std::uint8_t>(emitterCount));
    for (unsigned emitter = 0; emitter < emitterCount; ++emitter) {
        bytes.push_back(0); Word(bytes, 1);
        Scalar(bytes, interval); Scalar(bytes, interval);
        Scalar(bytes, start); Scalar(bytes, end);
        bytes.push_back(0); Scalar(bytes, 0); Scalar(bytes, 0);
        bytes.push_back(1); bytes.push_back(0);
        Word(bytes, 0); Word(bytes, lifetime);
        for (unsigned endpoint = 0; endpoint < 4; ++endpoint) { Scalar(bytes, 1); }
        for (unsigned channel = 1; channel < 8; ++channel) { bytes.push_back(0); }
        bytes.push_back(0);
        for (unsigned coordinate = 0; coordinate < 4; ++coordinate) { Scalar(bytes, 0); }
        bytes.push_back(0);
        for (unsigned component = 0; component < 4; ++component) { Scalar(bytes, 0); }
    }
    CArrayInputStream stream(bytes);
    return effect.Init(stream) && stream.Available() == 0;
}
}

bool CheckParticlePlayers() {
    CParticleEffect continuous;
    if (!Effect(continuous, -1, -1, 0, 100)) { return false; }
    auto pool = std::make_shared<CParticlePool>(3);
    if (!Expect(pool->GetAvailableCount() == 2, "reserved free-stack slot")) { return false; }
    std::uint32_t random = 10;
    CParticleEffectPlayer first;
    CParticleEffectPlayer second;
    first.Init(continuous, pool);
    second.Init(continuous, pool);
    first.Update(0, random);
    if (!Expect(first.GetParticleCount() == 0, "paused update emitted")) { return false; }
    first.Update(1, random);
    second.Update(1, random);
    first.Update(1, random);
    if (!Expect(first.GetParticleCount() == 1 && second.GetParticleCount() == 1 &&
        pool->GetAvailableCount() == 0, "shared pool exhaustion")) { return false; }
    first.StopSpawning();
    if (!Expect(first.GetParticleCount() == 1 && !first.IsDone(), "StopSpawning discarded a particle")) { return false; }
    first.Update(99, random);
    if (!Expect(first.IsDone() && first.GetParticleCount() == 0 && pool->GetAvailableCount() == 1,
        "drain did not return its slot")) { return false; }
    second.Update(1, random);
    if (!Expect(second.GetParticleCount() == 2, "released slot was not reusable")) { return false; }
    second.Stop();
    if (!Expect(second.IsDone() && pool->GetAvailableCount() == 2, "Stop did not clear immediately")) { return false; }
    second.Start();
    second.Update(1, random);
    second.Start();
    if (!Expect(second.GetParticleCount() == 0 && pool->GetAvailableCount() == 2,
        "restart retained old particles")) { return false; }
    {
        CParticleEffectPlayer temporary;
        temporary.Init(continuous, pool);
        temporary.Update(1, random);
        CParticleEffectPlayer moved(std::move(temporary));
        moved.Update(1, random);
    }
    if (!Expect(pool->GetAvailableCount() == 2, "move/destruction leaked slots")) { return false; }

    CParticleEffect finite;
    if (!Effect(finite, 0.1f, 0.2f, 0, 300)) { return false; }
    first.Init(finite, pool);
    first.SetLooping(false);
    first.Update(150, random);
    if (!Expect(first.GetParticleCount() == 0, "crossing only the start emitted early")) { return false; }
    first.Update(25, random);
    if (!Expect(first.GetParticleCount() == 1 && first.GetParticle(0).ageMs == 0,
        "birth frame age")) { return false; }
    first.Update(50, random);
    first.Update(300, random);
    if (!Expect(first.IsDone() && pool->GetAvailableCount() == 2, "finite window did not drain")) { return false; }

    // Non-looping continuous effects end at the maximum authored lifetime.
    first.Init(continuous, pool);
    first.SetLooping(false);
    first.Update(100, random);
    first.Update(1, random);
    if (!Expect(first.IsDone() && first.GetParticleCount() == 0, "one-shot continuous effect never ended")) { return false; }

    // Interval remainder survives a period wrap; it must not restart a burst.
    if (!Effect(finite, 0, 0.1f, 0.03f, 500)) { return false; }
    auto largerPool = std::make_shared<CParticlePool>(20);
    first.Init(finite, largerPool);
    first.SetLooping(true);
    first.Update(90, random);
    const auto beforeWrap = first.GetParticleCount();
    first.Update(20, random);
    first.Update(10, random);
    if (!Expect(first.GetParticleCount() == beforeWrap + 1, "loop discarded interval remainder")) { return false; }
    first.Stop();

    // A zero interval emits once per emitter per update, with no frame cap table.
    CParticleEffect multiple;
    if (!Effect(multiple, -1, -1, 0, 100, 2)) { return false; }
    first.Init(multiple, largerPool);
    first.Update(1, random);
    first.Update(1, random);
    if (!Expect(first.GetParticleCount() == 4, "zero-interval emitter cadence")) { return false; }
    first.Stop();

    CParticleEffect spaced;
    if (!Effect(spaced, -1, -1, 0.1f, 1000)) { return false; }
    auto singleSlot = std::make_shared<CParticlePool>(2);
    first.Init(spaced, singleSlot);
    second.Init(spaced, singleSlot);
    first.Update(1, random);
    second.Update(1, random);
    first.Stop();
    second.Update(1, random);
    if (!Expect(second.GetParticleCount() == 0, "full pool deferred a missed birth")) { return false; }
    second.Update(98, random);
    if (!Expect(second.GetParticleCount() == 1, "full pool lost interval cadence")) { return false; }
    std::printf("[particle-player-check] pool, drain, stop, restart, move, windows and looping passed\n");
    return true;
}
