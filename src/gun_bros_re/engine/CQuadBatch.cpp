/**
 * @file CQuadBatch.cpp
 * @brief Collects textured quads and draws them grouped by texture.
 */

#include "engine/CQuadBatch.h"

#include <cstdio>

namespace {

// Six vertices per quad: two triangles, since grouping rules out a strip.
constexpr std::size_t kVerticesPerQuad = 6;

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
    if (positionLocation < 0 || texCoordLocation < 0) {
        std::printf("[batch] shader has no Position/TexCoord attribute\n");
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
}

void CQuadBatch::Begin() {
    m_groups.clear();
    m_groupFirst.clear();
    m_groupCount.clear();
    m_groupTexture.clear();
}

CQuadBatch::Group &CQuadBatch::GroupFor(const CTexture &texture) {
    const GLuint handle = texture.GetHandle();
    for (std::size_t i = 0; i < m_groups.size(); ++i) {
        if (m_groups[i].textureHandle == handle) {
            return m_groups[i];
        }
    }

    Group group;
    group.textureHandle = handle;
    m_groups.push_back(group);
    return m_groups.back();
}

void CQuadBatch::AddQuad(const CTexture &texture, float x, float y, float width,
                         float height, const SourceRect &source, bool flipHorizontal,
                         bool flipVertical) {
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

    const Vertex topLeft = {left, top, u0, v0};
    const Vertex topRight = {right, top, u1, v0};
    const Vertex bottomLeft = {left, bottom, u0, v1};
    const Vertex bottomRight = {right, bottom, u1, v1};

    Group &group = GroupFor(texture);
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
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_groupTexture[i]);
        glDrawArrays(GL_TRIANGLES, m_groupFirst[i], m_groupCount[i]);
    }
    glBindVertexArray(0);
}

std::uint32_t CQuadBatch::GetQuadCount() const {
    std::size_t vertices = 0;
    for (std::size_t i = 0; i < m_groups.size(); ++i) {
        vertices += m_groups[i].vertices.size();
    }
    return static_cast<std::uint32_t>(vertices / kVerticesPerQuad);
}
