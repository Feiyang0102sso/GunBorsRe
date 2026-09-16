#include "gun_bros_re/debug/Capture.h"
#include "engine/core/ZPaths.h"
/**
 * @file M2Texture.cpp
 * @brief M2 milestone harness: a PNG out of a .big, on screen.
 *
 * Deliberately unarchitected. PLAN.md calls for "a minimal 2D batch, dynamic
 * VBO refilled every frame, no architecture yet" -- this is one quad's worth of
 * that, enough to prove the chain from archive to pixels. The batching layer
 * comes in M3 when there is something to batch.
 */

#include "tests/research/TextureStudy.h"

#include "engine/graphics/ZPNG.h"
#include "engine/graphics/ZShaderProgram.h"
#include "engine/platform/ZWindow.h"
#include "engine/platform/ZGLLoader.h"
#include "engine/resources/CResTOCManager.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

// Where the ported shaders live. Development-time absolute path, as PLAN.md
// specifies -- there is no asset pipeline and no path search.
const char *const kShaderDirectory = Paths::Shaders().c_str();

// The vertex shader multiplies TexCoord by 1/4096, because the art data stores
// UVs as integers. To address the whole texture we therefore have to feed it
// 4096, not 1. Getting this wrong is the classic way to see a garbled image,
// which makes it a useful thing to exercise this early.
constexpr float kTexCoordScale = 4096.0f;

/** One vertex: screen position and an integer-scaled UV, matching the shader. */
struct Vertex {
    float x;
    float y;
    float u;
    float v;
};

/**
 * Build a row-major orthographic projection with the origin at the top left
 * and y growing downward, which is how 2D game coordinates run.
 *
 * Row-major because it reads like the matrix it is; glUniformMatrix4fv is told
 * to transpose on upload.
 */
void MakeOrthoTopLeft(float width, float height, float *matrix) {
    for (int i = 0; i < 16; ++i) {
        matrix[i] = 0.0f;
    }
    matrix[0] = 2.0f / width;
    matrix[3] = -1.0f;
    matrix[5] = -2.0f / height;  // negative: y grows downward
    matrix[7] = 1.0f;
    matrix[10] = -1.0f;
    matrix[15] = 1.0f;
}

/** Upload an RGBA8 image as a texture. Returns 0 on failure. */
GLuint CreateTexture(const ZPNGImage &image) {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // The engine draws sprites straight out of atlases, so clamping matters
    // more than wrapping and there are no mips to speak of.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 static_cast<GLsizei>(image.width), static_cast<GLsizei>(image.height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());

    if (!GLCheckErrors("texture upload")) {
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}

/** Fetch a PNG resource out of a pack and decode it. */
bool LoadImageFromPack(const std::string &bigDirectory, const std::string &packShortName,
                       std::uint32_t resourceId, ZPNGImage &image) {
    CResTOCManager tocManager;
    if (!tocManager.InitAuto(bigDirectory)) {
        return false;
    }

    const int packIndex = tocManager.GetPackIndexFromName(packShortName.c_str());
    CResPackTOC *pack = tocManager.GetPack(packIndex);
    if (pack == nullptr || pack->GetShortName() != packShortName) {
        std::printf("[m2] no pack named %s\n", packShortName.c_str());
        return false;
    }
    if (!tocManager.Bind()) {
        return false;
    }

    std::vector<std::uint8_t> payload;
    if (!pack->GetReader().GetResourceById(resourceId, payload)) {
        std::printf("[m2] resource %u not found in %s\n", resourceId, packShortName.c_str());
        return false;
    }
    std::printf("[m2] resource %u: %zu bytes\n", resourceId, payload.size());

    if (!PNGDecode(payload, image)) {
        return false;
    }
    std::printf("[m2] decoded %ux%u\n", image.width, image.height);
    return true;
}

}  // namespace

int RunTextureStudy(const std::string &bigDirectory, const std::string &packShortName,
                 std::uint32_t resourceId, const std::string &screenshotPath) {
    std::printf("=== M2: a PNG from a .big, on screen ===\n\n");

    // --- the image, before any GL exists ---
    ZPNGImage image;
    if (!LoadImageFromPack(bigDirectory, packShortName, resourceId, image)) {
        return 1;
    }

    // --- window and context ---
    ZWindow window;
    if (!window.Open("gun_bros_re -- M2", kDefaultWindowWidth, kDefaultWindowHeight)) {
        return 1;
    }

    // --- shaders ---
    ZShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) {
        return 1;
    }

    const GLint positionLocation = program.GetAttribLocation("Position");
    const GLint texCoordLocation = program.GetAttribLocation("TexCoord");
    const GLint mvpLocation = program.GetUniformLocation("mvp");
    const GLint tex0Location = program.GetUniformLocation("tex0");
    if (positionLocation < 0 || texCoordLocation < 0 || mvpLocation < 0) {
        std::printf("[m2] shader is missing an expected attribute or uniform\n");
        return 1;
    }

    // --- texture ---
    const GLuint texture = CreateTexture(image);
    if (texture == 0) {
        return 1;
    }

    // --- one quad, centred, at the image's native size ---
    int drawableWidth = 0;
    int drawableHeight = 0;
    window.GetDrawableSize(drawableWidth, drawableHeight);

    const float imageWidth = static_cast<float>(image.width);
    const float imageHeight = static_cast<float>(image.height);
    const float left = (static_cast<float>(drawableWidth) - imageWidth) * 0.5f;
    const float top = (static_cast<float>(drawableHeight) - imageHeight) * 0.5f;

    const Vertex vertices[4] = {
        {left,              top,               0.0f,            0.0f},
        {left + imageWidth, top,               kTexCoordScale,  0.0f},
        {left,              top + imageHeight, 0.0f,            kTexCoordScale},
        {left + imageWidth, top + imageHeight, kTexCoordScale,  kTexCoordScale},
    };

    GLuint vertexArray = 0;
    GLuint vertexBuffer = 0;
    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);

    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(static_cast<GLuint>(positionLocation));
    glVertexAttribPointer(static_cast<GLuint>(positionLocation), 2, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), reinterpret_cast<const void *>(0));

    glEnableVertexAttribArray(static_cast<GLuint>(texCoordLocation));
    glVertexAttribPointer(static_cast<GLuint>(texCoordLocation), 2, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex),
                          reinterpret_cast<const void *>(sizeof(float) * 2));

    if (!GLCheckErrors("geometry setup")) {
        return 1;
    }

    float mvp[16];
    MakeOrthoTopLeft(static_cast<float>(drawableWidth),
                     static_cast<float>(drawableHeight), mvp);

    // Alpha blending, matching the engine's only blend mode.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::printf("\n[m2] drawing %ux%u at (%.0f, %.0f) -- Esc or close to quit\n",
                image.width, image.height, left, top);

    // --- main loop ---
    // Frame counting is here for M0's "steady 60fps" sign-off, not because
    // anything needs the timing yet.
    bool reportedFirstFrame = false;
    std::uint64_t frameCount = 0;
    std::uint64_t lastReportMs = window.GetTicksMs();

    while (window.PumpEvents()) {
        window.GetDrawableSize(drawableWidth, drawableHeight);
        glViewport(0, 0, drawableWidth, drawableHeight);

        glClearColor(0.15f, 0.16f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        program.Use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        if (tex0Location >= 0) {
            glUniform1i(tex0Location, 0);
        }

        // Transposed on upload because mvp is stored row-major above.
        glUniformMatrix4fv(mvpLocation, 1, GL_TRUE, mvp);

        glBindVertexArray(vertexArray);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        if (!reportedFirstFrame) {
            GLCheckErrors("first frame");
            reportedFirstFrame = true;

            // Grab before the swap, while the finished frame is still the
            // back buffer glReadPixels reads from.
            if (!screenshotPath.empty()) {
                if (!Capture::SaveFrame(window, screenshotPath)) {
                    return 1;
                }
                window.Present();
                break;
            }
        }

        window.Present();

        frameCount++;
        const std::uint64_t nowMs = window.GetTicksMs();
        const std::uint64_t elapsedMs = nowMs - lastReportMs;
        if (elapsedMs >= 1000) {
            std::printf("[m2] %.1f fps\n",
                        static_cast<double>(frameCount) * 1000.0 /
                            static_cast<double>(elapsedMs));
            frameCount = 0;
            lastReportMs = nowMs;
        }
    }

    glDeleteBuffers(1, &vertexBuffer);
    glDeleteVertexArrays(1, &vertexArray);
    glDeleteTextures(1, &texture);

    std::printf("[m2] done\n");
    return 0;
}
