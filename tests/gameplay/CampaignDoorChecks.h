#pragma once
namespace MapDetail { struct ZLoadedMap; }
class CLevel;
class CGame;
class ZPickupScene;
int RunCampaignDoorCheck();
int RunCampaignTargetCheck();
int RunCampaignProgressionCheck();
int RunCampaignRescueCheck();
int RunCampaignPortalCheck();
int RunCampaignCacheCheck();
int RunCampaignLava2Check();
int CheckCampaignCache(MapDetail::ZLoadedMap &map, CLevel &scene, CGame &session, ZPickupScene &pickups);
int CheckCampaignPortal(MapDetail::ZLoadedMap &map, CLevel &scene, CGame &session);
int CheckCampaignRescue(MapDetail::ZLoadedMap &map, CLevel &scene, CGame &session);
int CheckCampaignProgression(MapDetail::ZLoadedMap &map, CLevel &scene, CGame &session, unsigned mapIndex);
int CheckCampaignTargets(MapDetail::ZLoadedMap &map, CLevel &scene, CGame &session);
int CheckCampaignDoorPassage(MapDetail::ZLoadedMap &map, CLevel &scene, CGame &session);
