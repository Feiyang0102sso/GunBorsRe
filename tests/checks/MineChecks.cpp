/** Real BIG mine scripts, animated firing, and original map boundary regression. */
#define NOMINMAX
#include "engine/core/ZPaths.h"
#include "engine/core/ZMatrix4d.h"
#include "engine/platform/ZWindow.h"
#include "engine/platform/ZGLLoader.h"
#include "gun_bros_re/data/ZWeaponCatalog.h"
#include "gun_bros_re/gameplay/ZWeaponEffects.h"
#include "gun_bros_re/gameplay/ZMapWorldInternal.h"
#include "gun_bros_re/gameplay/CBullet.h"
#include <cstdio>
#include <cmath>

/** Observe real projectile splash dispatch without unrelated enemy scheduling. */
class MineCheckWorld : public ZProjectileWorld {
public:
    unsigned explosions = 0;
    float damage = 0;
    ZCombatTrace Trace(const ZCombatHit &, float, float, float, float, float,
        const std::vector<ZCombatId> &) override { return {}; }
    ZHitResult ApplyHit(ZCombatId, const ZCombatHit &) override { return ZHitResult::Hit; }
    void Splash(const ZCombatHit &hit, float, float, float, int) override {
        ++explosions;
        damage += hit.damage;
    }
    void SpawnFromProjectile(const GameObjectRef &, const ZCombatHit &) override {}
    bool FindTarget(const ZCombatHit &, float, float &, float &) override { return false; }
    bool Anchor(ZCombatId, int, int, float &, float &, float &, float &) override { return false; }
};

int RunMineCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    std::vector<ZWeaponEntry> weapons;
    ZPlayerTemplateData playerTemplate;
    if (!LoadWeaponCatalog(toc, tables, weapons) || !FindPlayerTemplate(toc, tables, playerTemplate)) { return 1; }
    ZWindow window;
    if (!window.Open("Mine diagnostic", 800, 600)) { return 1; }
    ZShaderProgram program;
    if (!program.Load(Paths::Shaders(), "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    ZWeaponEffects effects(toc, tables, program);
    MineCheckWorld world;
    effects.SetCombatWorld(&world);
    float identity[16], model[16];
    Matrix4dIdentity(identity);
    MapDetail::ZLoadedMap map;
    if (!MapDetail::LoadMap(toc, toc.GetPackIndexFromName("pack2"), 7, map)) { return 1; }
    MapDetail::BuildCollisionScene(map);
    unsigned failures = 0;
    for (const auto &entry : weapons) {
        const bool ordinaryBullet = entry.name == "ER97E Elite";
        if (entry.name != "Load Dropper" && entry.name != "Deuce Dropper X90" && entry.name != "Eggsecutioner" && !ordinaryBullet) { continue; }
        effects.Clear();
        const std::size_t startingShots = effects.GetShotCount();
        ZPlayerModel player;
        if (!BuildPlayerBody(tables, playerTemplate.moveSet, player) ||
            !EquipPlayerWeapon(tables, playerTemplate.script, entry.data, entry.owner, player) ||
            !CreatePlayerBuffers(player, program)) { return 1; }
        BuildPlayerGameMatrix(identity, 400, 540,
            PlayerModelWorldScale(player, playerTemplate.gameScale, 1), 0, model);
        SetPlayerInput(player, false, true);
        std::size_t halfwayShots = 0;
        for (int frame = 0; frame < 1250; ++frame) {
            AdvancePlayer(player, 16);
            effects.Update(player, model, 0, 16);
            if (frame == 624) { halfwayShots = effects.GetShotCount(); }
        }
        const bool stalled = effects.GetShotCount() == halfwayShots;
        if (stalled) { ++failures; }
        std::printf("[mine-check] %s shots10s=%zu shots20s=%zu live=%zu canFire=%d result=%s\n",
            entry.name.c_str(), halfwayShots - startingShots, effects.GetShotCount() - startingShots, effects.GetBulletCount(),
            player.weapon->gun.CanFire(), stalled ? "FAIL-stalled" : "PASS-continuing");
        // These expectations are fixtures decoded from the original gun scripts.
        if (entry.name == "Load Dropper" && effects.GetBulletCount() != 6) { ++failures; }
        if (entry.name == "Deuce Dropper X90" && effects.GetBulletCount() != 5) { ++failures; }
        // Use an actual original map edge; only shooter placement is synthetic.
        const auto &vertices = map.weaponCollision.terrain.GetVertices();
        for (const auto &edge : map.weaponCollision.terrain.GetEdges()) {
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
                if (!EquipPlayerWeapon(tables, playerTemplate.script, entry.data, entry.owner, player) ||
                    !CreatePlayerBuffers(player, program)) { return 1; }
                BuildPlayerGameMatrix(identity, midpointX - nx * setback, midpointY - ny * setback,
                    PlayerModelWorldScale(player, playerTemplate.gameScale, 1), facing, model);
                SetPlayerInput(player, false, true);
                unsigned outside = 0;
                unsigned particles = 0;
                for (int frame = 0; frame < 60; ++frame) {
                    AdvancePlayer(player, 16);
                    effects.Update(player, model, facing, 16, &map.weaponCollision);
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
        if (!EquipPlayerWeapon(tables, playerTemplate.script, entry.data, entry.owner, player) ||
            !CreatePlayerBuffers(player, program)) { return 1; }
        BuildPlayerGameMatrix(identity, 400, 540,
            PlayerModelWorldScale(player, playerTemplate.gameScale, 1), 0, model);
        SetPlayerInput(player, false, true);
        for (int frame = 0; frame < 100 && effects.GetBulletCount() == 0; ++frame) {
            AdvancePlayer(player, 16);
            effects.Update(player, model, 0, 16);
        }
        SetPlayerInput(player, false, false);
        if (effects.GetBulletCount() != 1) { ++failures; }
        for (int frame = 0; frame < 1812; ++frame) {
            AdvancePlayer(player, 16);
            effects.Update(player, model, 0, 16);
        }
        const auto beforeExpiry = effects.GetBulletCount();
        if (beforeExpiry != 1 || world.explosions != 0) { ++failures; }
        for (int frame = 0; frame < 125; ++frame) {
            AdvancePlayer(player, 16);
            effects.Update(player, model, 0, 16);
        }
        if (effects.GetBulletCount() != 0 || world.explosions != 1) { ++failures; }
        if (player.weapon->gun.FunctionResolver(13, nullptr, 0) != 0) { ++failures; }
        std::printf("[mine-check] %s live29s=%zu live31s=%zu explosions=%u\n",
            entry.name.c_str(), beforeExpiry, effects.GetBulletCount(), world.explosions);
        if (entry.name != "Load Dropper") { continue; }
        // Two instances of the SAME gun and SAME owner ID must stay isolated.
        // This catches both the old owner-only search and resource-ID substitutes.
        effects.Clear();
        if (!EquipPlayerWeapon(tables, playerTemplate.script, entry.data, entry.owner, player) ||
            !CreatePlayerBuffers(player, program)) { return 1; }
        {
            ZPlayerModel other;
            if (!BuildPlayerBody(tables, playerTemplate.moveSet, other) ||
                !EquipPlayerWeapon(tables, playerTemplate.script, entry.data, entry.owner, other) ||
                !CreatePlayerBuffers(other, program)) { return 1; }
            float otherModel[16];
            BuildPlayerGameMatrix(identity, 900, 540,
                PlayerModelWorldScale(other, playerTemplate.gameScale, 1), 0, otherModel);
            other.weapon->gun.Fire();
            effects.EmitBrother(other, otherModel, 0, kPlayerCombatId);
            SetPlayerInput(player, false, true);
            for (int frame = 0; frame < 1250; ++frame) {
                AdvancePlayer(player, 16);
                effects.Update(player, model, 0, 16);
            }
            unsigned otherMines = 0;
            for (const auto &shot : effects.GetProjectileStates()) {
                if (shot.x > 700) { ++otherMines; }
            }
            if (otherMines != 1 || effects.GetBulletCount() != 7) { ++failures; }
            const unsigned explosionsBefore = world.explosions;
            unsigned removed = 0;
            while (player.weapon->gun.FunctionResolver(13, nullptr, 0) != 0) {
                ++removed;
                if (removed > 6) { ++failures; break; }
            }
            SetPlayerInput(player, false, false);
            effects.Update(player, model, 0, 16);
            if (removed != 6 || effects.GetBulletCount() != 1 || world.explosions - explosionsBefore != 6) {
                ++failures;
            }
            std::printf("[mine-check] source-isolation other=%u ownRemoved=%u blasts=%u\n",
                otherMines, removed, world.explosions - explosionsBefore);
            // Re-equipping invalidates the old source, not the still-living bullet.
            if (!EquipPlayerWeapon(tables, playerTemplate.script, entry.data, entry.owner, other) ||
                !CreatePlayerBuffers(other, program)) { return 1; }
            if (other.weapon->gun.FunctionResolver(13, nullptr, 0) != 0) { ++failures; }
        }
        // Destroy the source before the old bullet, then clear twice. Neither may
        // dereference the old gun or send its decrement to the current gun.
        effects.Clear();
        effects.Clear();
        SetPlayerInput(player, false, true);
        for (int frame = 0; frame < 1250; ++frame) {
            AdvancePlayer(player, 16);
            effects.Update(player, model, 0, 16);
        }
        if (effects.GetBulletCount() != 6) { ++failures; }
        std::printf("[mine-check] after-clear live=%zu\n", effects.GetBulletCount());
    }
    std::printf("[mine-check] failures=%u\n", failures);
    return failures == 0 ? 0 : 1;
}
