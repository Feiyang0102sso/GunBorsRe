#pragma once
#include "engine/core/Paths.h"
/** @file Arena.cpp
 * @brief Empty arenas sharing the same player, weapons and enemy script hosts.
 */
#define NOMINMAX
#if GB_ENABLE_TESTS
#include "TestOutput.h"
#endif
#include "gun_bros_viewer/scenes/ArenaPreview.h"
#include "gun_bros_re/data/ArmorCatalog.h"
#include "gun_bros_re/ui/HudText.h"
#include "gun_bros_re/gameplay/CombatScene.h"
#include "gun_bros_re/data/WeaponCatalog.h"
#include "engine/graphics/CMarkerBatch.h"
#include "engine/core/CMatrix4d.h"
#include "gun_bros_re/gameplay/CBullet.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace ArenaDetail {
constexpr int kStepMs = 16;
bool Equip(PackTables &tables, const PlayerTemplateData &data, const WeaponEntry &entry, PlayerModel &player, const CShaderProgram &program);
#if GB_ENABLE_TESTS
int CheckArena(CWindow &window, CResTOCManager &toc, PackTables &tables, const CShaderProgram &program,
    const std::vector<EnemyTemplateData> &catalog, const std::vector<WeaponEntry> &weapons,
    const PlayerTemplateData &playerData, PlayerModel &player, PlayerVitals &vitals,
    WeaponEffects &effects, CombatScene &scene);
#endif
}
