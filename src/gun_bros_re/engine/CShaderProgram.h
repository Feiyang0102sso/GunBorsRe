/**
 * @file CShaderProgram.h
 * @brief Compile and link a vertex/fragment pair.
 *
 * Stands in for platform/shared/graphics/src/CShaderProgram_OGLES2.cpp. The
 * original fed shaders in as resources through CResourceShader; here they are
 * plain files under src/gun_bros_re/shaders/ so they can be edited without a
 * rebuild. That is a development convenience, not a design decision -- see the
 * asset-path note in PLAN.md.
 */

#ifndef GUN_BROS_RE_ENGINE_CSHADERPROGRAM_H
#define GUN_BROS_RE_ENGINE_CSHADERPROGRAM_H

#include "engine/platform/GLLoader.h"

#include <string>

/** A linked GL program plus the uniform lookups callers need. */
class CShaderProgram {
public:
    CShaderProgram();
    ~CShaderProgram();

    CShaderProgram(const CShaderProgram &) = delete;
    CShaderProgram &operator=(const CShaderProgram &) = delete;

    /**
     * Compile and link a shader pair from the shaders directory.
     *
     * @param shaderDirectory Directory holding the .vert/.frag files.
     * @param baseName        Name without extension, e.g. "ogles_vs_mvp_tex0".
     *                        The fragment shader is named separately because
     *                        the original mixes and matches them.
     */
    bool Load(const std::string &shaderDirectory, const std::string &vertexName,
              const std::string &fragmentName);

    void Destroy();

    bool IsValid() const { return m_program != 0; }

    void Use() const { glUseProgram(m_program); }

    GLuint GetHandle() const { return m_program; }

    /** Uniform location, or -1 when the name is absent or optimised out. */
    GLint GetUniformLocation(const char *name) const;

    /** Attribute location, or -1. */
    GLint GetAttribLocation(const char *name) const;

private:
    /** Compile one stage; returns 0 and prints the log on failure. */
    GLuint CompileStage(GLenum stageType, const std::string &path) const;

    GLuint m_program;
};

#endif  // GUN_BROS_RE_ENGINE_CSHADERPROGRAM_H
