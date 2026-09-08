/**
 * @file CMeshBuffer.h
 * @brief The GL buffers behind one 3D model.
 *
 * The other half of what CQuadBatch does for sprites, for geometry that is
 * shaped nothing like a sprite. Not a port: in the original these buffers live
 * inside CMesh itself (:97705 creates them while parsing), but keeping the
 * parser free of GL is worth the split -- it is what lets `--meshes` check all
 * 334 models without opening a window.
 *
 * Three buffers, because the three things change at different rates:
 *
 * - the index buffer never changes,
 * - the texture coordinates never change,
 * - the vertex positions change every animation frame, since the animation is
 *   a full vertex array per frame rather than a skeleton.
 *
 * The index buffer is **one triangle strip** for the whole model. Separate
 * strips are stitched together with repeated indices, which build zero-area
 * triangles that the rasteriser drops.
 *
 * Texture coordinates go in PRE-SCALED BY 4096, the same trick CQuadBatch
 * uses: the ported vertex shaders multiply TexCoord by 1/4096 because sprite
 * UVs are integers in the art data. Mesh UVs are already normalised floats, so
 * they are scaled up here to come back out unchanged -- which keeps the whole
 * project on the seven original shaders instead of adding an eighth.
 *
 * Texture coordinates also go in WITH V FLIPPED. The two halves of the art
 * data disagree about where v = 0 is:
 *
 * - sprite UVs are pixel rows counted from the TOP of the PNG (CQuadBatch
 *   divides source.y straight through), which is also where CTexture puts
 *   row 0 when it uploads,
 * - mesh UVs come out of a 3D authoring tool, which counts from the BOTTOM.
 *
 * Left alone, a model wears its atlas upside down: the player's head samples
 * the vest at the bottom of the sheet and his chest samples the face at the
 * top. One of the two conventions has to give, and it is this one, because
 * CTexture is shared with the sprite path that already works.
 */

#ifndef GUN_BROS_RE_ENGINE_CMESHBUFFER_H
#define GUN_BROS_RE_ENGINE_CMESHBUFFER_H

#include "engine/CShaderProgram.h"
#include "engine/CTexture.h"
#include "engine/platform/GLLoader.h"
#include "gun_bros/CMesh.h"

#include <cstdint>
#include <vector>

class CMeshBuffer {
public:
    CMeshBuffer();
    ~CMeshBuffer();

    CMeshBuffer(const CMeshBuffer &) = delete;
    CMeshBuffer &operator=(const CMeshBuffer &) = delete;

    /** Create the GL objects. Requires a current context. */
    bool Create(const CShaderProgram &program);

    void Destroy();

    /**
     * Upload the parts of a model that never change: indices and UVs.
     *
     * Leaves the positions empty; call SetFrame before drawing.
     */
    bool SetMesh(const CMesh &mesh);

    /** Rewrite the vertex positions from one of the model's frames. */
    void SetFrame(const CMesh &mesh, std::size_t frameIndex);

    /**
     * Rewrite the vertex positions from a pose the evaluator built.
     *
     * Three floats per vertex, same layout as a frame's. This is the path an
     * animated model takes every frame; SetFrame is the still-pose shortcut
     * onto the same buffer.
     */
    void SetVertices(const std::vector<float> &vertices);

    /**
     * Draw the model.
     *
     * Culling stays off: the strips are stitched with degenerate triangles,
     * which flips the winding at every seam, so there is no single front face
     * to keep. Depth testing has to be enabled by the caller.
     *
     * @param mvp Row-major 4x4; uploaded transposed.
     */
    void Draw(const CShaderProgram &program, const float *mvp,
              const CTexture &texture, float heatIntensity = 0.0f,
              const float *overlayRgb = nullptr) const;

    std::uint32_t GetIndexCount() const { return m_indexCount; }

private:
    GLuint m_vertexArray;
    GLuint m_positionBuffer;
    GLuint m_texCoordBuffer;
    GLuint m_indexBuffer;

    GLint m_positionLocation;
    GLint m_texCoordLocation;
    GLint m_alphaLocation;
    GLint m_mvpLocation;
    GLint m_tex0Location;
    GLint m_overlayLocation = -1;

    std::uint32_t m_indexCount;
};

#endif  // GUN_BROS_RE_ENGINE_CMESHBUFFER_H
