#pragma once
class CResTOCManager;
class ZPackTables;
class CLevel;
class CBrother;
struct ZPlayerVitals;

unsigned CheckPowerupRuntime(CResTOCManager &toc, ZPackTables &tables, CLevel &scene,
    CBrother &player, ZPlayerVitals &vitals);
