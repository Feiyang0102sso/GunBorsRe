/**
 * @file CMeshBuffer.cpp
 * @brief The GL buffers behind one 3D model.
 */

#include "engine/CMeshBuffer.h"

#include "engine/CQuadBatch.h"  // kTexCoordScale

#include <cstdio>
#include <vector>

CMeshBuffer::CMeshBuffer()
    : m_vertexArray(0),
      m_positionBuffer(0),
      m_texCoordBuffer(0),
      m_indexBuffer(0),
      m_positionLocation(-1),
      m_texCoordLocation(-1),
      m_mvpLocation(-1),
      m_tex0Location(-1),
      m_indexCount(0) {}

CMeshBuffer::~CMeshBuffer() {
    Destroy();
}

bool CMeshBuffer::Create(const CShaderProgram &program) {
    Destroy();

    m_positionLocation = program.GetAttribLocation("Position");
    m_texCoordLocation = program.GetAttribLocation("TexCoord");
    if (m_positionLocation < 0 || m_texCoordLocation < 0) {
        std::printf("[meshbuf] shader has no Position/TexCoord attribute\n");
        return false;
    }

    m_mvpLocation = program.GetUniformLocation("mvp");
    m_tex0Location = program.GetUniformLocation("tex0");
    if (m_mvpLocation < 0) {
        std::printf("[meshbuf] shader has no mvp uniform\n");
        return false;
    }

    glGenVertexArrays(1, &m_vertexArray);
    glBindVertexArray(m_vertexArray);

    glGenBuffers(1, &m_positionBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_positionBuffer);
    glEnableVertexAttribArray(static_cast<GLuint>(m_positionLocation));
    glVertexAttribPointer(static_cast<GLuint>(m_positionLocation), 3, GL_FLOAT,
                          GL_FALSE, 3 * sizeof(float), nullptr);

    glGenBuffers(1, &m_texCoordBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_texCoordBuffer);
    glEnableVertexAttribArray(static_cast<GLuint>(m_texCoordLocation));
    glVertexAttribPointer(static_cast<GLuint>(m_texCoordLocation), 2, GL_FLOAT,
                          GL_FALSE, 2 * sizeof(float), nullptr);

    // The element buffer binding is part of the vertex array state, so this
    // one bind is all it takes.
    glGenBuffers(1, &m_indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);

    glBindVertexArray(0);
    return GLCheckErrors("mesh buffer create");
}

void CMeshBuffer::Destroy() {
    if (m_indexBuffer != 0) {
        glDeleteBuffers(1, &m_indexBuffer);
        m_indexBuffer = 0;
    }
    if (m_texCoordBuffer != 0) {
        glDeleteBuffers(1, &m_texCoordBuffer);
        m_texCoordBuffer = 0;
    }
    if (m_positionBuffer != 0) {
        glDeleteBuffers(1, &m_positionBuffer);
        m_positionBuffer = 0;
    }
    if (m_vertexArray != 0) {
        glDeleteVertexArrays(1, &m_vertexArray);
        m_vertexArray = 0;
    }
    m_indexCount = 0;
}

bool CMeshBuffer::SetMesh(const CMesh &mesh) {
    m_indexCount = static_cast<std::uint32_t>(mesh.GetIndices().size());
    if (m_indexCount == 0 || mesh.GetVertexCount() == 0) {
        std::printf("[meshbuf] nothing to draw: %u indices, %u vertices\n",
                    m_indexCount, mesh.GetVertexCount());
        m_indexCount = 0;
        return false;
    }

    glBindVertexArray(m_vertexArray);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.GetIndices().size() * sizeof(std::uint16_t)),
                 mesh.GetIndices().data(), GL_STATIC_DRAW);

    // Two fixups on the way in, both explained in the header: v is measured
    // from the bottom of the image in a mesh resource and from the top by
    // CTexture, and the shader divides every texture coordinate by 4096.
    const std::vector<float> &sourceTexCoords = mesh.GetTexCoords();
    std::vector<float> texCoords(sourceTexCoords.size());
    for (std::size_t i = 0; i + 1 < sourceTexCoords.size(); i += 2) {
        const float u = sourceTexCoords[i];
        const float v = 1.0f - sourceTexCoords[i + 1];
        texCoords[i] = u * kTexCoordScale;
        texCoords[i + 1] = v * kTexCoordScale;
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_texCoordBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(texCoords.size() * sizeof(float)),
                 texCoords.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
    return GLCheckErrors("mesh buffer upload");
}

void CMeshBuffer::SetFrame(const CMesh &mesh, std::size_t frameIndex) {
    if (frameIndex >= mesh.GetFrames().size()) {
        return;
    }

    const std::vector<float> &vertices = mesh.GetFrames()[frameIndex].vertices;
    if (vertices.empty()) {
        return;
    }

    glBindVertexArray(m_vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, m_positionBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_DYNAMIC_DRAW);
    glBindVertexArray(0);
}

void CMeshBuffer::Draw(const CShaderProgram &program, const float *mvp,
                       const CTexture &texture) const {
    if (m_indexCount == 0) {
        return;
    }

    program.Use();

    // Transposed on upload: mvp is stored row-major so it reads like a matrix.
    glUniformMatrix4fv(m_mvpLocation, 1, GL_TRUE, mvp);
    if (m_tex0Location >= 0) {
        glUniform1i(m_tex0Location, 0);
    }

    texture.Bind(GL_TEXTURE0);

    glBindVertexArray(m_vertexArray);
    glDrawElements(GL_TRIANGLE_STRIP, static_cast<GLsizei>(m_indexCount),
                   GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);
}
