#include "TestOutput.h"
#include "checks/ArenaChecks.h"
#include "gun_bros_viewer/scenes/ArenaPreviewInternal.h"
#include "gun_bros_viewer/scenes/ArenaTools.h"
#include "gun_bros_re/data/StoreCatalog.h"
namespace ArenaDetail {
int CheckArena(ArenaScene &ready);
}

int RunArenaCheck(const std::string &bigDirectory, std::uint32_t enemyIndex, std::uint32_t weaponIndex) {
    return RunArena(bigDirectory, enemyIndex, weaponIndex, "", 0, false, false, -1, ArenaDetail::CheckArena);
}

namespace ArenaDetail {
/** Exercise actual archive scripts, then write a per-entry audit for inspection. */
int CheckArena(ArenaScene &ready) {
    auto &window = ready.window;
    auto &toc = ready.toc;
    auto &tables = ready.tables;
    const auto &program = ready.program;
    const auto &catalog = ready.catalog;
    const auto &weapons = ready.weapons;
    const auto &playerData = ready.playerData;
    auto &player = ready.player;
    auto &vitals = ready.vitals;
    auto &effects = ready.effects;
    auto &scene = ready.scene;
    std::filesystem::create_directories(TestOutput::Path(""));
    std::ofstream report(TestOutput::Path("arena-check.csv"));
    report << "index,owner,script,health,allegiance,target_type,parts,shots,travel,hits,unknown_natives,death_count,incoming_damage,deferred\n";
    unsigned failures = 0;
    unsigned scripted = 0;
    unsigned unused = 0;
    unsigned unsupported = 0;
    // The viewer uses BIG names, real throw animations and nonfatal damage.
    if (ReadGameString(toc, catalog[0].name).empty()) { ++failures; }
    std::array<PowerupEntry, 3> grenades;
    if (!LoadArenaGrenades(toc, tables, grenades)) { return 1; }
    for (const auto &grenade : grenades) {
        scene.Reset();
        scene.Update(kStepMs, 0, 0, false);
        const auto shotsBefore = effects.GetShotCount();
        const bool requested = ThrowArenaGrenade(player.weapon->brother, grenade);
        const bool replacedPending = ThrowArenaGrenade(player.weapon->brother, grenades[0]);
        unsigned thrown = 0;
        for (int tick = 0; tick < 180; ++tick) {
            scene.Update(kStepMs, 0, 0, false);
            thrown += player.weapon->brother.TakeThrownGrenades(0);
        }
        const auto shots = effects.GetShotCount() - shotsBefore;
        std::printf("[arena-check] grenade=%s requested=%d thrown=%u shots=%zu\n",
            grenade.name.c_str(), requested, thrown, shots);
        if (!requested || replacedPending || thrown != 1 || shots == 0) { ++failures; }
    }
    scene.Reset();
    scene.Update(kStepMs, 0, 0, false);
    vitals.invincible = false;
    // PLAYER pack0_core: event 0x0604 is authored in stun state 13
    // (@0x3BE), which transitions to recovery state 14. Idle ignores it.
    player.weapon->brother.Stun(1000);
    player.weapon->brother.ReceiveDamage(1);
    const int hurtState = player.weapon->brother.GetStateId();
    scene.Reset();
    scene.Update(kStepMs, 0, 0, false);
    player.weapon->brother.Stun(1000);
    const int stunnedState = player.weapon->brother.GetStateId();
    vitals.unlimitedHealth = true;
    const float lethalDamage = vitals.maximum * 100;
    const HitResult unlimitedResult = player.weapon->brother.ReceiveDamage(lethalDamage);
    std::printf("[arena-check] unlimited result=%d hp=%.1f/%.1f dead=%d hits=%u incoming=%.1f flash=%.1f states=%d/%d/%d\n",
        static_cast<int>(unlimitedResult), vitals.health, vitals.maximum, vitals.dead,
        vitals.hits, vitals.incomingDamage, vitals.flash, stunnedState, hurtState, player.weapon->brother.GetStateId());
    if (unlimitedResult != HitResult::Hit || vitals.health != vitals.maximum || vitals.dead ||
        vitals.hits != 1 || vitals.incomingDamage != lethalDamage || vitals.flash != 1 ||
        hurtState == stunnedState || player.weapon->brother.GetStateId() != hurtState) {
        std::printf("[arena-check] FAIL unlimited health damage feedback\n");
        ++failures;
    }
    if (!Equip(tables, playerData, weapons[1], player, program) || !vitals.unlimitedHealth) { ++failures; }
    scene.Reset();
    if (!vitals.unlimitedHealth || vitals.dead || vitals.incomingDamage != 0) { ++failures; }
    vitals.unlimitedHealth = false;
    vitals.invincible = true;
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
    std::ofstream weaponReport(TestOutput::Path("arena-weapons.csv"));
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
    // Use a known three-slot archive outfit, independently checked in the
    // original scripts: defense 4+8+2, attack 0+0+5, speed -1-3-3.
    std::vector<ArmorEntry> armorCatalog;
    if (!LoadArmorCatalog(toc, tables, armorCatalog) || armorCatalog.size() <= 11) {
        return 1;
    }
    scene.Reset();
    const std::size_t outfit[] = {7, 11, 4};
    for (std::size_t index : outfit) {
        if (!EquipPlayerArmor(tables, armorCatalog[index].data, program, player)) {
            return 1;
        }
    }
    if (std::abs(PlayerArmorMultiplier(player, 0) - 1.14f) > 0.0001f ||
        std::abs(PlayerArmorMultiplier(player, 1) - 1.05f) > 0.0001f ||
        std::abs(PlayerArmorMultiplier(player, 2) - 0.93f) > 0.0001f) {
        ++failures;
    }
    const float originalMaximum = vitals.maximum;
    vitals.maximum = 100;
    vitals.Reset();
    vitals.invincible = false;
    CombatHit armorHit;
    armorHit.ownerType = 1;
    armorHit.damage = 10;
    scene.ApplyHit(kPlayerCombatId, armorHit);
    const float armoredIncoming = vitals.lastDamage;
    if (std::abs(vitals.health - 91.4f) > 0.001f) {
        ++failures;
    }
    actor = scene.Spawn(0, 600, 300);
    if (actor == nullptr) {
        return 1;
    }
    actor->model.enemy.combat.health = 100;
    actor->model.enemy.combat.maxHealth = 100;
    armorHit.owner = kPlayerCombatId;
    armorHit.ownerType = 0;
    scene.ApplyHit(actor->model.enemy.combat.id, armorHit);
    const float armoredOutgoing = 100 - actor->model.enemy.combat.health;
    if (std::abs(armoredOutgoing - 10.5f) > 0.001f) {
        ++failures;
    }
    scene.Reset();
    const float beforeMove = scene.playerX;
    scene.Update(100, 1, 0, false);
    const float armoredTravel = scene.playerX - beforeMove;
    if (std::abs(armoredTravel - 20.46f) > 0.001f) {
        ++failures;
    }
    if (!Equip(tables, playerData, weapons[0], player, program) ||
        std::abs(PlayerArmorMultiplier(player, 0) - 1.14f) > 0.0001f ||
        std::abs(PlayerArmorMultiplier(player, 1) - 1.05f) > 0.0001f) {
        ++failures;
    }
    ClearPlayerArmor(player);
    if (std::abs(PlayerArmorMultiplier(player, 0) - 1.0f) > 0.0001f) {
        ++failures;
    }
    vitals.maximum = originalMaximum;
    vitals.Reset();
    std::printf("[armor-combat] incoming=%.3f expected=8.600 outgoing=%.3f expected=10.500 travel=%.3f expected=20.460\n",
        armoredIncoming, armoredOutgoing, armoredTravel);
    failures += unsupported;
    // Independent numbers exercise the original native units: 5 * 1.5 * 2 HP,
    // 100 units/s * 2 speed for 100 ms, and 1.5 * 2 outgoing damage.
    scene.Reset();
    CMap multiplierMap;
    CLevel::Template multiplierTemplate;
    CLevel multiplierLevel;
    multiplierLevel.Bind(multiplierTemplate, multiplierMap);
    const std::int16_t localHealth[] = {0, 1, 384};
    const std::int16_t globalHealth[] = {1, 512};
    const std::int16_t localDamage[] = {0, 0, 384};
    const std::int16_t globalDamage[] = {0, 512};
    const std::int16_t globalSpeed[] = {4, 512};
    multiplierLevel.FunctionResolver(54, localHealth, 3);
    multiplierLevel.FunctionResolver(55, globalHealth, 2);
    multiplierLevel.FunctionResolver(54, localDamage, 3);
    multiplierLevel.FunctionResolver(55, globalDamage, 2);
    multiplierLevel.FunctionResolver(55, globalSpeed, 2);
    scene.SetLevel(&multiplierLevel);
    actor = scene.Spawn(0, 600, 300);
    if (actor == nullptr) { return 1; }
    CEnemy &scaledEnemy = actor->model.enemy;
    const float scaledHealth = scaledEnemy.combat.health;
    const int scriptHealth = scaledEnemy.FunctionResolver(51, nullptr, 0);
    scaledEnemy.combat.variables[0] = 100;
    scaledEnemy.combat.triggerDistance = 0;
    const std::int16_t follow[] = {0, 0};
    scaledEnemy.FunctionResolver(0, follow, 2);
    scene.Update(100, 0, 0, false);
    const float scaledTravel = std::hypot(scaledEnemy.combat.x - 600, scaledEnemy.combat.y - 300);
    const float scaledDamage = scene.GetDamageMultiplier(scaledEnemy.combat.id);
    if (std::abs(scaledHealth - 15) > 0.001f || scriptHealth != 5 ||
        std::abs(scaledTravel - 20) > 0.001f || std::abs(scaledDamage - 3) > 0.001f) { ++failures; }
    std::printf("[level-multipliers] health=%.3f script=%d travel=%.3f damage=%.3f\n",
        scaledHealth, scriptHealth, scaledTravel, scaledDamage);
    scene.Reset();
    scene.SetLevel(nullptr);
    // Render the same original mesh twice: plain and native-29 hit flash.
    // This catches confusing an enemy's white overlay with a gun's red heat.
    CombatEnemy *plain = scene.Spawn(0, 400, 450);
    CombatEnemy *flashed = scene.Spawn(0, 800, 450);
    if (plain == nullptr || flashed == nullptr) { return 1; }
    plain->model.enemy.Update(1000);
    flashed->model.enemy.Update(1000);
    plain->model.enemy.combat.x = 400;
    plain->model.enemy.combat.y = 450;
    flashed->model.enemy.combat.x = 800;
    flashed->model.enemy.combat.y = 450;
    flashed->model.enemy.FunctionResolver(29, nullptr, 0);
    int renderWidth = 0, renderHeight = 0;
    window.GetDrawableSize(renderWidth, renderHeight);
    glViewport(0, 0, renderWidth, renderHeight);
    glClearColor(0.04f, 0.05f, 0.07f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    float renderProjection[16];
    Matrix4dOrthoTopLeft(kArenaWidth, kArenaHeight, 4000, renderProjection);
    for (const auto &actor : scene.enemies) {
        float world[16], model[16];
        scene.EnemyMatrix(*actor, world);
        Matrix4dMultiply(renderProjection, world, model);
        DrawEnemyModel(actor->model, program, model);
    }
    glDisable(GL_DEPTH_TEST);
    if (glGetError() != 0 || !GB_SAVE_FRAME(window, TestOutput::Path("enemy-hit-flash-check.png"))) { ++failures; }
    std::printf("[arena-check] catalog=%zu scripted=%u no_script=%u unknown_calls=%u failures=%u\n",
        catalog.size(), scripted, unused, unsupported, failures);
    if (failures != 0) { return 1; }
    return 0;
}
}
