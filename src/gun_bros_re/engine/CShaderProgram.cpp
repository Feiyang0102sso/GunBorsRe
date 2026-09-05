/**
 * @file CShaderProgram.cpp
 * @brief Compile and link a vertex/fragment pair.
 */

#include "engine/CShaderProgram.h"

#include <cstdio>
#include <fstream>
#include <vector>

namespace {

/** Read a text file whole. Returns false when it cannot be opened. */
bool ReadTextFile(const std::string &path, std::string &out) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::printf("[shader] cannot open %s\n", path.c_str());
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);

    out.resize(static_cast<std::size_t>(size));
    if (size > 0) {
        file.read(&out[0], size);
    }
    return true;
}

/** Print a compile or link log, which GL hands back as a sized string. */
void PrintInfoLog(GLuint object, bool isProgram, const char *label) {
    GLint length = 0;
    if (isProgram) {
        glGetProgramiv(object, GL_INFO_LOG_LENGTH, &length);
    } else {
        glGetShaderiv(object, GL_INFO_LOG_LENGTH, &length);
    }
    if (length <= 1) {
        return;
    }

    std::vector<GLchar> log(static_cast<std::size_t>(length));
    if (isProgram) {
        glGetProgramInfoLog(object, length, nullptr, log.data());
    } else {
        glGetShaderInfoLog(object, length, nullptr, log.data());
    }
    std::printf("[shader] %s:\n%s\n", label, log.data());
}

}  // namespace

CShaderProgram::CShaderProgram() : m_program(0) {}

CShaderProgram::~CShaderProgram() {
    Destroy();
}

void CShaderProgram::Destroy() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

GLuint CShaderProgram::CompileStage(GLenum stageType, const std::string &path) const {
    std::string source;
    if (!ReadTextFile(path, source)) {
        return 0;
    }

    const GLuint shader = glCreateShader(stageType);
    const GLchar *sourcePointer = source.c_str();
    const GLint sourceLength = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &sourcePointer, &sourceLength);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        std::printf("[shader] %s failed to compile\n", path.c_str());
        PrintInfoLog(shader, false, "compile log");
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool CShaderProgram::Load(const std::string &shaderDirectory,
                          const std::string &vertexName,
                          const std::string &fragmentName) {
    Destroy();

    const std::string vertexPath = shaderDirectory + "/" + vertexName + ".vert";
    const std::string fragmentPath = shaderDirectory + "/" + fragmentName + ".frag";

    const GLuint vertexShader = CompileStage(GL_VERTEX_SHADER, vertexPath);
    if (vertexShader == 0) {
        return false;
    }

    const GLuint fragmentShader = CompileStage(GL_FRAGMENT_SHADER, fragmentPath);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // The program keeps its own reference once linked.
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        std::printf("[shader] %s + %s failed to link\n",
                    vertexName.c_str(), fragmentName.c_str());
        PrintInfoLog(program, true, "link log");
        glDeleteProgram(program);
        return false;
    }

    m_program = program;
    std::printf("[shader] linked %s + %s\n", vertexName.c_str(), fragmentName.c_str());
    return true;
}

GLint CShaderProgram::GetUniformLocation(const char *name) const {
    if (m_program == 0) {
        return -1;
    }
    return glGetUniformLocation(m_program, name);
}

GLint CShaderProgram::GetAttribLocation(const char *name) const {
    if (m_program == 0) {
        return -1;
    }
    return glGetAttribLocation(m_program, name);
}
