/** @file Arena.cpp
 * @brief Empty arenas sharing the same player, weapons and enemy script hosts.
 */
#define NOMINMAX
#include "milestones/Arena.h"
#include "runtime/CombatScene.h"
#include "runtime/WeaponCatalog.h"
#include "engine/CMarkerBatch.h"
#include "engine/CMatrix4d.h"
#include "gun_bros/CBullet.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace {
constexpr int kStepMs = 16;
constexpr float kRadians = 3.14159265f / 180;
const char *const kShaders = ASSET_ROOT "/src/gun_bros_re/shaders";

/** Tiny diagnostic font: five columns per glyph, low bit at the top. */
void Text(CMarkerBatch &batch, float x, float y, const std::string &text, float scale = 2) {
    static const unsigned char digits[10][5] = {
        {62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},{24,20,18,127,16},
        {39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30}
    };
    static const unsigned char letters[26][5] = {
        {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
        {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
        {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
        {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
        {62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},
        {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
        {7,8,112,8,7},{97,81,73,69,67}
    };
    const float startX = x;
    for (char c : text) {
        if (c == '\n') { y += 10 * scale; x = startX; continue; }
        if (c >= 'a' && c <= 'z') { c -= 'a' - 'A'; }
        const unsigned char *glyph = nullptr;
        if (c >= '0' && c <= '9') { glyph = digits[c - '0']; }
        if (c >= 'A' && c <= 'Z') { glyph = letters[c - 'A']; }
        if (glyph != nullptr) {
            for (int column = 0; column < 5; ++column) {
                for (int row = 0; row < 7; ++row) {
                    if ((glyph[column] & (1 << row)) != 0) {
                        batch.AddRect(x + column * scale, y + row * scale, scale, scale);
                    }
                }
            }
        } else if (c == '-' || c == '_') { batch.AddRect(x, y + 3 * scale, 5 * scale, scale); }
        else if (c == '.' || c == ':') {
            batch.AddRect(x + 2 * scale, y + 6 * scale, scale, scale);
            if (c == ':') { batch.AddRect(x + 2 * scale, y + 2 * scale, scale, scale); }
        } else if (c == '/') {
            for (int i = 0; i < 5; ++i) { batch.AddRect(x + i * scale, y + (5 - i) * scale, scale, scale); }
        }
        x += 6 * scale;
    }
}

void Circle(CMarkerBatch &batch, float x, float y, float radius) {
    for (int i = 0; i < 32; ++i) {
        const float a = i * 360 / 32.0f * kRadians;
        const float b = (i + 1) * 360 / 32.0f * kRadians;
        batch.AddSegment(x + std::cos(a) * radius, y + std::sin(a) * radius,
            x + std::cos(b) * radius, y + std::sin(b) * radius, 1.5f);
    }
}

bool Equip(PackTables &tables, const PlayerTemplateData &data, const WeaponEntry &entry,
    PlayerModel &player, const CShaderProgram &program) {
    return EquipPlayerWeapon(tables, data.script, entry.data, entry.owner, player) &&
        CreatePlayerBuffers(player, program);
}

/** Exercise actual archive scripts, then write a per-entry audit for inspection. */
int CheckArena(CWindow &window, PackTables &tables, const CShaderProgram &program,
    const std::vector<EnemyTemplateData> &catalog, const std::vector<WeaponEntry> &weapons,
    const PlayerTemplateData &playerData, PlayerModel &player, PlayerVitals &vitals,
    WeaponEffects &effects, CombatScene &scene) {
    std::filesystem::create_directories("out");
    std::ofstream report("out/arena-check.csv");
    report << "index,owner,script,health,allegiance,target_type,parts,shots,travel,hits,unknown_natives,death_count,incoming_damage,deferred\n";
    unsigned failures = 0;
    unsigned scripted = 0;
    unsigned unused = 0;
    unsigned unsupported = 0;
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        if (!window.PumpEvents()) { return 1; }
        scene.Reset();
        if (!Equip(tables, playerData, weapons[0], player, program)) { return 1; }
        CombatEnemy *actor = scene.Spawn(i, 600, 340);
        if (actor == nullptr) { ++failures; continue; }
        const CombatId id = actor->model.enemy.combat.id;
        const float initialHealth = actor->model.enemy.combat.health;
        const int allegiance = actor->model.enemy.combat.variables[16];
        const int targetType = actor->model.enemy.combat.targetType;
        const unsigned parts = actor->model.enemy.GetPartCount();
        const std::size_t beforeShots = effects.GetShotCount();
        if (catalog[i].script.IsPresent()) { ++scripted; } else { ++unused; }
        // Far, near and moving targets cover attack range entry and departure.
        for (int tick = 0; tick < 500; ++tick) {
            if (tick == 160) { scene.playerY = 400; }
            if (tick == 320) { scene.playerY = 740; }
            scene.Update(kStepMs, 0, 0, false);
        }
        actor = scene.Find(id);
        float travel = 0;
        std::size_t unknown = 0;
        unsigned hitCount = 0;
        unsigned deathCount = 0;
        unsigned deferred = 0;
        if (actor != nullptr) {
            CEnemy &enemy = actor->model.enemy;
            std::printf("[arena-audit] %zu state=%u behaviour=%d range=%.0f contact=%d/%d/%d hp_in=%.1f\n",
                i, enemy.GetStateId(), enemy.combat.behaviour, enemy.combat.triggerDistance,
                enemy.combat.variables[12], enemy.combat.variables[13], enemy.combat.variables[17], vitals.incomingDamage);
            travel = std::hypot(enemy.combat.x - 600, enemy.combat.y - 340);
            CombatHit hit;
            hit.owner = kPlayerCombatId;
            hit.ownerType = 0;
            hit.damage = 1;
            hit.part = 0;
            hit.x = enemy.combat.x;
            hit.y = enemy.combat.y + 10;
            const float before = enemy.combat.health;
            const HitResult result = enemy.ReceiveHit(hit);
            enemy.Update(16);
            if (allegiance == 1 && (result != HitResult::Ignored || enemy.combat.health != before)) { ++failures; }
            hitCount = enemy.combat.hitCount;
            if (enemy.combat.enabled && !enemy.combat.dead) {
                enemy.Damage(enemy.combat.health + 100);
                enemy.Damage(100);
                if (enemy.combat.deathCount != 1 || enemy.combat.health != 0) { ++failures; }
            }
            deathCount = enemy.combat.deathCount;
            unknown = enemy.GetUnsupportedFunctionCount();
            deferred = enemy.combat.deferredMechanisms;
            // Run the death state too: its timers, effects and removal are part
            // of the contract, not just the transition to zero health.
            for (int tick = 0; tick < 200; ++tick) {
                scene.Update(kStepMs, 0, 0, false);
                CombatEnemy *corpse = scene.Find(id);
                if (corpse == nullptr) { break; }
                unknown = corpse->model.enemy.GetUnsupportedFunctionCount();
                deferred |= corpse->model.enemy.combat.deferredMechanisms;
                if (corpse->model.enemy.combat.deathCount > 1) { ++failures; break; }
            }
            unsupported += static_cast<unsigned>(unknown);
        }
        report << i << ',' << catalog[i].owner << ',' << catalog[i].script.IsPresent() << ','
            << initialHealth << ',' << allegiance << ',' << targetType << ',' << parts << ','
            << effects.GetShotCount() - beforeShots << ',' << travel << ',' << hitCount << ','
            << unknown << ',' << deathCount << ',' << vitals.incomingDamage << ',' << deferred << '\n';
        std::printf("[arena-check] %zu %s hp=%.0f team=%d target=%d shots=%zu unknown=%zu\n",
            i, catalog[i].owner.c_str(), initialHealth, allegiance, targetType,
            effects.GetShotCount() - beforeShots, unknown);
    }
    // Contracts use a real ordinary enemy and a real pistol projectile.
    scene.Reset();
    Equip(tables, playerData, weapons[0], player, program);
    CombatEnemy *actor = scene.Spawn(0, 600, 340);
    if (actor == nullptr) { return 1; }
    CEnemy &enemy = actor->model.enemy;
    enemy.combat.behaviour = 7;
    enemy.combat.health = 10000;
    enemy.combat.maxHealth = 10000;
    const GameObjectRef bullet = weapons[0].data.GetBulletRef();
    float matrix[16];
    scene.PlayerMatrix(matrix);
    // A segment crosses the complete target in one update.
    effects.SpawnProjectile(bullet, 600, 700, 0, -90, 45000, kPlayerCombatId, 0);
    effects.Update(player, matrix, 0, 16);
    if (enemy.combat.health >= 10000 || enemy.combat.hitCount != 1) {
        std::printf("[arena-check] FAIL swept projectile\n"); ++failures;
    }
    const float afterHit = enemy.combat.health;
    effects.SpawnProjectile(bullet, 100, 700, 0, -90, 45000, kPlayerCombatId, 0);
    effects.Update(player, matrix, 0, 16);
    if (enemy.combat.health != afterHit) { std::printf("[arena-check] FAIL miss\n"); ++failures; }
    enemy.combat.variables[16] = 1;
    effects.SpawnProjectile(bullet, 600, 700, 0, -90, 45000, kPlayerCombatId, 0);
    effects.Update(player, matrix, 0, 16);
    if (enemy.combat.health != afterHit) { std::printf("[arena-check] FAIL friendly fire\n"); ++failures; }
    // Invincibility logs incoming damage. Equipment must preserve health.
    vitals.Reset();
    vitals.invincible = true;
    player.weapon->brother.ReceiveDamage(7);
    if (vitals.health != vitals.maximum || vitals.incomingDamage != 7) { ++failures; }
    vitals.invincible = false;
    player.weapon->brother.ReceiveDamage(1);
    const float wounded = vitals.health;
    if (vitals.dead || wounded != vitals.maximum - 1) { ++failures; }
    Equip(tables, playerData, weapons[1], player, program);
    if (vitals.health != wounded || vitals.dead) { ++failures; }
    player.weapon->brother.ReceiveDamage(vitals.maximum + 1);
    player.weapon->brother.ReceiveDamage(99);
    if (!vitals.dead || vitals.deaths != 1) { ++failures; }
    scene.Reset();
    if (vitals.health != vitals.maximum || vitals.dead || effects.GetBulletCount() != 0 || scene.AliveCount() != 0) { ++failures; }
    CombatEnemy *first = scene.Spawn(0, 300, 300);
    CombatEnemy *second = scene.Spawn(0, 900, 300);
    if (first == nullptr || second == nullptr) { ++failures; }
    else {
        const float otherHealth = second->model.enemy.combat.health;
        first->model.enemy.Damage(1);
        if (second->model.enemy.combat.health != otherHealth || first->model.enemy.combat.id == second->model.enemy.combat.id) { ++failures; }
    }
    for (int count = 0; count < 12; ++count) {
        if (scene.SpawnNearby(0) == nullptr) { ++failures; break; }
    }
    for (std::size_t i = 0; i < scene.enemies.size(); ++i) {
        const EnemyCombat &one = scene.enemies[i]->model.enemy.combat;
        if (one.x < 40 || one.x > kArenaWidth - 40 || one.y < 145 || one.y > kArenaHeight - 40) { ++failures; }
        for (std::size_t j = i + 1; j < scene.enemies.size(); ++j) {
            const EnemyCombat &other = scene.enemies[j]->model.enemy.combat;
            if (one.id == other.id || std::hypot(one.x - other.x, one.y - other.y) < 70) { ++failures; }
        }
    }
    player.weapon->brother.Stun(64);
    const float stunnedX = scene.playerX;
    scene.Update(16, 1, 0, true);
    if (scene.playerX != stunnedX || vitals.stunMs <= 0) { ++failures; }
    for (int tick = 0; tick < 20; ++tick) { scene.Update(16, 0, 0, false); }
    if (vitals.stunMs != 0) { ++failures; }
    // A friendly turret must select a hostile Enemy, without shooting its owner.
    scene.Reset();
    first = scene.Spawn(54, 500, 500);
    second = scene.Spawn(0, 500, 240);
    if (first == nullptr || second == nullptr) { ++failures; }
    else {
        const CombatId ally = first->model.enemy.combat.id;
        const CombatId hostile = second->model.enemy.combat.id;
        const std::size_t shots = effects.GetShotCount();
        bool selectedHostile = false;
        for (int tick = 0; tick < 300; ++tick) {
            scene.Update(16, 0, 0, false);
            CombatEnemy *turret = scene.Find(ally);
            if (turret != nullptr && turret->model.enemy.combat.targetId == hostile) { selectedHostile = true; }
        }
        if (!selectedHostile || effects.GetShotCount() == shots) {
            std::printf("[arena-check] FAIL friendly targeting\n"); ++failures;
        }
    }
    // Every real weapon fires into the same large stationary hurtbox. This
    // tests the shared firing path, including scripts that spawn deployables.
    std::ofstream weaponReport("out/arena-weapons.csv");
    weaponReport << "index,name,shots,damage,spawned,visual_only\n";
    std::size_t firstBeam = weapons.size();
    for (std::size_t i = 0; i < weapons.size(); ++i) {
        if (!window.PumpEvents()) { return 1; }
        scene.Reset();
        vitals.invincible = true;
        if (!Equip(tables, playerData, weapons[i], player, program)) { ++failures; continue; }
        actor = scene.Spawn(0, 600, 350);
        if (actor == nullptr) { ++failures; continue; }
        actor->model.enemy.combat.health = 10000;
        actor->model.enemy.combat.maxHealth = 10000;
        actor->model.enemy.GetPart(0).radius = 130;
        actor->model.enemy.combat.behaviour = 7;
        const std::size_t initialShots = effects.GetShotCount();
        scene.PlayerMatrix(matrix);
        SetPlayerInput(player, false, true);
        for (int time = 0; time < 3000; time += 16) {
            actor->model.enemy.combat.variables[0] = 0;
            scene.Update(16, 0, 0, true);
        }
        // Short-range and deployable weapons need a target near their landing
        // point. Keep the same real scene update so spawned units also run.
        if (actor->model.enemy.combat.health == 10000 && !weapons[i].visualOnly) {
            actor->model.enemy.combat.x = 600;
            actor->model.enemy.combat.y = 580;
            actor->model.enemy.GetPart(0).radius = 40;
            for (int time = 0; time < 5000; time += 16) {
                actor->model.enemy.combat.variables[0] = 0;
                scene.Update(16, 0, 0, true);
            }
        }
        const float dealt = 10000 - actor->model.enemy.combat.health;
        weaponReport << i << ',' << weapons[i].name << ',' << effects.GetShotCount() - initialShots << ','
            << dealt << ',' << scene.spawned << ',' << weapons[i].visualOnly << '\n';
        if (!weapons[i].visualOnly && effects.GetShotCount() == initialShots) {
            std::printf("[arena-check] FAIL weapon %zu did not fire\n", i); ++failures;
        }
        if (!weapons[i].visualOnly && dealt <= 0) {
            std::printf("[arena-check] FAIL weapon %zu did not damage\n", i); ++failures;
        }
        if (player.weapon->gun.IsBeam() && firstBeam == weapons.size()) { firstBeam = i; }
    }
    if (firstBeam < weapons.size()) {
        const int steps[] = {8, 16, 32};
        float damage[3]{};
        for (int run = 0; run < 3; ++run) {
            scene.Reset();
            Equip(tables, playerData, weapons[firstBeam], player, program);
            actor = scene.Spawn(0, 600, 350);
            if (actor == nullptr) { return 1; }
            actor->model.enemy.combat.health = 10000;
            actor->model.enemy.combat.maxHealth = 10000;
            actor->model.enemy.GetPart(0).radius = 130;
            player.weapon->gun.SetShooting(true);
            player.weapon->gun.TakeCues();
            scene.PlayerMatrix(matrix);
            effects.SpawnProjectile(weapons[firstBeam].data.GetBulletRef(), 600, 600, 0, -90, 1, kPlayerCombatId, 0);
            for (int time = 0; time < 960; time += steps[run]) { effects.Update(player, matrix, 0, steps[run]); }
            damage[run] = 10000 - actor->model.enemy.combat.health;
        }
        std::printf("[arena-check] beam %zu at 8/16/32 ms: %.3f %.3f %.3f\n", firstBeam, damage[0], damage[1], damage[2]);
        const float minimum = std::min(damage[0], std::min(damage[1], damage[2]));
        const float maximum = std::max(damage[0], std::max(damage[1], damage[2]));
        if (minimum <= 0 || maximum - minimum > maximum * 0.02f) { ++failures; }
    } else { std::printf("[arena-check] FAIL no beam tested\n"); ++failures; }
    failures += unsupported;
    std::printf("[arena-check] catalog=%zu scripted=%u no_script=%u unknown_calls=%u failures=%u\n",
        catalog.size(), scripted, unused, unsupported, failures);
    if (failures != 0) { return 1; }
    return 0;
}
}

int RunArena(const std::string &bigDirectory, std::uint32_t enemyIndex,
    std::uint32_t weaponIndex, const std::string &screenshot, std::uint32_t advanceMs,
    bool fire, bool check, bool showCollisions) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    std::vector<EnemyTemplateData> catalog;
    std::vector<WeaponEntry> weapons;
    PlayerTemplateData playerData;
    PlayerVitals vitals;
    if (!LoadEnemyCatalog(toc, tables, catalog) || catalog.empty() ||
        !LoadWeaponCatalog(toc, tables, weapons) || !FindPlayerTemplate(toc, tables, playerData) ||
        !LoadInitialPlayerHealth(toc, tables, vitals.maximum)) { return 1; }
    CWindow window;
    if (!window.Open("Gun Bros - Arena", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    CShaderProgram program, markerProgram;
    if (!program.Load(kShaders, "ogles_vs_mvp_tex0", "ogles_ps_tex0") ||
        !markerProgram.Load(kShaders, "ogles_vs_mvp_constcolor", "ogles_ps_constcolor")) { return 1; }
    CMarkerBatch markers;
    if (!markers.Create(markerProgram)) { return 1; }
    PlayerModel player;
    player.vitals = &vitals;
    std::size_t weapon = weaponIndex % weapons.size();
    std::size_t entry = enemyIndex % catalog.size();
    if (!BuildPlayerBody(tables, playerData.moveSet, player) ||
        !Equip(tables, playerData, weapons[weapon], player, program)) { return 1; }
    WeaponEffects effects(toc, tables, program);
    CombatScene scene(tables, program, catalog, player, vitals, effects,
        playerData.gameScale);
    if (check) { return CheckArena(window, tables, program, catalog, weapons, playerData, player, vitals, effects, scene); }
    scene.Reset();
    scene.Spawn(entry, 600, 330);
    for (std::uint32_t time = 0; time < advanceMs; time += kStepMs) {
        scene.Update(kStepMs, 0, 0, fire);
    }
    std::printf("[arena] arrows: template, X: spawn, R: reset, G: invincible, 1-7/N/M: weapons\n"
        "[arena] WASD: move, mouse/left click: aim/fire, space: pause, period: 16 ms step, C: collision\n");
    bool paused = false;
    bool collisions = showCollisions;
    std::uint64_t previous = window.GetTicksMs();
    int accumulator = 0;
    std::uint64_t noticeUntil = 0;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    while (window.PumpEvents()) {
        bool step = false;
        bool reset = false;
        std::size_t nextEntry = entry;
        std::size_t nextWeapon = weapon;
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None; key = window.TakeKeyPress()) {
            if (key == KeyCode::Left) { nextEntry = (nextEntry + catalog.size() - 1) % catalog.size(); }
            else if (key == KeyCode::Right) { nextEntry = (nextEntry + 1) % catalog.size(); }
            else if (key == KeyCode::X) {
                if (scene.SpawnNearby(entry) == nullptr) { noticeUntil = window.GetTicksMs() + 2500; }
            }
            else if (key == KeyCode::R) { reset = true; }
            else if (key == KeyCode::G) { vitals.invincible = !vitals.invincible; }
            else if (key == KeyCode::Space) { paused = !paused; accumulator = 0; effects.SetPaused(paused); }
            else if (key == KeyCode::Period) { paused = true; step = true; effects.SetPaused(true); }
            else if (key == KeyCode::C) { collisions = !collisions; }
            else { nextWeapon = SelectWeaponKey(weapons, nextWeapon, key); }
        }
        if (nextEntry != entry || reset) {
            entry = nextEntry;
            scene.Reset();
            scene.Spawn(entry, 600, 330);
            accumulator = 0;
        }
        if (nextWeapon != weapon && !vitals.dead) {
            // Release the old gun's continuous effects; enemy attacks and
            // already launched projectiles keep their independent lifetimes.
            effects.RetireOwner(kPlayerCombatId);
            if (Equip(tables, playerData, weapons[nextWeapon], player, program)) { weapon = nextWeapon; }
        }
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        float mouseX = 0, mouseY = 0;
        if (screenshot.empty() && window.GetMousePosition(mouseX, mouseY) && !vitals.dead) {
            scene.facing = std::atan2(mouseX * kArenaWidth / width - scene.playerX,
                scene.playerY - mouseY * kArenaHeight / height) / kRadians;
        }
        float moveX = 0, moveY = 0;
        if (window.IsKeyDown(KeyCode::A)) { moveX -= 1; }
        if (window.IsKeyDown(KeyCode::D)) { moveX += 1; }
        if (window.IsKeyDown(KeyCode::W)) { moveY -= 1; }
        if (window.IsKeyDown(KeyCode::S)) { moveY += 1; }
        const std::uint64_t now = window.GetTicksMs();
        if (!paused && screenshot.empty()) { accumulator += static_cast<int>(std::min<std::uint64_t>(now - previous, 100)); }
        previous = now;
        if (step) { accumulator = kStepMs; }
        while (accumulator >= kStepMs) {
            scene.Update(kStepMs, moveX, moveY, fire || window.IsLeftMouseDown());
            accumulator -= kStepMs;
        }
        float projection[16];
        Matrix4dOrthoTopLeft(kArenaWidth, kArenaHeight, 4000, projection);
        glViewport(0, 0, width, height);
        glClearColor(0.055f, 0.075f, 0.09f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        markers.Begin();
        for (float x = 0; x <= kArenaWidth; x += 100) { markers.AddSegment(x, 140, x, kArenaHeight, 1); }
        for (float y = 200; y <= kArenaHeight; y += 100) { markers.AddSegment(0, y, kArenaWidth, y, 1); }
        markers.Draw(markerProgram, projection, 0.1f, 0.15f, 0.18f, 1);
        float playerMatrix[16], model[16], mvp[16];
        scene.PlayerMatrix(playerMatrix);
        effects.Draw(projection, nullptr, 1, WeaponDrawPass::BehindPlayer);
        // Actor meshes share a depth buffer; UI and billboards are layered after.
        glEnable(GL_DEPTH_TEST);
        for (auto &actor : scene.enemies) {
            if (actor->model.enemy.combat.removed) { continue; }
            scene.EnemyMatrix(*actor, model);
            Matrix4dMultiply(projection, model, mvp);
            DrawEnemyModel(actor->model, program, mvp);
        }
        Matrix4dMultiply(projection, playerMatrix, mvp);
        DrawPlayer(player, program, mvp);
        effects.Draw(projection, nullptr, 1, WeaponDrawPass::InFrontOfPlayer);
        glDisable(GL_DEPTH_TEST);
        // Every bar uses the same predicate as projectile damage filtering.
        for (auto &actor : scene.enemies) {
            CEnemy &enemy = actor->model.enemy;
            const EnemyCombat &state = enemy.combat;
            if (state.removed || state.dead) { continue; }
            float barY = state.y - std::max(60.0f, actor->data->gameScale * 0.55f);
            markers.Begin();
            markers.AddRect(state.x - 34, barY, 68, 8);
            markers.Draw(markerProgram, projection, 0.01f, 0.015f, 0.02f, 1);
            markers.Begin();
            float fraction = 0;
            if (state.maxHealth > 0) { fraction = std::clamp(state.health / state.maxHealth, 0.0f, 1.0f); }
            markers.AddRect(state.x - 32, barY + 2, 64 * fraction, 4);
            if (!state.enabled) { markers.Draw(markerProgram, projection, 0.5f, 0.5f, 0.5f, 1); }
            else if (enemy.CanReceiveProjectile(0, kPlayerCombatId)) { markers.Draw(markerProgram, projection, 0.95f, 0.23f, 0.2f, 1); }
            else { markers.Draw(markerProgram, projection, 0.22f, 0.95f, 0.5f, 1); }
            markers.Begin();
            char health[64];
            std::snprintf(health, sizeof(health), "%.0f/%.0f", state.health, state.maxHealth);
            Text(markers, state.x - 32, barY - 12, health, 1.2f);
            markers.Draw(markerProgram, projection, 0.82f, 0.88f, 0.9f, 1);
        }
        if (collisions) {
            markers.Begin();
            Circle(markers, scene.playerX, scene.playerY, kPlayerCollisionRadius);
            for (const auto &actor : scene.enemies) {
                const CEnemy &enemy = actor->model.enemy;
                const EnemyCombat &state = enemy.combat;
                if (!state.enabled || state.dead || state.removed) { continue; }
                if (state.collision.GetEdges().empty()) {
                    for (std::uint32_t part = 0; part < enemy.GetPartCount(); ++part) {
                        float x = 0, y = 0, radius = 0;
                        scene.EnemyCircle(*actor, part, x, y, radius);
                        if (radius > 0 && enemy.GetPart(part).visible) { Circle(markers, x, y, radius); }
                    }
                }
                const auto &vertices = state.collision.GetVertices();
                for (const auto &edge : state.collision.GetEdges()) {
                    if (!edge.enabled || edge.firstVertex >= vertices.size() || edge.secondVertex >= vertices.size()) { continue; }
                    const auto &a = vertices[edge.firstVertex], &b = vertices[edge.secondVertex];
                    const float c = std::cos(state.facing * kRadians), s = std::sin(state.facing * kRadians);
                    markers.AddSegment(state.x + (a.x * c - a.y * s) * state.scaleFactor,
                        state.y + (a.x * s + a.y * c) * state.scaleFactor,
                        state.x + (b.x * c - b.y * s) * state.scaleFactor,
                        state.y + (b.x * s + b.y * c) * state.scaleFactor, 2);
                }
            }
            markers.Draw(markerProgram, projection, 0.3f, 0.8f, 1, 0.8f);
        }
        markers.Begin();
        markers.AddRect(0, 0, kArenaWidth, 132);
        markers.Draw(markerProgram, projection, 0.025f, 0.04f, 0.05f, 0.97f);
        unsigned kills = scene.kills, hits = scene.hits;
        float damage = scene.damageDealt;
        unsigned deferred = 0;
        for (const auto &actor : scene.enemies) {
            const EnemyCombat &state = actor->model.enemy.combat;
            kills += state.deathCount; hits += state.hitCount;
            damage += state.totalDamage;
            deferred |= state.deferredMechanisms;
        }
        char line[256];
        std::snprintf(line, sizeof(line), "ARENA %zu/%zu - %s", entry, catalog.size() - 1, catalog[entry].owner.c_str());
        markers.Begin();
        Text(markers, 20, 16, line);
        const char *godLabel = "GOD OFF";
        if (vitals.invincible) { godLabel = "GOD ON"; }
        std::snprintf(line, sizeof(line), "HP %.1f/%.0f   INCOMING %.1f   LAST %.1f   G: %s", vitals.health, vitals.maximum,
            vitals.incomingDamage, vitals.lastDamage, godLabel);
        Text(markers, 20, 42, line, 1.6f);
        std::snprintf(line, sizeof(line), "ALIVE %zu   HITS %u   KILLS %u   DAMAGE %.1f   LAST %.1f", scene.AliveCount(), hits, kills, damage, scene.lastDamage);
        Text(markers, 20, 62, line, 1.6f);
        Text(markers, 20, 83, WeaponSelectionLabel(weapons, weapon).substr(0, 95), 1.4f);
        Text(markers, 20, 106, "ARROWS ENEMY  X SPAWN  R RESET  1-7/N/M WEAPON  WASD MOVE  SPACE PAUSE  . STEP  C COLLISION", 1.3f);
        if (paused) { Text(markers, 985, 18, "PAUSED", 2); }
        if (!scene.enemies.empty()) {
            const EnemyCombat &state = scene.enemies.front()->model.enemy.combat;
            std::snprintf(line, sizeof(line), "FILTER %d  TARGET TYPE %d", state.variables[16], state.targetType);
            Text(markers, 20, 145, line, 1.4f);
        }
        if ((deferred & 3) != 0) { Text(markers, 20, 167, "BOSS / LEVEL MECHANISMS DEFERRED", 1.4f); }
        if (now < noticeUntil) { Text(markers, 380, 220, "NO FREE SPAWN POSITION", 2); }
        if (!catalog[entry].script.IsPresent()) { Text(markers, 380, 180, "UNUSED - NO SCRIPT", 3); }
        else if (catalog[entry].gameScale == 0) { Text(markers, 380, 180, "NO VISIBLE MODEL", 3); }
        if (vitals.dead) { Text(markers, 400, 450, "PLAYER DEAD - R RESET", 3); }
        markers.Draw(markerProgram, projection, 0.84f, 0.91f, 0.94f, 1);
        markers.Begin();
        markers.AddRect(20, 124, 260 * std::clamp(vitals.health / vitals.maximum, 0.0f, 1.0f), 4);
        markers.Draw(markerProgram, projection, 0.2f, 0.7f, 1, 1);
        window.SetTitle("Arena | " + catalog[entry].owner + " | " + weapons[weapon].name);
        if (!screenshot.empty()) {
            if (window.SaveFrame(screenshot)) { return 0; }
            return 1;
        }
        window.Present();
    }
    return 0;
}
