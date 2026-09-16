/** Exercise Live decisions against the real BIG catalog and actor instances. */
#include "gameplay/SurvivalChecks.h"
#include "gun_bros_re/gameplay/ZLiveShopSession.h"

int CheckLivePolicies(SurvivalDeathFixture fixture, ZPowerupScene &powerups, CProfileManager &profile) {
    ZLiveShopSession shop;
    if (!shop.RequestForWave(1, 0, 0, false) || !shop.Close(1) ||
        shop.RequestForWave(1, 1000, 0, false) ||
        !shop.RequestForWave(1, 1000, 0, false, true) || !shop.Close(1) ||
        shop.RequestForWave(1, 2000, 0, false) ||
        shop.RequestForWave(1, 2000, 1, true) || shop.RequestForWave(1, 2000, 1, true, true) ||
        !shop.RequestForWave(1, 2000, 1, false) || !shop.Close(1)) { return 1; }
    // An exempt opening before the normal one must not spend the new wave.
    if (!shop.RequestForWave(1, 3000, 2, false, true) || !shop.Close(1) ||
        !shop.RequestForWave(1, 4000, 2, false) || !shop.Close(1)) { return 1; }
    if (!shop.RequestForWave(0, 5000, 2, false) || !shop.Close(0) ||
        !shop.RequestForWave(0, 6000, 2, false) || !shop.Close(0) ||
        shop.RequestForWave(0, 7000, 2, true)) { return 1; }
    auto &scene = fixture.scene;
    auto &session = fixture.session;
    session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
    fixture.vitals.invincible = true;
    fixture.brother.vitals.invincible = true;
    GameObjectRef enemyRef;
    for (unsigned elapsed = 0; elapsed < 10000 && enemyRef.IsNull(); elapsed += 16) {
        session.Update(16, 0, 0, false);
        for (const auto &actor : scene.enemies) {
            if (actor->mapPlaced || !actor->model.enemy.CanReceiveProjectile(0, kPlayerCombatId)) { continue; }
            enemyRef.packHash = actor->data->packHash;
            enemyRef.localIndex = static_cast<std::uint8_t>(actor->data->ordinal);
            break;
        }
    }
    if (enemyRef.IsNull()) { return 1; }
    while (scene.enemies.size() < 11) {
        if (!session.SpawnEnemy(enemyRef, -1, -1, -1)) { return 1; }
    }
    // Position fixtures without updating them, so boundary decisions do not
    // depend on spawn routing, AI shots or projectile travel time.
    for (auto &actor : scene.enemies) {
        auto &enemy = actor->model.enemy.combat;
        enemy.dead = false; enemy.removed = false; enemy.enabled = true; enemy.health = 1;
        enemy.x = fixture.brother.x + ZPowerupScene::BotGrenadeRadius + 1;
        enemy.y = fixture.brother.y;
    }
    auto &last = scene.enemies.back()->model.enemy.combat;
    auto &first = scene.enemies[0]->model.enemy.combat;
    auto &second = scene.enemies[1]->model.enemy.combat;
    GameObjectRef item = powerups.GetEquipped(0);
    if (item.IsNull()) { return 1; }
    // Fixture identities only. Production derives the category from BIG.
    for (unsigned ordinal : {0u, 10u, 11u}) {
        item.localIndex = static_cast<std::uint8_t>(ordinal);
        if (!powerups.SelectResource(item) || !powerups.CanBotUseSelected()) { return 1; }
        last.dead = true;
        if (powerups.CanBotUseSelected()) { return 1; }
        last.dead = false;
    }
    for (unsigned ordinal : {13u, 14u, 15u}) {
        item.localIndex = static_cast<std::uint8_t>(ordinal);
        if (!powerups.SelectResource(item) || powerups.CanBotUseSelected()) { return 1; }
        first.x = fixture.brother.x;
        if (powerups.CanBotUseSelected()) { return 1; }
        second.x = fixture.brother.x + ZPowerupScene::BotGrenadeRadius;
        if (!powerups.CanBotUseSelected()) { return 1; }
        second.health = 0;
        if (powerups.CanBotUseSelected()) { return 1; }
        second.health = 1;
        second.removed = true;
        if (powerups.CanBotUseSelected()) { return 1; }
        second.removed = false;
        second.enabled = false;
        if (powerups.CanBotUseSelected()) { return 1; }
        second.enabled = true;
        first.x = second.x = fixture.brother.x + ZPowerupScene::BotGrenadeRadius + 1;
    }
    // UseAny must skip an owned air strike at ten enemies without consuming it.
    const auto inventory = profile.powerups;
    profile.powerups.clear();
    item.localIndex = 0;
    profile.AddPowerup(item, 1);
    last.dead = true;
    if (powerups.UseAny() || profile.GetPowerupCount(item) != 1 || powerups.IsMovieActive()) { return 1; }
    last.dead = false;
    if (!powerups.UseAny() || profile.GetPowerupCount(item) != 0 || !powerups.IsMovieActive()) { return 1; }
    if (session.StartBossSkip()) { return 1; }
    for (unsigned elapsed = 0; elapsed < 15000 && powerups.IsMovieActive(); elapsed += 16) { session.Update(16, 0, 0, false); }
    if (powerups.IsMovieActive() || powerups.failures != 0) { return 1; }
    profile.powerups = inventory;
    session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
    std::printf("[live-policy] wave-limit=1 cheat-exempt=1 rescue-block=1 airstrike-10-11=1 grenade-radius-250=1 inventory=1\n");
    return 0;
}
