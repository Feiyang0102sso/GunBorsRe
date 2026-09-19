#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/ui/menus/CMenuMission.h"
#include "engine/graphics/ZQuadBatch.h"
namespace MenuDetail {
/** Native CMenuMission callbacks and Planet::CreateLargeImage :170099.
 * The surface owns this per-window instance so textures die before GL context. */
class CMenuMission::Presentation {
public:
    explicit Presentation(ZMovieRenderer &renderer) : movies(renderer) {}
    bool Load(CResTOCManager &toc, CGunBros &tables, ZShaderProgram &image, ZShaderProgram &text);
    std::vector<std::string> names, descriptions;
    std::vector<MenuDetail::CMenuMission::PlanetEntry> planetEntries;

    int PlanetForMapSlot(unsigned slot) const {
        for (unsigned index = 0; index < planetEntries.size(); ++index) {
            if (planetEntries[index].data.mapSlot == slot) { return static_cast<int>(index); }
        }
        return -1;
    }

    /** PlanetImageCallback :188957 retains original sprite geometry and origin. */
    void DrawPlanetOriginal(unsigned index, const ZMovieRegion &region);

    /** Original PlanetCallback uses native sprite pixels and a centered hit box. */
    ZMovieRegion DrawPlanetThumb(unsigned index, const ZMovieRegion &region, float fade = 1);

    void PlanetFlagLines(float centerX, float centerY, const ZMovieRegion &area);
private:
    ZMovieRenderer &movies;
    ZShaderProgram *imageProgram = nullptr;
    ZShaderProgram *textProgram = nullptr;
    ZQuadBatch images;
    ZMarkerBatch markers;
    std::vector<std::vector<ZSpriteQuad>> planetQuads, planetThumbs;
    std::map<int, std::unique_ptr<CSpriteGlu>> spritePacks;
};
}
