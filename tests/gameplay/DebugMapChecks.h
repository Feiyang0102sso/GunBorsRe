#pragma once
class PackTables;
class CPlayerProgress;
class CombatScene;
class CLevel;
struct PlayerModel;
struct SurvivalGameContext;
int RunDebugMapProfileCheck();
int CheckDebugMapProfile(PackTables &tables, const PlayerModel &player,
    const CPlayerProgress &progress, SurvivalGameContext &context, const CombatScene &scene, const CLevel &level);
int RunCampaignContentCheck();
