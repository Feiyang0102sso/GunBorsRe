/**
 * @file CWindow.cpp
 * @brief Window and OpenGL context, on SDL3.
 */

#include "engine/platform/CWindow.h"

#include "engine/platform/GLLoader.h"

#include <SDL3/SDL.h>

#include <cstdio>

CWindow::CWindow()
    : m_window(nullptr),
      m_context(nullptr),
      m_sdlInitialised(false),
      m_quitRequested(false) {}

CWindow::~CWindow() {
    Close();
}

bool CWindow::Open(const std::string &title, int width, int height) {
    Close();
    m_quitRequested = false;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("[window] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    m_sdlInitialised = true;

    // Ask for a core profile before creating the window -- these are hints for
    // the context that SDL_GL_CreateContext will build.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, kRequiredGLMajor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, kRequiredGLMinor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    m_window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_OPENGL);
    if (m_window == nullptr) {
        std::printf("[window] SDL_CreateWindow failed: %s\n", SDL_GetError());
        Close();
        return false;
    }

    m_context = SDL_GL_CreateContext(m_window);
    if (m_context == nullptr) {
        std::printf("[window] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        Close();
        return false;
    }

    if (!GLLoaderInitialize()) {
        std::printf("[window] the GL context is missing entry points we need\n");
        Close();
        return false;
    }

    // vsync. Not fatal if the driver refuses.
    if (!SDL_GL_SetSwapInterval(1)) {
        std::printf("[window] vsync unavailable: %s\n", SDL_GetError());
    }

    const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    std::printf("[window] %dx%d, GL %s on %s\n", width, height,
                (version != nullptr) ? version : "?",
                (renderer != nullptr) ? renderer : "?");

    return true;
}

void CWindow::Close() {
    if (m_context != nullptr) {
        SDL_GL_DestroyContext(m_context);
        m_context = nullptr;
    }
    if (m_window != nullptr) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_sdlInitialised) {
        SDL_Quit();
        m_sdlInitialised = false;
    }
}

bool CWindow::PumpEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            m_quitRequested = true;
        } else if (event.type == SDL_EVENT_KEY_DOWN &&
                   event.key.key == SDLK_ESCAPE) {
            m_quitRequested = true;
        }
    }
    return !m_quitRequested;
}

void CWindow::Present() {
    SDL_GL_SwapWindow(m_window);
}

void CWindow::GetDrawableSize(int &width, int &height) const {
    width = 0;
    height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
}

std::uint64_t CWindow::GetTicksMs() const {
    return SDL_GetTicks();
}
