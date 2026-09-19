#include "gun_bros_re/ui/menus/CMenuPostGameOption.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
namespace MenuDetail {
CMenuPostGameOption::CMenuPostGameOption(ZMenuSurface &menu, const CMenuDataProvider::Entry &data, const std::string &number, unsigned elapsed, bool live)
        : view(menu), entry(data), value(number), iconTime(elapsed), effectIndex(data.index) {
        if (live) {
            // DM omits assists: resolve the shared effect by its original icon.
            for (unsigned index = 0; index < 8; ++index) {
                const auto *icon = CMenuDataProvider::Find("MDS_ICON_POSTGAME_MP", index);
                if (icon != nullptr && icon->sprites[0] == data.sprites[0]) { effectIndex = index + 7; break; }
            }
        }
    }
bool CMenuPostGameOption::DrawMovieRegion(const ZMovieRegion &region) {
        if (region.index == 1) {
            const unsigned sprite = entry.sprites[0];
            if (sprite == UINT32_MAX) { return true; }
            // A tab can become visible during this frame's input dispatch.
            if (!view.postGameEffects.AdvancePostGameEffect(effectIndex, 0)) { return false; }
            view.postGameEffects.DrawPostGameEffect(effectIndex, region);
            return view.movies.DrawSprite(sprite >> 16, sprite & 255, iconTime,
                region.x + static_cast<int>(region.width) / 2, region.y + static_cast<int>(region.height) / 2, 1, region.alpha);
        }
        // CMenuPostGameOption::Bind/Draw :249820/:249848: title then value.
        if (region.index == 2) { UpgradeCenteredText(view, region, view.movies.NamedString(entry.strings[0]), 0); }
        if (region.index == 3) { UpgradeCenteredText(view, region, value, 6); }
        return true;
    }
}
