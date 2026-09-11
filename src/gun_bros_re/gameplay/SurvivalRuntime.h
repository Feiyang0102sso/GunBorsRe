#pragma once
#include "gun_bros_re/gameplay/MapScene.h"
#if GB_ENABLE_TESTS
struct SurvivalDevelopment;
int RunSurvivalSession(const SurvivalLaunch &launch, const SurvivalDevelopment *development);
#else
int RunSurvivalSession(const SurvivalLaunch &launch);
#endif
