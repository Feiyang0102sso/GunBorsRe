/**
 * @file CQuadBatch.h
 * @brief Collects textured quads and draws them grouped by texture.
 *
 * This is the "minimal 2D batch" PLAN.md calls for, and the first thing in the
 * project with enough callers to justify a shape. It stands in for
 * CGraphics2d_OGLES without being a port of it: the original batched through
 * CBlit ops and a vertex buffer pool, which is machinery we have no need for
 * while everything on screen is an axis-aligned rectangle.
 *
 * Quads are accumulated per texture, then each texture's run is drawn in one
 * call. A map is a few hundred quads over two atlases, so that is two draws.
 *
 * Texture coordinates go into the buffer PRE-SCALED BY 4096, because the
 * ported vertex shaders multiply TexCoord by 1/4096 -- the art data stores UVs
 * as integers and PLAN.md requires that constant be copied exactly. Callers
 * hand over pixel rectangles and this does the conversion.
 */

#ifndef GUN_BROS_RE_ENGINE_CQUADBATCH_H
#define GUN_BROS_RE_ENGINE_CQUADBATCH_H

#include "engine/CShaderProgram.h"
#include "engine/CTexture.h"
#include "engine/platform/GLLoader.h"

#include <cstdint>
#include <vector>

// What the vertex shaders divide TexCoord by; see the file comment.
constexpr float kTexCoordScale = 4096.0f;

/** A rectangle on an atlas, in pixels. */
struct SourceRect {
    std::uint16_t x;
    std::uint16_t y;
    std::uint16_t width;
    std::uint16_t height;
};

/**
 * Accumulates quads and draws them.
 *
 * Usage is Begin, some AddQuad calls, Upload, then Draw as many times as you
 * like -- the geometry stays on the GPU until the next Begin. Static content
 * such as a map is therefore built once and redrawn every frame for free.
 */
class CQuadBatch {
public:
    CQuadBatch();
    ~CQuadBatch();

    CQuadBatch(const CQuadBatch &) = delete;
    CQuadBatch &operator=(const CQuadBatch &) = delete;

    /** Create the GL objects. Requires a current context. */
    bool Create(const CShaderProgram &program);

    void Destroy();

    /** Drop all accumulated geometry. */
    void Begin();

    /**
     * Queue one quad.
     *
     * @param texture  Atlas to sample. Quads are grouped by this.
     * @param x,y      Top-left corner in world pixels.
     * @param width    Destination size in pixels; usually the tile draw size,
     * @param height   which is not always the source rect's size.
     * @param source   Rectangle on the atlas, in pixels.
     * @param flipHorizontal Mirror left-right.
     * @param flipVertical   Mirror top-bottom.
     */
    void AddQuad(const CTexture &texture, float x, float y, float width, float height,
                 const SourceRect &source, bool flipHorizontal, bool flipVertical);

    /** Push the accumulated geometry to the GPU. */
    void Upload();

    /**
     * Draw every group.
     *
     * @param program Must be the program Create was given.
     * @param mvp     Row-major 4x4; uploaded transposed.
     */
    void Draw(const CShaderProgram &program, const float *mvp) const;

    std::uint32_t GetQuadCount() const;

    /** How many draw calls Draw will issue -- one per distinct texture. */
    std::uint32_t GetGroupCount() const {
        return static_cast<std::uint32_t>(m_groups.size());
    }

private:
    /** Screen position and a 4096-scaled UV, matching the shader's attributes. */
    struct Vertex {
        float x;
        float y;
        float u;
        float v;
    };

    /** All quads sharing one texture. */
    struct Group {
        GLuint textureHandle;
        std::vector<Vertex> vertices;
    };

    /** Find or start the group for a texture. */
    Group &GroupFor(const CTexture &texture);

    std::vector<Group> m_groups;

    GLuint m_vertexArray;
    GLuint m_vertexBuffer;

    // Offset and count into the uploaded buffer, one pair per group.
    std::vector<GLint> m_groupFirst;
    std::vector<GLsizei> m_groupCount;
    std::vector<GLuint> m_groupTexture;

    GLint m_mvpLocation;
    GLint m_tex0Location;
};

#endif  // GUN_BROS_RE_ENGINE_CQUADBATCH_H
