/** Strengthening players from CBrother::Start/StopShield/Frenzy (:137245-137430).
 * Native 11 ordinary bursts remain in CMap's CParticleSystem.
 */
#include "gun_bros_re/gameplay/brother/CBrother.h"

void CBrother::PowerupParticles::Apply(const ZGunCue &cue, const CParticleEffect *effect) {
    // Slot tags are native action channels, not resource IDs or layout data.
    const int index = cue.hand - 100;
    if (index < 0 || index >= static_cast<int>(players.size())) { return; }
    auto &player = players[index];
    if (cue.kind == ZGunCue::Kind::StopTrail) { player.Stop(); return; }
    if (effect == nullptr) { return; }
    // Reapplying an active strengthening effect extends its duration; original
    // StartFrenzyType/StartShield initialize only when the old timer was zero.
    if (!player.IsDone()) { return; }
    player.Init(*effect, pool);
    player.SetLooping(true);
    player.SetPosition(x, y, 0, 0); // CBrother::GetParticleEffectAnchor :134152.
}

void CBrother::PowerupParticles::Update(int deltaMs, std::uint32_t &randomState) {
    for (auto &player : players) {
        player.SetPosition(x, y, 0, 0);
        player.Update(deltaMs, randomState);
    }
}

void CBrother::PowerupParticles::Draw(ZSpriteRenderer &renderer, const float *projection) const {
    for (const auto &player : players) { player.QueueParticles(renderer, projection); }
}

void CBrother::PowerupParticles::Stop() {
    for (auto &player : players) { player.Stop(); }
}

std::size_t CBrother::PowerupParticles::GetParticleCount() const {
    std::size_t count = 0;
    for (const auto &player : players) { count += player.GetParticleCount(); }
    return count;
}

std::size_t CBrother::PowerupParticles::GetEffectCount() const {
    std::size_t count = 0;
    for (const auto &player : players) { if (!player.IsDone()) { ++count; } }
    return count;
}
