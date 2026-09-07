/**
 * @file CMarkerBatch.cpp
 * @brief Flat coloured rectangles, for showing what the data says but the art
 *        does not draw.
 */

#include "engine/CMarkerBatch.h"

#include <cmath>
#include <cstdio>

CMarkerBatch::CMarkerBatch()
    : m_vertexArray(0),
      m_vertexBuffer(0),
      m_positionLocation(-1),
      m_mvpLocation(-1),
      m_colorLocation(-1) {}

CMarkerBatch::~CMarkerBatch() {
    Destroy();
}

bool CMarkerBatch::Create(const CShaderProgram &program) {
    Destroy();

    m_positionLocation = program.GetAttribLocation("Position");
    m_mvpLocation = program.GetUniformLocation("mvp");
    m_colorLocation = program.GetUniformLocation("constColor");
    if (m_positionLocation < 0 || m_mvpLocation < 0 || m_colorLocation < 0) {
        std::printf("[marker] shader is missing Position, mvp or constColor\n");
        return false;
    }

    glGenVertexArrays(1, &m_vertexArray);
    glBindVertexArray(m_vertexArray);

    glGenBuffers(1, &m_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glEnableVertexAttribArray(static_cast<GLuint>(m_positionLocation));
    glVertexAttribPointer(static_cast<GLuint>(m_positionLocation), 2, GL_FLOAT,
                          GL_FALSE, 2 * sizeof(float), nullptr);

    glBindVertexArray(0);
    return GLCheckErrors("marker batch create");
}

void CMarkerBatch::Destroy() {
    if (m_vertexBuffer != 0) {
        glDeleteBuffers(1, &m_vertexBuffer);
        m_vertexBuffer = 0;
    }
    if (m_vertexArray != 0) {
        glDeleteVertexArrays(1, &m_vertexArray);
        m_vertexArray = 0;
    }
    m_vertices.clear();
}

void CMarkerBatch::Begin() {
    m_vertices.clear();
}

void CMarkerBatch::AddRect(float x, float y, float width, float height) {
    const float left = x;
    const float top = y;
    const float right = x + width;
    const float bottom = y + height;

    // Two triangles, written out rather than indexed: a marker batch is a
    // handful of rectangles, and an index buffer would cost more to read than
    // it saves.
    const float corners[12] = {left,  top,    right, top,    left, bottom,
                               right, top,    right, bottom, left, bottom};
    for (int i = 0; i < 12; ++i) {
        m_vertices.push_back(corners[i]);
    }
}

void CMarkerBatch::AddOutline(float x, float y, float width, float height,
                              float thickness) {
    AddRect(x, y, width, thickness);
    AddRect(x, y + height - thickness, width, thickness);
    AddRect(x, y + thickness, thickness, height - 2.0f * thickness);
    AddRect(x + width - thickness, y + thickness, thickness,
            height - 2.0f * thickness);
}

void CMarkerBatch::AddSegment(float firstX, float firstY, float secondX,
                              float secondY, float thickness) {
    const float directionX = secondX - firstX;
    const float directionY = secondY - firstY;
    const float length = std::sqrt(directionX * directionX +
                                   directionY * directionY);
    if (length <= 0.000001f || thickness <= 0.0f) {
        return;
    }

    const float halfThickness = thickness * 0.5f;
    const float normalX = -directionY / length * halfThickness;
    const float normalY = directionX / length * halfThickness;

    const float firstLeftX = firstX + normalX;
    const float firstLeftY = firstY + normalY;
    const float firstRightX = firstX - normalX;
    const float firstRightY = firstY - normalY;
    const float secondLeftX = secondX + normalX;
    const float secondLeftY = secondY + normalY;
    const float secondRightX = secondX - normalX;
    const float secondRightY = secondY - normalY;

    const float corners[12] = {
        firstLeftX, firstLeftY, secondLeftX, secondLeftY,
        firstRightX, firstRightY, secondLeftX, secondLeftY,
        secondRightX, secondRightY, firstRightX, firstRightY,
    };
    for (int i = 0; i < 12; ++i) {
        m_vertices.push_back(corners[i]);
    }
}

void CMarkerBatch::Draw(const CShaderProgram &program, const float *mvp,
                        float red, float green, float blue, float alpha) {
    if (m_vertices.empty()) {
        return;
    }

    program.Use();

    // Transposed on upload, the same way every other mvp in this project is:
    // matrices are stored row-major so they read like a matrix on paper.
    glUniformMatrix4fv(m_mvpLocation, 1, GL_TRUE, mvp);
    // glUniform4fv, because that is the one the GL loader carries -- the
    // sprite path already needed it for constColor.
    const float color[4] = {red, green, blue, alpha};
    glUniform4fv(m_colorLocation, 1, color);

    glBindVertexArray(m_vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_vertices.size() * sizeof(float)),
                 m_vertices.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0,
                 static_cast<GLsizei>(m_vertices.size() / 2));
    glBindVertexArray(0);
}
