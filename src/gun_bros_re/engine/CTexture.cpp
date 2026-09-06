/**
 * @file CTexture.cpp
 * @brief A GL texture holding one decoded image.
 */

#include "engine/CTexture.h"

#include <cstdio>

CTexture::CTexture() : m_handle(0), m_width(0), m_height(0) {}

CTexture::~CTexture() {
    Destroy();
}

void CTexture::Destroy() {
    if (m_handle != 0) {
        glDeleteTextures(1, &m_handle);
        m_handle = 0;
    }
    m_width = 0;
    m_height = 0;
}

bool CTexture::Create(const PNGImage &image, GLenum wrapMode) {
    Destroy();

    if (image.width == 0 || image.height == 0) {
        std::printf("[texture] refusing to upload an empty image\n");
        return false;
    }

    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_2D, m_handle);

    // Sprites come out of atlases, so clamping matters and wrapping would
    // bleed neighbouring tiles in. Models ask for GL_REPEAT instead.
    // No mips: nothing is ever minified far.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrapMode));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrapMode));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 static_cast<GLsizei>(image.width), static_cast<GLsizei>(image.height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());

    if (!GLCheckErrors("texture upload")) {
        Destroy();
        return false;
    }

    m_width = image.width;
    m_height = image.height;
    return true;
}

void CTexture::Bind(GLenum textureUnit) const {
    glActiveTexture(textureUnit);
    glBindTexture(GL_TEXTURE_2D, m_handle);
}
