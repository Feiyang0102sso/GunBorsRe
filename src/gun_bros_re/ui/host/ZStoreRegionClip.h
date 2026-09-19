#pragma once
#include "gun_bros_re/ui/host/ZMenuSession.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuMovieMultiplayerOverlay.h"
#include "gun_bros_re/ui/menus/CMenuMissionInfo.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/menus/CMenuGameResources.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/menus/CMenuList.h"
#include "gun_bros_re/ui/menus/CMenuGreeting.h"
#include "gun_bros_re/ui/host/ZLoadingScreen.h"
#include "gun_bros_re/ui/host/ZMenuWipe.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
#include "gun_bros_re/cheats/CheatActions.h"
#include "gun_bros_re/data/profile/CProfileManager.h"
#include "gun_bros_re/gameplay/powerup/CPowerup.h"
#include "gun_bros_re/startup/ZStartupSequence.h"
#include "engine/glu/sprite/CSpriteIterator.h"

namespace MenuDetail {
class ZStoreRegionClip {
public:
    ZStoreRegionClip(ZMenuSurface &view, const ZMovieRegion &area) {
        enabled = glIsEnabled(GL_SCISSOR_TEST);
        glGetIntegerv(GL_SCISSOR_BOX, previous);
        view.Clip(area.x, area.y, std::max(0.0f, area.width), std::max(0.0f, area.height));
        if (enabled) {
            GLint current[4];
            glGetIntegerv(GL_SCISSOR_BOX, current);
            const int left = std::max(previous[0], current[0]);
            const int bottom = std::max(previous[1], current[1]);
            const int right = std::min(previous[0] + previous[2], current[0] + current[2]);
            const int top = std::min(previous[1] + previous[3], current[1] + current[3]);
            glScissor(left, bottom, std::max(0, right - left), std::max(0, top - bottom));
        }
    }
    ~ZStoreRegionClip() {
        glScissor(previous[0], previous[1], previous[2], previous[3]);
        if (!enabled) { glDisable(GL_SCISSOR_TEST); }
    }
private:
    GLboolean enabled = GL_FALSE;
    GLint previous[4]{};
};
}
