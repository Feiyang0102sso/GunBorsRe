/**
 * @file CTexture.h
 * @brief A GL texture holding one decoded image.
 *
 * Stands in for the CResourceDIB / CDIB pair on the resource side and the
 * texture half of CGraphics_OGLES2. The engine's texture needs are tiny --
 * RGBA8, clamped, no mips, no compression -- so this stays a thin owner rather
 * than a port of either.
 */

#ifndef GUN_BROS_RE_ENGINE_CTEXTURE_H
#define GUN_BROS_RE_ENGINE_CTEXTURE_H

#include "engine/CPNG.h"
#include "engine/platform/GLLoader.h"

#include <cstdint>

class CTexture {
public:
    CTexture();
    ~CTexture();

    CTexture(const CTexture &) = delete;
    CTexture &operator=(const CTexture &) = delete;

    /** Upload an RGBA8 image. Replaces whatever this held before. */
    bool Create(const PNGImage &image);

    void Destroy();

    bool IsValid() const { return m_handle != 0; }

    GLuint GetHandle() const { return m_handle; }
    std::uint32_t GetWidth() const { return m_width; }
    std::uint32_t GetHeight() const { return m_height; }

    /** Bind to a texture unit. */
    void Bind(GLenum textureUnit) const;

private:
    GLuint m_handle;
    std::uint32_t m_width;
    std::uint32_t m_height;
};

#endif  // GUN_BROS_RE_ENGINE_CTEXTURE_H
