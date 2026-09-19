#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/ui/controls/ZMenuScrollMotion.h"
#include "gun_bros_re/data/CProfileManager.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
class CMenuFriends {
public:
    bool DrawModel(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile);

    bool DrawContent(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile, const ZMovieRegion &region);

    bool BindContent(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile);

    bool Draw(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile, bool hasCredentials);

    bool DrawOffline(ZMenuSurface &view, CMenuSystem &state, bool hasCredentials);

    std::vector<ZWeaponEntry> weapons;
    std::vector<ZArmorEntry> armors;
    CProfileManager defaultBrother;
    CGameAssetRef avatar;
    std::string brotherName;
    unsigned selectedLocalFriend = 0; // 0: original default, 1: simulated peer.
    std::vector<unsigned> friendTimes; // Each active friend's authored Focus/UnFocus timeline.
    bool contentBound = false;
    unsigned contentPage = 0;
    unsigned contentElapsed = 0;
    float scrollPosition = 0;
    ZMenuScrollMotion scrollMotion;
    unsigned renderedEntries = 0; // Actual local content cards drawn this frame.
    bool onlinePage = false;
    unsigned socialTab = 0;
    bool socialBound = false;
    unsigned socialTime = 0;
    std::uint64_t socialLastTick = 0;

};
}
