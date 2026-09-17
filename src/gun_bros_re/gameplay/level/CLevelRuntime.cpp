/** @file CLevelRuntime.cpp
 * @brief CLevel player progression and runtime binding.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include <algorithm>
#include <cstdio>

namespace {
// CEffectLayer::AddTextEffect :66884 and TextEffect::Update :67086.
constexpr unsigned kTextEffectLifetimeMs = 2000;
constexpr float kTextEffectRisePerSecond = 100;
using CombatGeometry::CircleFraction;
}

bool LoadInitialPlayerHealth(CResTOCManager &toc, ZPackTables &tables, float &health) {
    CPlayerProgress::Template progress;
    if (!LoadPlayerProgress(toc, tables, progress)) { return false; }
    // Progress is indexed by the displayed level. Entry zero is a sentinel;
    // a new player starts at level one.
    health = static_cast<float>(progress.health[1]);
    std::printf("[combat] initial player health %.0f from PLAYER_PROGRESS\n", health);
    return true;
}

void CLevel::SetPlayerProgress(CPlayerProgress *progress) {
    m_actor.BindProgress(progress);
}

void CLevel::AddExperience(unsigned amount) {
    if (m_actor.GetProgress() == nullptr) { return; }
    if (m_localLive && IsTeamDeathComplete()) { return; }
    const auto before = m_actor.GetProgress()->GetExperience();
    const bool leveled = m_actor.AddExperience(amount, !IsDeathmatch());
    if (m_localLive || IsDeathmatch()) {
        const auto earned = m_actor.GetProgress()->GetExperience() - before;
        m_multiplayer[0].wave.experience += earned;
        m_multiplayer[0].total.experience += earned;
    }
    if (!leveled || IsDeathmatch()) { return; }
    if (m_brother != nullptr && !m_localLive) {
        const float brotherFraction = m_brother->vitals.health / m_brother->vitals.maximum;
        m_brother->vitals.maximum = m_actor.GetProgress()->GetHealth();
        m_brother->vitals.health = m_brother->vitals.maximum * brotherFraction;
    }
    std::printf("[progress] level-up=%u health=%.1f/%.1f xp=%llu\n",
        m_actor.GetProgress()->GetLevel(), m_vitals->health, m_vitals->maximum, m_actor.GetProgress()->GetExperience());
}

void CLevel::UpdateExperienceTexts(int deltaMs) {
    if (deltaMs <= 0) { return; }
    for (auto text = m_experienceTexts.begin(); text != m_experienceTexts.end();) {
        text->elapsedMs += static_cast<unsigned>(deltaMs);
        if (text->elapsedMs >= kTextEffectLifetimeMs) {
            text = m_experienceTexts.erase(text);
            continue;
        }
        text->alpha = 1 - static_cast<float>(text->elapsedMs) / kTextEffectLifetimeMs;
        text->y -= kTextEffectRisePerSecond * deltaMs / 1000.0f;
        ++text;
    }
}

void CLevel::AddHealth(unsigned amount) {
    m_actor.AddHealth(amount);
}

void CLevel::AddXplodium(unsigned amount) {
    if (m_localLive && IsTeamDeathComplete()) { return; }
    unsigned percent = 100;
    percent = static_cast<unsigned>(std::max(0, GetXplodiumMultiplierPercent()));
    const std::uint64_t earned = m_actor.AddXplodium(amount, percent);
    if (m_localLive || IsDeathmatch()) {
        m_multiplayer[0].wave.xplodium += earned;
        m_multiplayer[0].total.xplodium += earned;
    }
}

unsigned CLevel::GetTotalKills() const {
    unsigned total = kills;
    // Dead actors can retain their original corpse animation for ten seconds.
    // Saving must include them before their render objects are retired.
    for (const auto &actor : m_objects.GetEnemies()) { total += actor->model.enemy.combat.deathCount; }
    return total;
}

float CLevel::GetEnemyTimeScale() const {
    return GetObjectTimeScale();
}

bool CLevel::TouchesPickup(float x, float y) const {
    // CPickup::Bind :99937 sets its fixed collision radius to 10.
    // CBrother::TestCollisions :138154 excludes GetBrotherType() == 1;
    // CBrotherAI::GetBrotherType :139638 returns 1 for the AI companion.
    return !IsMatchSpawnPending(0) && m_vitals != nullptr && !m_vitals->dead && CircleFraction(m_actor.x, m_actor.y,
        m_actor.previousX - m_actor.x, m_actor.previousY - m_actor.y, x, y, m_playerRadius + 10) <= 1;
}

CLevel::CLevel(ZPackTables &tables, const ZShaderProgram &program,
    const std::vector<ZEnemyTemplateData> &catalog, ZPlayerModel &player,
    ZPlayerVitals &vitals, ZWeaponEffects &effects, float playerGameScale)
    : CLevel() {
    m_tables = &tables;
    m_program = &program;
    m_catalog = &catalog;
    m_playerModel = &player;
    m_vitals = &vitals;
    m_effects = &effects;
    m_playerGameScale = playerGameScale;
    m_actor.BindActor(player, vitals);
    m_objects.BindRuntime(tables, program, catalog);
    m_objects.SetLevel(this);
    m_effects->SetCombatWorld(this);
}
