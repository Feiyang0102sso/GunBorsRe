#include "gun_bros_re/ui/menus/CMenuMissionPresentation.h"
#include "engine/glu/sprite/CSpriteIterator.h"
namespace MenuDetail {
bool CMenuMission::Presentation::Load(CResTOCManager &toc, CGunBros &tables,
    ZShaderProgram &image, ZShaderProgram &text) {
    imageProgram = &image;
    textProgram = &text;
    if (!images.Create(image) || !markers.Create(text)) { return false; }
        if (!MenuDetail::CMenuMission::LoadPlanets(toc, tables, planetEntries)) { return false; }
        names.resize(planetEntries.size());
        descriptions.resize(planetEntries.size());
        planetQuads.resize(planetEntries.size());
        planetThumbs.resize(planetEntries.size());
        for (unsigned index = 0; index < planetEntries.size(); ++index) {
            const Planet &planet = planetEntries[index].data;
            names[index] = tables.ReadString(planet.name);
            descriptions[index] = tables.ReadString(planet.description);
            // Planet::CreateLargeImage :170099 uses the map sprite's pack.
            const int spritePack = toc.GetPackIndexFromHash(planet.thumbnail.packHash);
            if (spritePacks.count(spritePack) == 0) {
                auto glu = std::make_unique<CSpriteGlu>();
                if (!glu->Init(*toc.GetPack(spritePack))) { return false; }
                spritePacks[spritePack] = std::move(glu);
            }
            CSpriteGlu &glu = *spritePacks[spritePack];
            const ZSpriteArchetype *large = glu.GetArchetype(planet.largeImage.archetype);
            const ZSpriteArchetype *thumb = glu.GetArchetype(planet.thumbnail.archetype);
            if (large == nullptr || thumb == nullptr) { return false; }
            CSpriteIterator largeIterator(glu, *large), thumbIterator(glu, *thumb);
            if (!largeIterator.Expand(planet.largeImage.animation, 0, planetQuads[index]) ||
                !thumbIterator.Expand(planet.thumbnail.animation, 0, planetThumbs[index])) { return false; }
            std::printf("[planet-menu] host=%u slot=%u resource=%u:%u missions=%zu name=%s\n",
                index, planet.mapSlot, planetEntries[index].resource.packHash, planetEntries[index].resource.localIndex,
                planet.missions.size(), names[index].c_str());
        }
    return true;
}
    /** PlanetImageCallback :188957 retains original sprite geometry and origin. */
    void CMenuMission::Presentation::DrawPlanetOriginal(unsigned index, const ZMovieRegion &region) {
        images.Begin();
        for (const auto &quad : planetQuads[index]) {
            images.AddTransformedQuad(*quad.page, region.x + region.width / 2 + quad.offsetX,
                region.y + region.height / 2 + quad.offsetY, float(quad.Width()), float(quad.Height()),
                quad.source, quad.flipHorizontal, quad.flipVertical, quad.blend, 0, 0, 1, 1, 0, region.alpha, quad.rotateTexture);
        }
        images.Upload();
        images.Draw(*imageProgram, movies.CurrentProjection());
    }

    /** Original PlanetCallback uses native sprite pixels and a centered hit box. */
    ZMovieRegion CMenuMission::Presentation::DrawPlanetThumb(unsigned index, const ZMovieRegion &region, float fade ) {
        const auto &quads = planetThumbs[index];
        float left = 0, top = 0, right = 0, bottom = 0;
        bool first = true;
        images.Begin();
        const float centerX = region.x + region.width / 2;
        const float centerY = region.y + region.height / 2;
        for (const auto &quad : quads) {
            if (first) {
                left = right = static_cast<float>(quad.offsetX);
                top = bottom = static_cast<float>(quad.offsetY);
                first = false;
            }
            left = std::min(left, float(quad.offsetX));
            top = std::min(top, float(quad.offsetY));
            right = std::max(right, float(quad.offsetX + quad.Width()));
            bottom = std::max(bottom, float(quad.offsetY + quad.Height()));
            images.AddTransformedQuad(*quad.page, centerX + quad.offsetX, centerY + quad.offsetY,
                float(quad.Width()), float(quad.Height()), quad.source, quad.flipHorizontal, quad.flipVertical,
                quad.blend, 0, 0, 1, 1, 0, region.alpha * fade, quad.rotateTexture);
        }
        images.Upload();
        images.Draw(*imageProgram, movies.CurrentProjection());
        return {region.index, region.type, centerX - (right - left) / 2, centerY - (bottom - top) / 2,
            right - left, bottom - top, region.alpha};
    }

    void CMenuMission::Presentation::PlanetFlagLines(float centerX, float centerY, const ZMovieRegion &area) {
        // FlagPoleCallback :161369 uses native color 0x807CC9F3 and 1px lines.
        markers.Begin();
        markers.AddSegment(centerX, centerY, area.x, area.y, 1);
        markers.AddSegment(centerX, centerY, area.x + area.width, area.y, 1);
        markers.AddSegment(centerX, centerY, area.x, area.y + area.height, 1);
        markers.AddSegment(centerX, centerY, area.x + area.width, area.y + area.height, 1);
        markers.Draw(*textProgram, movies.CurrentProjection(), 124.0f / 255, 201.0f / 255, 243.0f / 255, area.alpha * 0.5f);
        movies.Rectangle(area.x, area.y, area.width, area.height, 0, 0, 0, area.alpha * 0.5f);
    }


}
