/**
 * @file GLLoader.h
 * @brief Minimal OpenGL 3.3 Core function loader.
 *
 * Windows ships an opengl32.dll frozen at OpenGL 1.1, so anything newer has to
 * be fetched at runtime through SDL_GL_GetProcAddress. Rather than pull in a
 * generated loader, this declares only what the renderer actually calls -- the
 * engine's whole drawing repertoire is textured and coloured triangles, so the
 * list stays short and stays readable.
 *
 * GL 1.1 entry points (glClear, glBindTexture, glDrawArrays, ...) come straight
 * from opengl32.lib and are NOT listed here.
 */

#ifndef GUN_BROS_RE_ENGINE_PLATFORM_GLLOADER_H
#define GUN_BROS_RE_ENGINE_PLATFORM_GLLOADER_H

#include <windows.h>
#include <GL/gl.h>

#include <cstddef>

// ---------------------------------------------------------------------------
// Types that <GL/gl.h> predates
// ---------------------------------------------------------------------------

typedef char GLchar;
typedef std::ptrdiff_t GLsizeiptr;
typedef std::ptrdiff_t GLintptr;

// ---------------------------------------------------------------------------
// Constants that <GL/gl.h> predates
// ---------------------------------------------------------------------------

#define GL_ARRAY_BUFFER            0x8892
#define GL_ELEMENT_ARRAY_BUFFER    0x8893
#define GL_STATIC_DRAW             0x88E4
#define GL_DYNAMIC_DRAW            0x88E8

#define GL_FRAGMENT_SHADER         0x8B30
#define GL_VERTEX_SHADER           0x8B31
#define GL_COMPILE_STATUS          0x8B81
#define GL_LINK_STATUS             0x8B82
#define GL_INFO_LOG_LENGTH         0x8B84

#define GL_TEXTURE0                0x84C0
#define GL_CLAMP_TO_EDGE           0x812F

#define GL_MAJOR_VERSION           0x821B
#define GL_MINOR_VERSION           0x821C

// ---------------------------------------------------------------------------
// The function table
// ---------------------------------------------------------------------------

/**
 * Every GL entry point the renderer needs beyond 1.1, as
 * (return type, name, parameter list) so one macro can declare, define and
 * load them without the three lists ever drifting apart.
 */
#define GUN_BROS_GL_FUNCTIONS(X)                                                                   \
    /* buffers */                                                                                  \
    X(void,   glGenBuffers,              (GLsizei n, GLuint *buffers))                              \
    X(void,   glDeleteBuffers,           (GLsizei n, const GLuint *buffers))                        \
    X(void,   glBindBuffer,              (GLenum target, GLuint buffer))                            \
    X(void,   glBufferData,              (GLenum target, GLsizeiptr size, const void *data,         \
                                          GLenum usage))                                            \
    /* vertex arrays */                                                                             \
    X(void,   glGenVertexArrays,         (GLsizei n, GLuint *arrays))                               \
    X(void,   glDeleteVertexArrays,      (GLsizei n, const GLuint *arrays))                         \
    X(void,   glBindVertexArray,         (GLuint array))                                            \
    X(void,   glEnableVertexAttribArray, (GLuint index))                                            \
    X(void,   glVertexAttrib1f,          (GLuint index, GLfloat x))                                 \
    X(void,   glVertexAttribPointer,     (GLuint index, GLint size, GLenum type,                    \
                                          GLboolean normalized, GLsizei stride,                     \
                                          const void *pointer))                                     \
    /* shaders */                                                                                   \
    X(GLuint, glCreateShader,            (GLenum type))                                             \
    X(void,   glDeleteShader,            (GLuint shader))                                           \
    X(void,   glShaderSource,            (GLuint shader, GLsizei count,                             \
                                          const GLchar *const *string, const GLint *length))        \
    X(void,   glCompileShader,           (GLuint shader))                                           \
    X(void,   glGetShaderiv,             (GLuint shader, GLenum pname, GLint *params))              \
    X(void,   glGetShaderInfoLog,        (GLuint shader, GLsizei bufSize, GLsizei *length,          \
                                          GLchar *infoLog))                                         \
    /* programs */                                                                                  \
    X(GLuint, glCreateProgram,           (void))                                                    \
    X(void,   glDeleteProgram,           (GLuint program))                                          \
    X(void,   glAttachShader,            (GLuint program, GLuint shader))                           \
    X(void,   glLinkProgram,             (GLuint program))                                          \
    X(void,   glUseProgram,              (GLuint program))                                          \
    X(void,   glGetProgramiv,            (GLuint program, GLenum pname, GLint *params))             \
    X(void,   glGetProgramInfoLog,       (GLuint program, GLsizei bufSize, GLsizei *length,         \
                                          GLchar *infoLog))                                         \
    X(GLint,  glGetAttribLocation,       (GLuint program, const GLchar *name))                      \
    X(GLint,  glGetUniformLocation,      (GLuint program, const GLchar *name))                      \
    X(void,   glBindAttribLocation,      (GLuint program, GLuint index, const GLchar *name))        \
    /* uniforms */                                                                                  \
    X(void,   glUniform1i,               (GLint location, GLint v0))                                \
    X(void,   glUniform4fv,              (GLint location, GLsizei count, const GLfloat *value))     \
    X(void,   glUniformMatrix4fv,        (GLint location, GLsizei count, GLboolean transpose,       \
                                          const GLfloat *value))                                    \
    /* textures */                                                                                  \
    X(void,   glActiveTexture,           (GLenum texture))

// Declare a function-pointer typedef and an `extern` slot for each entry.
#define GUN_BROS_GL_DECLARE(returnType, name, parameters) \
    typedef returnType(APIENTRY *PFN_##name) parameters;  \
    extern PFN_##name name;

GUN_BROS_GL_FUNCTIONS(GUN_BROS_GL_DECLARE)

#undef GUN_BROS_GL_DECLARE

/**
 * Resolve every entry point above. Requires a current GL context.
 * Returns false and names the first missing function.
 */
bool GLLoaderInitialize();

/** Drain and report the GL error queue. Returns true when it was empty. */
bool GLCheckErrors(const char *where);

#endif  // GUN_BROS_RE_ENGINE_PLATFORM_GLLOADER_H
