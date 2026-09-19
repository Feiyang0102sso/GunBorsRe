#include "gun_bros_re/ui/host/ZStoreRegionClip.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuMovieMultiplayerOverlay.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
namespace MenuDetail {

class CMenuMovieMultiplayerOverlay::RegionCallback : public ZMovieRegionCallback {
public:
    RegionCallback(ZMenuSurface &menu, CMenuSystem &state, float otherAlpha) : view(menu), state(state), otherAlpha(otherAlpha) {}
    bool DrawMovieRegion(const ZMovieRegion &region) override {
        if (region.index > 5) { return true; }
        const unsigned mode = region.index / 2;
        const bool selected = state.mode.modeSelected && mode == state.gameMode;
        if (!selected && state.mode.modePhase == 2) { return true; }
        float alpha = region.alpha;
        if (!selected) { alpha *= otherAlpha; }
        const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_MP_TOGGLE", mode);
        if (entry == nullptr) { return false; }
        if (region.index % 2 == 1) {
            return view.movies.DrawSprite(entry->sprites[0] >> 16, entry->sprites[0] & 255, state.mode.modeSpriteTime,
                region.x + region.width / 2, region.y + region.height / 2, 1, alpha);
        }
        const std::string label = view.movies.NamedString(entry->strings[0]);
        if (selected) { view.modeEffects.DrawModeEffects(region); }
        return view.movies.Text(label, region.x + (region.width - view.movies.TextWidth(label, 0)) / 2,
            region.y + (region.height - view.movies.TextHeight(0)) / 2, 0, 1, 0, alpha);
    }
    ZMenuSurface &view;
    CMenuSystem &state;
    float otherAlpha;
};

/** CMenuMovieMultiplayerOverlay :250020..250880, region callbacks 0..5,
 * font 0 and MDS_BUTTON_MP_TOGGLE. No locally fabricated online mode. */
bool CMenuMovieMultiplayerOverlay::Draw(ZMenuSurface &view, CMenuSystem &state) {
    UpdateLocalConnection(state);
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_MULTIPLAYER_AND_VERSUS_MAP");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned openStart = 0, openEnd = 0, foldStart = 0, foldEnd = 0, unfoldStart = 0, unfoldEnd = 0;
    unsigned idleStart = 0, idleEnd = 0;
    if (movie == nullptr || !movie->GetChapterRange(0, openStart, openEnd) ||
        !movie->GetChapterRange(2, foldStart, foldEnd) || !movie->GetChapterRange(3, unfoldStart, unfoldEnd) ||
        !movie->GetChapterRange(4, idleStart, idleEnd)) { return false; }
    if (!state.mode.modeBound) {
        state.mode.modeBound = true;
        state.mode.modeLastTick = view.clock;
        state.mode.modeTime = openStart;
        state.mode.modePhase = 0;
        if (!view.animateNavigation) { state.mode.modeTime = openEnd; }
        if (state.mode.modeSelected) { state.mode.modePhase = 2; state.mode.modeTime = idleStart; }
    }
    if (state.mode.modeLastTick == 0) { state.mode.modeLastTick = view.clock; }
    const unsigned elapsed = static_cast<unsigned>(view.clock - state.mode.modeLastTick);
    state.mode.modeLastTick = view.clock;
    state.mode.modeSpriteTime += elapsed;
    if (!view.modeEffects.AdvanceModeEffects(elapsed)) { return false; }
    if (state.mode.modePhase == 0) { state.mode.modeTime = std::min(openEnd, state.mode.modeTime + elapsed); }
    if (state.mode.modePhase == 1) {
        state.mode.modeTime = std::min(idleEnd, state.mode.modeTime + elapsed);
        if (state.mode.modeTime == idleEnd) { state.mode.modePhase = 2; }
    } else if (state.mode.modePhase == 2) { state.mode.modeTime = idleStart + (state.mode.modeTime - idleStart + elapsed) % (idleEnd - idleStart + 1); }
    else if (state.mode.modePhase == 3) {
        state.mode.modeTime -= std::min(elapsed, state.mode.modeTime - foldStart);
        if (state.mode.modeTime == foldStart) { state.mode.modePhase = 0; }
    }
    float otherAlpha = 1;
    if (state.mode.modePhase == 1 || state.mode.modePhase == 3) {
        otherAlpha = std::max(0.0f, 1.0f - 2.0f * (state.mode.modeTime - foldStart) / (foldEnd - foldStart));
    }
    RegionCallback callback(view, state, otherAlpha);
    if (!view.movies.Draw(ordinal, state.mode.modeTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callback)) { return false; }
    if (state.mode.modePhase == 1 || state.mode.modePhase == 3 || (state.mode.modePhase == 0 && state.mode.modeTime < openEnd)) { return true; }
    for (const auto &region : view.movies.Regions(ordinal, state.mode.modeTime)) {
        if (region.index > 5 || region.index % 2 != 1) { continue; }
        const unsigned mode = region.index / 2;
        if (state.mode.modePhase == 2 && mode != state.gameMode) { continue; }
        if (!view.Hit(region.x, region.y, region.width, region.height)) { continue; }
        if (state.mode.modePhase == 2) {
            state.mode.modeTime = unfoldStart;
            state.mode.modePhase = 3;
        } else if (mode == 0 || state.online.IsConnected()) {
            state.gameMode = mode;
            state.mode.modeSelected = true;
            state.mode.modeTime = foldStart;
            state.mode.modePhase = 1;
            if (!view.modeEffects.StartModeSelectionEffect()) { return false; }
            if (state.stack.page == 22) { state.Navigate(0, true); }
        } else {
            // SetSelection :250261 uses table 189/2 when multiplayer service
            // availability (provider 82) is false, before changing game type.
            state.ShowStorePrompt("MDS_PROMPT_MP_UNAVAILABLE", false, true, 2);
            std::printf("[mode] unavailable original prompt; game type unchanged\n");
        }
    }
    return true;
}
} // namespace MenuDetail
