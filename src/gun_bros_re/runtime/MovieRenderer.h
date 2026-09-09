/** @file MovieRenderer.h
 * @brief Draw original CMovie resources and expose their interactive regions.
 */
#ifndef GUN_BROS_RE_MOVIERENDERER_H
#define GUN_BROS_RE_MOVIERENDERER_H
#include "gun_bros/CMovie.h"
#include "sprite_glu/CSpriteGlu.h"
#include "sprite_glu/CSpriteIterator.h"
#include "engine/CBitmapFont.h"
#include "engine/CMarkerBatch.h"
#include <map>
#include <string>

struct MovieRegion {
    unsigned index = 0;
    unsigned type = 0;
    float x = 0, y = 0, width = 0, height = 0;
    float alpha = 1;
    bool Contains(float pointX, float pointY) const {
        return pointX >= x && pointY >= y && pointX < x + width && pointY < y + height;
    }
};

/** Host adapter owns GL caches; CMovie itself remains pure original data. */
class MovieRenderer {
public:
    bool Init(CResPackTOC &pack, CResPackTOC &core);
    CMovie *GetMovie(unsigned ordinal);
    int FindMovie(const char *name) const;
    /** Resolve and cache a movie ordinal from its original resource alias. */
    unsigned Ordinal(const char *name);
    /** One user region of a movie; the original screen layouts are built from these. */
    bool Region(unsigned ordinal, unsigned index, unsigned time, MovieRegion &region);
    bool Draw(unsigned ordinal, unsigned time, float x = 512, float y = 384, float width = 1024, float height = 768,
        unsigned depth = 0, float alpha = 1);
    bool DrawNamed(const char *name, unsigned time, float x = 512, float y = 384);
    bool DrawFitted(unsigned ordinal, unsigned time, float x, float y, float width, float height, unsigned regionIndex = 0);
    std::string NamedString(const char *name);
    bool DrawSprite(unsigned archetype, unsigned animation, unsigned time, float x, float y, float scale = 1, float alpha = 1, float rotation = 0);
    bool DrawSpriteFitted(unsigned archetype, unsigned animation, unsigned time, float x, float y, float width, float height);
    bool ButtonBackground(float x, float y, float width, float height, bool selected, bool hovered);
    unsigned SpriteDuration(unsigned archetype, unsigned animation);
    void Image(const CTexture &texture, float x, float y, float width, float height);
    bool Text(const std::string &text, float x, float y, unsigned font = 0, float scale = 1, float maxWidth = 0, float alpha = 1);
    float TextWidth(const std::string &text, unsigned font = 0, float scale = 1);
    std::vector<MovieRegion> Regions(unsigned ordinal, unsigned time, float x = 512, float y = 384);
    void Rectangle(float x, float y, float width, float height, float r, float g, float b, float alpha = 1);
    void SetRegionOverlay(bool enabled) { m_regionOverlay = enabled; }
    unsigned Failures() const { return m_failures; }
private:
    struct Metrics { float x = 0, y = 0, left = 0, top = 0, width = 0, height = 0; };
    struct Animation {
        std::vector<std::vector<SpriteQuad>> steps;
        std::vector<unsigned> durations;
        Metrics bounds;
        unsigned duration = 0;
    };
    Animation *GetAnimation(unsigned archetype, unsigned animation);
    CBitmapFont *GetFont(unsigned index);
    MovieKeyFrame AtTime(const MovieObject &object, unsigned time) const;
    Metrics GetMetrics(const CMovie &movie, unsigned object, unsigned time, float width, float height, unsigned depth = 0);
    void Flush();
    bool StretchButtonSprite(unsigned animation, float x, float y, float width, float height);
    CResPackTOC *m_pack = nullptr;
    CResPackTOC *m_core = nullptr;
    CSpriteGlu m_sprites;
    CShaderProgram m_program, m_colorProgram;
    CQuadBatch m_batch;
    CMarkerBatch m_markers;
    float m_projection[16]{};
    std::map<unsigned, CMovie> m_movies;
    std::map<std::string, unsigned> m_ordinals;
    std::map<unsigned, Animation> m_animations;
    std::map<unsigned, std::unique_ptr<CBitmapFont>> m_fonts;
    std::map<std::string, std::string> m_strings;
    std::map<std::uint64_t, std::unique_ptr<CTexture>> m_gradients;
    bool m_regionOverlay = false;
    unsigned m_failures = 0;
};
#endif
