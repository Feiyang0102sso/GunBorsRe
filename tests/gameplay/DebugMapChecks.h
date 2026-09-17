#pragma once
class ZPackTables;
class CPlayerProgress;
class CLevel;
struct ZPlayerModel;
struct ZSurvivalGameContext;
int RunDebugMapProfileCheck();
int CheckDebugMapProfile(ZPackTables &tables, const ZPlayerModel &player,
    const CPlayerProgress &progress, ZSurvivalGameContext &context, const CLevel &scene, const CLevel &level);
int RunCampaignContentCheck();
