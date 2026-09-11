/** @file CBitmapFont.h
 * @brief Original bitmap-font metrics and atlas rendering, without host fonts.
 */
#ifndef GUN_BROS_RE_CBITMAPFONT_H
#define GUN_BROS_RE_CBITMAPFONT_H
#include "engine/graphics/CQuadBatch.h"
#include "engine/resources/CResPackTOC.h"
#include <map>

class CBitmapFont {
public:
    bool Init(CResPackTOC &core, unsigned index);
    float Width(const std::string &text, float scale = 1) const;
    float Height(float scale = 1) const { return m_height * scale; }
    void Draw(CQuadBatch &batch, const std::string &text, float x, float y, float scale = 1, float alpha = 1) const;
private:
    struct Glyph {
        SourceRect source{};
        int offsetX = 0, offsetY = 0, advance = 0;
    };
    CTexture m_texture;
    std::map<unsigned, Glyph> m_glyphs;
    std::map<unsigned, int> m_controls;
    int m_height = 0;
    int m_spacing = 0;
};
#endif
