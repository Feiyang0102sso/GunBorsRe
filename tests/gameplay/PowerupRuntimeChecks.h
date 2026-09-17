#pragma once
class CResTOCManager;
class ZPackTables;
class CLevel;
struct ZPlayerModel;
struct ZPlayerVitals;

unsigned CheckPowerupRuntime(CResTOCManager &toc, ZPackTables &tables, CLevel &scene,
    ZPlayerModel &player, ZPlayerVitals &vitals);
