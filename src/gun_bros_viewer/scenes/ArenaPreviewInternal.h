#pragma once
#include "engine/core/ZPaths.h"
/** @file ArenaPreviewInternal.h
 * @brief Empty arenas sharing the same player, weapons and enemy script hosts.
 */
#define NOMINMAX
#include "gun_bros_viewer/scenes/ArenaPreview.h"
#include "gun_bros_re/data/ZArmorCatalog.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/data/ZWeaponCatalog.h"
#include "engine/graphics/ZMarkerBatch.h"
#include "engine/core/ZMatrix4d.h"
#include "gun_bros_re/gameplay/CBullet.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace ArenaDetail {
constexpr int kStepMs = 16;
bool Equip(ZPackTables &tables, const CBrother::Template &data, const ZWeaponEntry &entry, CBrother &player, const ZShaderProgram &program);
}

/** Borrowed scene state, valid only during RunArena's scene-ready callback. */
struct ArenaScene {
    ZWindow &window;
    CResTOCManager &toc;
    ZPackTables &tables;
    const ZShaderProgram &program;
    const std::vector<CEnemy::Template> &catalog;
    const std::vector<ZWeaponEntry> &weapons;
    const CBrother::Template &playerData;
    CBrother &player;
    ZPlayerVitals &vitals;
    CLevel &scene;
};
