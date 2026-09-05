/**
 * @file GLLoader.cpp
 * @brief Minimal OpenGL 3.3 Core function loader.
 */

#include "engine/platform/GLLoader.h"

#include <SDL3/SDL.h>

#include <cstdio>

// One definition per entry point, all initialised to null so a missed load is
// a clean crash rather than a silent wrong call.
#define GUN_BROS_GL_DEFINE(returnType, name, parameters) PFN_##name name = nullptr;

GUN_BROS_GL_FUNCTIONS(GUN_BROS_GL_DEFINE)

#undef GUN_BROS_GL_DEFINE

bool GLLoaderInitialize() {
    bool allResolved = true;

    // SDL_GL_GetProcAddress handles the platform quirks -- on Windows the
    // 1.1 entry points live in opengl32.dll while newer ones come from
    // wglGetProcAddress, and SDL knows to try both.
#define GUN_BROS_GL_LOAD(returnType, name, parameters)                       \
    name = reinterpret_cast<PFN_##name>(SDL_GL_GetProcAddress(#name));       \
    if (name == nullptr) {                                                   \
        std::printf("[gl] missing entry point: %s\n", #name);                \
        allResolved = false;                                                 \
    }

    GUN_BROS_GL_FUNCTIONS(GUN_BROS_GL_LOAD)

#undef GUN_BROS_GL_LOAD

    return allResolved;
}

bool GLCheckErrors(const char *where) {
    bool clean = true;
    for (;;) {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            break;
        }
        clean = false;

        const char *name = "unknown";
        switch (error) {
            case GL_INVALID_ENUM:      name = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE:     name = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: name = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY:     name = "GL_OUT_OF_MEMORY"; break;
            default: break;
        }
        std::printf("[gl] %s: %s (0x%04X)\n", where, name, error);
    }
    return clean;
}
