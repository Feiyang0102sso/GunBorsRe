/** @file CBitmapFont.cpp
 * @brief CFontMgr::GetFont :56753; CBitmapFont::ParseFontMetrics :395475.
 */
#define NOMINMAX
#include "engine/CBitmapFont.h"
#include "engine/CArrayInputStream.h"
#include <algorithm>
#include <cstdio>

namespace {
std::vector<unsigned> Codepoints(const std::string &text) {
    std::vector<unsigned> output;
    for (std::size_t index = 0; index < text.size();) {
        unsigned value = static_cast<unsigned char>(text[index++]);
        unsigned trailing = 0;
        if ((value & 0xE0) == 0xC0) { value &= 31; trailing = 1; }
        else if ((value & 0xF0) == 0xE0) { value &= 15; trailing = 2; }
        else if ((value & 0xF8) == 0xF0) { value &= 7; trailing = 3; }
        for (unsigned count = 0; count < trailing && index < text.size(); ++count) {
            value = (value << 6) | (static_cast<unsigned char>(text[index++]) & 63);
        }
        output.push_back(value);
    }
    return output;
}

std::vector<unsigned> ReadJmUtf(CArrayInputStream &input) {
    // ReadJMUtf temporarily selects big endian for the byte-length prefix.
    const unsigned high = input.ReadUInt8();
    const unsigned low = input.ReadUInt8();
    const unsigned size = high * 256 + low;
    std::string text;
    for (unsigned index = 0; index < size; ++index) { text.push_back(static_cast<char>(input.ReadUInt8())); }
    return Codepoints(text);
}
}

bool CBitmapFont::Init(CResPackTOC &core, unsigned index) {
    std::vector<std::uint8_t> bytes;
    if (!core.GetResource(core.GetResValue("FONT_KEYSET"), bytes)) { return false; }
    CArrayInputStream keys(bytes);
    if (index >= keys.ReadUInt16() / 2) { return false; }
    keys.Skip(index * 8);
    const unsigned metrics = keys.ReadUInt32();
    const unsigned texture = keys.ReadUInt32();
    if (!core.GetResource(metrics, bytes) || bytes.size() < 12) { return false; }
    CArrayInputStream input(bytes);
    const unsigned version = input.ReadUInt8();
    input.Skip(3);
    input.ReadUInt8(); // Maximum glyph height; line height is the next byte.
    const int bodyHeight = static_cast<std::int8_t>(input.ReadUInt8());
    m_spacing = static_cast<std::int8_t>(input.ReadUInt8());
    m_height = bodyHeight + static_cast<std::int8_t>(input.ReadUInt8());
    const unsigned glyphCount = input.ReadUInt16();
    const unsigned controlCount = input.ReadUInt16();
    std::vector<unsigned> codes;
    if (version == 2) { codes = ReadJmUtf(input); if (codes.size() != glyphCount) { return false; } }
    m_glyphs.clear();
    m_controls.clear();
    for (unsigned glyph = 0; glyph < glyphCount; ++glyph) {
        unsigned code = 0;
        if (version == 2) { code = codes[glyph]; }
        else { code = input.ReadUInt16(); }
        Glyph value;
        value.source.x = input.ReadUInt16();
        value.source.y = input.ReadUInt16();
        value.source.width = input.ReadUInt8();
        value.source.height = input.ReadUInt8();
        value.offsetX = static_cast<std::int8_t>(input.ReadUInt8());
        value.offsetY = static_cast<std::int8_t>(input.ReadUInt8());
        value.advance = static_cast<std::int8_t>(input.ReadUInt8());
        input.ReadUInt8(); // Reserved style byte in the original glyph descriptor.
        m_glyphs[code] = value;
    }
    if (version == 2) { codes = ReadJmUtf(input); if (codes.size() != controlCount) { return false; } }
    for (unsigned control = 0; control < controlCount; ++control) {
        unsigned code = 0;
        if (version == 2) { code = codes[control]; }
        else { code = input.ReadUInt16(); }
        input.ReadUInt8();
        m_controls[code] = static_cast<std::int8_t>(input.ReadUInt8());
    }
    if (input.Overran() || input.Available() != 0 || !core.GetResource(texture, bytes)) { return false; }
    PNGImage image;
    if (!PNGDecode(bytes, image) || !m_texture.Create(image)) { return false; }
    for (const auto &entry : m_glyphs) {
        const SourceRect &rect = entry.second.source;
        if (unsigned(rect.x) + rect.width > image.width || unsigned(rect.y) + rect.height > image.height) { return false; }
    }
    std::printf("[font] index=%u glyphs=%u controls=%u height=%d atlas=%ux%u\n", index, glyphCount, controlCount, m_height, image.width, image.height);
    return true;
}

float CBitmapFont::Width(const std::string &text, float scale) const {
    float width = 0, maximum = 0;
    for (unsigned code : Codepoints(text)) {
        if (code == '\n') { maximum = std::max(maximum, width); width = 0; continue; }
        const auto control = m_controls.find(code);
        if (control != m_controls.end()) { width += control->second + m_spacing; continue; }
        auto glyph = m_glyphs.find(code);
        if (glyph == m_glyphs.end()) { glyph = m_glyphs.find('?'); }
        if (glyph != m_glyphs.end()) { width += glyph->second.advance + m_spacing; }
    }
    return std::max(maximum, width) * scale;
}

void CBitmapFont::Draw(CQuadBatch &batch, const std::string &text, float x, float y, float scale, float alpha) const {
    const float startX = x;
    for (unsigned code : Codepoints(text)) {
        if (code == '\n') { x = startX; y += Height(scale); continue; }
        const auto control = m_controls.find(code);
        if (control != m_controls.end()) { x += (control->second + m_spacing) * scale; continue; }
        auto glyph = m_glyphs.find(code);
        if (glyph == m_glyphs.end()) { glyph = m_glyphs.find('?'); }
        if (glyph == m_glyphs.end()) { continue; }
        const Glyph &value = glyph->second;
        batch.AddTransformedQuad(m_texture, x + value.offsetX * scale, y + value.offsetY * scale,
            value.source.width * scale, value.source.height * scale, value.source, false, false,
            BlendMode::Alpha, 0, 0, 1, 1, 0, alpha);
        x += (value.advance + m_spacing) * scale;
    }
}
