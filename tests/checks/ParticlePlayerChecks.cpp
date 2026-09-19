#include "ParticlePlayerChecks.h"
#include "gun_bros_re/effects/CParticleEffectPlayer.h"
#include "gun_bros_re/effects/CParticleSystem.h"
#include "gun_bros_re/effects/EffectContainer.h"
#include "gun_bros_re/effects/ParticleEffectHolder.h"
#include "gun_bros_re/effects/TrailEffectHolder.h"
#include "gun_bros_re/gameplay/weapon/CBullet.h"
#include "gun_bros_re/effects/CEffectLayer.h"
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
    unsigned emitterCount = 1, float velocityX = 0) {
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
        for (unsigned component = 0; component < 4; ++component) {
            float value = 0;
            if (component < 2) { value = velocityX; }
            Scalar(bytes, value);
        }
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
    // Map instance capacity must not depend on how many particles have spawned.
    CParticleSystem system;
    CParticleEffect dense;
    if (!Effect(dense, -1, -1, 0.001f, 1000)) { return false; }
    std::array<CParticleSystem::Handle, 20> handles{};
    for (unsigned slot = 0; slot < handles.size(); ++slot) {
        auto *player = system.AddEffect(dense, static_cast<float>(slot), 0);
        if (!Expect(player != nullptr, "map refused a free effect slot")) { return false; }
        player->SetLooping(true);
        handles[slot] = system.GetHandle(*player);
    }
    if (!Expect(system.GetParticleCount() == 0 && system.AddEffect(dense, 0, 0) == nullptr,
        "map effect limit depends on particle count")) { return false; }
    system.Update(20, random);
    if (!Expect(system.GetEffectCount() == 20 && system.GetParticleCount() == 199,
        "map particle capacity differs from reserved-stack semantics")) { return false; }
    auto *retiring = system.Get(handles[0]);
    retiring->StopSpawning();
    if (!Expect(system.AddEffect(dense, 0, 0) == nullptr, "draining effect released its instance early")) { return false; }
    retiring->Stop();
    auto *replacement = system.AddEffect(dense, 900, 0);
    if (!Expect(replacement == retiring && system.Get(handles[0]) == nullptr,
        "reused slot accepted a stale handle")) { return false; }
    system.Clear();
    if (!Expect(system.GetEffectCount() == 0 && system.GetPool()->GetAvailableCount() == 199 &&
        system.Get(handles[1]) == nullptr, "map clear leaked particles or identities")) { return false; }

    // Anchors move the emitter, not particles already emitted in world space.
    float anchorX = 10;
    bool anchorAlive = true;
    auto *anchored = system.AddEffect(continuous, 0, 0);
    anchored->SetLooping(true);
    anchored->SetAnchor([&anchorX, &anchorAlive](float &x, float &y, float &z, float &angle) {
        if (!anchorAlive) { return false; }
        x = anchorX; y = 20; z = 0; angle = 0;
        return true;
    });
    system.Update(1, random);
    anchorX = 90;
    system.Update(1, random);
    if (!Expect(anchored->GetParticle(0).x == 90 && anchored->GetParticle(1).x == 10,
        "anchor moved already emitted particles")) { return false; }
    anchorAlive = false;
    system.Update(1, random);
    if (!Expect(anchored->GetParticleCount() == 2, "invalid anchor spawned or discarded particles")) { return false; }
    anchorAlive = true;
    anchorX = 500;
    system.Update(1, random);
    if (!Expect(anchored->GetParticleCount() == 2, "retired anchor reattached to another life")) { return false; }
    system.Update(100, random);
    if (!Expect(anchored->IsDone() && system.GetEffectCount() == 0, "invalid anchor never drained")) { return false; }

    // Respawning an actor must neither move its old burst nor lose the query
    // that holds final results until the last burst has drained.
    CParticleEffect deathBurst;
    if (!Effect(deathBurst, 0, 0.1f, 0, 500)) { return false; }
    anchorX = 10;
    auto *burst = system.AddEffect(deathBurst, anchorX, 20);
    burst->SetAnchor([&anchorX](float &x, float &y, float &z, float &angle) {
        x = anchorX; y = 20; z = 0; angle = 0;
        return true;
    });
    system.SetOwner(system.GetHandle(*burst), 7);
    auto *linked = system.AddEffect(continuous, 10, 20);
    linked->SetLooping(true);
    system.SetOwner(system.GetHandle(*linked), 7, 2);
    system.Update(1, random);
    system.RetireOwner(7);
    anchorX = 500;
    system.Update(1, random);
    if (!Expect(system.HasActorBurst(7) && !system.HasActorBurst(8) &&
        burst->GetParticleCount() == 2 && burst->GetParticle(0).x == 10 &&
        linked->GetParticleCount() == 1,
        "retired owner moved its burst, lost ownership or kept emitting linked particles")) { return false; }
    for (unsigned frame = 0; frame < 10; ++frame) { system.Update(100, random); }
    if (!Expect(!system.HasActorBurst(7) && system.GetPool()->GetAvailableCount() == 199,
        "retired owner's final burst did not release its completion query")) { return false; }
    system.Clear();

    // Effect slots are occupied before the first particle, and stopped holders
    // retain their slot until the next container update observes completion.
    EffectContainer attachments;
    EffectHolder::Anchor attachmentAnchor;
    std::array<EffectContainer::Handle, 4> attached{};
    for (unsigned index = 0; index < attached.size(); ++index) {
        attached[index] = attachments.Attach(std::make_unique<ParticleEffectHolder>(continuous, largerPool, true),
            [&attachmentAnchor]() { return attachmentAnchor; });
        if (!Expect(attached[index] != 0, "bullet refused a free attachment slot")) { return false; }
    }
    if (!Expect(attachments.Attach(std::make_unique<ParticleEffectHolder>(continuous, largerPool, true),
        [&attachmentAnchor]() { return attachmentAnchor; }) == 0, "bullet attachment capacity expanded")) { return false; }
    attachments.Update(1, random);
    attachments.Get(attached[0])->Stop();
    if (!Expect(attachments.GetParticleCount() == 4, "holder Stop discarded particles")) { return false; }
    attachments.Get(attached[0])->StopInstant();
    attachments.Update(1, random);
    const auto reused = attachments.Attach(std::make_unique<ParticleEffectHolder>(continuous, largerPool, true),
        [&attachmentAnchor]() { return attachmentAnchor; });
    if (!Expect(reused != 0 && reused != attached[0] && attachments.Get(attached[0]) == nullptr,
        "attachment reuse accepted a stale handle")) { return false; }
    attachments.Stop();
    attachments.Update(101, random);
    if (!Expect(attachments.IsDone(), "removed bullet attachment never drained")) { return false; }
    attachments.Clear();
    if (!Expect(largerPool->GetAvailableCount() == 19, "attachment destruction leaked pool slots")) { return false; }

    TrailEffectHolder trail(2, 4, 10);
    trail.Update(attachmentAnchor, 1, random);
    trail.Update(attachmentAnchor, 10, random);
    if (!Expect(trail.GetAmount() == 1, "ribbon sampled on the strict interval boundary")) { return false; }
    trail.Update(attachmentAnchor, 100, random);
    if (!Expect(trail.GetAmount() == 2, "ribbon caught up skipped samples")) { return false; }
    trail.Stop();
    trail.Update(attachmentAnchor, 11, random);
    if (!Expect(!trail.IsDone() && trail.GetAmount() == 1, "ribbon stopped without draining")) { return false; }
    trail.Update(attachmentAnchor, 11, random);
    if (!Expect(trail.IsDone(), "ribbon never completed after retirement")) { return false; }

    // A full attachment container rejects this native request permanently.
    // Only another explicit request may claim a slot after one becomes free.
    CBullet bullet;
    std::array<EffectContainer::Handle, 4> occupied{};
    for (unsigned index = 0; index < occupied.size(); ++index) {
        occupied[index] = bullet.effects.Attach(
            std::make_unique<ParticleEffectHolder>(continuous, largerPool, true),
            [&attachmentAnchor]() { return attachmentAnchor; });
    }
    ZGunCue ribbonCue;
    ribbonCue.kind = ZGunCue::Kind::RibbonTrail;
    ribbonCue.ribbon.capacity = 4;
    ribbonCue.ribbon.intervalMs = 10;
    ribbonCue.ribbon.width = 2;
    bullet.ApplyRibbonCue(ribbonCue);
    bullet.UpdateAttachedEffects(1, random);
    bullet.effects.Get(occupied[0])->StopInstant();
    bullet.UpdateAttachedEffects(1, random);
    if (!Expect(bullet.effects.GetEffectCount() == 3 && bullet.effects.GetRibbonCount() == 0,
        "rejected ribbon was queued for a later frame")) { return false; }
    bullet.ApplyRibbonCue(ribbonCue);
    bullet.UpdateAttachedEffects(1, random);
    if (!Expect(bullet.effects.GetEffectCount() == 3 && bullet.effects.GetRibbonCount() == 1,
        "explicit ribbon retry did not claim the freed slot")) { return false; }
    bullet.removed = true;
    bullet.UpdateAttachedEffects(1, random);
    if (!Expect(!bullet.effects.IsDone(), "ordinary bullet discarded its retiring attachments")) { return false; }
    bullet.UpdateAttachedEffects(200, random);
    bullet.UpdateAttachedEffects(200, random);
    if (!Expect(bullet.effects.IsDone(), "ordinary bullet retirement never completed")) { return false; }

    CBullet beam;
    beam.beam = true;
    beam.AttachParticleEffect(continuous, largerPool, false, false);
    beam.UpdateAttachedEffects(1, random);
    beam.removed = true;
    beam.UpdateAttachedEffects(1, random);
    if (!Expect(beam.effects.IsDone() && largerPool->GetAvailableCount() == 19,
        "removed beam retained attachments or leaked particles")) { return false; }

    CEffectLayer layer;
    layer.SetPool(largerPool);
    for (unsigned index = 0; index < 20; ++index) {
        if (!Expect(layer.AddParticleEffect(continuous, 0, 0, 0, 0),
            "effect layer refused a free slot")) { return false; }
    }
    if (!Expect(!layer.AddParticleEffect(continuous, 0, 0, 0, 0) && system.AddEffect(continuous, 0, 0) != nullptr,
        "effect layer expanded or consumed map slots")) { return false; }
    layer.Update(1, random);
    layer.Update(100, random);
    layer.Update(1, random);
    if (!Expect(layer.GetEffectCount() == 0 && layer.AddParticleEffect(continuous, 0, 0, 0, 0),
        "effect layer did not reclaim completed slots")) { return false; }
    layer.Clear();
    system.Clear();

    auto *grouped = system.AddEffect(continuous, 10, 23.75f);
    grouped->SetLooping(true);
    grouped->SetScale(0.5f);
    grouped->SetZOrderGroup(5);
    system.Update(1, random);
    grouped->SetZOrderGroup(2);
    system.Update(1, random);
    const auto drawItems = system.GetRenderItems();
    if (!Expect(drawItems.size() == 2 && drawItems[0].group == 2 && drawItems[1].group == 5 &&
        drawItems[0].y == 23 && grouped->GetScale() == 0.5f,
        "particle draw group did not retain its birth value")) { return false; }
    system.Clear();
    grouped = system.AddEffect(continuous, 0, 0);
    if (!Expect(grouped->GetScale() == 1, "reused player retained linked scale")) { return false; }

    CParticleEffect travelling;
    if (!Effect(travelling, -1, -1, 0, 1000, 1, 100)) { return false; }
    first.Init(travelling, largerPool);
    second.Init(travelling, largerPool);
    first.SetWorldSpace(false);
    second.SetWorldSpace(true);
    first.SetPosition(10, 20, 0, 0);
    second.SetPosition(10, 20, 0, 0);
    first.Update(1, random);
    second.Update(1, random);
    first.SetPosition(40, 50, 0, 0);
    second.SetPosition(40, 50, 0, 0);
    first.Update(100, random);
    second.Update(100, random);
    first.SetPosition(40, 50, 0, 90);
    second.SetPosition(40, 50, 0, 90);
    first.Update(100, random);
    second.Update(100, random);
    float localX = 0, localY = 0, localZ = 0, worldX = 0, worldY = 0, worldZ = 0;
    first.GetParticlePosition(2, localX, localY, localZ);
    second.GetParticlePosition(2, worldX, worldY, worldZ);
    if (!Expect(std::abs(localX - 50) < 0.001f && std::abs(localY - 60) < 0.001f &&
        std::abs(worldX - 20) < 0.001f && std::abs(worldY - 30) < 0.001f,
        "relative position or current-anchor rotation was lost")) { return false; }
    std::printf("[particle-player-check] pools, stop, restart, windows, map slots, anchors and bullet holders passed\n");
    return true;
}
