#include "gun_bros_viewer/ViewerControls.h"
#include "gun_bros_re/debug/Capture.h"
#include "engine/core/ZPaths.h"
/**
 * @file M35Mesh.cpp
 * @brief M3.5 and M3.7 harnesses: the 3D models in Section 31.
 *
 * Four entry points over one body of knowledge:
 *
 * - `--meshes` parses every model and reports what is in it,
 * - `--movesets` follows every model to the atlas it wears,
 * - `--mesh` puts one on screen and plays its moves,
 * - `--character` stands a whole player up out of three of them.
 *
 * All four share a walk over the five template types that own models, because
 * "which atlas does this model wear" has exactly one answer and it should be
 * computed in exactly one place. That sharing is why the M3.7 viewer is here
 * rather than in a file of its own.
 *
 * What is NOT here is the character assembly itself: torso, legs and the bone
 * a gun hangs off now live in PlayerModel.h, which the map viewer shares. What
 * stays is the weapon CATALOGUE, which is a viewer feature -- the game hands a
 * player one gun and never a list to page through.
 * The new WeaponCatalog now shares that list with GameView's equipment keys;
 * the original catalogue-only helpers below remain as historical reference.
 *
 * PackTables has already moved out to its own header, which M3.8 shares. The
 * walk itself should follow the next time something outside this file needs
 * it; neither M3.8 nor the map viewer does, because an enemy is reached by its
 * own template and the player by a direct look-up.
 */

#define NOMINMAX
#include "TestOutput.h"
#include "gun_bros_viewer/scenes/MeshPreview.h"

#include "gun_bros_re/data/ZPackTables.h"
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "gun_bros_re/data/ZArmorCatalog.h"
#include "gun_bros_re/data/ZWeaponCatalog.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/effects/CParticleEffect.h"

#include "engine/resources/CArrayInputStream.h"
#include "engine/core/ZMatrix4d.h"
#include "engine/graphics/ZMeshBuffer.h"
#include "engine/graphics/ZPNG.h"
#include "engine/graphics/ZShaderProgram.h"
#include "engine/graphics/ZTexture.h"
#include "engine/platform/ZWindow.h"
#include "engine/platform/ZGLLoader.h"
#include "engine/glu/script/CScript.h"
#include "gun_bros_re/gameplay/CArmor.h"
#include "gun_bros_re/gameplay/CBullet.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "gun_bros_re/data/CGameObjectPack.h"
#include "gun_bros_re/gameplay/CGun.h"
#include "engine/graphics/CMesh.h"
#include "engine/graphics/CMeshAnimationController.h"
#include "engine/graphics/CMeshCamera.h"
#include "engine/graphics/CMoveSetMesh.h"
#include "engine/graphics/CMoveSetMeshController.h"
#include "engine/resources/CResTOCManager.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "gun_bros_viewer/scenes/MeshPreviewInternal.h"
using namespace MeshPreviewDetail;
#include "Checks.h"
#include "ParticleRuntimeChecks.h"

/** A target beside the muzzle ray reproduces invisible wide-beam obstruction. */
class WeaponRayCheckWorld : public ZProjectileWorld {
public:
    bool moveAnchor = false;
    unsigned beamContacts = 0;
    float targetOffset = 50;
    float targetDistance = 100;
    std::vector<float> splashDamage;
    ZCombatTrace Trace(const ZCombatHit &hit, float x, float y, float dx, float dy,
        float radius, const std::vector<ZCombatId> &skip) override {
        if ((hit.flags & 0x100) == 0 || !skip.empty()) { return {}; }
        const float fraction = CombatGeometry::CircleFraction(x, y, dx, dy, x + targetOffset, y - targetDistance, 10 + radius);
        if (fraction > 1) { return {}; }
        ++beamContacts;
        return {99, fraction};
    }
    ZHitResult ApplyHit(ZCombatId, const ZCombatHit &) override { return ZHitResult::Hit; }
    void Splash(const ZCombatHit &hit, float, float, float, int) override { splashDamage.push_back(hit.damage); }
    void SpawnFromProjectile(const GameObjectRef &, const ZCombatHit &) override {}
    bool FindTarget(const ZCombatHit &, float, float &, float &) override { return false; }
    bool Anchor(ZCombatId, int, int, float &x, float &y, float &z, float &direction) override {
        if (!moveAnchor) { return false; }
        x = 300; y = 400; z = 0; direction = 73;
        return true;
    }
};
int RunWeaponCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    std::vector<ZWeaponEntry> weapons;
    CBrother::Template playerTemplate;
    if (!LoadWeaponCatalog(toc, tables, weapons) ||
        !playerTemplate.Load(toc, tables)) { return 1; }

    // Test the input mapping independently of each category's catalogue size.
    for (std::size_t i = 0; i < weapons.size(); ++i) {
        const std::size_t next = SelectWeaponKey(weapons, i, ZKeyCode::M);
        if (weapons[next].category != weapons[i].category ||
            SelectWeaponKey(weapons, next, ZKeyCode::N) != i ||
            SelectWeaponKey(weapons, i, ZKeyCode::E) != i ||
            SelectWeaponKey(weapons, i, ZKeyCode::Digit8) != i ||
            SelectWeaponKey(weapons, i, ZKeyCode::Digit9) != i) { return 1; }
        for (int category = 0; category < kWeaponCategoryCount; ++category) {
            const ZKeyCode key = static_cast<ZKeyCode>(static_cast<int>(ZKeyCode::Digit1) + category);
            if (weapons[SelectWeaponKey(weapons, i, key)].category != category) { return 1; }
        }
    }
    ZWindow window;
    if (!window.Open("Weapon verification", 800, 600)) { return 1; }
    ZShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    CLevel effects(toc, tables, program);
    // The observed Kraken failure: particle templates 0x10/0x20 must select
    // explosion animations 4/5, never laser animations 0/1 from the same atlas.
    ZParticleEmitterTemplate emitter;
    emitter.animationMask = 0x10;
    if (emitter.SelectAnimation(0) != 4 || emitter.SelectAnimation(1) != 4) { return 1; }
    emitter.animationMask = 0x20;
    if (emitter.SelectAnimation(0.5f) != 5) { return 1; }
    // Validate the actual STORE join, not hand-selected resource ordinals.
    std::vector<ZStoreEntry> storeEntries;
    if (!LoadStoreCatalog(toc, tables, storeEntries)) { return 1; }
    for (const auto &store : storeEntries) {
        if (store.data.objects.size() != 1 || store.data.objects.front().type != 6) { continue; }
        const auto &reference = store.data.objects.front().object;
        bool matched = false;
        for (const auto &weapon : weapons) {
            if (weapon.packHash != reference.packHash || weapon.ordinal != reference.localIndex) { continue; }
            matched = weapon.hasStoreEntry && weapon.category == store.data.type;
        }
        if (!matched) { std::printf("[weapon-check] original store category join failed\n"); return 1; }
    }
    float identity[kMatrix4dElements];
    float modelToScene[kMatrix4dElements];
    float sceneMvp[kMatrix4dElements];
    Matrix4dIdentity(identity);
    Matrix4dOrthoTopLeft(800, 600, 1000, sceneMvp);
    std::size_t placeholders = 0;
    // A wall ahead of the muzzle exercises impact callbacks before the fuse.
    const std::vector<std::uint8_t> wallBytes = {
        2, 0, 0, 0, 0, 0, 100, 0, 0, 0,
        32, 3, 0, 0, 100, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0
    };
    ZWeaponCollision impactScene;
    CArrayInputStream wallStream(wallBytes);
    if (!impactScene.walls.Load(wallStream)) { return 1; }
    impactScene.terrain = impactScene.walls;
    for (std::size_t i = 0; i < weapons.size(); ++i) {
        if (!window.PumpEvents()) { return 1; }
        effects.Clear();
        CBrother player;
        const ZWeaponEntry &entry = weapons[i];
        if (!player.BuildBody(tables, playerTemplate.GetMoveSet()) ||
            !player.EquipWeapon(tables, playerTemplate.GetScript(), entry.data, entry.owner) ||
            !player.CreateBuffers(program)) { return 1; }
        // Real thresholds exercise each boundary and both critical outcomes;
        // restoring zero keeps the existing firing regression at base mastery.
        if (entry.hasStoreEntry && entry.data.GetMasteryLimit() > 0) {
            for (unsigned tier = 0; tier < 3; ++tier) {
                const unsigned threshold = entry.data.GetMasteryThreshold(tier);
                player.weapon->SetMasteryExperience(threshold - 1);
                if (player.weapon->GetMasteryLevel() != tier) { return 1; }
                player.weapon->SetMasteryExperience(threshold);
                // Some scripted launchers intentionally have a zero fire-rate
                // field; their attack state sets the timer itself.
                if (player.weapon->GetMasteryLevel() != tier + 1 || player.weapon->GetMasterySpeedMod() < 100 ||
                    player.weapon->GetMasteryDamageMultiplier(0) <
                    player.weapon->GetMasteryDamageMultiplier(1)) {
                    std::printf("[mastery-check] FAIL %s tier=%u threshold=%u level=%u speed=%u interval=%u normal=%.2f critical=%.2f\n",
                        entry.owner.c_str(), tier, threshold, player.weapon->GetMasteryLevel(), player.weapon->GetMasterySpeedMod(),
                        player.weapon->GetFireRateMs(), player.weapon->GetMasteryDamageMultiplier(1), player.weapon->GetMasteryDamageMultiplier(0));
                    return 1;
                }
            }
            player.weapon->SetMasteryExperience(0);
        }
        MeshCameraBuildGameMatrix(identity, 400, 450,
            player.GetWorldScale(playerTemplate.GetGameScale(), 1), 0, modelToScene);
        if (i == 0) {
            // Actual core pistol bullet, isolated from input timing. Original
            // native factors 0, 0.5, 1 and 2 produce these 100ms distances.
            for (const std::int16_t factor : {0, 128, 256, 512}) {
                effects.Clear();
                player.weapon->TakeCues();
                const std::int16_t fire[] = {0, 0, 0, 0, factor};
                player.weapon->FunctionResolver(1, fire, 5);
                effects.EmitBrother(player, modelToScene, 0, kPlayerCombatId);
                const auto before = effects.GetProjectileStates();
                effects.Update(player, modelToScene, 0, 100);
                const auto after = effects.GetProjectileStates();
                if (before.size() != 1 || after.size() != 1) { return 1; }
                const float travel = std::hypot(after[0].x - before[0].x, after[0].y - before[0].y);
                const float expected = 43.0f * factor / 256.0f;
                std::printf("[weapon-speed] factor=%d distance=%.3f expected=%.3f\n", factor, travel, expected);
                if (std::abs(travel - expected) > 0.01f) { return 1; }
            }
            effects.Clear();
            // Regression: the default pistols must use the gun's player moves
            // and two distinct hands, even though the mesh is shared with rifles.
            ZMeshBoneTransform right{}, left{};
            if (!player.TorsoUsesWeapon() || entry.data.GetHandedness() != 2 ||
                !player.GetMuzzle(0, 0, right) || !player.GetMuzzle(1, 0, left) ||
                (right.posX == left.posX && right.posY == left.posY && right.posZ == left.posZ)) {
                std::printf("[weapon-check] pistol holding regression\n");
                return 1;
            }
        }
        // Stationary, collision-free launch probes cannot pass on footsteps
        // or distant impact sounds. Cover the reported silent weapon groups.
        const bool checkLaunch = (entry.category == 0 && entry.hasStoreEntry) ||
            (i >= 12 && i <= 15) || (i >= 24 && i <= 26) ||
            i == 33 || i == 34 || i == 36 || i == 44 || i == 46 || i == 68 || i == 69;
        if (checkLaunch) {
            const std::size_t launchSounds = effects.GetSoundCueCount();
            player.SetInput(false, true);
            for (int elapsed = 0; elapsed < 160; elapsed += 16) {
                player.Update(16);
                effects.Update(player, modelToScene, 0, 16);
            }
            if (effects.GetSoundCueCount() == launchSounds) {
                std::printf("[weapon-check] FAIL %zu launch has no audio: %s\n", i, entry.name.c_str());
                return 1;
            }
            std::printf("[weapon-check] %zu launch audio queued before impact\n", i);
            effects.Clear();
            if (!player.EquipWeapon(tables, playerTemplate.GetScript(), entry.data, entry.owner) ||
                !player.CreateBuffers(program)) { return 1; }
        }
        // Reloading weapons must not loop attack sounds ahead of their next
        // shot. Use real scripts, no movement/impacts, and a held trigger.
        if ((i >= 12 && i <= 15) || (i >= 24 && i <= 26)) {
            const std::size_t startShots = effects.GetShotCount();
            const std::size_t startSounds = effects.GetSoundCueCount();
            player.SetInput(false, true);
            for (int elapsed = 0; elapsed < 3200; elapsed += 16) {
                player.Update(16);
                effects.Update(player, modelToScene, 0, 16);
                if (effects.GetSoundCueCount() - startSounds > effects.GetShotCount() - startShots) {
                    std::printf("[weapon-check] FAIL %zu held attack precedes its projectile at %d ms\n", i, elapsed + 16);
                    return 1;
                }
            }
            if (effects.GetShotCount() - startShots < 2) {
                std::printf("[weapon-check] FAIL %zu held trigger never resumes after reload\n", i);
                return 1;
            }
            const std::size_t releaseShots = effects.GetShotCount();
            player.SetInput(false, false);
            for (int elapsed = 0; elapsed < 1600; elapsed += 16) {
                player.Update(16);
                effects.Update(player, modelToScene, 0, 16);
            }
            if (effects.GetShotCount() != releaseShots) {
                std::printf("[weapon-check] FAIL %zu released trigger resumes after reload\n", i);
                return 1;
            }
            std::printf("[weapon-check] %zu reload keeps attack cues aligned with shots; release stops\n", i);
            effects.Clear();
            if (!player.EquipWeapon(tables, playerTemplate.GetScript(), entry.data, entry.owner) ||
                !player.CreateBuffers(program)) { return 1; }
        }
        const std::size_t before = effects.GetShotCount();
        const std::size_t soundBefore = effects.GetSoundCueCount();
        if (i == 21) {
            player.SetInput(false, true);
            int elapsed = 0;
            while (elapsed < 12000 && player.weapon->CanFire()) {
                player.Update(16);
                effects.Update(player, modelToScene, 0, 16, &impactScene);
                elapsed += 16;
            }
            if (elapsed >= 12000) { std::printf("[weapon-check] Gatling never overheated\n"); return 1; }
            const std::size_t coolingShots = effects.GetShotCount();
            player.SetInput(false, false);
            player.Update(16);
            effects.Update(player, modelToScene, 0, 16);
            player.SetInput(false, true);
            for (int cooling = 0; cooling < 960; cooling += 16) {
                player.Update(16);
                effects.Update(player, modelToScene, 0, 16);
            }
            if (effects.GetShotCount() != coolingShots) {
                std::printf("[weapon-check] release/repress bypassed Gatling cooling\n"); return 1;
            }
            for (int cooling = 0; cooling < 1000; cooling += 16) {
                player.Update(16);
                effects.Update(player, modelToScene, 0, 16);
            }
            if (effects.GetShotCount() == coolingShots) { return 1; }
            player.SetInput(false, false);
            std::printf("[weapon-check] Gatling cooldown blocks repress and resumes automatically\n");
        }
        // Idle -> walk -> fire while walking -> stationary fire -> release -> fire again.
        const bool moving[] = {false, true, true, false, false, false, false};
        const bool shooting[] = {false, false, true, true, false, true, false};
        const int durations[] = {400, 400, 1600, 1600, 800, 800, 4000};
        std::size_t settledReleaseShots = 0;
        for (int phase = 0; phase < 7; ++phase) {
            player.SetInput(moving[phase], shooting[phase]);
            for (int elapsed = 0; elapsed < durations[phase]; elapsed += 16) {
                player.Update(16);
                effects.Update(player, modelToScene, 0, 16, &impactScene);
                if (phase == 6 && elapsed == 2992) { settledReleaseShots = effects.GetShotCount(); }
                if (i == 12 && phase == 2 && elapsed == 144 && effects.GetParticleCount() > 40) {
                    std::printf("[weapon-check] zero-interval shotgun emitter overproduced particles\n");
                    return 1;
                }
                if ((i == 29 || i == 32) && phase == 2 && elapsed == 16 && effects.GetTrailCount() == 0) {
                    std::printf("[weapon-check] FAIL flame trail stopped before its next emission\n");
                    return 1;
                }
            }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            float modelMvp[kMatrix4dElements];
            Matrix4dMultiply(sceneMvp, modelToScene, modelMvp);
            effects.Draw(sceneMvp, nullptr, 1.0f, ZWeaponDrawPass::BehindPlayer);
            glEnable(GL_DEPTH_TEST);
            player.Draw(program, modelMvp);
            effects.Draw(sceneMvp, nullptr, 1.0f, ZWeaponDrawPass::InFrontOfPlayer);
            if (glGetError() != GL_NO_ERROR) { return 1; }
        }
        const std::size_t emitted = effects.GetShotCount() - before;
        if (i >= 24 && i <= 26 && effects.GetSoundCueCount() == soundBefore) {
            std::printf("[weapon-check] shoulder launcher explosion sound missing\n"); return 1;
        }
        if (entry.visualOnly) {
            ++placeholders;
            std::printf("[weapon-check] %zu visual-only: archive has no firing script or projectile reference\n", i);
        }
        else if (emitted == 0) {
            std::printf("[weapon-check] %zu failed to fire: %s\n", i, entry.name.c_str());
            std::printf("  mode=%d ammo=%d defaultBullet=%08x:%u exports:", player.weapon->GetFireMode(),
                *player.weapon->VariableResolver(0), entry.data.GetBulletRef().packHash,
                entry.data.GetBulletRef().localIndex);
            for (int value : entry.data.GetScript().GetExportFunctions()) { std::printf(" %d", value); }
            std::printf("\n");
            for (const CScriptCode &code : entry.data.GetScript().GetFunctions()) {
                std::printf("  code:");
                for (int byte = 0; byte <= code.GetByteLength(); ++byte) {
                    std::printf(" %02x", code.Begin()[byte]);
                }
                std::printf("\n");
            }
            return 1;
        }
        // Released mines persist until their BIG timer/contact, not a host fuse.
        // Verify no continued firing after pending launch callbacks settle.
        bool liveBeam = false;
        for (const auto &shot : effects.GetProjectileStates()) { liveBeam = liveBeam || shot.beam; }
        if (player.weapon->IsShooting() || liveBeam || effects.GetShotCount() != settledReleaseShots) {
            std::printf("[weapon-check] %zu release left live bullets=%zu\n", i, effects.GetBulletCount());
            return 1;
        }
        std::printf("[weapon-check] %zu PASS shots=%zu %s\n", i, emitted, entry.name.c_str());
    }
    std::printf("[weapon-check] PASS %zu templates, %zu visual-only entries; keys and transitions verified\n",
        weapons.size(), placeholders);
    return 0;
}

/** BIG scripts and the production projectile update, with no map or input noise. */
int RunWeaponEffectsCheck(const std::string &bigDirectory) {
    if (!CheckParticleRuntime()) { return 1; }
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    std::vector<ZWeaponEntry> weapons;
    CBrother::Template playerTemplate;
    if (!LoadWeaponCatalog(toc, tables, weapons) || !playerTemplate.Load(toc, tables)) { return 1; }
    ZWindow window;
    if (!window.Open("Weapon effect verification", 800, 600)) { return 1; }
    glViewport(0, 0, 800, 600);
    glEnable(GL_BLEND);
    ZShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    CLevel effects(toc, tables, program);
    WeaponRayCheckWorld rayWorld;
    effects.SetCombatWorld(&rayWorld);
    // The fixture has a real 800x600 camera; off-screen rifle bullets retire by
    // CanBeCulled, not the removed host-wide lifetime shortcut.
    effects.SetViewBounds(400, 300, 800, 600);
    float identity[kMatrix4dElements], modelToScene[kMatrix4dElements], mvp[kMatrix4dElements];
    Matrix4dIdentity(identity);
    Matrix4dOrthoTopLeft(800, 600, 1000, mvp);
    unsigned failures = 0;
    for (std::size_t index = 0; index < weapons.size(); ++index) {
        const auto &entry = weapons[index];
        const bool kraken = entry.name.find("Kraken") != std::string::npos;
        const bool rifle = entry.name == "ER97E Elite";
        if (entry.category != 6 && !kraken && !rifle) { continue; }
        effects.Clear();
        rayWorld.beamContacts = 0;
        rayWorld.targetOffset = 50;
        rayWorld.targetDistance = 100;
        rayWorld.splashDamage.clear();
        CBrother player;
        if (!player.BuildBody(tables, playerTemplate.GetMoveSet()) ||
            !player.EquipWeapon(tables, playerTemplate.GetScript(), entry.data, entry.owner) ||
            !player.CreateBuffers(program)) { return 1; }
        if (kraken) { player.weapon->SetMasteryExperience(entry.data.GetMasteryThreshold(2)); }
        MeshCameraBuildGameMatrix(identity, 400, 540,
            player.GetWorldScale(playerTemplate.GetGameScale(), 1), 0, modelToScene);
        player.SetInput(false, true);
        unsigned beamFrames = 0, missingFrames = 0;
        unsigned ribbonFrames = 0;
        bool sawBeam = false;
        std::vector<GameObjectRef> seen;
        for (int elapsed = 0; elapsed < 6000; elapsed += 16) {
            player.Update(16);
            effects.Update(player, modelToScene, 0, 16);
            if (effects.GetRibbonCount() > 0) { ++ribbonFrames; }
            bool beam = false;
            for (const auto &shot : effects.GetProjectileStates()) {
                beam = beam || shot.beam;
                bool known = false;
                for (const auto &ref : seen) {
                    if (ref.packHash == shot.resource.packHash && ref.localIndex == shot.resource.localIndex) { known = true; }
                }
                if (known) { continue; }
                seen.push_back(shot.resource);
                std::vector<std::uint8_t> payload;
                if (!tables.ReadSectionResource(shot.resource.packHash, ZGameSection::Bullet, shot.resource.localIndex, payload)) { return 1; }
                CBullet::Template data;
                CArrayInputStream stream(payload);
                if (!data.Init(stream)) { return 1; }
                CBullet script;
                script.Bind(data, false);
                std::printf("[weapon-effects-check] %s bullet=%08x:%u flags=%x radius=%.2f template-animation=%u active-animation=%d\n",
                    entry.name.c_str(), shot.resource.packHash, shot.resource.localIndex, data.GetFlags(), data.GetRadius(),
                    data.GetSpriteRef().animation, shot.animation);
                if (rifle && shot.animation != data.GetSpriteRef().animation) {
                    ++failures;
                    std::printf("[weapon-effects-check] FAIL rifle lost authored sprite animation\n");
                }
                // Native 18 sets a beam's range; it must never become an expiry timer.
                if (shot.beam) {
                    CBullet reference;
                    reference.Bind(data, false);
                    const std::int16_t range[] = {123};
                    script.FunctionResolver(18, range, 1);
                    if (script.maximumBeamLength != 123) { ++failures; }
                    // Some beams have authored short timers. Changing range must
                    // preserve their removal timeline, not force indefinite life.
                    for (int time = 0; time < 3500; time += 16) {
                        script.Update(16, 1000000);
                        reference.Update(16, 1000000);
                        if (script.removed != reference.removed) { ++failures; break; }
                    }
                }
                if (script.ribbon.capacity != 0) {
                    std::printf("[weapon-effects-check] ribbon points=%u width=%.1f interval=%u color=%u,%u,%u,%u\n",
                        script.ribbon.capacity, script.ribbon.width, script.ribbon.intervalMs,
                        script.ribbon.color[0], script.ribbon.color[1], script.ribbon.color[2], script.ribbon.color[3]);
                }
            }
            if (beam) { sawBeam = true; ++beamFrames; }
            else if (sawBeam && entry.name == "Infinity Laser") { ++missingFrames; }
            if (elapsed == 992 || elapsed == 2992) {
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                effects.Draw(mvp);
                const std::string path = TestOutput::Path("weapon-effects-") + std::to_string(index) + "-" + std::to_string(elapsed + 16) + ".png";
                if (!Capture::SaveFrame(window, path)) { return 1; }
                if (rifle && elapsed == 992) {
                    std::vector<unsigned char> pixels(800 * 350 * 4);
                    glReadPixels(0, 250, 800, 350, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                    unsigned yellow = 0, blue = 0;
                    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
                        const int red = pixels[pixel], green = pixels[pixel + 1], cyan = pixels[pixel + 2];
                        if (red > cyan + 30 && green > cyan + 20) { ++yellow; }
                        if (cyan > red + 30) { ++blue; }
                    }
                    if (yellow <= blue) { ++failures; }
                    std::printf("[weapon-effects-check] rifle pixels yellow=%u blue=%u\n", yellow, blue);
                }
            }
        }
        if (!rifle && !sawBeam) { ++failures; }
        if (missingFrames != 0) { ++failures; }
        if (rayWorld.beamContacts != 0) { ++failures; }
        if ((rifle || kraken) && ribbonFrames == 0) { ++failures; }
        if (kraken) {
            float maximumDamage = 0;
            for (float damage : rayWorld.splashDamage) {
                maximumDamage = std::max(maximumDamage, damage);
                // BIG pack5 BULLET86/124 native0(5,75); source :61169 does
                // not apply the gun's mastery or critical multiplier again.
                if (std::abs(damage - 5) > 0.001f) { ++failures; }
            }
            if (rayWorld.splashDamage.empty() || player.weapon->GetMasteryDamageMultiplier(0) <= 1) { ++failures; }
            std::printf("[splash-mastery-check] %s mastery=%u explosions=%zu maximum=%.2f expected=5 failures=%u\n",
                entry.name.c_str(), player.weapon->GetMasteryLevel(), rayWorld.splashDamage.size(), maximumDamage, failures);
        }
        std::printf("[weapon-effects-check] off-axis-beam-contacts=%u\n", rayWorld.beamContacts);
        std::printf("[weapon-effects-check] weapon=%s beam-frames=%u missing-held-frames=%u\n", entry.name.c_str(), beamFrames, missingFrames);
        // A real point on the ray must still stop the beam at the target's edge.
        rayWorld.targetOffset = 0;
        unsigned clipped = 0;
        for (unsigned tick = 0; tick < 250; ++tick) {
            player.Update(16);
            effects.Update(player, modelToScene, 0, 16);
            for (const auto &shot : effects.GetProjectileStates()) {
                if (!shot.beam) { continue; }
                if (std::abs(shot.length - 90) > 0.1f) { ++failures; }
                ++clipped;
            }
        }
        if (!rifle && clipped == 0) { ++failures; }
        rayWorld.targetDistance = 11;
        unsigned closeFrames = 0, invisibleFrames = 0;
        for (unsigned tick = 0; tick < 250; ++tick) {
            player.Update(16);
            effects.Update(player, modelToScene, 0, 16);
            bool hasBeam = false;
            for (const auto &shot : effects.GetProjectileStates()) { hasBeam = hasBeam || shot.beam; }
            if (!hasBeam) { continue; }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            effects.Draw(mvp);
            ++closeFrames;
            if (effects.GetDrawnBeamQuadCount() == 0) { ++invisibleFrames; }
            if (closeFrames == 1 && !Capture::SaveFrame(window, TestOutput::Path("weapon-effects-close-") + std::to_string(index) + ".png")) { return 1; }
        }
        if (invisibleFrames != 0) { ++failures; }
        std::printf("[weapon-effects-check] close-beam-frames=%u invisible=%u\n", closeFrames, invisibleFrames);
        player.SetInput(false, false);
        for (unsigned tick = 0; tick < 625; ++tick) {
            player.Update(16);
            effects.Update(player, modelToScene, 0, 16);
        }
        if (effects.GetBulletCount() != 0 || effects.GetRibbonCount() != 0) { ++failures; }
        if (!GLCheckErrors("weapon effect regression")) { ++failures; }
        std::printf("[weapon-effects-check] on-axis-clipped=%u ribbon-frames=%u release-bullets=%zu release-ribbons=%zu\n",
            clipped, ribbonFrames, effects.GetBulletCount(), effects.GetRibbonCount());
    }
    // Mechanical Boss Flow pack6 ENEMY8 references pack1 BULLET16 and
    // pack5 BULLET104. Exercise those real visuals independently of aiming.
    CBrother probe;
    if (!probe.BuildBody(tables, playerTemplate.GetMoveSet()) ||
        !probe.EquipWeapon(tables, playerTemplate.GetScript(), weapons.front().data, weapons.front().owner) ||
        !probe.CreateBuffers(program)) { return 1; }
    for (const auto &sample : std::array<std::pair<const char *, unsigned>, 4>{{{"pack1", 0}, {"pack1", 13}, {"pack1", 16}, {"pack5", 104}}}) {
        effects.Clear();
        effects.SetCombatWorld(nullptr);
        GameObjectRef ref;
        ref.packHash = toc.GetPack(toc.GetPackIndexFromName(sample.first))->GetPackHash();
        ref.localIndex = static_cast<std::uint8_t>(sample.second);
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Bullet, ref.localIndex, payload)) { return 1; }
        CBullet::Template data;
        CArrayInputStream input(payload);
        if (!data.Init(input)) { return 1; }
        const auto &sprite = data.GetSpriteRef();
        std::printf("[boss-beam-check] flags=%x scale=%.3f sprite=%08x:%u/%u/%u\n", data.GetFlags(),
            data.GetSpriteScale(), sprite.packHash, sprite.archetype, sprite.action, sprite.animation);
        effects.SpawnProjectile(ref, 50, 300, 0, 0, 0, 999, 1);
        for (int time = 0; time < 1000; time += 16) { effects.Update(probe, identity, 0, 16); }
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        effects.Draw(mvp);
        for (const auto &shot : effects.GetProjectileStates()) {
            std::printf("[boss-beam-check] %s:%u beam=%d animation=%d length=%.1f quads=%zu arc-quads=%zu\n",
                sample.first, sample.second, shot.beam, shot.animation, shot.length,
                effects.GetDrawnBeamQuadCount(), effects.GetDrawnLightningQuadCount());
            if (shot.beam) {
                const int sourceAnimation = static_cast<std::uint8_t>(sprite.animation + 1);
                const int endAnimation = static_cast<std::uint8_t>(sprite.animation + 2);
                if (shot.animation != sprite.animation || shot.beamSourceAnimation != sourceAnimation ||
                    shot.beamEndAnimation != endAnimation) { ++failures; }
                std::printf("[beam-binding-check] body=%d source=%d end=%d authored=%u failures=%u\n",
                    shot.animation, shot.beamSourceAnimation, shot.beamEndAnimation, sprite.animation, failures);
                CBullet reference;
                reference.Bind(data, false);
                reference.SetScriptSequenceFrame(0);
                if (reference.beamSourceAnimation != sourceAnimation || reference.beamEndAnimation != endAnimation) { ++failures; }
            }
        }
        if (sample.second == 104 && effects.GetDrawnLightningQuadCount() != 56) { ++failures; }
        if (sample.second == 104) {
            // The mech boss beam has to read as one line. Tiling the muzzle
            // flare slot instead of the body slot turns it into a bead chain,
            // whose troughs fall to a few percent of a bead's brightness.
            // Column brightness, not coverage: the flare glow still touches
            // every column, so counting unlit columns cannot tell them apart.
            std::vector<unsigned char> pixels(800 * 600 * 4);
            glReadPixels(0, 0, 800, 600, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            std::vector<int> columnGreen(800, 0);
            for (int column = 0; column < 800; ++column) {
                for (int row = 0; row < 600; ++row) {
                    columnGreen[column] += pixels[(row * 800 + column) * 4 + 1];
                }
            }
            int brightest = 0;
            for (int column = 0; column < 800; ++column) {
                if (columnGreen[column] > brightest) { brightest = columnGreen[column]; }
            }
            int firstLit = -1;
            int lastLit = -1;
            for (int column = 0; column < 800; ++column) {
                if (columnGreen[column] * 2 < brightest) { continue; }
                if (firstLit < 0) { firstLit = column; }
                lastLit = column;
            }
            int weakest = brightest;
            for (int column = firstLit; column >= 0 && column <= lastLit; ++column) {
                if (columnGreen[column] < weakest) { weakest = columnGreen[column]; }
            }
            int troughPercent = 0;
            if (brightest > 0) { troughPercent = weakest * 100 / brightest; }
            std::printf("[boss-beam-check] beam span=%d..%d trough=%d%% of the brightest column\n",
                firstLit, lastLit, troughPercent);
            // Measured: bead chain 3%, tiled beam body 44%.
            // R03 correction: that brightness threshold encoded a visual guess.
            // Keep the measurement for comparison; validate BIG slots, not smoothness.
            if (firstLit < 0) { ++failures; }
        }
        Capture::SaveFrame(window, TestOutput::Path("boss-beam-") + std::string(sample.first) + "-" + std::to_string(sample.second) + ".png");
    }
    effects.Clear();
    rayWorld.moveAnchor = true;
    effects.SetCombatWorld(&rayWorld);
    GameObjectRef bossBeam;
    bossBeam.packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
    bossBeam.localIndex = 104;
    effects.SpawnProjectile(bossBeam, 50, 300, 0, 0, 350, 999, 1);
    effects.Update(probe, identity, 0, 16);
    const auto anchored = effects.GetProjectileStates();
    if (anchored.size() != 1 || anchored[0].x != 50 || anchored[0].y != 300 || anchored[0].direction != 0) { ++failures; }
    if (!anchored.empty()) {
        std::printf("[boss-beam-anchor-check] position=%.1f,%.1f direction=%.1f expected=50,300/0\n",
            anchored[0].x, anchored[0].y, anchored[0].direction);
    }
    std::printf("[weapon-effects-check] failures=%u\n", failures);
    return failures == 0 ? 0 : 1;
}
