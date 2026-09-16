#pragma once
namespace MapDetail { struct ZLoadedMap; }
class ZCombatWorld;
class CGame;
class ZPickupScene;
int RunCampaignDoorCheck();
int RunCampaignTargetCheck();
int RunCampaignProgressionCheck();
int RunCampaignRescueCheck();
int RunCampaignPortalCheck();
int RunCampaignCacheCheck();
int RunCampaignLava2Check();
int CheckCampaignCache(MapDetail::ZLoadedMap &map, ZCombatWorld &scene, CGame &session, ZPickupScene &pickups);
int CheckCampaignPortal(MapDetail::ZLoadedMap &map, ZCombatWorld &scene, CGame &session);
int CheckCampaignRescue(MapDetail::ZLoadedMap &map, ZCombatWorld &scene, CGame &session);
int CheckCampaignProgression(MapDetail::ZLoadedMap &map, ZCombatWorld &scene, CGame &session, unsigned mapIndex);
int CheckCampaignTargets(MapDetail::ZLoadedMap &map, ZCombatWorld &scene, CGame &session);
int CheckCampaignDoorPassage(MapDetail::ZLoadedMap &map, ZCombatWorld &scene, CGame &session);
