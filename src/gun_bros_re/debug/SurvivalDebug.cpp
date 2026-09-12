/** Host-only combat sidebar. Authored movies and reward calculations remain unchanged. */
#define NOMINMAX
#include "gun_bros_re/debug/SurvivalDebug.h"
#include "gun_bros_re/debug/DebugKeys.h"
#include "gun_bros_re/ui/SurvivalHud.h"
#include "gun_bros_re/HostSettings.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include <cstdio>
#include <cstdarg>
#include <sstream>

namespace {
void AddLine(std::vector<std::string> &lines, const char *format, ...) {
    char text[512];
    va_list values;
    va_start(values, format);
    std::vsnprintf(text, sizeof(text), format, values);
    va_end(values);
    lines.emplace_back(text);
}
}

bool HandleDebugKey(KeyCode key, const CWindow &window, bool &showCollisions) {
    if (GameDebugKeys::TogglesCollision(key, window)) {
        showCollisions = !showCollisions;
        std::printf("[debug] collisions=%d\n", showCollisions);
        return true;
    }
    if (GameDebugKeys::TogglesInfo(key, window)) {
        GameHostSettings().drawDebugInfo = !GameHostSettings().drawDebugInfo;
        std::printf("[debug] info=%d\n", GameHostSettings().drawDebugInfo);
        return true;
    }
    return false;
}

void PopulateSurvivalDebugInfo(SurvivalHudState &state, const CombatScene &scene,
    const WeaponEffects &effects, const std::string &pack, unsigned map, bool collisions) {
    char label[128];
    std::snprintf(label, sizeof(label), DebugConfig::Text::Map, pack.c_str(), map);
    state.debugMap = label;
    if (scene.GetLevel() != nullptr) { state.levelState = scene.GetLevel()->GetStateId(); }
    state.projectiles = effects.GetBulletCount();
    state.particles = effects.GetParticleCount();
    state.perfectWaves = scene.GetPerfectWaves();
    state.clearedWaves = scene.GetClearedWaves();
    if (!scene.GetWavePerfectResults().empty()) { state.lastWavePerfect = scene.GetWavePerfectResults().back(); }
    state.showCollisions = collisions;
}

void PopulateDebugBuffs(SurvivalHudState &state, const PlayerModel &player) {
    state.buffs.clear();
    const int buffTimers[] = {player.powerups.shieldMs, player.powerups.frenzyMs[0],
        player.powerups.frenzyMs[1], player.powerups.frenzyMs[2], player.powerups.autoFireMs, player.powerups.legacyFrenzyMs};
    for (unsigned index = 0; index < 6; ++index) {
        if (buffTimers[index] <= 0) { continue; }
        if (!state.buffs.empty()) { state.buffs += "   "; }
        char buffText[64];
        std::snprintf(buffText, sizeof(buffText), DebugConfig::Text::BuffTimer,
            DebugConfig::Text::BuffNames[index], (buffTimers[index] + 999) / 1000);
        state.buffs += buffText;
    }
    if (player.weapon->brother.IsTurretActive()) { state.buffs += "   "; state.buffs += DebugConfig::Text::Turret; }
}

void DrawSurvivalDebugInfo(MovieRenderer &movies, const SurvivalHudState &state) {
    if (!GameHostSettings().debugMode || !GameHostSettings().drawDebugInfo) { return; }
    const auto &style = DebugConfig::Sidebar;
    using namespace DebugConfig::Text;
    std::vector<std::string> lines;
    lines.emplace_back(Heading);
    lines.push_back(state.debugMap);
    if (state.horde) {
        AddLine(lines, Horde, state.wave + 1);
        AddLine(lines, Time, state.stopwatchMs / 1000);
    } else {
        AddLine(lines, Wave, state.wave % 50 + 1, 50u);
        AddLine(lines, Revolution, state.wave / 50 + 1, 10u);
    }
    AddLine(lines, LevelState, state.levelState);
    AddLine(lines, Health, state.health, state.maximumHealth);
    if (state.withBrother) { AddLine(lines, BrotherHealth, state.brotherHealth, state.brotherMaximumHealth); }
    AddLine(lines, Position, state.playerX, state.playerY);
    AddLine(lines, Enemies, state.enemies, state.kills);
    AddLine(lines, Damage, state.damageDealt, state.damageHits);
    AddLine(lines, Weapon, state.weapon.c_str());
    AddLine(lines, Effects, state.projectiles, state.particles);
    AddLine(lines, Experience, state.experience, state.experienceDelta);
    if (state.horde) { AddLine(lines, Points, state.score); }
    else { AddLine(lines, Xplodium, state.xplodium); }
    AddLine(lines, Perfect, state.perfectWaves, state.clearedWaves);
    if (state.clearedWaves > 0) {
        // CombatScene::OnWaveCleared stores the actual credited delta, including rounding.
        if (state.lastWavePerfect) { lines.emplace_back(LastPerfect); }
        else { lines.emplace_back(LastNormal); }
        AddLine(lines, Bonus, state.perfectBonus);
    }
    if (!state.buffs.empty()) { lines.push_back(state.buffs); }
    if (state.showCollisions) { lines.emplace_back(CollisionOn); }
    else { lines.emplace_back(CollisionOff); }

    // Wrap explicitly so the rows use the same measured bitmap font.
    std::vector<std::string> rows;
    for (const auto &line : lines) {
        std::istringstream words(line);
        std::string word, row;
        while (words >> word) {
            std::string candidate = word;
            if (!row.empty()) { candidate = row + " " + word; }
            if (!row.empty() && movies.TextWidth(candidate, style.font, style.scale) > style.width) {
                rows.push_back(row);
                row = word;
            } else { row = candidate; }
        }
        if (!row.empty()) { rows.push_back(row); }
    }
    const float rowHeight = movies.TextHeight(style.font, style.scale) + style.rowGap;
    float y = style.y;
    for (const auto &row : rows) {
        movies.Text(row, style.x, y, style.font, style.scale, style.width, style.alpha);
        y += rowHeight;
    }
}
