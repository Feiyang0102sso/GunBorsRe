#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/ui/controls/ZMenuScrollMotion.h"
#include "gun_bros_re/data/CProfileManager.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
class CMenuMissionInfo {
public:
    bool Draw(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile, bool &launched);

    ZMenuScrollMotion missionMotion, waveMotion;
    float missionPosition = 0, wavePosition = 0;
    float missionScroll = 0;
    bool missionBound = false, missionClosing = false;
    unsigned missionTime = 0, missionListTime = 0, missionCardTime = 0, missionFocusTime = 0;
    unsigned missionFirst = 0, missionWaveTime = 0, missionWaveButtonTime = 0;
    bool missionListMoving = false, missionListReverse = false, missionWaveMoving = false, missionWaveReverse = false;
    int missionFocused = -1;
    float missionFocusX = 0, missionFocusY = 0;
    std::uint64_t missionLastTick = 0;
    unsigned revolution = 0, wavePage = 0, missionTab = 0;

};
}

namespace MenuDetail {

/** Paint dynamic planets at their original Movie layer, and retain native bounds. */

/** Original UpdatePosition :161040 advances along the dominant axis. */
void MoveStarPoint(float &x, float &y, float targetX, float targetY, unsigned elapsed, float speed);

/** CMenuMission :161015..163482, MENU_MISSION_ROOT VA 0x402d38.
 * The map is a bounded Movie timeline, not host coordinates or depth factors. */

/** CMissionWaveStatus collection 1003; the four live retail projections may
 * contain progress that has not reached the next disk checkpoint yet. */
unsigned NativeMissionProgress(const CProfileManager &profile, const GameObjectRef &level);

bool IsMissionLocked(const CProfileManager &profile, const Mission &mission, const ZPlanetMissionInfo &info);

void DrawMissionText(ZMenuSurface &view, const ZMovieRegion &region, const std::string &text, unsigned font, bool centered = false);

/** A page transition is the authored chapter 1; chapter 2 is the next page's
 * matching pose. The wheel is a Windows adapter for one native page gesture.
 * Historical note above described the former one-page adapter. Continuous
 * control now maps arbitrary positions onto those same authored poses.
 */
/** Map continuous scroll onto the original repeating Movie chapter. */
bool ScrollMissionMovie(ZMenuSurface &view, ZMenuScrollMotion &motion, float &position,
    const CMovie &movie, const ZMovieRegion &viewport, float stride, unsigned maximum,
    bool enabled, unsigned &page, unsigned &time, bool &moving);

/** CMenuMissionOption::WaveSelectCallback :189920, invoked in Movie65 layers. */

/** MENU_MISSION_DETAIL: main48/list49/box50; original callbacks and references. */

/** Horizontal revolution/horde cards use the same two-dimensional sprites as iOS. */

/** Returns true only after an unlocked wave/horde is explicitly launched. */

}
