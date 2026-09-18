#pragma once
#include "gameplay/SurvivalStudy.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include <string>
#include <vector>

class CResTOCManager;
class ZPackTables;

int RunArmorCheck(const std::string &bigDirectory);

int RunArmorRenderCheck(const std::string &bigDirectory);

int RunDailyBonusCheck(const std::string &bigDirectory);

int RunMissionCheck(const std::string &bigDirectory);

int RunNativeProfileCheck(const std::string &bigDirectory);

int RunNativeProfilePlayCheck(const std::string &bigDirectory);

int RunOriginalProfileCheck(const std::string &bigDirectory);

int RunOriginalProfilePlayCheck(const std::string &bigDirectory);

/** Enumerate original pickup references for research checks; no runtime catalog. */
std::vector<GameObjectRef> GetPickupCheckReferences(CResTOCManager &toc, ZPackTables &tables);
int RunPickupCheck(const std::string &bigDirectory);

int RunPickupRenderCheck(const std::string &bigDirectory);

int RunPowerupCheck(const std::string &bigDirectory);

int RunPropCheck(const std::string &bigDirectory);
int RunPropCombatCheck(const std::string &bigDirectory);
int RunActorFeedbackCheck(const std::string &bigDirectory);

int RunProgressCheck(const std::string &bigDirectory);

int RunOriginalPowerupSelectorCheck(const std::string &bigDirectory);

/** Original pack12 LEVEL string references exercise the radio popup end to end. */
int RunOriginalDialogCheck(const std::string &bigDirectory);

/** Retained HUD milestone now checks original controls, selector and pause tree. */
int RunSurvivalHudCheck(const std::string &bigDirectory);

int RunOriginalPauseCheck(const std::string &bigDirectory);

int RunOriginalHudCheck(const std::string &bigDirectory);

int RunMediaCheck();

int RunWeaponCheck(const std::string &bigDirectory);

/** BIG scripts and the production projectile update, with no map or input noise. */
int RunWeaponEffectsCheck(const std::string &bigDirectory);
int RunMineCheck(const std::string &bigDirectory);

int RunLevelFlowCheck(const std::string &bigDirectory);

int RunMovieCheck(const std::string &bigDirectory);
int RunFontBitmapCheck(const std::string &bigDirectory);
