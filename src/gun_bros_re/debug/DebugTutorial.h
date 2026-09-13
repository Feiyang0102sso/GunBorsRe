#pragma once
#include "gun_bros_re/gameplay/MapScene.h"
class CResTOCManager;
class PackTables;

inline constexpr int kDebugTutorialMenuChoice = -5;
/** Prepare a full original tutorial with a fresh in-memory account and no persistence. */
bool PrepareDebugTutorial(CResTOCManager &toc, PackTables &tables, SurvivalGameContext &context,
    SurvivalLaunch &launch);
