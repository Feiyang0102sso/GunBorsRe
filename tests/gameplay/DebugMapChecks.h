#pragma once
class ZPackTables;
class CPlayerProgress;
class ZCombatWorld;
class CLevel;
struct ZPlayerModel;
struct ZSurvivalGameContext;
int RunDebugMapProfileCheck();
int CheckDebugMapProfile(ZPackTables &tables, const ZPlayerModel &player,
    const CPlayerProgress &progress, ZSurvivalGameContext &context, const ZCombatWorld &scene, const CLevel &level);
int RunCampaignContentCheck();
