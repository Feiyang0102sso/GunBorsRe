/** Directed checks run the production scene, original Flow and BIG maps. */
#define NOMINMAX
#include "gameplay/SurvivalCheckScenario.h"
#include "gun_bros_re/gameplay/ZSurvivalRuntime.h"
#include "gun_bros_re/gameplay/ZSurvivalGameContext.h"
#include "gun_bros_re/gameplay/ZLevelHost.h"
#include "gun_bros_re/gameplay/ZDeathmatchBot.h"
#include "gun_bros_re/gameplay/ZPickupScene.h"
#include "gun_bros_re/gameplay/ZPowerupScene.h"
#include "gun_bros_re/data/ZPlanetCatalog.h"
#include "gun_bros_re/data/ZMissionCatalog.h"
#include "gameplay/SurvivalStudy.h"
#include "gameplay/SurvivalChecks.h"
#include "TestOutput.h"
#include "engine/core/CStringToKey.h"

int RunDeathmatchCombatCheck(const std::string &directory, bool feedback) {
    CResTOCManager toc;
    if (!toc.Init(directory, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    std::vector<ZPlanetEntry> planets;
    if (!LoadPlanetCatalog(toc, tables, planets)) { return 1; }
    CProfileManager source;
    if (!LoadProfile(toc, tables, source, TestOutput::Path("deathmatch-profile"), TestOutput::Fixtures())) { return 1; }
    int CheckDeathmatchMenus(CResTOCManager &, ZPackTables &, CProfileManager &);
    if (CheckDeathmatchMenus(toc, tables, source) != 0) { return 1; }
    for (unsigned index = 0; index < 5; ++index) {
        ZMissionEntry mission;
        mission.resource = planets[index].data.object12;
        std::vector<std::uint8_t> bytes;
        if (!tables.ReadSectionResource(mission.resource.packHash, ZGameSection::Mission, mission.resource.localIndex, bytes)) { return 1; }
        CArrayInputStream input(bytes);
        if (!mission.data.Init(input) || mission.data.type != 3) { return 1; }
        if (!tables.ReadSectionResource(mission.data.level.packHash, ZGameSection::Level, mission.data.level.localIndex, bytes)) { return 1; }
        CArrayInputStream levelInput(bytes);
        CLevel::Template level;
        if (!level.Init(levelInput)) { return 1; }
        CProfileManager profile = source;
        ZSurvivalGameContext context{profile, {}};
        context.persistProgress = false;
        ZSurvivalLaunch launch;
        launch.bigDirectory = directory;
        launch.packShortName = tables.GetPackName(level.mapRef.packHash);
        launch.mapIndex = level.mapRef.localIndex;
        launch.deathmatch = true;
        launch.matchIndex = index;
        launch.archiveMission = &mission;
        launch.gameContext = &context;
        SurvivalDevelopment development;
        DevelopmentBinding binding(launch, development);
        development.deathmatchCheck = true;
        development.deathmatchFeedbackCheck = feedback;
        std::printf("[deathmatch-check] map=%u tier=%u\n", index, index);
        if (RunSurvivalSession(launch) != 0) { return 1; }
        if (!feedback && index == 0) {
            development.deathmatchCheck = false;
            development.advanceMs = 8000;
            development.screenshotPath = TestOutput::Path("deathmatch-play.png");
            if (RunSurvivalSession(launch) != 0) { return 1; }
        }
    }
    return 0;
}

int CheckDeathmatchCombat(SurvivalDeathFixture fixture, CMPMatch &match, ZPickupScene &pickups, ZPowerupScene &powerups, CProfileManager &profile, ZSurvivalGameContext &context) {
    auto &scene = fixture.scene; auto &session = fixture.session; auto &bot = fixture.brother;
    auto &player = fixture.player; auto &opponent = fixture.brotherModel; auto &vitals = fixture.vitals;
    if (!scene.RespawnDeathmatch(0, true) || !scene.RespawnDeathmatch(1, true)) { return 1; }
    if (!scene.IsDeathmatch() || dynamic_cast<ZDeathmatchBot *>(&bot) == nullptr || !player.weapon->brother.IsDeathmatch() ||
        !opponent.weapon->brother.IsDeathmatch() || scene.invalidSpawns != 0) { std::printf("[deathmatch-check] init failed invalid=%u\n", scene.invalidSpawns); return 1; }
    vitals.invincible = true; bot.vitals.invincible = true;
    const float startX = scene.GetPlayer().x, startY = scene.GetPlayer().y;
    for (unsigned time = 0; time < 35000; time += 16) { session.Update(16, 0, 0, false); }
    std::printf("[deathmatch-check] initial supply=%u collected=%u invalid=%u sightings=%u\n", pickups.spawned, pickups.collected, scene.invalidSpawns, bot.GetTargetCount());
    if (pickups.spawned == 0 || scene.invalidSpawns != 0) { return 1; }
    float supplyX = 0, supplyY = 0;
    if (pickups.FindNearest(scene.GetPlayer().x, scene.GetPlayer().y, supplyX, supplyY)) {
        scene.GetPlayer().x = supplyX; scene.GetPlayer().y = supplyY;
        for (unsigned time = 0; time < 1000; time += 16) { session.Update(16, 0, 0, false); }
    }
    if (pickups.collected == 0) { std::printf("[deathmatch-check] pickup not collected\n"); return 1; }
    const unsigned spawned = pickups.spawned;
    for (unsigned time = 0; time < 35000; time += 16) { session.Update(16, 0, 0, false); }
    if (pickups.spawned <= spawned) { std::printf("[deathmatch-check] pickup did not respawn\n"); return 1; }
    scene.GetPlayer().x = startX; scene.GetPlayer().y = startY;

    // Both projectile and beam tracing must identify the opposing participant.
    for (unsigned flags : {0u, 0x100u}) {
        ZCombatHit traceHit; traceHit.ownerType = 0; traceHit.flags = flags;
        traceHit.owner = kPlayerCombatId;
        const auto toBot = scene.Trace(traceHit, bot.previousX, bot.previousY, bot.x - bot.previousX, bot.y - bot.previousY, 1, {});
        traceHit.owner = kBrotherCombatId;
        const auto toPlayer = scene.Trace(traceHit, scene.GetPlayer().x, scene.GetPlayer().y, 0, 0, 1, {});
        if (toBot.target != kBrotherCombatId || toPlayer.target != kPlayerCombatId) {
            std::printf("[deathmatch-check] trace failed flags=%u targets=%llu,%llu\n", flags, toBot.target, toPlayer.target); return 1;
        }
    }
    for (unsigned peer = 0; peer < 2; ++peer) {
        if (!scene.CollectMatchWeapon(peer, 0)) { return 1; }
        scene.UpdateDeathmatch(match.Data().pickupRules[0].seconds * 1000);
        const auto restored = scene.ActiveMatchGun(peer);
        const auto &selected = scene.MatchGun(peer, peer == 0 ? player.gunSlot : scene.GetBrotherWeaponSlot());
        if (restored.packHash != selected.packHash || restored.localIndex != selected.localIndex) { return 1; }
    }
    vitals.invincible = false; bot.vitals.invincible = false;
    for (unsigned peer = 0; peer < 2; ++peer) {
        ZCombatHit blast; blast.ownerType = 0; blast.damage = 10;
        ZPlayerVitals *target = &bot.vitals;
        blast.owner = kPlayerCombatId; blast.x = bot.x; blast.y = bot.y;
        if (peer == 1) { target = &vitals; blast.owner = kBrotherCombatId; blast.x = scene.GetPlayer().x; blast.y = scene.GetPlayer().y; }
        const float before = target->health;
        scene.Splash(blast, 1, 360, 0, 0);
        if (target->health >= before || target->dead) { std::printf("[deathmatch-check] splash failed peer=%u\n", peer); return 1; }
    }
    vitals.health = vitals.maximum; bot.vitals.health = bot.vitals.maximum;
    vitals.invincible = true; bot.vitals.invincible = true;
    const float botX = bot.x, botY = bot.y;
    GameObjectRef crate; crate.packHash = CStringToKey("pack5"); crate.localIndex = 8;
    for (unsigned winner = 0; winner < 2; ++winner) {
        scene.GetPlayer().x = startX; scene.GetPlayer().y = startY;
        bot.x = startX + 5; bot.y = startY;
        float x = scene.GetPlayer().x;
        if (winner == 1) { x = bot.x; }
        if (!pickups.Spawn(crate, x, startY, -900)) { return 1; }
        pickups.Update(16, scene, fixture.effects);
        unsigned awards = 0;
        for (const auto &collection : pickups.collections) {
            if (collection.objectId == -900) {
                ++awards;
                if (collection.peer != winner) { return 1; }
            }
        }
        if (awards != 1) { return 1; }
        pickups.Update(16, scene, fixture.effects);
        for (const auto &collection : pickups.collections) { if (collection.objectId == -900) { return 1; } }
    }
    bot.x = botX; bot.y = botY;

    GameObjectRef health; health.packHash = CStringToKey("pack5"); health.localIndex = 9;
    GameObjectRef grenade = health; grenade.localIndex = 13;
    GameObjectRef forbidden = health; forbidden.localIndex = 14;
    profile.AddPowerup(health, 4); profile.AddPowerup(grenade, 4); profile.AddPowerup(forbidden, 4);
    if (powerups.SelectResource(forbidden)) { return 1; }
    for (unsigned use = 0; use < 2; ++use) {
        bot.vitals.health = 1;
        if (!powerups.UseMatchConsumable(false) || bot.vitals.health <= 1) { std::printf("[deathmatch-check] heal failed use=%u\n", use); return 1; }
    }
    bot.vitals.health = 1;
    if (powerups.UseMatchConsumable(false) || match.GetLife(1).healthPacks != 2) { return 1; }
    bot.vitals.health = bot.vitals.maximum;
    for (unsigned use = 0; use < 2; ++use) {
        bool requested = false;
        for (unsigned time = 0; time < 6000; time += 16) {
            if (!requested) { requested = powerups.UseMatchConsumable(true); }
            session.Update(16, 0, 0, false);
        }
        if (match.GetLife(1).grenades != use + 1) { std::printf("[deathmatch-check] grenade commit failed count=%u\n", match.GetLife(1).grenades); return 1; }
    }
    if (powerups.UseMatchConsumable(true) || !match.EnterShop(1) || !match.EnterShop(1) || match.EnterShop(1)) { return 1; }

    // Friendly fire remains disabled on the shooter; the opposing actor takes damage.
    ZCombatHit hit; hit.owner = kPlayerCombatId; hit.ownerType = 0; hit.damage = 100000;
    vitals.invincible = false; bot.vitals.invincible = false;
    if (scene.ApplyHit(kPlayerCombatId, hit) != ZHitResult::Ignored) { return 1; }
    std::printf("[deathmatch-check] directed-fatal remaining=%u limit=%u health=%.1f protection=%d shield=%d\n",
        match.RemainingMs(), match.Data().seconds, bot.vitals.health, *opponent.weapon->brother.VariableResolver(3), opponent.weapon->brother.IsShield());
    for (unsigned kill = 0; kill < match.Data().killLimit; ++kill) {
        const auto fatalResult = scene.ApplyHit(kBrotherCombatId, hit);
        if (fatalResult != ZHitResult::Killed) {
            std::printf("[deathmatch-check] fatal hit failed result=%u match=%u health=%.1f dead=%d invincible=%d unlimited=%d protection=%d shield=%d\n",
                unsigned(fatalResult), unsigned(match.GetResult()), bot.vitals.health, bot.vitals.dead, bot.vitals.invincible,
                bot.vitals.unlimitedHealth, *opponent.weapon->brother.VariableResolver(3), opponent.weapon->brother.IsShield());
            return 1;
        }
        if (match.Score(0) != kill + 1) { return 1; }
        for (unsigned time = 0; time < (match.Data().respawnSeconds + 8) * 1000 && !session.IsFinished(); time += 16) { session.Update(16, 0, 0, false); }
        if (kill + 1 < match.Data().killLimit && (bot.vitals.dead || match.GetLife(1).shops != 0 || match.GetLife(1).grenades != 0 || match.GetLife(1).healthPacks != 0)) {
            std::printf("[deathmatch-check] respawn failed dead=%u animation=%u timer=%u\n", bot.vitals.dead, bot.vitals.deathAnimationComplete, match.GetLife(1).respawnMs); return 1;
        }
    }
    if (!session.IsFinished() || match.GetResult() != CMPMatch::Result::PlayerWon || scene.invalidSpawns != 0) { return 1; }
    // Save the copied fixture twice: kills/ore must be credited once, and
    // survival records and permanent gun selections must remain unchanged.
    const auto previousWaves = context.profile.clearedWaves;
    const auto previousGuns = context.profile.configuration.guns;
    const auto previousKills = context.profile.statistics[37];
    CPlayerProgress savedProgress;
    savedProgress.Bind(context.profile.nativeArchive->progression);
    savedProgress.SetExperience(scene.GetExperience());
    context.savePath = TestOutput::Path("deathmatch-reward-" + std::to_string(match.Data().health));
    context.persistProgress = true;
    std::uint64_t accountedOre = 0;
    for (unsigned save = 0; save < 2; ++save) {
        if (!SaveSurvivalProgress(&context, savedProgress, scene, session.GetLevel(), accountedOre)) { return 1; }
    }
    CProfileManager reloaded = context.profile;
    if (!ReloadProfile(reloaded, context.savePath) || reloaded.statistics[37] != previousKills + match.Score(0) ||
        reloaded.experience != scene.GetExperience() || reloaded.clearedWaves != previousWaves || scene.GetScore() == 0) { return 1; }
    for (unsigned slot = 0; slot < 2; ++slot) {
        if (reloaded.configuration.guns[slot].packHash != previousGuns[slot].packHash ||
            reloaded.configuration.guns[slot].localIndex != previousGuns[slot].localIndex) { return 1; }
    }
    context.persistProgress = false;
    std::printf("[deathmatch-check] map-flow=1 pickups=1 budgets=1 opposing-damage=1 respawn=1 result=1\n");
    session.Restart(startX, startY, 0);
    if (!scene.RespawnDeathmatch(0, true) || !scene.RespawnDeathmatch(1, true)) { return 1; }
    vitals.invincible = false; bot.vitals.invincible = true;
    const auto shots = fixture.effects.GetShotCount();
    unsigned matchTime = 0;
    for (; matchTime < 600000 && !session.IsFinished(); matchTime += 16) {
        session.Update(16, 0, 0, false);
        if (matchTime % 60000 == 0) { static_cast<ZDeathmatchBot &>(bot).PrintNavigation(); }
    }
    std::printf("[deathmatch-check] autonomous elapsed=%u ms\n", matchTime);
    std::printf("[deathmatch-check] autonomous shots=%zu\n", fixture.effects.GetShotCount() - shots);
    std::printf("[deathmatch-check] autonomous score=%u:%u sightings=%u playerHP=%.1f bot=%.1f,%.1f player=%.1f,%.1f\n",
        match.Score(0), match.Score(1), bot.GetTargetCount(), vitals.health, bot.x, bot.y, scene.GetPlayer().x, scene.GetPlayer().y);
    static_cast<ZDeathmatchBot &>(bot).PrintNavigation();
    bool CaptureRescueEffect(SurvivalDeathFixture &, const char *);
    if (!CaptureRescueEffect(fixture, "deathmatch-navigation.png")) { return 1; }
    if (match.GetResult() != CMPMatch::Result::BotWon) { return 1; }
    session.Restart(startX, startY, 0);
    if (!scene.RespawnDeathmatch(0, true) || !scene.RespawnDeathmatch(1, true)) { return 1; }
    if (!scene.Suicide()) { return 1; }
    for (unsigned time = 0; time < 8000 && match.Score(1) == 0; time += 16) { session.Update(16, 0, 0, false); }
    if (match.Score(1) != 1 || !match.GetLife(0).dead) { return 1; }
    std::printf("[deathmatch-check] environment-death=1 persistent-rewards=1 unique-pickup=1 trace-beam-splash=1\n");
    return 0;
}
