#pragma once
class ZPackTables;
class CPlayerProgress;
class CLevel;
class CBrother;
struct ZSurvivalGameContext;
int RunDebugMapProfileCheck();
int CheckDebugMapProfile(ZPackTables &tables, const CBrother &player,
    const CPlayerProgress &progress, ZSurvivalGameContext &context, const CLevel &scene, const CLevel &level);
int RunCampaignContentCheck();
