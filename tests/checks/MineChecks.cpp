#include "gun_bros_viewer/scenes/ZMapViewer.h"
#include "engine/graphics/CMeshCamera.h"
/** Real BIG mine scripts, animated firing, and original map boundary regression. */
#define NOMINMAX
#include "engine/core/ZPaths.h"
#include "engine/core/ZMatrix4d.h"
#include "engine/platform/ZWindow.h"
#include "engine/platform/ZGLLoader.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/map/CMapInternal.h"
#include "gun_bros_re/gameplay/weapon/CBullet.h"
#include <cstdio>
#include <cmath>

/** Observe real projectile splash dispatch without unrelated enemy scheduling. */
class MineCheckWorld : public CBullet::World {
public:
    unsigned explosions = 0;
    float damage = 0;
    Collision::Trace Trace(const Collision::Hit &, float, float, float, float, float,
        const std::vector<Collision::ObjectId> &) override { return {}; }
    Collision::HitResult ApplyHit(Collision::ObjectId, const Collision::Hit &) override { return Collision::HitResult::Hit; }
    void Splash(const Collision::Hit &hit, float, float, float, int) override {
        ++explosions;
        damage += hit.damage;
    }
    void SpawnFromProjectile(const GameObjectRef &, const Collision::Hit &) override {}
    Collision::ObjectId FindSeekTarget(const Collision::Hit &, float) override { return Collision::NoObject; }
    bool Anchor(Collision::ObjectId, int, int, float &, float &, float &, float &) override { return false; }
};

int RunMineCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    CGunBros tables(toc);
    std::vector<CGun::Entry> weapons;
    CBrother::Template playerTemplate;
    if (!CGun::LoadEntries(toc, tables, weapons) || !playerTemplate.Load(toc, tables)) { return 1; }
    ZWindow window;
    if (!window.Open("Mine diagnostic", 800, 600)) { return 1; }
    ZShaderProgram program;
    if (!program.Load(Paths::Shaders(), "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    CLevel effects(toc, tables, program);
    MineCheckWorld world;
    effects.SetCombatWorld(&world);
    float identity[16], model[16];
    Matrix4dIdentity(identity);
    CMap map;
    if (!MapDetail::LoadPreviewMap(toc, toc.GetPackIndexFromName("pack2"), 7, map)) { return 1; }
    map.BuildCollisionScene();
    unsigned failures = 0;
    for (const auto &entry : weapons) {
        const bool ordinaryBullet = entry.name == "ER97E Elite";
        if (entry.name != "Load Dropper" && entry.name != "Deuce Dropper X90" && entry.name != "Eggsecutioner" && !ordinaryBullet) { continue; }
        effects.Clear();
        const std::size_t startingShots = effects.GetShotCount();
        CBrother player;
        if (!player.BuildBody(tables, playerTemplate.GetMoveSet()) ||
            !player.EquipWeapon(tables, playerTemplate.GetScript(), entry.data, entry.owner) ||
            !player.CreateBuffers(program)) { return 1; }
        MeshCameraBuildGameMatrix(identity, 400, 540,
            player.GetWorldScale(playerTemplate.GetGameScale(), 1), 0, model);
        player.SetInput(false, true);
        std::size_t halfwayShots = 0;
        for (int frame = 0; frame < 1250; ++frame) {
            player.Update(16);
            effects.Update(player, model, 0, 16);
            if (frame == 624) { halfwayShots = effects.GetShotCount(); }
        }
        const bool stalled = effects.GetShotCount() == halfwayShots;
        if (stalled) { ++failures; }
        std::printf("[mine-check] %s shots10s=%zu shots20s=%zu live=%zu canFire=%d result=%s\n",
            entry.name.c_str(), halfwayShots - startingShots, effects.GetShotCount() - startingShots, effects.GetBulletCount(),
            player.weapon->CanFire(), stalled ? "FAIL-stalled" : "PASS-continuing");
        // These expectations are fixtures decoded from the original gun scripts.
        if (entry.name == "Load Dropper" && effects.GetBulletCount() != 6) { ++failures; }
        if (entry.name == "Deuce Dropper X90" && effects.GetBulletCount() != 5) { ++failures; }
        // Use an actual original map edge; only shooter placement is synthetic.
        const auto &vertices = map.GetResources().weaponCollision.terrain.GetVertices();
        for (const auto &edge : map.GetResources().weaponCollision.terrain.GetEdges()) {
            const auto &a = vertices[edge.firstVertex];
            const auto &b = vertices[edge.secondVertex];
            const float length = std::hypot(b.x - a.x, b.y - a.y);
            if (!edge.enabled || length < 300) { continue; }
            const float nx = (b.y - a.y) / length;
            const float ny = (a.x - b.x) / length;
            const float midpointX = (a.x + b.x) / 2;
            const float midpointY = (a.y + b.y) / 2;
            const float facing = std::atan2(ny, nx) * 180 / 3.14159265f + 90;
            for (float setback : {12.0f, 120.0f}) {
                effects.Clear();
                world.explosions = 0;
                world.damage = 0;
                if (!player.EquipWeapon(tables, playerTemplate.GetScript(), entry.data, entry.owner) ||
                    !player.CreateBuffers(program)) { return 1; }
                MeshCameraBuildGameMatrix(identity, midpointX - nx * setback, midpointY - ny * setback,
                    player.GetWorldScale(playerTemplate.GetGameScale(), 1), facing, model);
                player.SetInput(false, true);
                unsigned outside = 0;
                unsigned particles = 0;
                for (int frame = 0; frame < 60; ++frame) {
                    player.Update(16);
                    effects.Update(player, model, facing, 16, &map.GetResources().weaponCollision);
                    particles += static_cast<unsigned>(effects.GetParticleCount());
                    for (const auto &shot : effects.GetProjectileStates()) {
                        const float side = (shot.x - midpointX) * nx + (shot.y - midpointY) * ny;
                        if (side > 1) { ++outside; }
                    }
                }
                if (ordinaryBullet) {
                    // The orange map boundary must not become an ordinary bullet wall.
                    if (outside == 0) { ++failures; }
                } else {
                    if (outside != 0) { ++failures; }
                    if (setback == 12 && (world.explosions == 0 || world.damage <= 0 || particles == 0)) {
                        ++failures;
                    }
                }
                std::printf("[mine-check] %s setback=%.0f outside=%u explosions=%u damage=%.0f particleFrames=%u\n",
                    entry.name.c_str(), setback, outside, world.explosions, world.damage, particles);
            }
            break;
        }
        if (entry.name == "Eggsecutioner" || ordinaryBullet) { continue; }
        // Fire once through production, then leave it alone until the BIG timer fires.
        effects.Clear();
        world.explosions = 0;
        if (!player.EquipWeapon(tables, playerTemplate.GetScript(), entry.data, entry.owner) ||
            !player.CreateBuffers(program)) { return 1; }
        MeshCameraBuildGameMatrix(identity, 400, 540,
            player.GetWorldScale(playerTemplate.GetGameScale(), 1), 0, model);
        player.SetInput(false, true);
        for (int frame = 0; frame < 100 && effects.GetBulletCount() == 0; ++frame) {
            player.Update(16);
            effects.Update(player, model, 0, 16);
        }
        player.SetInput(false, false);
        if (effects.GetBulletCount() != 1) { ++failures; }
        for (int frame = 0; frame < 1812; ++frame) {
            player.Update(16);
            effects.Update(player, model, 0, 16);
        }
        const auto beforeExpiry = effects.GetBulletCount();
        if (beforeExpiry != 1 || world.explosions != 0) { ++failures; }
        for (int frame = 0; frame < 125; ++frame) {
            player.Update(16);
            effects.Update(player, model, 0, 16);
        }
        if (effects.GetBulletCount() != 0 || world.explosions != 1) { ++failures; }
        if (player.weapon->FunctionResolver(13, nullptr, 0) != 0) { ++failures; }
        std::printf("[mine-check] %s live29s=%zu live31s=%zu explosions=%u\n",
            entry.name.c_str(), beforeExpiry, effects.GetBulletCount(), world.explosions);
        if (entry.name != "Load Dropper") { continue; }
        // Two instances of the SAME gun and SAME owner ID must stay isolated.
        // This catches both the old owner-only search and resource-ID substitutes.
        effects.Clear();
        if (!player.EquipWeapon(tables, playerTemplate.GetScript(), entry.data, entry.owner) ||
            !player.CreateBuffers(program)) { return 1; }
        {
            CBrother other;
            if (!other.BuildBody(tables, playerTemplate.GetMoveSet()) ||
                !other.EquipWeapon(tables, playerTemplate.GetScript(), entry.data, entry.owner) ||
                !other.CreateBuffers(program)) { return 1; }
            float otherModel[16];
            MeshCameraBuildGameMatrix(identity, 900, 540,
                other.GetWorldScale(playerTemplate.GetGameScale(), 1), 0, otherModel);
            other.weapon->Fire();
            effects.EmitBrother(other, otherModel, 0, Collision::Player);
            player.SetInput(false, true);
            for (int frame = 0; frame < 1250; ++frame) {
                player.Update(16);
                effects.Update(player, model, 0, 16);
            }
            unsigned otherMines = 0;
            for (const auto &shot : effects.GetProjectileStates()) {
                if (shot.x > 700) { ++otherMines; }
            }
            if (otherMines != 1 || effects.GetBulletCount() != 7) { ++failures; }
            const unsigned explosionsBefore = world.explosions;
            unsigned removed = 0;
            while (player.weapon->FunctionResolver(13, nullptr, 0) != 0) {
                ++removed;
                if (removed > 6) { ++failures; break; }
            }
            player.SetInput(false, false);
            effects.Update(player, model, 0, 16);
            if (removed != 6 || effects.GetBulletCount() != 1 || world.explosions - explosionsBefore != 6) {
                ++failures;
            }
            std::printf("[mine-check] source-isolation other=%u ownRemoved=%u blasts=%u\n",
                otherMines, removed, world.explosions - explosionsBefore);
            // Re-equipping invalidates the old source, not the still-living bullet.
            if (!other.EquipWeapon(tables, playerTemplate.GetScript(), entry.data, entry.owner) ||
                !other.CreateBuffers(program)) { return 1; }
            if (other.weapon->FunctionResolver(13, nullptr, 0) != 0) { ++failures; }
        }
        // Destroy the source before the old bullet, then clear twice. Neither may
        // dereference the old gun or send its decrement to the current gun.
        effects.Clear();
        effects.Clear();
        player.SetInput(false, true);
        for (int frame = 0; frame < 1250; ++frame) {
            player.Update(16);
            effects.Update(player, model, 0, 16);
        }
        if (effects.GetBulletCount() != 6) { ++failures; }
        std::printf("[mine-check] after-clear live=%zu\n", effects.GetBulletCount());
    }
    std::printf("[mine-check] failures=%u\n", failures);
    return failures == 0 ? 0 : 1;
}
