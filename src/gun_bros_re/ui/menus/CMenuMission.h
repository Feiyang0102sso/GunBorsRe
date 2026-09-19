#pragma once
#include "gun_bros_re/data/mission/Planet.h"
#include "gun_bros_re/data/mission/Mission.h"
#include "gun_bros_re/gameplay/script/CMissionScriptContext.h"
#include "gun_bros_re/data/profile/CProfileManager.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
class CMenuMission {
public:
    struct MissionInfo {
        std::string title, description, requirements, overview;
        std::vector<CMissionScriptContext::Requirement> prerequisites;
        int requiredLevel = 0;
        unsigned waveCount = 0;
        GameObjectRef map;
    };

    struct PlanetEntry {
        GameObjectRef resource;
        Planet data;
        std::vector<Mission> missions;
        std::vector<MissionInfo> missionInfo;
    };

    static bool LoadPlanets(CResTOCManager &toc, CGunBros &tables, std::vector<PlanetEntry> &result);
    class Presentation;
    bool Draw(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile);

    int startingWave = -1;
    float starPanX = 0, starPanY = 0;
    bool starBound = false, starReverse = false, starLocked = false, starEntering = false;
    unsigned starTime = 0, starReticleTime = 0, starFlagTime = 0, starFadeTime = 0;
    int starSelectedSlot = -1, starTargetTime = -1;
    float starSpeed = 0;
    float starSelectorX = 0, starSelectorY = 0, starFlagX = 0, starFlagY = 0;
    std::uint64_t starLastTick = 0;

};
}
