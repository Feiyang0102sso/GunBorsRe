/**
 * @file CQuadBatch.cpp
 * @brief Collects textured quads and draws them grouped by texture.
 */

#include "engine/CQuadBatch.h"

#include <cmath>
#include <cstdio>

namespace {

// Six vertices per quad: two triangles, since grouping rules out a strip.
constexpr std::size_t kVerticesPerQuad = 6;
constexpr float kDegreesToRadians = 3.14159265f / 180.0f;

/** Set the blend function a mode stands for. */
void ApplyBlendMode(BlendMode blend) {
    if (blend == BlendMode::Additive) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        return;
    }
    if (blend == BlendMode::AdditiveOpaque) {
        glBlendFunc(GL_ONE, GL_ONE);
        return;
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

}  // namespace

CQuadBatch::CQuadBatch()
    : m_vertexArray(0),
      m_vertexBuffer(0),
      m_mvpLocation(-1),
      m_tex0Location(-1) {}

CQuadBatch::~CQuadBatch() {
    Destroy();
}

bool CQuadBatch::Create(const CShaderProgram &program) {
    Destroy();

    const GLint positionLocation = program.GetAttribLocation("Position");
    const GLint texCoordLocation = program.GetAttribLocation("TexCoord");
    const GLint alphaLocation = program.GetAttribLocation("Alpha");
    if (positionLocation < 0 || texCoordLocation < 0 || alphaLocation < 0) {
        std::printf("[batch] shader has no Position/TexCoord/Alpha attribute\n");
        return false;
    }
    m_mvpLocation = program.GetUniformLocation("mvp");
    m_tex0Location = program.GetUniformLocation("tex0");
    if (m_mvpLocation < 0) {
        std::printf("[batch] shader has no mvp uniform\n");
        return false;
    }

    glGenVertexArrays(1, &m_vertexArray);
    glBindVertexArray(m_vertexArray);

    glGenBuffers(1, &m_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);

    glEnableVertexAttribArray(static_cast<GLuint>(positionLocation));
    glVertexAttribPointer(static_cast<GLuint>(positionLocation), 2, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), reinterpret_cast<const void *>(0));

    glEnableVertexAttribArray(static_cast<GLuint>(texCoordLocation));
    glVertexAttribPointer(static_cast<GLuint>(texCoordLocation), 2, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<const void *>(sizeof(float) * 2));

    glEnableVertexAttribArray(static_cast<GLuint>(alphaLocation));
    glVertexAttribPointer(static_cast<GLuint>(alphaLocation), 1, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<const void *>(sizeof(float) * 4));

    glBindVertexArray(0);

    return GLCheckErrors("batch setup");
}

void CQuadBatch::Destroy() {
    if (m_vertexBuffer != 0) {
        glDeleteBuffers(1, &m_vertexBuffer);
        m_vertexBuffer = 0;
    }
    if (m_vertexArray != 0) {
        glDeleteVertexArrays(1, &m_vertexArray);
        m_vertexArray = 0;
    }
    m_groups.clear();
    m_groupFirst.clear();
    m_groupCount.clear();
    m_groupTexture.clear();
    m_groupBlend.clear();
}

void CQuadBatch::Begin() {
    m_groups.clear();
    m_groupFirst.clear();
    m_groupCount.clear();
    m_groupTexture.clear();
    m_groupBlend.clear();
}

CQuadBatch::Group &CQuadBatch::GroupFor(const CTexture &texture, BlendMode blend) {
    const GLuint handle = texture.GetHandle();

    // Only the group still being filled can be extended. Reaching back into an
    // earlier group with the same texture would draw this quad before quads
    // that were added first, and sprites are stacked back to front -- a prop
    // and the tile underneath it are on different atlases, so merging by
    // texture puts the ground on top of the rock. Runs stay long anyway: a
    // tile layer is one texture, and so is most of a sprite.
    if (!m_groups.empty() && m_groups.back().textureHandle == handle &&
        m_groups.back().blend == blend) {
        return m_groups.back();
    }

    Group group;
    group.textureHandle = handle;
    group.blend = blend;
    m_groups.push_back(group);
    return m_groups.back();
}

void CQuadBatch::AddQuad(const CTexture &texture, float x, float y, float width,
                         float height, const SourceRect &source, bool flipHorizontal,
                         bool flipVertical, BlendMode blend) {
    if (!texture.IsValid() || texture.GetWidth() == 0 || texture.GetHeight() == 0) {
        return;
    }

    // Pixels -> the shader's 4096-per-texture-width units.
    const float uScale = kTexCoordScale / static_cast<float>(texture.GetWidth());
    const float vScale = kTexCoordScale / static_cast<float>(texture.GetHeight());

    float u0 = static_cast<float>(source.x) * uScale;
    float v0 = static_cast<float>(source.y) * vScale;
    float u1 = static_cast<float>(source.x + source.width) * uScale;
    float v1 = static_cast<float>(source.y + source.height) * vScale;

    if (flipHorizontal) {
        const float swap = u0;
        u0 = u1;
        u1 = swap;
    }
    if (flipVertical) {
        const float swap = v0;
        v0 = v1;
        v1 = swap;
    }

    const float left = x;
    const float top = y;
    const float right = x + width;
    const float bottom = y + height;

    const Vertex topLeft = {left, top, u0, v0, 1.0f};
    const Vertex topRight = {right, top, u1, v0, 1.0f};
    const Vertex bottomLeft = {left, bottom, u0, v1, 1.0f};
    const Vertex bottomRight = {right, bottom, u1, v1, 1.0f};

    AddVertices(texture, blend, topLeft, topRight, bottomLeft, bottomRight);
}

void CQuadBatch::AddTransformedQuad(
    const CTexture &texture, float x, float y, float width, float height,
    const SourceRect &source, bool flipHorizontal, bool flipVertical,
    BlendMode blend, float pivotX, float pivotY, float scaleX, float scaleY,
    float rotationDegrees, float alpha, bool rotateTexture) {
    if (!texture.IsValid() || texture.GetWidth() == 0 || texture.GetHeight() == 0) {
        return;
    }

    const float uScale = kTexCoordScale / static_cast<float>(texture.GetWidth());
    const float vScale = kTexCoordScale / static_cast<float>(texture.GetHeight());
    // Sample texel centres. Filtering beyond an atlas rectangle picks up the
    // separator row or the neighbouring effect, exposing beam tile seams.
    float u0 = (static_cast<float>(source.x) + 0.5f) * uScale;
    float v0 = (static_cast<float>(source.y) + 0.5f) * vScale;
    float u1 = (static_cast<float>(source.x + source.width) - 0.5f) * uScale;
    float v1 = (static_cast<float>(source.y + source.height) - 0.5f) * vScale;
    if (flipHorizontal) {
        const float swap = u0;
        u0 = u1;
        u1 = swap;
    }
    if (flipVertical) {
        const float swap = v0;
        v0 = v1;
        v1 = swap;
    }

    const float radians = rotationDegrees * kDegreesToRadians;
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    const float cornersX[4] = {x, x + width, x, x + width};
    const float cornersY[4] = {y, y, y + height, y + height};
    float transformedX[4];
    float transformedY[4];
    for (std::size_t corner = 0; corner < 4; ++corner) {
        const float localX = (cornersX[corner] - pivotX) * scaleX;
        const float localY = (cornersY[corner] - pivotY) * scaleY;
        transformedX[corner] = pivotX + localX * cosine - localY * sine;
        transformedY[corner] = pivotY + localX * sine + localY * cosine;
    }

    Vertex topLeft = {transformedX[0], transformedY[0], u0, v0, alpha};
    Vertex topRight = {transformedX[1], transformedY[1], u1, v0, alpha};
    Vertex bottomLeft = {transformedX[2], transformedY[2], u0, v1, alpha};
    Vertex bottomRight = {transformedX[3], transformedY[3], u1, v1, alpha};
    if (rotateTexture) {
        // SpriteGlu packs long sprites sideways; restore the 90-degree blit.
        topLeft.u = u1; topLeft.v = v0;
        topRight.u = u1; topRight.v = v1;
        bottomLeft.u = u0; bottomLeft.v = v0;
        bottomRight.u = u0; bottomRight.v = v1;
    }
    AddVertices(texture, blend, topLeft, topRight, bottomLeft, bottomRight);
}

void CQuadBatch::AddVertices(const CTexture &texture, BlendMode blend,
                             const Vertex &topLeft, const Vertex &topRight,
                             const Vertex &bottomLeft,
                             const Vertex &bottomRight) {

    Group &group = GroupFor(texture, blend);
    group.vertices.push_back(topLeft);
    group.vertices.push_back(topRight);
    group.vertices.push_back(bottomLeft);

    group.vertices.push_back(topRight);
    group.vertices.push_back(bottomRight);
    group.vertices.push_back(bottomLeft);
}

void CQuadBatch::Upload() {
    m_groupFirst.clear();
    m_groupCount.clear();
    m_groupTexture.clear();
    m_groupBlend.clear();

    std::size_t totalVertices = 0;
    for (std::size_t i = 0; i < m_groups.size(); ++i) {
        totalVertices += m_groups[i].vertices.size();
    }
    if (totalVertices == 0) {
        return;
    }

    // Flatten the groups back to back and remember where each one starts.
    std::vector<Vertex> flattened;
    flattened.reserve(totalVertices);
    for (std::size_t i = 0; i < m_groups.size(); ++i) {
        const Group &group = m_groups[i];
        if (group.vertices.empty()) {
            continue;
        }
        m_groupFirst.push_back(static_cast<GLint>(flattened.size()));
        m_groupCount.push_back(static_cast<GLsizei>(group.vertices.size()));
        m_groupTexture.push_back(group.textureHandle);
        m_groupBlend.push_back(group.blend);
        flattened.insert(flattened.end(), group.vertices.begin(), group.vertices.end());
    }

    glBindVertexArray(m_vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(flattened.size() * sizeof(Vertex)),
                 flattened.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    GLCheckErrors("batch upload");
}

void CQuadBatch::Draw(const CShaderProgram &program, const float *mvp) const {
    if (m_groupFirst.empty()) {
        return;
    }

    program.Use();

    // Transposed on upload: mvp is stored row-major so it reads like a matrix.
    glUniformMatrix4fv(m_mvpLocation, 1, GL_TRUE, mvp);
    if (m_tex0Location >= 0) {
        glUniform1i(m_tex0Location, 0);
    }

    glBindVertexArray(m_vertexArray);
    for (std::size_t i = 0; i < m_groupFirst.size(); ++i) {
        ApplyBlendMode(m_groupBlend[i]);
        glUniform1i(program.GetUniformLocation("additiveOpaque"),
                    m_groupBlend[i] == BlendMode::AdditiveOpaque);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_groupTexture[i]);
        glDrawArrays(GL_TRIANGLES, m_groupFirst[i], m_groupCount[i]);
    }
    glBindVertexArray(0);
    glUniform1i(program.GetUniformLocation("additiveOpaque"), 0);
    // The following mesh/HUD pass expects ordinary straight-alpha blending.
    // Keep the last particle group from making dark panels additive.
    ApplyBlendMode(BlendMode::Alpha);
}

std::uint32_t CQuadBatch::GetQuadCount() const {
    std::size_t vertices = 0;
    for (std::size_t i = 0; i < m_groups.size(); ++i) {
        vertices += m_groups[i].vertices.size();
    }
    return static_cast<std::uint32_t>(vertices / kVerticesPerQuad);
}
