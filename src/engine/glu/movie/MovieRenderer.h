/** @file MovieRenderer.h
 * @brief Draw original CMovie resources and expose their interactive regions.
 */
#ifndef GUN_BROS_RE_MOVIERENDERER_H
#define GUN_BROS_RE_MOVIERENDERER_H
#include "engine/glu/movie/CMovie.h"
#include "engine/glu/sprite/CSpriteGlu.h"
#include "engine/glu/sprite/CSpriteIterator.h"
#include "engine/glu/sprite/CSpritePlayer.h"
#include "engine/graphics/CBitmapFont.h"
#include "engine/graphics/CMarkerBatch.h"
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
class IMovieRegionCallback {
public:
    virtual ~IMovieRegionCallback() = default;
    virtual bool DrawMovieRegion(const MovieRegion &region) = 0;
};

class MovieRenderer {
public:
    bool Init(CResPackTOC &pack, CResPackTOC &core);
    CResPackTOC &CorePack() const { return *m_core; }
    CMovie *GetMovie(unsigned ordinal);
    int FindMovie(const char *name) const;
    /** Resolve and cache a movie ordinal from its original resource alias. */
    unsigned Ordinal(const char *name);
    /** One user region of a movie; the original screen layouts are built from these. */
    bool Region(unsigned ordinal, unsigned index, unsigned time, MovieRegion &region);
    bool Draw(unsigned ordinal, unsigned time, float x = 512, float y = 384, float width = 1024, float height = 768,
        unsigned depth = 0, float alpha = 1, IMovieRegionCallback *callback = nullptr);
    bool DrawNamed(const char *name, unsigned time, float x = 512, float y = 384);
    bool DrawFitted(unsigned ordinal, unsigned time, float x, float y, float width, float height, unsigned regionIndex = 0);
    /** Some original tables index consecutive resource handles, not alias suffixes. */
    std::string NamedString(const char *name, unsigned offset = 0);
    bool DrawSprite(unsigned archetype, unsigned animation, unsigned time, float x, float y, float scale = 1, float alpha = 1, float rotation = 0);
    bool DrawSpriteFitted(unsigned archetype, unsigned animation, unsigned time, float x, float y, float width, float height, float alpha = 1);
    bool ButtonBackground(float x, float y, float width, float height, bool selected, bool hovered);
    unsigned SpriteDuration(unsigned archetype, unsigned animation);
    bool SpriteFrameTimes(unsigned archetype, unsigned animation, std::vector<unsigned> &times);
    /** Bind native step playback to cached BIG durations; cache outlives menu state. */
    bool BindSpritePlayer(unsigned archetype, unsigned animation, CSpritePlayer &player);
    bool DrawSpritePlayer(unsigned archetype, unsigned animation, const CSpritePlayer &player,
        float x, float y, float alpha = 1);
    /** Original sprite geometry, for callbacks that align without scaling. */
    bool SpriteBounds(unsigned archetype, unsigned animation, MovieRegion &bounds);
    void Image(const CTexture &texture, float x, float y, float width, float height, bool flipVertical = false);
    bool Text(const std::string &text, float x, float y, unsigned font = 0, float scale = 1, float maxWidth = 0, float alpha = 1);
    float TextWidth(const std::string &text, unsigned font = 0, float scale = 1);
    /** Authored line height of the BIG bitmap font used by Text. */
    float TextHeight(unsigned font = 0, float scale = 1);
    std::vector<MovieRegion> Regions(unsigned ordinal, unsigned time, float x = 512, float y = 384, bool includeInvisible = false);
    void Rectangle(float x, float y, float width, float height, float r, float g, float b, float alpha = 1);
    bool Gradient(float x, float y, float width, float height, unsigned topRgb, unsigned bottomRgb, float alpha = 1);
    void SetRegionOverlay(bool enabled) { m_regionOverlay = enabled; }
    unsigned Failures() const { return m_failures; }
    /** External mesh/sprite callbacks inherit the current region transform. */
    const float *CurrentProjection() const { return m_projection; }
private:
    struct Metrics { float x = 0, y = 0, left = 0, top = 0, width = 0, height = 0; };
    struct Animation {
        std::vector<std::vector<SpriteQuad>> steps;
        std::vector<std::uint16_t> durations;
        Metrics bounds;
        unsigned duration = 0;
    };
    Animation *GetAnimation(unsigned archetype, unsigned animation);
    CBitmapFont *GetFont(unsigned index);
    MovieKeyFrame AtTime(const MovieObject &object, unsigned time) const;
    Metrics GetMetrics(const CMovie &movie, unsigned object, unsigned time, float width, float height, unsigned depth = 0);
    Metrics GetFrameMetrics(const CMovie &movie, unsigned object, const MovieKeyFrame &frame,
        unsigned time, float width, float height, unsigned depth);
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
