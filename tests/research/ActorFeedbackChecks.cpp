/** Exercise knockback sound and AI weapon swaps through production scene updates. */
#include "gun_bros_re/gameplay/ZMapWorldInternal.h"
#include "tests/Checks.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
using namespace MapDetail;

namespace {
/** Reproduce player input entering an actual BIG enemy's body circle. */
unsigned CheckEnemyMovement(CLevel &scene, ZPlayerModel &player, ZPlayerVitals &vitals) {
    scene.Reset();
    ZCombatEnemy *actor = scene.Spawn(0, 650, 550);
    if (actor == nullptr) { return 1; }
    CEnemy &enemy = actor->model.enemy;
    // Freeze AI to isolate movement resolution, without disabling collision.
    enemy.stun.SetStunned(10000, 100, 0);
    float centerX = enemy.combat.x, centerY = enemy.combat.y, radius = 0;
    EnemyCollisionCircle(enemy, actor->data->gameScale, 0, centerX, centerY, radius);
    // CBrother constructor :139098 uses 22, independently of the wall radius.
    scene.GetPlayer().x = centerX - (22 + enemy.GetPart(0).radius * 0.8f - 1);
    scene.GetPlayer().y = centerY;
    const float startX = scene.GetPlayer().x;
    scene.Update(16, 1, 0, false);
    const float inward = scene.GetPlayer().x - startX;
    scene.Update(16, -1, 0, false);
    const float outward = scene.GetPlayer().x - startX;
    const bool blocked = std::abs(inward) < 0.001f;
    const bool escaped = outward < -1;
    unsigned failures = 0;
    if (!blocked || !escaped) { ++failures; }
    std::printf("[actor-body-check] blocked=%d escaped=%d inward=%.3f outward=%.3f\n",
        blocked, escaped, inward, outward);
    // Red damage feedback alone must not grant body immunity.
    scene.GetPlayer().x = startX;
    ZCombatHit ranged;
    ranged.ownerType = 1;
    ranged.damage = 1;
    scene.ApplyHit(kPlayerCombatId, ranged);
    scene.Update(16, 1, 0, false);
    if (std::abs(scene.GetPlayer().x - startX) > 0.001f || player.weapon->brother.CanPassEnemies()) { ++failures; }
    // CollisionMode and live membership, not targeting or the debug HP switch.
    enemy.combat.variables[16] = 1;
    scene.Update(16, 1, 0, false);
    if (scene.GetPlayer().x <= startX + 1) { ++failures; }
    enemy.combat.variables[16] = 0;
    scene.GetPlayer().x = startX;
    vitals.invincible = true;
    scene.Update(16, 1, 0, false);
    if (std::abs(scene.GetPlayer().x - startX) > 0.001f) { ++failures; }
    vitals.invincible = false;
    player.weapon->brother.StartShield({}, 1000);
    scene.Update(16, 1, 0, false);
    if (std::abs(scene.GetPlayer().x - startX) > 0.001f || player.weapon->brother.CanPassEnemies()) { ++failures; }
    enemy.combat.health = 0;
    scene.Update(16, 1, 0, false);
    if (scene.GetPlayer().x <= startX + 1) { ++failures; }
    scene.Reset();
    return failures;
}

/** Traverse several frozen BIG enemies using the immunity from a real melee hit. */
unsigned CheckMeleeEscape(CLevel &scene, ZPlayerModel &player, std::size_t entry) {
    unsigned failures = 0;
    CBrother &brother = player.weapon->brother;
    // Keep the original PLAYER script/timer; only isolate the enemy AI motion.
    scene.GetEnemies().clear();
    const float startX = 400, startY = 550;
    for (int index = 0; index < 3; ++index) {
        ZCombatEnemy *actor = scene.Spawn(entry, startX + 25 + index * 55, startY);
        if (actor == nullptr) { return 1; }
        actor->model.enemy.stun.SetStunned(10000, 100, 0);
    }
    scene.GetPlayer().x = startX;
    scene.GetPlayer().y = startY;
    if (!brother.CanPassEnemies()) { ++failures; }
    for (int frame = 0; frame < 50; ++frame) { scene.Update(16, 1, 0, false); }
    const float travel = scene.GetPlayer().x - startX;
    if (travel < 150 || !brother.CanPassEnemies()) { ++failures; }
    // Stop outside the enemies, so no new contact can restart immunity.
    scene.GetPlayer().x = 300;
    scene.GetPlayer().y = 750;
    for (int frame = 0; frame < 150 && brother.CanPassEnemies(); ++frame) { scene.Update(16, 0, 0, false); }
    if (brother.CanPassEnemies() || brother.IsImmunityHidden()) { ++failures; }
    ZCombatEnemy &actor = *scene.GetEnemies().front();
    float centerX = actor.model.enemy.combat.x, centerY = actor.model.enemy.combat.y, radius = 0;
    EnemyCollisionCircle(actor.model.enemy, actor.data->gameScale, 0, centerX, centerY, radius);
    scene.GetPlayer().x = centerX - (brother.GetRadius() + actor.model.enemy.GetPart(0).radius * 0.8f - 1);
    scene.GetPlayer().y = centerY;
    const float beforeBlocked = scene.GetPlayer().x;
    scene.Update(16, 1, 0, false);
    const bool blockedAgain = std::abs(scene.GetPlayer().x - beforeBlocked) < 0.001f;
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
    ZWindow window;
    if (!window.Open("Actor feedback check", 640, 480)) { return 1; }
    ZShaderProgram program;
    if (!program.Load(Paths::Shaders(), "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    ZPackTables tables(toc);
    ZPlayerTemplateData playerData;
    std::vector<ZWeaponEntry> weapons;
    std::vector<ZEnemyTemplateData> enemies;
    if (!FindPlayerTemplate(toc, tables, playerData) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadEnemyCatalog(toc, tables, enemies)) { return 1; }
    const ZWeaponEntry *pistol = nullptr;
    const ZWeaponEntry *rifle = nullptr;
    for (const ZWeaponEntry &weapon : weapons) {
        if (weapon.packHash == toc.GetPack(toc.GetCorePackIndex())->GetPackHash() && weapon.ordinal == 0) { pistol = &weapon; }
        if (weapon.packHash == CStringToKey("pack5") && weapon.ordinal == 4) { rifle = &weapon; }
    }
    if (!pistol || !rifle) { return 1; }
    ZPlayerVitals vitals;
    vitals.maximum = 100;
    vitals.invincible = false;
    vitals.Reset();
    ZPlayerModel player;
    player.vitals = &vitals;
    if (!BuildPlayerBody(tables, playerData.moveSet, player) ||
        !EquipPlayerWeapon(tables, playerData.script, pistol->data, "feedback player", player) ||
        !CreatePlayerBuffers(player, program)) { return 1; }
    CLevel scene(toc, tables, program);
    scene.BindCombat(enemies, player, vitals, playerData.gameScale);
    unsigned failures = 0;
    failures += CheckOriginalCircleCircle();
    failures += CheckEnemyMovement(scene, player, vitals);
    // A barrel's native 10 can apply zero damage and still knock the player back.
    ZCombatHit blast;
    blast.ownerType = 1;
    blast.x = scene.GetPlayer().x - 20;
    blast.y = scene.GetPlayer().y;
    const auto beforeBlast = scene.GetSoundCueCount();
    scene.Splash(blast, 100, 360, 300, 100);
    const unsigned knockbackState = player.weapon->brother.GetStateId();
    for (int elapsed = 0; elapsed < 768; elapsed += 16) { scene.Update(16, 0, 0, false); }
    const auto blastSounds = scene.GetSoundCueCount() - beforeBlast;
    if (knockbackState != 10 || blastSounds == 0 || vitals.health != 100) { ++failures; }
    std::printf("[actor-feedback-check] barrel state=%u sounds=%zu hp=%.0f\n", knockbackState, blastSounds, vitals.health);

    scene.Reset();
    // Ordinary ranged damage must not acquire the knockback vocal animation.
    ZCombatHit ranged;
    ranged.ownerType = 1;
    ranged.projectile = 123;
    ranged.damage = 1;
    const auto beforeRanged = scene.GetSoundCueCount();
    scene.ApplyHit(kPlayerCombatId, ranged);
    for (int elapsed = 0; elapsed < 768; elapsed += 16) { scene.Update(16, 0, 0, false); }
    const auto rangedSounds = scene.GetSoundCueCount() - beforeRanged;
    if (rangedSounds != 0 || vitals.health != 99) { ++failures; }
    std::printf("[actor-feedback-check] ranged sounds=%zu hp=%.0f\n", rangedSounds, vitals.health);

    scene.Reset();
    CBrotherAI brother;
    brother.vitals.maximum = 100;
    brother.vitals.invincible = false;
    brother.Reset(scene.GetPlayer().x, scene.GetPlayer().y, 0);
    ZPlayerModel partner;
    partner.human = false;
    partner.vitals = &brother.vitals;
    if (!BuildPlayerBody(tables, playerData.moveSet, partner) ||
        !EquipPlayerWeapon(tables, playerData.script, pistol->data, "feedback brother", partner) ||
        !CreatePlayerBuffers(partner, program)) { return 1; }
    scene.SetBrother(&partner, &brother);
    scene.SetBrotherWeapons(playerData.script, pistol->data, rifle->data);
    // Every real pickup must ignore the AI partner, then remain collectable by the player.
    CProfileManager pickupProfile;
    const auto pickupReferences = GetPickupCheckReferences(toc, tables);
    if (pickupReferences.empty() || !scene.InitPickups(toc, tables, program, &pickupProfile)) { return 1; }
    unsigned brotherCollections = 0;
    unsigned playerCollections = 0;
    for (const GameObjectRef &resource : pickupReferences) {
        scene.GetPlayer().x = 600;
        scene.GetPlayer().y = 650;
        scene.Update(16, 0, 0, false);
        brother.Reset(200, 200, 0);
        scene.ResetPickups();
        if (!scene.SpawnPickupAt(resource, brother.x, brother.y)) { return 1; }
        scene.UpdatePickups(16);
        brotherCollections += scene.GetPickupCollectedCount();
        if (scene.GetPickupCount() != 1 || scene.GetPickupCollectedCount() != 0) { ++failures; }
        scene.GetPlayer().x = brother.x;
        scene.GetPlayer().y = brother.y;
        scene.Update(16, 0, 0, false);
        const unsigned collectedBefore = scene.GetPickupCollectedCount();
        scene.UpdatePickups(16);
        playerCollections += scene.GetPickupCollectedCount() - collectedBefore;
        if (scene.GetPickupCount() != 0 || scene.GetPickupCollectedCount() - collectedBefore != 1) { ++failures; }
    }
    std::printf("[actor-feedback-check] pickups=%zu brother-collected=%u player-collected=%u\n",
        pickupReferences.size(), brotherCollections, playerCollections);
    scene.Reset();
    CBrother *originalHost = &partner.weapon->brother;
    bool swapAnimation = false;
    bool swapped = false;
    const auto beforeSwap = scene.GetSoundCueCount();
    for (int elapsed = 0; elapsed < 240000; elapsed += 16) {
        scene.Update(16, 0, 0, false);
        // BIG states 4/5 lower the old gun and raise the new one.
        const unsigned state = partner.weapon->brother.GetStateId();
        if (state == 4 || state == 5) { swapAnimation = true; }
        if (scene.GetBrotherWeaponSlot() == 1) { swapped = true; }
        if (swapped && state != 4 && state != 5) { break; }
    }
    const auto swapSounds = scene.GetSoundCueCount() - beforeSwap;
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
        brother.Reset(scene.GetPlayer().x, scene.GetPlayer().y, 0);
        blast.x = scene.GetPlayer().x - 20;
        blast.y = scene.GetPlayer().y;
        const float startX = scene.GetPlayer().x;
        scene.Splash(blast, 100, 360, 300, durationMs);
        for (int elapsed = 0; elapsed < durationMs + 64; elapsed += 16) {
            scene.Update(16, 0, 0, false);
            if (std::hypot(scene.GetPlayer().x - brother.x, scene.GetPlayer().y - brother.y) > 0.001f) { ++failures; }
        }
        const float travel = scene.GetPlayer().x - startX;
        if (travel <= 0 || travel >= 300 * durationMs * 0.0005f) { ++failures; }
        std::printf("[actor-feedback-check] force-hosts ms=%d player=%.3f brother=%.3f\n",
            durationMs, travel, brother.x - startX);
    }

    scene.SetBrother(nullptr, nullptr);
    bool meleeChecked = false;
    for (std::size_t index = 0; index < enemies.size(); ++index) {
        scene.Reset();
        ZCombatEnemy *enemy = scene.Spawn(index, scene.GetPlayer().x + 8, scene.GetPlayer().y);
        if (!enemy) { return 1; }
        const auto beforeMelee = scene.GetSoundCueCount();
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
        const float startX = scene.GetPlayer().x;
        const float startY = scene.GetPlayer().y;
        float firstStep = 0;
        float lateStep = 0;
        for (int elapsed = 16; elapsed <= forceMs + 16; elapsed += 16) {
            const float previousX = scene.GetPlayer().x;
            const float previousY = scene.GetPlayer().y;
            scene.Update(16, 0, 0, false);
            if (elapsed < forceMs && *player.weapon->brother.VariableResolver(3) != 1400) { ++failures; }
            const float step = std::hypot(scene.GetPlayer().x - previousX, scene.GetPlayer().y - previousY);
            if (elapsed == 16) { firstStep = step; }
            if (elapsed <= forceMs * 3 / 4) { lateStep = step; }
        }
        const float travel = std::hypot(scene.GetPlayer().x - startX, scene.GetPlayer().y - startY);
        // CBrother::Update :135201 slows to zero instead of stopping abruptly.
        if (forceMs <= 32 || firstStep <= 0 || lateStep >= firstStep * 0.5f) { ++failures; }
        std::printf("[actor-feedback-check] melee-motion ms=%d first=%.3f late=%.3f travel=%.3f\n",
            forceMs, firstStep, lateStep, travel);
        failures += CheckMeleeEscape(scene, player, index);
        for (int elapsed = 0; elapsed < 768; elapsed += 16) { scene.Update(16, 0, 0, false); }
        const auto meleeSounds = scene.GetSoundCueCount() - beforeMelee;
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
    ZLoadedMap wallMap;
    if (!LoadMap(toc, toc.GetPackIndexFromName("pack2"), 7, wallMap)) { return 1; }
    BuildCollisionScene(wallMap);
    scene.SetMap(wallMap.map, wallMap.collisionScene, wallMap.weaponCollision, 1, kPlayerCollisionRadius);
    const ZMapRectangle bounds = wallMap.map.GetCameraExtent();
    bool wallChecked = false;
    const auto &vertices = wallMap.collisionScene.GetVertices();
    for (const ZCollisionEdge &edge : wallMap.collisionScene.GetEdges()) {
        if (!edge.enabled) { continue; }
        const ZCollisionPoint &a = vertices[edge.firstVertex], &b = vertices[edge.secondVertex];
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
        scene.GetPlayer().x = startX;
        scene.GetPlayer().y = startY;
        for (int frame = 0; frame < 10; ++frame) { scene.Update(16, -nx, -ny, false); }
        const float separation = (scene.GetPlayer().x - centerX) * nx + (scene.GetPlayer().y - centerY) * ny;
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
