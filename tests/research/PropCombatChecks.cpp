/** Verify isolated barrel detonations through the real map and combat hosts. */
#include "gun_bros_re/gameplay/ZMapWorldInternal.h"
#include "tests/Checks.h"
using namespace MapDetail;

int RunPropCombatCheck(const std::string &bigDirectory) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    CResTOCManager toc;
    if (!toc.InitAuto(bigDirectory) || !toc.Bind()) { return 1; }
    ZWindow window;
    if (!window.Open("Prop combat check", 640, 480)) { return 1; }
    ZShaderProgram program;
    if (!program.Load(Paths::Shaders(), "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    ZPackTables tables(toc);
    std::vector<ZEnemyTemplateData> enemies;
    if (!LoadEnemyCatalog(toc, tables, enemies) || enemies.empty()) { return 1; }
    unsigned failures = 0;
    unsigned tested = 0;
    for (const ZCatalogMap &entry : BuildCatalog(toc)) {
        if (!((entry.packName == "pack2" && entry.mapIndex == 7) ||
              (entry.packName == "pack12" && entry.mapIndex == 0))) { continue; }
        ZLoadedMap loaded;
        if (!LoadMap(toc, entry.packIndex, entry.mapIndex, loaded)) { return 1; }
        LoadProps(toc, loaded);
        ZPlayerModel player;
        ZPlayerVitals vitals;
        // This fixture isolates props; no player damage or account is needed.
        vitals.dead = true;
        ZWeaponEffects effects(toc, tables, program);
        ZCombatWorld scene(tables, program, enemies, player, vitals, effects, 1.0f);
        CLevel level;
        CLevel::Template levelTemplate;
        level.Bind(levelTemplate, loaded.map);
        ZMapPropWorld props(loaded, scene, level, effects);
        scene.SetProps(&props);
        bool checkedEnemyDamage = false;
        for (ZPlacedProp &target : loaded.props) {
            if (target.sprite->interactiveKind != ZInteractivePropKind::Barrel) { continue; }
            props.Reset();
            props.StartLayer(target.objectLayer);
            std::vector<float> healthBefore;
            for (const ZPlacedProp &prop : loaded.props) {
                float health = 0;
                if (prop.active && prop.runtime) { health = prop.runtime->GetHealth(); }
                healthBefore.push_back(health);
            }
            ZCombatHit bullet;
            bullet.owner = kPlayerCombatId;
            bullet.projectile = 123;
            bullet.damage = target.runtime->GetHealth();
            bullet.applyArmorAttack = false;
            const ZCombatTrace contact = props.Trace(bullet, target.x - 150, target.y,
                300, 0, 1, {});
            // Use the production target ID returned by its authored collision shape.
            if (contact.target == 0) {
                ++failures;
                continue;
            }
            const auto &vertices = target.runtime->GetEntryCollision().GetVertices();
            if (vertices.empty()) { ++failures; continue; }
            float centerX = 0, centerY = 0;
            for (const auto &vertex : vertices) { centerX += vertex.x; centerY += vertex.y; }
            ZCombatHit splash = bullet;
            splash.x = target.x + centerX / static_cast<float>(vertices.size());
            splash.y = target.y + centerY / static_cast<float>(vertices.size());
            splash.damage = 1;
            splash.projectile = 0;
            const float initialHealth = target.runtime->GetHealth();
            props.Splash(splash, 0);
            splash.owner = kBrotherCombatId;
            props.Splash(splash, 0);
            if (target.runtime->GetHealth() != initialHealth) { ++failures; }
            // A real human bullet splash still reaches the same target.
            splash.projectile = bullet.projectile;
            props.Splash(splash, 0);
            if (target.runtime->GetHealth() != initialHealth - 1) { ++failures; }
            ZCombatEnemy *blastTarget = nullptr;
            float enemyHealth = 0;
            if (!checkedEnemyDamage) {
                blastTarget = scene.Spawn(0, 600, 450);
                if (!blastTarget) { return 1; }
                // Finish the original spawn state, then place this target inside
                // the blast. Its health and hit behavior still come from BIG.
                for (int elapsed = 0; elapsed < 1024; elapsed += 16) { blastTarget->model.enemy.Update(16); }
                blastTarget->model.enemy.combat.x = target.x + 30;
                blastTarget->model.enemy.combat.y = target.y;
                enemyHealth = blastTarget->model.enemy.combat.health;
            }
            if (props.ApplyHit(contact.target, bullet) != ZHitResult::Hit) { ++failures; continue; }
            for (int elapsed = 0; elapsed < 128; elapsed += 16) { props.Update(16); }
            if (blastTarget) {
                blastTarget->model.enemy.Update(16);
                const float remaining = blastTarget->model.enemy.combat.health;
                if (enemyHealth <= 0 || remaining >= enemyHealth) { ++failures; }
                std::printf("[prop-combat-check] %s enemy-hp=%.0f->%.0f\n", entry.packName.c_str(), enemyHealth, remaining);
                checkedEnemyDamage = true;
            }
            unsigned otherDamaged = 0;
            for (std::size_t index = 0; index < loaded.props.size(); ++index) {
                const ZPlacedProp &prop = loaded.props[index];
                if (&prop != &target && prop.active && prop.runtime &&
                    prop.runtime->GetHealth() < healthBefore[index]) { ++otherDamaged; }
            }
            if (target.runtime->GetHealth() != 0 || otherDamaged != 0) { ++failures; }
            std::printf("[prop-combat-check] %s object=%d pos=%.0f,%.0f target-hp=%.0f other-damaged=%u\n",
                entry.packName.c_str(), target.objectId, target.x, target.y,
                target.runtime->GetHealth(), otherDamaged);
            ++tested;
        }
        if (entry.packName == "pack2") {
            ZPlayerTemplateData playerData;
            std::vector<ZWeaponEntry> weapons;
            if (!FindPlayerTemplate(toc, tables, playerData) ||
                !LoadWeaponCatalog(toc, tables, weapons) || weapons.empty()) { return 1; }
            if (!BuildPlayerBody(tables, playerData.moveSet, player) ||
                !EquipPlayerWeapon(tables, playerData.script, weapons[0].data, "barrel chain check", player) ||
                !CreatePlayerBuffers(player, program)) { return 1; }
            scene.Reset();
            std::vector<ZPlacedProp *> cluster;
            float centerX = 0, centerY = 0;
            // The three barrels in the user's first-map example; positions stay authored.
            for (ZPlacedProp &prop : loaded.props) {
                if (prop.objectId == 60 || prop.objectId == 63 || prop.objectId == 64) {
                    cluster.push_back(&prop);
                    centerX += prop.x;
                    centerY += prop.y;
                }
            }
            if (cluster.size() != 3) { return 1; }
            centerX /= 3;
            centerY /= 3;
            props.Reset();
            props.StartLayer(cluster[0]->objectLayer);
            ZCombatEnemy *target = nullptr;
            for (std::size_t index = 0; index < enemies.size(); ++index) {
                if (enemies[index].packHash == CStringToKey("pack1") && enemies[index].ordinal == 27) {
                    target = scene.Spawn(index, centerX, centerY);
                    break;
                }
            }
            if (!target) { return 1; }
            CEnemy &enemy = target->model.enemy;
            const float initialHealth = enemy.combat.health;
            GameObjectRef grenade;
            grenade.packHash = CStringToKey("pack5");
            grenade.localIndex = 90; // Original standard grenade, also used by boss checks.
            if (effects.SpawnProjectile(grenade, centerX, centerY, 0, 0, 0, kPlayerCombatId, 0) == 0) { return 1; }
            float matrix[16];
            scene.PlayerMatrix(matrix);
            for (int elapsed = 0; elapsed < 4000; elapsed += 16) {
                enemy.combat.x = centerX;
                enemy.combat.y = centerY;
                enemy.combat.behaviour = 7;
                enemy.combat.targetAlive = false;
                // Match SurvivalSession: projectiles, then the prop update.
                effects.Update(player, matrix, 0, 16);
                props.Update(16);
                enemy.Update(16);
                if (enemy.GetPartCount() == 1) { break; }
            }
            unsigned exploded = 0;
            for (const ZPlacedProp *prop : cluster) {
                if (prop->runtime->GetHealth() == 0) { ++exploded; }
            }
            std::printf("[prop-combat-check] grenade-chain barrels=%u parts=%u hp=%.0f->%.0f hits=%u\n",
                exploded, enemy.GetPartCount(), initialHealth, enemy.combat.health, enemy.combat.hitCount);
            if (exploded != 3 || enemy.GetPartCount() != 1 ||
                enemy.combat.health != initialHealth || enemy.combat.hitCount != 0) { ++failures; }
            // A separate barrel hit immediately afterwards must still damage
            // the unarmored enemy. A fabricated invulnerability timer fails this.
            bool subsequentBlastChecked = false;
            for (ZPlacedProp &prop : loaded.props) {
                if (!prop.active || !prop.runtime || prop.runtime->GetHealth() <= 0 ||
                    prop.sprite->interactiveKind != ZInteractivePropKind::Barrel) { continue; }
                enemy.combat.x = prop.x + 30;
                enemy.combat.y = prop.y;
                ZCombatHit bullet;
                bullet.owner = kPlayerCombatId;
                bullet.projectile = 123;
                bullet.damage = prop.runtime->GetHealth();
                const ZCombatTrace contact = props.Trace(bullet, prop.x - 150, prop.y, 300, 0, 1, {});
                const float before = enemy.combat.health;
                if (contact.target == 0 || props.ApplyHit(contact.target, bullet) != ZHitResult::Hit) { return 1; }
                for (unsigned tick = 0; tick < 8; ++tick) { props.Update(16); }
                std::printf("[prop-combat-check] after-chain barrel hp=%.0f->%.0f\n", before, enemy.combat.health);
                if (enemy.combat.health >= before) { ++failures; }
                subsequentBlastChecked = true;
                break;
            }
            if (!subsequentBlastChecked) { ++failures; }
        }
    }
    if (tested == 0) { ++failures; }
    std::printf("[prop-combat-check] tested=%u failures=%u\n", tested, failures);
    return failures != 0;
}
