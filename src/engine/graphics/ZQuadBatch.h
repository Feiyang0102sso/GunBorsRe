/**
 * @file ZQuadBatch.h
 * @brief Collects textured quads and draws them grouped by texture.
 *
 * This is the "minimal 2D batch" PLAN.md calls for, and the first thing in the
 * project with enough callers to justify a shape. It stands in for
 * CGraphics2d_OGLES without being a port of it: the original batched through
 * CBlit ops and a vertex buffer pool, which is machinery we have no need for
 * while everything on screen is an axis-aligned rectangle.
 *
 * Quads are accumulated in the order they are added and split into runs
 * wherever the texture changes; each run is one draw call. Submission order is
 * kept because that is the draw order -- sprites are stacked back to front,
 * and a batch that reordered by texture would put the ground on top of the
 * rock standing on it.
 *
 * Texture coordinates go into the buffer PRE-SCALED BY 4096, because the
 * ported vertex shaders multiply TexCoord by 1/4096 -- the art data stores UVs
 * as integers and PLAN.md requires that constant be copied exactly. Callers
 * hand over pixel rectangles and this does the conversion.
 */

#ifndef GUN_BROS_RE_ENGINE_ZQUADBATCH_H
#define GUN_BROS_RE_ENGINE_ZQUADBATCH_H

#include "engine/graphics/ZShaderProgram.h"
#include "engine/graphics/ZTexture.h"
#include "engine/platform/ZGLLoader.h"

#include <cstdint>
#include <vector>

// What the vertex shaders divide TexCoord by; see the file comment.
constexpr float kTexCoordScale = 4096.0f;

/**
 * How a quad is composited.
 *
 * The engine picks these per sprite map, out of the blend factor pairs
 * CRasterizerState_v1_OGLES understands. Only the three the art actually uses
 * are here.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:59160, :342068
 */
enum class ZBlendMode {
    Alpha,           // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA -- the default
    Additive,        // GL_SRC_ALPHA, GL_ONE -- glows and fire
    AdditiveOpaque,  // GL_ONE, GL_ONE
    AlphaColorFade,  // Alpha blending with RGB and alpha fading together.
};

/** A rectangle on an atlas, in pixels. */
struct ZSourceRect {
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
class ZQuadBatch {
public:
    ZQuadBatch();
    ~ZQuadBatch();

    ZQuadBatch(const ZQuadBatch &) = delete;
    ZQuadBatch &operator=(const ZQuadBatch &) = delete;

    /** Create the GL objects. Requires a current context. */
    bool Create(const ZShaderProgram &program);

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
     * @param blend    How to composite it. Quads are grouped by this as well
     *                 as by texture, so mixing modes costs draw calls.
     */
    void AddQuad(const ZTexture &texture, float x, float y, float width, float height,
                 const ZSourceRect &source, bool flipHorizontal, bool flipVertical,
                 ZBlendMode blend);

    /** Queue a tinted quad transformed around a world-space pivot. */
    void AddTransformedQuad(const ZTexture &texture, float x, float y,
                            float width, float height, const ZSourceRect &source,
                            bool flipHorizontal, bool flipVertical, ZBlendMode blend,
                            float pivotX, float pivotY, float scaleX, float scaleY,
                            float rotationDegrees, float alpha, bool rotateTexture = false);

    /** Push the accumulated geometry to the GPU. */
    void Upload();

    /** Untextured effect geometry, using a constant-color texel and vertex alpha. */
    void AddGradientQuad(const ZTexture &color, const float *positions, const float *alpha);

    /**
     * Draw every group.
     *
     * Sets the blend function per group and leaves it on the last one used, so
     * callers that care must set their own afterwards. Blending itself has to
     * be enabled by the caller.
     *
     * @param program Must be the program Create was given.
     * @param mvp     Row-major 4x4; uploaded transposed.
     */
    void Draw(const ZShaderProgram &program, const float *mvp) const;

    std::uint32_t GetQuadCount() const;

    /** How many draw calls Draw will issue -- one per texture run. */
    std::uint32_t GetGroupCount() const {
        return static_cast<std::uint32_t>(m_groups.size());
    }

private:
    /** Screen position, a 4096-scaled UV, and per-vertex opacity. */
    struct Vertex {
        float x;
        float y;
        float u;
        float v;
        float alpha;
    };

    /** A run of consecutive quads sharing one texture and one blend mode. */
    struct Group {
        GLuint textureHandle;
        ZBlendMode blend;
        std::vector<Vertex> vertices;
    };

    /** Extend the current run, or start a new one when its state changes. */
    Group &GroupFor(const ZTexture &texture, ZBlendMode blend);

    /** Append two triangles after UV and corner positions have been resolved. */
    void AddVertices(const ZTexture &texture, ZBlendMode blend,
                     const Vertex &topLeft, const Vertex &topRight,
                     const Vertex &bottomLeft, const Vertex &bottomRight);

    std::vector<Group> m_groups;

    GLuint m_vertexArray;
    GLuint m_vertexBuffer;

    // Offset and count into the uploaded buffer, plus the state to draw it
    // with, one entry per group.
    std::vector<GLint> m_groupFirst;
    std::vector<GLsizei> m_groupCount;
    std::vector<GLuint> m_groupTexture;
    std::vector<ZBlendMode> m_groupBlend;

    GLint m_mvpLocation;
    GLint m_tex0Location;
};

#endif  // GUN_BROS_RE_ENGINE_CQUADBATCH_H
