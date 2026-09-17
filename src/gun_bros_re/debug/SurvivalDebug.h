#pragma once
#include "engine/platform/ZWindow.h"
class ZMovieRenderer;
class CLevel;
struct ZInputPadState;
struct ZPlayerModel;

/** The game owns its state; this module owns diagnostic labels, layout and toggles. */
bool HandleDebugKey(ZKeyCode key, const ZWindow &window, bool &showCollisions);
void DrawSurvivalDebugInfo(ZMovieRenderer &movies, const ZInputPadState &state);
void DrawTutorialDebugNotice(ZMovieRenderer &movies, std::uint64_t ticks);
void PopulateDebugBuffs(ZInputPadState &state, const ZPlayerModel &player);
void PopulateSurvivalDebugInfo(ZInputPadState &state, const CLevel &scene,
    const std::string &pack, unsigned map, bool collisions);
