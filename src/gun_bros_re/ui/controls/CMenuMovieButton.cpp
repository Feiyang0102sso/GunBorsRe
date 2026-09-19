#include "gun_bros_re/ui/controls/CMenuMovieButton.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
namespace MenuDetail {

void CMenuMovieButton::Cancel() {
    playback.Cancel();
    binding = nullptr;
    selected = false;
    captured = false;
}

bool CMenuMovieButton::Draw(ZMenuSurface &view, const CMenuDataProvider::Entry &entry,
    const ZMovieRegion &origin, unsigned font, bool enabled, bool &activated) {
    activated = false;
    const unsigned ordinal = view.movies.Ordinal(entry.movies[0]);
    const CMovie *movie = view.movies.GetMovie(ordinal);
    if (movie == nullptr) { return false; }
    if (binding != &entry) {
        Cancel();
        binding = &entry;
        playback.Bind(*movie);
        if (!playback.SetChapter(2)) { return false; }
        playback.SetLoop(true);
        lastTick = view.clock;
    }
    unsigned elapsed = 0;
    if (view.clock >= lastTick) {
        elapsed = static_cast<unsigned>(std::min<std::uint64_t>(view.clock - lastTick, UINT32_MAX));
    }
    lastTick = view.clock;
    if (!enabled && (selected || captured)) {
        selected = false;
        captured = false;
        if (!playback.SetChapter(2)) { return false; }
        playback.SetLoop(true);
    }
    playback.Update(elapsed);
    if (selected && playback.TakeCompletion()) {
        std::printf("[menu-button] complete table=%s row=%u\n", entry.table, entry.index);
        activated = true;
        selected = false;
        if (!playback.SetChapter(2)) { return false; }
        playback.SetLoop(true);
    }
    ZMovieRegion touch;
    bool foundTouch = false;
    for (const auto &region : view.movies.Regions(ordinal, playback.GetTime(), origin.x, origin.y, true)) {
        if (region.index == 0) { touch = region; foundTouch = true; break; }
    }
    if (!foundTouch) { return false; }
    const bool inside = view.MouseIn(touch.x, touch.y, touch.width, touch.height);

    if (enabled && !selected && !activated) {
        if (view.pointerPressed) { captured = inside; }
        if (view.pointerHeld && !inside) { captured = false; }
        // A scripted tap is a complete press/release supplied by the host.
        const bool canRelease = captured || view.HasInjectedClick();
        if (canRelease && view.Hit(touch.x, touch.y, touch.width, touch.height)) {
            std::printf("[menu-button] selected table=%s row=%u clock=%llu\n", entry.table, entry.index, static_cast<unsigned long long>(view.clock));
            captured = false;
            selected = true;
            playback.SetLoop(false);
            if (!playback.SetChapter(1)) { return false; }
        }
    } else if (enabled && inside) {
        // Consume repeated taps instead of letting them fold the parent card.
        view.Hit(touch.x, touch.y, touch.width, touch.height);
    }
    if (view.pointerReleased) { captured = false; }
    unsigned chapter = 2;
    if (selected) { chapter = 1; }
    else if (captured) { chapter = 3; }
    bool unused = false;
    return CMenuMovieButton::DrawFrame(view, entry, origin, view.movies.NamedString(entry.strings[0]),
        font, false, unused, chapter, elapsed, playback.GetTime(), false);
}

/** A layout region must exist; a miss means the movie or chapter is wrong. */
bool RequireRegion(ZMenuSurface &view, unsigned movie, unsigned index, unsigned time, ZMovieRegion &region, const char *what) {
    if (view.movies.Region(movie, index, time, region)) { return true; }
    std::printf("[store] missing region %u of movie %u at %u ms (%s)\n", index, movie, time, what);
    return false;
}

/** Centre one original label inside a plate. */
void PlateLabel(ZMenuSurface &view, const std::string &label, float x, float y, float width, float height) {
    view.movies.Text(label, x + (width - view.movies.TextWidth(label, 5)) * 0.5f,
        y + (height - view.movies.TextHeight(5)) * 0.5f, 5, 1);
}

bool CMenuMovieButton::DrawFrame(ZMenuSurface &view, const CMenuDataProvider::Entry &entry, const ZMovieRegion &area,
    const std::string &label, unsigned font, bool interactive, bool &pressed, unsigned chapter, unsigned elapsed, unsigned timeOverride, bool stateArtwork) {
    pressed = false;
    const unsigned ordinal = view.movies.Ordinal(entry.movies[0]);
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(chapter, start, end)) { return false; }
    unsigned time = end;
    if (chapter == 2 || chapter == 3) { time = start + elapsed % (end - start + 1); }
    if (timeOverride != UINT32_MAX) { time = timeOverride; }
    // CMenuMovieButton::ButtonCallback :144426 selects sprite1 while idle,
    // sprite0 while focused/selected. The Movie chapter only owns the glow.
    unsigned sprite = entry.sprites[0];
    if (stateArtwork && chapter != 1 && chapter != 3) { sprite = entry.sprites[1]; }
    std::string text = label;
    // CMenuMovieButton::Init :144942: optional resource-authored ^fN font prefix.
    if (text.size() >= 4 && text.compare(0, 2, "^f") == 0 && text[2] >= '0' && text[2] <= '9') {
        font = static_cast<unsigned>(text[2] - '0');
        text.erase(0, 3);
    }
    bool foundGraphic = false, foundTouch = false;
    class ButtonCallback : public ZMovieRegionCallback {
    public:
        ButtonCallback(ZMenuSurface &menu, const CMenuDataProvider::Entry &data, const std::string &caption, unsigned face,
            unsigned artwork, bool enabled, bool &hit, bool &graphic, bool &touch) : view(menu), entry(data), text(caption), font(face), sprite(artwork),
            interactive(enabled), pressed(hit), foundGraphic(graphic), foundTouch(touch) {}
        bool DrawMovieRegion(const ZMovieRegion &region) override {
            if (region.index == 1) {
                foundGraphic = true;
                if (sprite != UINT32_MAX && !view.movies.DrawSprite(sprite >> 16, sprite & 255, 0,
                    region.x, region.y, 1, region.alpha)) { return false; }
                if (!text.empty()) {
                    view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, font)) / 2,
                        region.y + (region.height - view.movies.TextHeight(font)) / 2, font, 1, 0, region.alpha);
                }
            }
            if (region.index == 0) {
                foundTouch = true;
                if (interactive && region.alpha > 0) { pressed = view.Hit(region.x, region.y, region.width, region.height); }
            }
            return true;
        }
        ZMenuSurface &view;
        const CMenuDataProvider::Entry &entry;
        const std::string &text;
        unsigned font;
        unsigned sprite;
        bool interactive;
        bool &pressed, &foundGraphic, &foundTouch;
    } callback(view, entry, text, font, sprite, interactive, pressed, foundGraphic, foundTouch);
    // Place the content in its original type-6 layer so later glow layers cover it.
    if (!view.movies.Draw(ordinal, time, area.x, area.y, kMenuWidth, kMenuHeight, 0, area.alpha, &callback)) { return false; }
    // BACK_BUTTON has only an invisible logical region0. Input is updated
    // independently of Draw in CMenuMovieButton :144250; alpha0 is not missing data.
    for (const auto &region : view.movies.Regions(ordinal, time, area.x, area.y, true)) {
        if (region.index == 1) { foundGraphic = true; }
        if (region.index == 0 && !foundTouch) {
            foundTouch = true;
            if (interactive) { pressed = view.Hit(region.x, region.y, region.width, region.height); }
        }
    }
    if (entry.sprites[0] == UINT32_MAX && text.empty()) { foundGraphic = true; }
    return foundGraphic && foundTouch;
}
}
