/**
 * @file CMarkerBatch.h
 * @brief Flat coloured rectangles, for showing what the data says but the art
 *        does not draw.
 *
 * Not a port: the original has no such thing, because the original never needs
 * to see a spawn point. This exists so the map viewer can put the object
 * layer's own coordinates on screen next to the terrain they belong to, which
 * is the only way to tell a spawn point that is in the right place from one
 * that is merely plausible.
 *
 * Runs on the two constant-colour shaders the project already carries
 * (`ogles_vs_mvp_constcolor` and `ogles_ps_constcolor`), so it adds no eighth
 * shader. One draw call per colour: the colour is a uniform, not an attribute,
 * which is how those shaders were written.
 */

#ifndef GUN_BROS_RE_ENGINE_CMARKERBATCH_H
#define GUN_BROS_RE_ENGINE_CMARKERBATCH_H

#include "engine/graphics/CShaderProgram.h"
#include "engine/platform/GLLoader.h"

#include <cstdint>
#include <vector>

class CMarkerBatch {
public:
    CMarkerBatch();
    ~CMarkerBatch();

    CMarkerBatch(const CMarkerBatch &) = delete;
    CMarkerBatch &operator=(const CMarkerBatch &) = delete;

    /** Create the GL objects. Requires a current context. */
    bool Create(const CShaderProgram &program);

    void Destroy();

    /** Throw away whatever was collected last frame. */
    void Begin();

    /** A rectangle in the same space the map is drawn in. */
    void AddRect(float x, float y, float width, float height);

    /** A hollow rectangle, `thickness` wide on every side. */
    void AddOutline(float x, float y, float width, float height, float thickness);

    /** A thick line segment, used to reveal collision geometry. */
    void AddSegment(float firstX, float firstY, float secondX, float secondY,
                    float thickness);

    /** Upload and draw everything collected, in one colour. */
    void Draw(const CShaderProgram &program, const float *mvp, float red,
              float green, float blue, float alpha);

    std::size_t GetRectCount() const { return m_vertices.size() / 12; }

private:
    GLuint m_vertexArray;
    GLuint m_vertexBuffer;
    GLint m_positionLocation;
    GLint m_mvpLocation;
    GLint m_colorLocation;

    // Two triangles per rectangle, two floats per vertex.
    std::vector<float> m_vertices;
};

#endif  // GUN_BROS_RE_ENGINE_CMARKERBATCH_H
