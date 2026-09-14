#pragma once
#include "gun_bros_re/ui/MenuInternal.h"
namespace MenuDetail {
class PostGameCardCallbacks : public IMovieRegionCallback {
public:
    PostGameCardCallbacks(GameMenu &menu, const OriginalMenuEntry &data, const std::string &number, unsigned elapsed = 0, bool live = false)
        : view(menu), entry(data), value(number), iconTime(elapsed), effectIndex(data.index) {
        if (live) { effectIndex += 7; }
    }
    bool DrawMovieRegion(const MovieRegion &region) override {
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
    GameMenu &view;
    const OriginalMenuEntry &entry;
    const std::string &value;
    unsigned iconTime;
    unsigned effectIndex;
};
}
