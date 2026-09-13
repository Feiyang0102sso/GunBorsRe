/** Exercise knockback sound and AI weapon swaps through production scene updates. */
#include "gun_bros_re/gameplay/MapWorldInternal.h"
#include "tests/Checks.h"
#include "gun_bros_re/gameplay/CombatGeometry.h"
using namespace MapDetail;

namespace {
/** Reproduce player input entering an actual BIG enemy's body circle. */
unsigned CheckEnemyMovement(CombatScene &scene, PlayerModel &player, PlayerVitals &vitals) {
    scene.Reset();
    CombatEnemy *actor = scene.Spawn(0, 650, 550);
    if (actor == nullptr) { return 1; }
    CEnemy &enemy = actor->model.enemy;
    // Freeze AI to isolate movement resolution, without disabling collision.
    enemy.stun.SetStunned(10000, 100, 0);
    float centerX = enemy.combat.x, centerY = enemy.combat.y, radius = 0;
    EnemyCollisionCircle(enemy, actor->data->gameScale, 0, centerX, centerY, radius);
    // CBrother constructor :139098 uses 22, independently of the wall radius.
    scene.playerX = centerX - (22 + enemy.GetPart(0).radius * 0.8f - 1);
    scene.playerY = centerY;
    const float startX = scene.playerX;
    scene.Update(16, 1, 0, false);
    const float inward = scene.playerX - startX;
    scene.Update(16, -1, 0, false);
    const float outward = scene.playerX - startX;
    const bool blocked = std::abs(inward) < 0.001f;
    const bool escaped = outward < -1;
    unsigned failures = 0;
    if (!blocked || !escaped) { ++failures; }
    std::printf("[actor-body-check] blocked=%d escaped=%d inward=%.3f outward=%.3f\n",
        blocked, escaped, inward, outward);
    // Red damage feedback alone must not grant body immunity.
    scene.playerX = startX;
    CombatHit ranged;
    ranged.ownerType = 1;
    ranged.damage = 1;
    scene.ApplyHit(kPlayerCombatId, ranged);
    scene.Update(16, 1, 0, false);
    if (std::abs(scene.playerX - startX) > 0.001f || player.weapon->brother.CanPassEnemies()) { ++failures; }
    // CollisionMode and live membership, not targeting or the debug HP switch.
    enemy.combat.variables[16] = 1;
    scene.Update(16, 1, 0, false);
    if (scene.playerX <= startX + 1) { ++failures; }
    enemy.combat.variables[16] = 0;
    scene.playerX = startX;
    vitals.invincible = true;
    scene.Update(16, 1, 0, false);
    if (std::abs(scene.playerX - startX) > 0.001f) { ++failures; }
    vitals.invincible = false;
    player.weapon->brother.StartShield({}, 1000);
    scene.Update(16, 1, 0, false);
    if (std::abs(scene.playerX - startX) > 0.001f || player.weapon->brother.CanPassEnemies()) { ++failures; }
    enemy.combat.health = 0;
    scene.Update(16, 1, 0, false);
    if (scene.playerX <= startX + 1) { ++failures; }
    scene.Reset();
    return failures;
}

/** Traverse several frozen BIG enemies using the immunity from a real melee hit. */
unsigned CheckMeleeEscape(CombatScene &scene, PlayerModel &player, std::size_t entry) {
    unsigned failures = 0;
    CBrother &brother = player.weapon->brother;
    // Keep the original PLAYER script/timer; only isolate the enemy AI motion.
    scene.enemies.clear();
    const float startX = 400, startY = 550;
    for (int index = 0; index < 3; ++index) {
        CombatEnemy *actor = scene.Spawn(entry, startX + 25 + index * 55, startY);
        if (actor == nullptr) { return 1; }
        actor->model.enemy.stun.SetStunned(10000, 100, 0);
    }
    scene.playerX = startX;
    scene.playerY = startY;
    if (!brother.CanPassEnemies()) { ++failures; }
    for (int frame = 0; frame < 50; ++frame) { scene.Update(16, 1, 0, false); }
    const float travel = scene.playerX - startX;
    if (travel < 150 || !brother.CanPassEnemies()) { ++failures; }
    // Stop outside the enemies, so no new contact can restart immunity.
    scene.playerX = 300;
    scene.playerY = 750;
    for (int frame = 0; frame < 150 && brother.CanPassEnemies(); ++frame) { scene.Update(16, 0, 0, false); }
    if (brother.CanPassEnemies() || brother.IsImmunityHidden()) { ++failures; }
    CombatEnemy &actor = *scene.enemies.front();
    float centerX = actor.model.enemy.combat.x, centerY = actor.model.enemy.combat.y, radius = 0;
    EnemyCollisionCircle(actor.model.enemy, actor.data->gameScale, 0, centerX, centerY, radius);
    scene.playerX = centerX - (brother.GetRadius() + actor.model.enemy.GetPart(0).radius * 0.8f - 1);
    scene.playerY = centerY;
    const float beforeBlocked = scene.playerX;
    scene.Update(16, 1, 0, false);
    const bool blockedAgain = std::abs(scene.playerX - beforeBlocked) < 0.001f;
    if (!blockedAgain) { ++failures; }
    std::printf("[actor-body-check] melee-escape travel=%.3f blocked-again=%d failures=%u\n",
        travel, blockedAgain, failures);
    return failures;
}

/** Lock down the original's nonstandard CircleCircle numerical branches. */
unsigned CheckOriginalCircleCircle() {
    using CombatGeometry::CircleCircle;
    float fraction = -1;
    unsigned failures = 0;
    if (!CircleCircle({0, 0}, {1, 0}, 1, {1, 0}, {1, 0}, 1, fraction) || fraction != 0) { ++failures; }
    if (!CircleCircle({0, 0}, {1, 0}, 0.25f, {1, 0}, {1, 0}, 0.25f, fraction) ||
        std::abs(fraction - 0.5f) > 0.00001f) { ++failures; }
    // Discriminant > 1 must remain rejected, despite a standard sweep hitting.
    if (CircleCircle({0, 0}, {4, 0}, 1, {3, 0}, {3, 0}, 1, fraction)) { ++failures; }
    if (CircleCircle({0, 0}, {1, 0}, 0.25f, {1, 0}, {2, 0}, 0.25f, fraction)) { ++failures; }
    std::printf("[actor-body-check] original-circle failures=%u\n", failures);
    return failures;
}
}

int RunActorFeedbackCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.InitAuto(bigDirectory) || !toc.Bind()) { return 1; }
    CWindow window;
    if (!window.Open("Actor feedback check", 640, 480)) { return 1; }
    CShaderProgram program;
    if (!program.Load(Paths::Shaders(), "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    PackTables tables(toc);
    PlayerTemplateData playerData;
    std::vector<WeaponEntry> weapons;
    std::vector<EnemyTemplateData> enemies;
    if (!FindPlayerTemplate(toc, tables, playerData) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadEnemyCatalog(toc, tables, enemies)) { return 1; }
    const WeaponEntry *pistol = nullptr;
    const WeaponEntry *rifle = nullptr;
    for (const WeaponEntry &weapon : weapons) {
        if (weapon.packHash == toc.GetPack(toc.GetCorePackIndex())->GetPackHash() && weapon.ordinal == 0) { pistol = &weapon; }
        if (weapon.packHash == CStringToKey("pack5") && weapon.ordinal == 4) { rifle = &weapon; }
    }
    if (!pistol || !rifle) { return 1; }
    PlayerVitals vitals;
    vitals.maximum = 100;
    vitals.invincible = false;
    vitals.Reset();
    PlayerModel player;
    player.vitals = &vitals;
    if (!BuildPlayerBody(tables, playerData.moveSet, player) ||
        !EquipPlayerWeapon(tables, playerData.script, pistol->data, "feedback player", player) ||
        !CreatePlayerBuffers(player, program)) { return 1; }
    WeaponEffects effects(toc, tables, program);
    CombatScene scene(tables, program, enemies, player, vitals, effects, playerData.gameScale);
    unsigned failures = 0;
    failures += CheckOriginalCircleCircle();
    failures += CheckEnemyMovement(scene, player, vitals);
    // A barrel's native 10 can apply zero damage and still knock the player back.
    CombatHit blast;
    blast.ownerType = 1;
    blast.x = scene.playerX - 20;
    blast.y = scene.playerY;
    const auto beforeBlast = effects.GetSoundCueCount();
    scene.Splash(blast, 100, 360, 300, 100);
    const unsigned knockbackState = player.weapon->brother.GetStateId();
    for (int elapsed = 0; elapsed < 768; elapsed += 16) { scene.Update(16, 0, 0, false); }
    const auto blastSounds = effects.GetSoundCueCount() - beforeBlast;
    if (knockbackState != 10 || blastSounds == 0 || vitals.health != 100) { ++failures; }
    std::printf("[actor-feedback-check] barrel state=%u sounds=%zu hp=%.0f\n", knockbackState, blastSounds, vitals.health);

    scene.Reset();
    // Ordinary ranged damage must not acquire the knockback vocal animation.
    CombatHit ranged;
    ranged.ownerType = 1;
    ranged.projectile = 123;
    ranged.damage = 1;
    const auto beforeRanged = effects.GetSoundCueCount();
    scene.ApplyHit(kPlayerCombatId, ranged);
    for (int elapsed = 0; elapsed < 768; elapsed += 16) { scene.Update(16, 0, 0, false); }
    const auto rangedSounds = effects.GetSoundCueCount() - beforeRanged;
    if (rangedSounds != 0 || vitals.health != 99) { ++failures; }
    std::printf("[actor-feedback-check] ranged sounds=%zu hp=%.0f\n", rangedSounds, vitals.health);

    scene.Reset();
    CBrotherAI brother;
    brother.vitals.maximum = 100;
    brother.vitals.invincible = false;
    brother.Reset(scene.playerX, scene.playerY, 0);
    PlayerModel partner;
    partner.human = false;
    partner.vitals = &brother.vitals;
    if (!BuildPlayerBody(tables, playerData.moveSet, partner) ||
        !EquipPlayerWeapon(tables, playerData.script, pistol->data, "feedback brother", partner) ||
        !CreatePlayerBuffers(partner, program)) { return 1; }
    scene.SetBrother(&partner, &brother);
    scene.SetBrotherWeapons(playerData.script, pistol->data, rifle->data);
    // Every real pickup must ignore the AI partner, then remain collectable by the player.
    CProfileManager pickupProfile;
    PickupScene pickupScene(toc, tables, program, &pickupProfile);
    std::vector<PickupEntry> pickupCatalog;
    if (!pickupScene.Init() || !LoadPickupCatalog(toc, tables, pickupCatalog)) { return 1; }
    unsigned brotherCollections = 0;
    unsigned playerCollections = 0;
    for (const PickupEntry &entry : pickupCatalog) {
        scene.playerX = 600;
        scene.playerY = 650;
        scene.Update(16, 0, 0, false);
        brother.Reset(200, 200, 0);
        pickupScene.Reset();
        if (!pickupScene.Spawn(entry.ref, brother.x, brother.y)) { return 1; }
        pickupScene.Update(16, scene, effects);
        brotherCollections += pickupScene.collected;
        if (pickupScene.GetCount() != 1 || pickupScene.collected != 0) { ++failures; }
        scene.playerX = brother.x;
        scene.playerY = brother.y;
        scene.Update(16, 0, 0, false);
        const unsigned collectedBefore = pickupScene.collected;
        pickupScene.Update(16, scene, effects);
        playerCollections += pickupScene.collected - collectedBefore;
        if (pickupScene.GetCount() != 0 || pickupScene.collected - collectedBefore != 1) { ++failures; }
    }
    std::printf("[actor-feedback-check] pickups=%zu brother-collected=%u player-collected=%u\n",
        pickupCatalog.size(), brotherCollections, playerCollections);
    scene.Reset();
    CBrother *originalHost = &partner.weapon->brother;
    bool swapAnimation = false;
    bool swapped = false;
    const auto beforeSwap = effects.GetSoundCueCount();
    for (int elapsed = 0; elapsed < 240000; elapsed += 16) {
        scene.Update(16, 0, 0, false);
        // BIG states 4/5 lower the old gun and raise the new one.
        const unsigned state = partner.weapon->brother.GetStateId();
        if (state == 4 || state == 5) { swapAnimation = true; }
        if (scene.GetBrotherWeaponSlot() == 1) { swapped = true; }
        if (swapped && state != 4 && state != 5) { break; }
    }
    const auto swapSounds = effects.GetSoundCueCount() - beforeSwap;
    if (!swapAnimation || !swapped || swapSounds == 0 || originalHost != &partner.weapon->brother) { ++failures; }
    std::printf("[actor-feedback-check] AI swap animation=%d swapped=%d sounds=%zu host-preserved=%d\n",
        swapAnimation, swapped, swapSounds, originalHost == &partner.weapon->brother);
    bool swappedBack = false;
    for (int elapsed = 0; elapsed < 240000; elapsed += 16) {
        scene.Update(16, 0, 0, false);
        if (scene.GetBrotherWeaponSlot() == 0) { swappedBack = true; break; }
    }
    if (!swappedBack || originalHost != &partner.weapon->brother) { ++failures; }
    scene.Reset();
    if (scene.GetBrotherWeaponSlot() != 0 || partner.uiOtherWeapon != nullptr) { ++failures; }
    std::printf("[actor-feedback-check] AI swap-back=%d reset-slot=%u\n", swappedBack, scene.GetBrotherWeaponSlot());

    // The same pulse must follow the same curve in both movement hosts.
    for (int durationMs : {100, 200}) {
        scene.Reset();
        brother.Reset(scene.playerX, scene.playerY, 0);
        blast.x = scene.playerX - 20;
        blast.y = scene.playerY;
        const float startX = scene.playerX;
        scene.Splash(blast, 100, 360, 300, durationMs);
        for (int elapsed = 0; elapsed < durationMs + 64; elapsed += 16) {
            scene.Update(16, 0, 0, false);
            if (std::hypot(scene.playerX - brother.x, scene.playerY - brother.y) > 0.001f) { ++failures; }
        }
        const float travel = scene.playerX - startX;
        if (travel <= 0 || travel >= 300 * durationMs * 0.0005f) { ++failures; }
        std::printf("[actor-feedback-check] force-hosts ms=%d player=%.3f brother=%.3f\n",
            durationMs, travel, brother.x - startX);
    }

    scene.SetBrother(nullptr, nullptr);
    bool meleeChecked = false;
    for (std::size_t index = 0; index < enemies.size(); ++index) {
        scene.Reset();
        CombatEnemy *enemy = scene.Spawn(index, scene.playerX + 8, scene.playerY);
        if (!enemy) { return 1; }
        const auto beforeMelee = effects.GetSoundCueCount();
        bool knockedBack = false;
        bool hadContact = false;
        // Let the actual enemy script enable its attack; no injected force/damage table.
        for (int elapsed = 0; elapsed < 2048; elapsed += 16) {
            scene.Update(16, 0, 0, false);
            if (enemy->contactTimer > 0) { hadContact = true; }
            if (player.weapon->brother.GetStateId() == 10) { knockedBack = true; }
            if (knockedBack) { break; }
        }
        if (!hadContact) { continue; }
        if (*player.weapon->brother.VariableResolver(3) != 1400) { ++failures; }
        // Isolate one real contact so another attack cannot extend the measured travel.
        const int forceMs = enemy->contactTimer;
        enemy->model.enemy.combat.enabled = false;
        const float startX = scene.playerX;
        const float startY = scene.playerY;
        float firstStep = 0;
        float lateStep = 0;
        for (int elapsed = 16; elapsed <= forceMs + 16; elapsed += 16) {
            const float previousX = scene.playerX;
            const float previousY = scene.playerY;
            scene.Update(16, 0, 0, false);
            if (elapsed < forceMs && *player.weapon->brother.VariableResolver(3) != 1400) { ++failures; }
            const float step = std::hypot(scene.playerX - previousX, scene.playerY - previousY);
            if (elapsed == 16) { firstStep = step; }
            if (elapsed <= forceMs * 3 / 4) { lateStep = step; }
        }
        const float travel = std::hypot(scene.playerX - startX, scene.playerY - startY);
        // CBrother::Update :135201 slows to zero instead of stopping abruptly.
        if (forceMs <= 32 || firstStep <= 0 || lateStep >= firstStep * 0.5f) { ++failures; }
        std::printf("[actor-feedback-check] melee-motion ms=%d first=%.3f late=%.3f travel=%.3f\n",
            forceMs, firstStep, lateStep, travel);
        failures += CheckMeleeEscape(scene, player, index);
        for (int elapsed = 0; elapsed < 768; elapsed += 16) { scene.Update(16, 0, 0, false); }
        const auto meleeSounds = effects.GetSoundCueCount() - beforeMelee;
        if (!knockedBack || meleeSounds == 0) { ++failures; }
        std::printf("[actor-feedback-check] melee enemy=%08x:%u knockback=%d sounds=%zu hits=%u\n",
            enemies[index].packHash, enemies[index].ordinal, knockedBack, meleeSounds, vitals.hits);
        meleeChecked = true;
        break;
    }
    if (!meleeChecked) { ++failures; }
    // Check the immunity branch against a real map edge, with the production
    // wall radius. Geometry remains sourced from BIG, not an injected shape.
    scene.Reset();
    LoadedMap wallMap;
    if (!LoadMap(toc, toc.GetPackIndexFromName("pack2"), 7, wallMap)) { return 1; }
    BuildCollisionScene(wallMap);
    scene.SetMap(wallMap.map, wallMap.collisionScene, wallMap.weaponCollision, 1, kPlayerCollisionRadius);
    const MapRectangle bounds = wallMap.map.GetCameraExtent();
    bool wallChecked = false;
    const auto &vertices = wallMap.collisionScene.GetVertices();
    for (const CollisionEdge &edge : wallMap.collisionScene.GetEdges()) {
        if (!edge.enabled) { continue; }
        const CollisionPoint &a = vertices[edge.firstVertex], &b = vertices[edge.secondVertex];
        const float length = std::hypot(b.x - a.x, b.y - a.y);
        if (length < 50) { continue; }
        const float nx = -(b.y - a.y) / length, ny = (b.x - a.x) / length;
        const float centerX = (a.x + b.x) * 0.5f, centerY = (a.y + b.y) * 0.5f;
        const float startX = centerX + nx * (kPlayerCollisionRadius + 1);
        const float startY = centerY + ny * (kPlayerCollisionRadius + 1);
        if (startX < bounds.x + 30 || startX > bounds.x + bounds.width - 30 ||
            startY < bounds.y + 30 || startY > bounds.y + bounds.height - 30) { continue; }
        if (!player.weapon->brother.BeginKnockback(1)) { ++failures; break; }
        scene.Update(1, 0, 0, false);
        // Let the authored recovery animation return control without consuming the window.
        for (int frame = 0; frame < 30 && !player.weapon->brother.CanMove(); ++frame) { scene.Update(16, 0, 0, false); }
        scene.playerX = startX;
        scene.playerY = startY;
        for (int frame = 0; frame < 10; ++frame) { scene.Update(16, -nx, -ny, false); }
        const float separation = (scene.playerX - centerX) * nx + (scene.playerY - centerY) * ny;
        if (separation < kPlayerCollisionRadius - 0.01f ||
            separation > kPlayerCollisionRadius + 0.1f || !player.weapon->brother.CanPassEnemies()) { ++failures; }
        std::printf("[actor-body-check] immune-wall separation=%.3f immune=%d\n",
            separation, player.weapon->brother.CanPassEnemies());
        wallChecked = true;
        break;
    }
    if (!wallChecked) { ++failures; }
    std::printf("[actor-feedback-check] failures=%u\n", failures);
    return failures != 0;
}
