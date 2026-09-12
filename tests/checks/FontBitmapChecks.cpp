/** Render every FONT_KEYSET pair with the production bitmap-font reader.
 * Evidence: bitmap_font.bt and CFontMgr::GetFont :56753. No font count/table is copied here.
 */
#define NOMINMAX
#include "Checks.h"
#include "TestOutput.h"
#include "PNGEncode.h"
#include "engine/core/Paths.h"
#include "engine/core/CMatrix4d.h"
#include "engine/graphics/CBitmapFont.h"
#include "engine/platform/CWindow.h"
#include "engine/resources/CResTOCManager.h"
#include "engine/resources/CArrayInputStream.h"
#include <cstdio>
#include <cstring>
#include <fstream>

namespace {
// Host contact-sheet layout only. Each font is rendered at native pixel scale.
constexpr int SheetWidth = 1400, RowHeight = 240;
constexpr float SampleY = 48, ScaledY = 126;
}

int RunFontBitmapCheck(const std::string &bigDirectory) {
    CWindow window;
    if (!window.Open("Gun Bros - Font Bitmap", SheetWidth, RowHeight)) { return 1; }
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    auto &core = *toc.GetPack(toc.GetCorePackIndex());
    std::vector<std::uint8_t> keyBytes;
    if (!core.GetResource(core.GetResValue("FONT_KEYSET"), keyBytes)) { return 1; }
    CArrayInputStream keys(keyBytes);
    const unsigned keyCount = keys.ReadUInt16();
    if (keyCount == 0 || keyCount % 2 != 0 || keys.Available() != keyCount * 4) { return 1; }
    const unsigned fontCount = keyCount / 2;
    CShaderProgram program;
    CQuadBatch batch;
    CBitmapFont labelFont;
    if (!program.Load(Paths::Shaders(), "ogles_vs_mvp_tex0", "ogles_ps_tex0") ||
        !batch.Create(program) || !labelFont.Init(core, 0)) { return 1; }
    int width = 0, height = 0;
    window.GetDrawableSize(width, height);
    if (width < SheetWidth || height < RowHeight) { return 1; }
    PNGImage sheet;
    sheet.width = width;
    sheet.height = height * fontCount;
    sheet.pixels.resize(static_cast<std::size_t>(sheet.width) * sheet.height * 4);
    std::vector<std::uint8_t> rowPixels(static_cast<std::size_t>(width) * height * 4);
    std::ofstream report(TestOutput::Path("fontbitmap-index.md"));
    report << "# FONT_KEYSET 字体索引\n\n来源：原 BIG，pack0_core / xga。\n\n"
        << "| 字体编号 | 原始行高（像素，scale=1） | 图集 | 排版数据句柄 | 图集句柄 |\n"
        << "| ---: | ---: | --- | --- | --- |\n";
    float projection[16];
    Matrix4dOrthoTopLeft(float(width), float(height), 1, projection);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    for (unsigned index = 0; index < fontCount; ++index) {
        const unsigned metrics = keys.ReadUInt32(), texture = keys.ReadUInt32();
        CBitmapFont font;
        if (!font.Init(core, index)) { return 1; }
        std::vector<std::uint8_t> atlasBytes;
        PNGImage atlas;
        if (!core.GetResource(texture, atlasBytes) || !PNGDecode(atlasBytes, atlas)) { return 1; }
        const auto atlasName = "fontbitmap-atlases/font-" + std::to_string(index) + ".png";
        std::ofstream atlasFile(TestOutput::Path(atlasName), std::ios::binary);
        atlasFile.write(reinterpret_cast<const char *>(atlasBytes.data()), atlasBytes.size());
        if (!atlasFile) { return 1; }
        char heading[200];
        std::snprintf(heading, sizeof(heading), "FONT %u / HEIGHT %.0f PX / ATLAS %u X %u / SCALE 1.0",
            index, font.Height(), atlas.width, atlas.height);
        glClearColor(0.035f, 0.05f, 0.07f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        batch.Begin();
        labelFont.Draw(batch, heading, 24, 14, 0.65f);
        font.Draw(batch, "0123456789", 24, SampleY);
        font.Draw(batch, "GUN BROS", 750, SampleY);
        labelFont.Draw(batch, "SCALE 0.5", 24, ScaledY, 0.55f);
        font.Draw(batch, "60 165", 145, ScaledY, 0.5f);
        labelFont.Draw(batch, "SCALE 0.75", 470, ScaledY, 0.55f);
        font.Draw(batch, "60 165", 610, ScaledY, 0.75f);
        labelFont.Draw(batch, "SCALE 1.0", 980, ScaledY, 0.55f);
        font.Draw(batch, "60", 1120, ScaledY, 1);
        batch.Upload();
        batch.Draw(program, projection);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rowPixels.data());
        if (!GLCheckErrors("font bitmap")) { return 1; }
        const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
        for (int y = 0; y < height; ++y) {
            const auto destination = (static_cast<std::size_t>(index) * height + y) * rowBytes;
            std::memcpy(sheet.pixels.data() + destination, rowPixels.data() + (height - 1 - y) * rowBytes, rowBytes);
        }
        report << "| " << index << " | " << font.Height() << " | [" << atlas.width << 'x' << atlas.height
            << "](" << atlasName << ") | " << metrics << " | " << texture << " |\n";
        std::printf("[fontbitmap] font=%u height=%.0f atlas=%ux%u\n", index, font.Height(), atlas.width, atlas.height);
    }
    if (!report || keys.Overran() || keys.Available() != 0) { return 1; }
    if (!PNGEncode(sheet, TestOutput::Path("fontbitmap.png"))) { return 1; }
    std::printf("[fontbitmap] fonts=%u failures=0\n", fontCount);
    return 0;
}
