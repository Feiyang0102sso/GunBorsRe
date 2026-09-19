#pragma once
class ZPackTables;
class CPlayerProgress;
class CLevel;
class CBrother;
struct CGameFlow;
int RunDebugMapProfileCheck();
int CheckDebugMapProfile(ZPackTables &tables, const CBrother &player,
    const CPlayerProgress &progress, CGameFlow &context, const CLevel &scene, const CLevel &level);
int RunCampaignContentCheck();
