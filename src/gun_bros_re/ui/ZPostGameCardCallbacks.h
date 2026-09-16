#pragma once
#include "gun_bros_re/ui/ZMenuInternal.h"
namespace MenuDetail {
class ZPostGameCardCallbacks : public ZMovieRegionCallback {
public:
    ZPostGameCardCallbacks(ZGameMenu &menu, const ZMenuDataEntry &data, const std::string &number, unsigned elapsed = 0, bool live = false)
        : view(menu), entry(data), value(number), iconTime(elapsed), effectIndex(data.index) {
        if (live) {
            // DM omits assists: resolve the shared effect by its original icon.
            for (unsigned index = 0; index < 8; ++index) {
                const auto *icon = FindMenuData("MDS_ICON_POSTGAME_MP", index);
                if (icon != nullptr && icon->sprites[0] == data.sprites[0]) { effectIndex = index + 7; break; }
            }
        }
    }
    bool DrawMovieRegion(const ZMovieRegion &region) override {
        if (region.index == 1) {
            const unsigned sprite = entry.sprites[0];
            if (sprite == UINT32_MAX) { return true; }
            // A tab can become visible during this frame's input dispatch.
            if (!view.AdvancePostGameEffect(effectIndex, 0)) { return false; }
            view.DrawPostGameEffect(effectIndex, region);
            return view.movies.DrawSprite(sprite >> 16, sprite & 255, iconTime,
                region.x + static_cast<int>(region.width) / 2, region.y + static_cast<int>(region.height) / 2, 1, region.alpha);
        }
        // CMenuPostGameOption::Bind/Draw :249820/:249848: title then value.
        if (region.index == 2) { UpgradeCenteredText(view, region, view.movies.NamedString(entry.strings[0]), 0); }
        if (region.index == 3) { UpgradeCenteredText(view, region, value, 6); }
        return true;
    }
    ZGameMenu &view;
    const ZMenuDataEntry &entry;
    const std::string &value;
    unsigned iconTime;
    unsigned effectIndex;
};
}
