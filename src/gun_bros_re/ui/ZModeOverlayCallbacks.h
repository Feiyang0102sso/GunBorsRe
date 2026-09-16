#pragma once
#include "gun_bros_re/ui/ZMenuInternal.h"

namespace MenuDetail {
class ZModeOverlayCallbacks : public ZMovieRegionCallback {
public:
    ZModeOverlayCallbacks(ZGameMenu &menu, ZMenuState &state, float otherAlpha) : view(menu), state(state), otherAlpha(otherAlpha) {}
    bool DrawMovieRegion(const ZMovieRegion &region) override {
        if (region.index > 5) { return true; }
        const unsigned mode = region.index / 2;
        const bool selected = state.mode.modeSelected && mode == state.gameMode;
        if (!selected && state.mode.modePhase == 2) { return true; }
        float alpha = region.alpha;
        if (!selected) { alpha *= otherAlpha; }
        const auto *entry = FindMenuData("MDS_BUTTON_MP_TOGGLE", mode);
        if (entry == nullptr) { return false; }
        if (region.index % 2 == 1) {
            return view.movies.DrawSprite(entry->sprites[0] >> 16, entry->sprites[0] & 255, state.mode.modeSpriteTime,
                region.x + region.width / 2, region.y + region.height / 2, 1, alpha);
        }
        const std::string label = view.movies.NamedString(entry->strings[0]);
        if (selected) { view.DrawModeEffects(region); }
        return view.movies.Text(label, region.x + (region.width - view.movies.TextWidth(label, 0)) / 2,
            region.y + (region.height - view.movies.TextHeight(0)) / 2, 0, 1, 0, alpha);
    }
    ZGameMenu &view;
    ZMenuState &state;
    float otherAlpha;
};
}
