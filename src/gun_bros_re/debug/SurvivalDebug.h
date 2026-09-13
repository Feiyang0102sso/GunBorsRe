#pragma once
#include "engine/platform/CWindow.h"
class MovieRenderer;
class CombatScene;
class WeaponEffects;
struct SurvivalHudState;
struct PlayerModel;

/** The game owns its state; this module owns diagnostic labels, layout and toggles. */
bool HandleDebugKey(KeyCode key, const CWindow &window, bool &showCollisions);
void DrawSurvivalDebugInfo(MovieRenderer &movies, const SurvivalHudState &state);
void DrawTutorialDebugNotice(MovieRenderer &movies, std::uint64_t ticks);
void PopulateDebugBuffs(SurvivalHudState &state, const PlayerModel &player);
void PopulateSurvivalDebugInfo(SurvivalHudState &state, const CombatScene &scene,
    const WeaponEffects &effects, const std::string &pack, unsigned map, bool collisions);
