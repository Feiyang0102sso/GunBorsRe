#pragma once
namespace MapDetail { struct LoadedMap; }
class CombatScene;
class SurvivalSession;
class PickupScene;
int RunCampaignDoorCheck();
int RunCampaignTargetCheck();
int RunCampaignProgressionCheck();
int RunCampaignRescueCheck();
int RunCampaignPortalCheck();
int RunCampaignCacheCheck();
int RunCampaignLava2Check();
int CheckCampaignCache(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session, PickupScene &pickups);
int CheckCampaignPortal(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session);
int CheckCampaignRescue(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session);
int CheckCampaignProgression(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session, unsigned mapIndex);
int CheckCampaignTargets(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session);
int CheckCampaignDoorPassage(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session);
