#pragma once
class CGunBros;
class CPlayerProgress;
class CLevel;
class CBrother;
struct CGameFlow;
int RunDebugMapProfileCheck();
int CheckDebugMapProfile(CGunBros &tables, const CBrother &player,
    const CPlayerProgress &progress, CGameFlow &context, const CLevel &scene, const CLevel &level);
int RunCampaignContentCheck();
