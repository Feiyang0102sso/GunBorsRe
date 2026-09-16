#pragma once
namespace MapDetail { struct ZLoadedMap; }
class ZCombatWorld;
class ZLevelHost;
class ZPickupScene;
int RunCampaignDoorCheck();
int RunCampaignTargetCheck();
int RunCampaignProgressionCheck();
int RunCampaignRescueCheck();
int RunCampaignPortalCheck();
int RunCampaignCacheCheck();
int RunCampaignLava2Check();
int CheckCampaignCache(MapDetail::ZLoadedMap &map, ZCombatWorld &scene, ZLevelHost &session, ZPickupScene &pickups);
int CheckCampaignPortal(MapDetail::ZLoadedMap &map, ZCombatWorld &scene, ZLevelHost &session);
int CheckCampaignRescue(MapDetail::ZLoadedMap &map, ZCombatWorld &scene, ZLevelHost &session);
int CheckCampaignProgression(MapDetail::ZLoadedMap &map, ZCombatWorld &scene, ZLevelHost &session, unsigned mapIndex);
int CheckCampaignTargets(MapDetail::ZLoadedMap &map, ZCombatWorld &scene, ZLevelHost &session);
int CheckCampaignDoorPassage(MapDetail::ZLoadedMap &map, ZCombatWorld &scene, ZLevelHost &session);
