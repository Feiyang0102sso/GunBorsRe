#pragma once
#include "gun_bros_re/ui/content/CMenuDataProvider.h"
#include "engine/glu/movie/ZMovieRenderer.h"

namespace MenuDetail {
class ZMenuSurface;
/** Native toggle mode 3: release selects chapter 1; completion dispatches once.
 * Init :144910, HandleTouchInput :144483, Select :144661, Update :144715.
 * Geometry and playback bounds are supplied by ui_movie.bt resources. */
class CMenuMovieButton {
public:
    bool Draw(ZMenuSurface &view, const CMenuDataProvider::Entry &entry,
        const ZMovieRegion &origin, unsigned font, bool enabled, bool &activated);
    void Cancel();
    static bool DrawFrame(ZMenuSurface &view, const CMenuDataProvider::Entry &entry, const ZMovieRegion &area,
        const std::string &label, unsigned font, bool interactive, bool &pressed,
        unsigned chapter = 0, unsigned elapsed = 0, unsigned timeOverride = UINT32_MAX, bool stateArtwork = false);
    bool IsSelected() const { return selected; }

private:
    const CMenuDataProvider::Entry *binding = nullptr;
    CMovie::Playback playback;
    std::uint64_t lastTick = 0;
    bool selected = false;
    bool captured = false;
};

/** A layout region must exist; a miss means the movie or chapter is wrong. */
bool RequireRegion(ZMenuSurface &view, unsigned movie, unsigned index, unsigned time, ZMovieRegion &region, const char *what);

// Every menu button prints its label at the same size; the original never
// squeezes one to fit a narrower plate, it picks a wider plate instead.

/** Centre one original label inside a plate. */
void PlateLabel(ZMenuSurface &view, const std::string &label, float x, float y, float width, float height);
}
