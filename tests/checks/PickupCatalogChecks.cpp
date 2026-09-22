#include "gun_bros_re/debug/Capture.h"
#include "engine/core/ZPaths.h"
/** @file PickupCatalogChecks.cpp
 * @brief Verify the entire wire payload and every real collection export.
 */
#include "TestOutput.h"
#include "gun_bros_re/data/store/CStoreItem.h"
#include "engine/graphics/CBitmapFont.h"
#include "gun_bros_re/effects/CParticleEffect.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "engine/platform/ZWindow.h"
#include "engine/core/CMatrix4d.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include "Checks.h"

/** Research enumeration only; template loading belongs to CPickup::Template. */
std::vector<GameObjectRef> GetPickupCheckReferences(CResTOCManager &toc, CGunBros &tables) {
    std::vector<GameObjectRef> references;
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const auto *pack = toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Pickup);
        for (unsigned index = 0; index < count; ++index) {
            GameObjectRef resource;
            resource.packHash = pack->GetPackHash();
            resource.localIndex = static_cast<std::uint8_t>(index);
            references.push_back(resource);
        }
    }
    return references;
}

int RunPickupCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CGunBros tables(toc);
    const auto references = GetPickupCheckReferences(toc, tables);
    if (references.empty()) { return 1; }
    CPickup::Template poolTemplate;
    if (!poolTemplate.Load(tables, references.front())) { return 1; }
    // GetPickup :145625 has twenty slots, independent of particle capacity.
    CLevelObjectPool pool;
    for (unsigned index = 0; index < 20; ++index) {
        CPickup *pickup = pool.GetPickup();
        if (pickup == nullptr) { return 1; }
        pickup->Bind(poolTemplate);
    }
    if (pool.GetPickup() != nullptr || pool.GetPickups().size() != 20) { return 1; }
    CPickup &first = *pool.GetPickups().front();
    if (!first.Collect() || first.Collect() || pool.GetPickup() != nullptr) { return 1; }
    pool.ReleasePickup(0);
    if (pool.GetPickup() == nullptr || pool.GetPickups().size() != 20) { return 1; }
    pool.Clear();
    if (!pool.GetPickups().empty() || pool.GetPickup() == nullptr) { return 1; }
    std::filesystem::create_directories(TestOutput::Path(""));
    std::ofstream report(TestOutput::Path("pickup-check.txt"));
    unsigned failures = 0;
    unsigned collectedActions = 0;
    for (const GameObjectRef &resource : references) {
        CPickup::Template data;
        if (!data.Load(tables, resource)) { return 1; }
        CPickup pickup;
        pickup.Bind(data);
        if (!pickup.Collect() || !pickup.IsCollected()) { ++failures; }
        const auto actions = pickup.TakeActions();
        if (pickup.Collect() || !pickup.TakeActions().empty() || pickup.GetUnsupportedCount() != 0) { ++failures; }
        report << tables.GetPackName(resource.packHash) << " pickup " << unsigned(resource.localIndex)
            << " name=" << std::quoted(tables.ReadString(data.name)) << " sprite="
            << std::hex << data.sprite.packHash << std::dec << ':' << unsigned(data.sprite.archetype)
            << ':' << unsigned(data.sprite.animation) << " items=" << data.items.size();
        if (!data.particleEffect.IsNull()) {
            report << " effect=" << tables.GetPackName(data.particleEffect.packHash) << ':' << unsigned(data.particleEffect.localIndex);
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(data.particleEffect.packHash, ZGameSection::ParticleEffect,
                data.particleEffect.localIndex, payload)) { ++failures; }
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
    std::printf("[pickup-check] templates=%zu actions=%u failures=%u\n", references.size(), collectedActions, failures);
    return failures != 0;
}

int RunPickupRenderCheck(const std::string &bigDirectory) {
    ZWindow window;
    if (!window.Open("Gun Bros - Pickup Research", 1200, 900)) { return 1; }
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CGunBros tables(toc);
    ZShaderProgram program;
    const char *directory = Paths::Shaders().c_str();
    if (!program.Load(directory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    CLevel effects(toc, tables, program);
    ZQuadBatch labels;
    CBitmapFont font;
    if (!effects.InitPickups(toc, tables, program) || !labels.Create(program) || !font.Init(*toc.GetPack(toc.GetCorePackIndex()), 0)) { return 1; }
    const auto references = GetPickupCheckReferences(toc, tables);
    if (references.empty()) { return 1; }
    labels.Begin();
    font.Draw(labels, "ORIGINAL PICKUPS - ALL NINE TEMPLATES", 26, 18, 0.7f);
    for (unsigned index = 0; index < references.size(); ++index) {
        const auto &resource = references[index];
        CPickup::Template data;
        if (!data.Load(tables, resource)) { return 1; }
        const float x = 133 + (index % 3) * 267.0f;
        const float y = 130 + (index / 3) * 165.0f;
        if (!effects.SpawnPickupAt(resource, x, y)) { return 1; }
        const std::string owner = tables.GetPackName(resource.packHash) + " pickup " + std::to_string(resource.localIndex);
        const std::string name = tables.ReadString(data.name);
        font.Draw(labels, owner, x - 100, y + 42, 0.6f);
        if (!name.empty()) { font.Draw(labels, name, x - 100, y + 62, 0.55f); }
    }
    // All five authored infinite emitters must still be alive after two seconds.
    for (int elapsed = 0; elapsed < 2000; elapsed += 16) {
        effects.UpdatePickupAnimations(16);
        effects.AdvanceAmbientEffects(16);
    }
    std::printf("[pickup-render-check] emitters=%zu particles=%zu\n", effects.GetEffectCount(), effects.GetParticleCount());
    if (effects.GetEffectCount() != 5 || effects.GetParticleCount() == 0 || effects.GetPickupFailureCount() != 0) { return 1; }
    glViewport(0, 0, 1200, 900);
    glClearColor(0.04f, 0.08f, 0.12f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float projection[16];
    Matrix4dOrthoTopLeft(800, 600, 100, projection);
    effects.DrawPickups(projection, 1.5f);
    effects.Draw(projection);
    labels.Upload();
    labels.Draw(program, projection);
    std::filesystem::create_directories(TestOutput::Path(""));
    if (glGetError() != 0 || !Capture::SaveFrame(window, TestOutput::Path("pickup-render-check.png"))) { return 1; }
    window.Present();
    const std::size_t livingParticles = effects.GetParticleCount();
    effects.ResetPickups();
    // CPickup calls StopSpawning: players remain alive until their particles drain.
    if (effects.GetEffectCount() != 5 || effects.GetParticleCount() != livingParticles) { return 1; }
    for (int elapsed = 0; elapsed < 30000; elapsed += 16) { effects.AdvanceAmbientEffects(16); }
    if (effects.GetParticleCount() != 0 || effects.GetEffectCount() != 0) { return 1; }
    GameObjectRef emittingPickup;
    GameObjectRef emittingEffect;
    for (const auto &resource : references) {
        CPickup::Template data;
        if (!data.Load(tables, resource)) { return 1; }
        if (!data.particleEffect.IsNull()) {
            emittingPickup = resource;
            emittingEffect = data.particleEffect;
            break;
        }
    }
    if (emittingPickup.IsNull()) { return 1; }
    for (unsigned index = 0; index < 20; ++index) {
        if (!effects.SpawnPickupAt(emittingPickup, 100, 100)) { return 1; }
    }
    if (effects.SpawnPickupAt(emittingPickup, 100, 100) || effects.GetPickupCount() != 20 ||
        effects.GetEffectCount() != 20 || effects.GetPickupFailureCount() != 0) { return 1; }
    // Recycling map-effect slots must not let retired pickups stop a newer effect.
    effects.Clear();
    const auto replacement = effects.StartPersistentEffect(emittingEffect, 100, 100, true);
    if (replacement == 0) { return 1; }
    effects.ResetPickups();
    for (int elapsed = 0; elapsed < 2000; elapsed += 16) { effects.AdvanceAmbientEffects(16); }
    if (effects.GetEffectCount() != 1 || effects.GetPickupCount() != 0 ||
        !effects.SpawnPickupAt(emittingPickup, 100, 100)) { return 1; }
    effects.ResetPickups();
    effects.StopSpawning(replacement);
    for (int elapsed = 0; elapsed < 30000; elapsed += 16) { effects.AdvanceAmbientEffects(16); }
    if (effects.GetParticleCount() != 0 || effects.GetEffectCount() != 0) { return 1; }
    std::printf("[pickup-boundary-check] capacity/reuse/stale-effect/draining passed\n");
    std::printf("[pickup-render-check] templates=%zu failures=0\n", references.size());
    return 0;
}
