#pragma once
class CResTOCManager;
class ZPackTables;
class CLevel;
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "gun_bros_re/gameplay/brother/CBrother.h"

unsigned CheckPowerupRuntime(CResTOCManager &toc, ZPackTables &tables, CLevel &scene,
    CBrother &player, CBrother::Vitals &vitals);
