#pragma once
class CMap;
class CLevel;
class CGame;
int RunCampaignDoorCheck();
int RunCampaignTargetCheck();
int RunCampaignProgressionCheck();
int RunCampaignRescueCheck();
int RunCampaignPortalCheck();
int RunCampaignCacheCheck();
int RunCampaignLava2Check();
int CheckCampaignCache(CMap &map, CLevel &scene, CGame &session);
int CheckCampaignPortal(CMap &map, CLevel &scene, CGame &session);
int CheckCampaignRescue(CMap &map, CLevel &scene, CGame &session);
int CheckCampaignProgression(CMap &map, CLevel &scene, CGame &session, unsigned mapIndex);
int CheckCampaignTargets(CMap &map, CLevel &scene, CGame &session);
int CheckCampaignDoorPassage(CMap &map, CLevel &scene, CGame &session);
