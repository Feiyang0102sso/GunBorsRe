#pragma once
#include "gun_bros_re/ui/content/CMenuDataProvider.h"
#include "engine/glu/movie/ZMovieRenderer.h"
namespace MenuDetail {
class ZMenuSurface;
class CMenuPostGameOption : public ZMovieRegionCallback {
public:
    class Effects;
    CMenuPostGameOption(ZMenuSurface &menu, const CMenuDataProvider::Entry &data,
        const std::string &number, unsigned elapsed = 0, bool live = false);
    bool DrawMovieRegion(const ZMovieRegion &region) override;

    ZMenuSurface &view;
    const CMenuDataProvider::Entry &entry;
    const std::string &value;
    unsigned iconTime;
    unsigned effectIndex;
};
}
