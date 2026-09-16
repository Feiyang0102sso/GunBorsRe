#include "gun_bros_re/debug/Capture.h"
#include "engine/core/ZPaths.h"
/** @file ZPickupCatalog.cpp
 * @brief Verify the entire wire payload and every real collection export.
 */
#include "TestOutput.h"
#include "gun_bros_re/data/ZPickupCatalog.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/gameplay/ZPickupScene.h"
#include "engine/graphics/CBitmapFont.h"
#include "gun_bros_re/gameplay/CParticleEffect.h"
#include "gun_bros_re/gameplay/ZWeaponEffects.h"
#include "engine/platform/ZWindow.h"
#include "engine/core/ZMatrix4d.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include "Checks.h"

int RunPickupCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    std::vector<ZPickupEntry> catalog;
    if (!LoadPickupCatalog(toc, tables, catalog)) { return 1; }
    std::filesystem::create_directories(TestOutput::Path(""));
    std::ofstream report(TestOutput::Path("pickup-check.txt"));
    unsigned failures = 0;
    unsigned collectedActions = 0;
    for (const ZPickupEntry &entry : catalog) {
        CPickup pickup;
        pickup.Bind(entry.data);
        if (!pickup.Collect() || !pickup.IsCollected()) { ++failures; }
        const auto actions = pickup.TakeActions();
        if (pickup.Collect() || !pickup.TakeActions().empty() || pickup.GetUnsupportedCount() != 0) { ++failures; }
        report << entry.owner << " name=" << std::quoted(entry.name) << " sprite="
            << std::hex << entry.data.sprite.packHash << std::dec << ':' << unsigned(entry.data.sprite.archetype)
            << ':' << unsigned(entry.data.sprite.animation) << " items=" << entry.data.items.size();
        if (!entry.data.particleEffect.IsNull()) {
            report << " effect=" << tables.GetPackName(entry.data.particleEffect.packHash) << ':' << unsigned(entry.data.particleEffect.localIndex);
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(entry.data.particleEffect.packHash, ZGameSection::ParticleEffect,
                entry.data.particleEffect.localIndex, payload)) { ++failures; }
            CParticleEffect effect;
            CArrayInputStream effectStream(payload);
            if (!effect.Init(effectStream) || effectStream.Available() != 0) { ++failures; }
            for (const ZParticleEmitterTemplate &emitter : effect.GetEmitters()) {
                report << " emitter=" << emitter.startSeconds << ':' << emitter.endSeconds
                    << " interval=" << emitter.intervalMinimumSeconds << ':' << emitter.intervalMaximumSeconds;
            }
        }
        for (const ZPickupAction &action : actions) {
            ++collectedActions;
            report << " [kind=" << static_cast<int>(action.kind) << " value=" << action.amount;
            if (!action.resource.IsNull()) {
                ZGameSection section = ZGameSection::StoreItem;
                if (action.kind == ZPickupAction::Kind::Sound) { section = ZGameSection::SoundEffect; }
                std::vector<std::uint8_t> payload;
                if (!tables.ReadSectionResource(action.resource.packHash, section, action.resource.localIndex, payload)) { ++failures; }
                report << " ref=" << tables.GetPackName(action.resource.packHash) << ':' << unsigned(action.resource.localIndex);
            }
            report << ']';
        }
        report << '\n';
    }
    std::printf("[pickup-check] templates=%zu actions=%u failures=%u\n", catalog.size(), collectedActions, failures);
    return failures != 0;
}

int RunPickupRenderCheck(const std::string &bigDirectory) {
    ZWindow window;
    if (!window.Open("Gun Bros - Pickup Research", 1200, 900)) { return 1; }
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    ZShaderProgram program;
    const char *directory = Paths::Shaders().c_str();
    if (!program.Load(directory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    ZPickupScene pickups(toc, tables, program);
    ZWeaponEffects effects(toc, tables, program);
    ZQuadBatch labels;
    CBitmapFont font;
    if (!pickups.Init() || !labels.Create(program) || !font.Init(*toc.GetPack(toc.GetCorePackIndex()), 0)) { return 1; }
    std::vector<ZPickupEntry> catalog;
    if (!LoadPickupCatalog(toc, tables, catalog)) { return 1; }
    labels.Begin();
    font.Draw(labels, "ORIGINAL PICKUPS - ALL NINE TEMPLATES", 26, 18, 0.7f);
    for (unsigned index = 0; index < catalog.size(); ++index) {
        const float x = 133 + (index % 3) * 267.0f;
        const float y = 130 + (index / 3) * 165.0f;
        if (!pickups.Spawn(catalog[index].ref, x, y)) { return 1; }
        font.Draw(labels, catalog[index].owner, x - 100, y + 42, 0.6f);
        if (!catalog[index].name.empty()) { font.Draw(labels, catalog[index].name, x - 100, y + 62, 0.55f); }
    }
    // All five authored infinite emitters must still be alive after two seconds.
    for (int elapsed = 0; elapsed < 2000; elapsed += 16) {
        pickups.UpdateEffects(16, effects);
        effects.AdvanceAmbientEffects(16);
    }
    std::printf("[pickup-render-check] emitters=%zu particles=%zu\n", effects.GetEffectCount(), effects.GetParticleCount());
    if (effects.GetEffectCount() != 5 || effects.GetParticleCount() == 0 || pickups.failures != 0) { return 1; }
    glViewport(0, 0, 1200, 900);
    glClearColor(0.04f, 0.08f, 0.12f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float projection[16];
    Matrix4dOrthoTopLeft(800, 600, 100, projection);
    pickups.Draw(projection, 1.5f);
    effects.Draw(projection);
    labels.Upload();
    labels.Draw(program, projection);
    std::filesystem::create_directories(TestOutput::Path(""));
    if (glGetError() != 0 || !Capture::SaveFrame(window, TestOutput::Path("pickup-render-check.png"))) { return 1; }
    window.Present();
    const std::size_t livingParticles = effects.GetParticleCount();
    pickups.Reset();
    if (effects.GetEffectCount() != 0 || effects.GetParticleCount() != livingParticles) { return 1; }
    for (int elapsed = 0; elapsed < 30000; elapsed += 16) { effects.AdvanceAmbientEffects(16); }
    if (effects.GetParticleCount() != 0) { return 1; }
    std::printf("[pickup-render-check] templates=%zu failures=0\n", catalog.size());
    return 0;
}
