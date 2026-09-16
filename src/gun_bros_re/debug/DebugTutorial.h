#pragma once
#include "gun_bros_re/gameplay/ZMapScene.h"
class CResTOCManager;
class ZPackTables;

inline constexpr int kDebugTutorialMenuChoice = -5;
/** Prepare a full original tutorial with a fresh in-memory account and no persistence. */
bool PrepareDebugTutorial(CResTOCManager &toc, ZPackTables &tables, ZSurvivalGameContext &context,
    ZSurvivalLaunch &launch);
