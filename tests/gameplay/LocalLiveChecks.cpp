#include "gun_bros_re/debug/Capture.h"
/** Real BIG regression for the local cooperative peer and original revive Flow. */
#include "gameplay/SurvivalCheckScenario.h"
#include "gameplay/SurvivalChecks.h"
#include "gameplay/SurvivalStudy.h"
#include "gun_bros_re/gameplay/game/CGameSession.h"
#include "gun_bros_re/gameplay/brother/bot/ZLocalCoopBot.h"
#include "gun_bros_re/data/ZMissionCatalog.h"
#include "TestOutput.h"
using namespace MapDetail;
int CheckLiveMode(CResTOCManager &toc, ZPackTables &tables, CProfileManager &profile);

// A stationary enemy in open space must not make the peer zigzag every frame.
class SteadyPeerWorld : public ZBrotherAIWorld {
public:
    ZCombatId FindBrotherTarget(float, float, float) override { return 1; }
    bool GetBrotherTarget(ZCombatId, float &x, float &y) override { x = 350; y = 0; return true; }
    bool GetBrotherWaypoint(float, float, float x, float y, float &outX, float &outY) override { outX = x; outY = y; return true; }
    void ResolveBrotherForce(float, float, float &, float &) override {}
    std::vector<Threat> GetBrotherThreats() const override { return {{350, 0, 22}}; }
};

bool CheckSteadyPeer(CBrother &actor) {
    SteadyPeerWorld world;
    ZLocalCoopBot policy;
    policy.Reset(0, 0, 0);
    float oldDX = 0, oldDY = 0;
    unsigned abruptTurns = 0;
    for (unsigned elapsed = 0; elapsed < 5000; elapsed += 16) {
        const float x = policy.x, y = policy.y;
        policy.Update(16, actor, world, -1000, -1000, 1);
        const float dx = policy.x - x, dy = policy.y - y;
        const float length = std::hypot(dx, dy), oldLength = std::hypot(oldDX, oldDY);
        if (length > 0.1f && oldLength > 0.1f && (dx * oldDX + dy * oldDY) / (length * oldLength) < 0.1f) { ++abruptTurns; }
        if (length > 0.1f) { oldDX = dx; oldDY = dy; }
    }
    std::printf("[live-regression] stationary-target-abrupt-turns=%u\n", abruptTurns);
    return abruptTurns <= 8;
}

bool CaptureRescueEffect(SurvivalDeathFixture &fixture, const char *name) {
    int width = 0, height = 0;
    fixture.window.GetDrawableSize(width, height);
    auto &scene = fixture.scene;
    fixture.loaded.GetResources().players[0].x = scene.GetPlayer().x;
    fixture.loaded.GetResources().players[0].y = scene.GetPlayer().y;
    fixture.loaded.GetResources().players[0].facingDegrees = scene.GetPlayer().facing;
    const float zoom = GameViewCameraZoom(width, height);
    float mvp[kMatrix4dElements];
    Matrix4dOrthoTopLeft(width / zoom, height / zoom, kMapDepthRange, mvp);
    Matrix4dTranslate(mvp, -scene.GetPlayer().x + width / zoom / 2, -scene.GetPlayer().y + height / zoom / 2);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    fixture.loaded.DrawBackground(fixture.batch, true, true, false);
    fixture.batch.Draw(fixture.program, mvp);
    fixture.scene.Draw(mvp, nullptr, kLevelCameraScale, ZWeaponDrawPass::BehindPlayer);
    CRenderQueue::Draw(fixture.loaded, fixture.batch, fixture.program, mvp, true, &scene,
        &fixture.brotherModel, fixture.brother.y, width);
    fixture.scene.Draw(mvp, nullptr, kLevelCameraScale, ZWeaponDrawPass::InFrontOfPlayer, true);
    return Capture::SaveFrame(fixture.window, TestOutput::Path(name));
}

int RunLocalLiveCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    CProfileManager source;
    if (!LoadProfile(toc, tables, source, TestOutput::Path("local-live-profile"), TestOutput::Fixtures())) { return 1; }
    if (CheckLiveMode(toc, tables, source) != 0) { return 1; }
    const auto sourceExperience = source.experience;
    const auto sourceCoins = source.coins;
    const auto sourceRecords = source.nativeArchive->records;
    CProfileManager practice = source;
    CGameFlow context{practice, {}};
    context.persistProgress = false;
    const auto &level = practice.nativeArchive->survivalLevels[0];
    std::vector<std::uint8_t> bytes;
    if (!tables.ReadSectionResource(level.packHash, ZGameSection::Level, level.localIndex, bytes)) { return 1; }
    CArrayInputStream input(bytes);
    CLevel::Template data;
    if (!data.Init(input) || input.Available() != 0) { return 1; }
    CGame::Launch launch;
    launch.bigDirectory = bigDirectory;
    launch.packShortName = tables.GetPackName(data.mapRef.packHash);
    launch.mapIndex = data.mapRef.localIndex;
    launch.gameContext = &context;
    launch.localLive = true;
    SurvivalDevelopment development;
    DevelopmentBinding binding(launch, development);
    development.localLiveCheck = true;
    const int result = CGame::Run(launch);
    if (result != 0 || source.experience != sourceExperience || source.coins != sourceCoins) { return 1; }
    for (unsigned index = 0; index < sourceRecords.size(); ++index) {
        if (source.nativeArchive->records[index].payload != sourceRecords[index].payload) { return 1; }
    }
    if (!context.SaveProfile()) { return 1; } // Empty path is never opened.
    development.localLiveCheck = false;
    development.advanceMs = 8000;
    development.screenshotPath = TestOutput::Path("local-live-play.png");
    if (CGame::Run(launch) != 0) { return 1; }
    launch.localLive = false;
    launch.localBot = true;
    development.screenshotPath.clear();
    development.advanceMs = 0;
    development.localLiveCheck = true;
    if (CGame::Run(launch) != 0) { return 1; }
    std::printf("[local-live-check] profile-copy=1 no-save=1\n");
    std::vector<ZMissionEntry> missions;
    if (!LoadMissionCatalog(toc, tables, missions)) { return 1; }
    const ZMissionEntry *horde = nullptr;
    for (const auto &mission : missions) { if (mission.data.type == 2) { horde = &mission; break; } }
    if (horde == nullptr || !tables.ReadSectionResource(horde->data.level.packHash, ZGameSection::Level, horde->data.level.localIndex, bytes)) { return 1; }
    CArrayInputStream hordeInput(bytes);
    CLevel::Template hordeLevel;
    if (!hordeLevel.Init(hordeInput) || hordeInput.Available() != 0) { return 1; }
    launch.localLive = true;
    launch.archiveMission = horde;
    launch.packShortName = tables.GetPackName(hordeLevel.mapRef.packHash);
    launch.mapIndex = hordeLevel.mapRef.localIndex;
    launch.startWave = horde->data.value64;
    context.hordeStart = 0;
    development.localLiveCheck = false;
    development.advanceMs = 8000;
    development.screenshotPath = TestOutput::Path("live-bokor.png");
    if (CGame::Run(launch) != 0) { return 1; }
    std::printf("[local-live-check] bokor-live-entry=1\n");
    return 0;
}

int CheckLocalLive(SurvivalDeathFixture fixture, CInputPad *hud) {
    auto &scene = fixture.scene;
    auto &session = fixture.session;
    auto &bot = fixture.brother;
    auto &player = fixture.player;
    auto &vitals = fixture.vitals;
    if (!scene.IsLocalLive()) {
        const bool originalPolicy = dynamic_cast<ZLocalCoopBot *>(&bot) == nullptr;
        std::printf("[live-regression] solo-original-policy=%d\n", originalPolicy);
        if (!originalPolicy) { return 1; }
        if (player.IsCooperative() || session.GetLevel().IsCooperative() || !scene.Suicide()) { return 1; }
        for (unsigned elapsed = 0; elapsed < 8000 && !session.IsFinished(); elapsed += 16) { scene.Update(16, 0, 0, false); }
        if (!session.IsFinished() || bot.vitals.dead) { return 1; }
        std::printf("[local-live-check] selected-bot-solo=1 single-player-death=1\n");
        return 0;
    }
    if (dynamic_cast<ZLocalCoopBot *>(&bot) == nullptr) { return 1; }
    if (!CheckSteadyPeer(fixture.brotherModel)) { return 1; }
    if (
        !player.IsCooperative() || !session.GetLevel().IsCooperative()) { return 1; }
    vitals.invincible = true;
    bot.vitals.invincible = true;
    // Actual wave Flow must spawn enemies, acquire targets and emit gun shots.
    session.SetHud(nullptr);
    session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
    for (unsigned elapsed = 0; elapsed < 45000 && fixture.scene.GetShotCount() == 0; elapsed += 16) {
        session.Update(16, 0, 0, false);
    }
    if (scene.GetSpawnCount() == 0 || bot.GetTargetCount() == 0 || fixture.scene.GetShotCount() == 0) {
        std::printf("[local-live-check] no combat spawned=%u targets=%u shots=%zu\n", scene.GetSpawnCount(), bot.GetTargetCount(), fixture.scene.GetShotCount());
        return 1;
    }
    const float botBefore = bot.x;
    scene.GetPlayer().x += 350;
    for (unsigned elapsed = 0; elapsed < 1000; elapsed += 16) { session.Update(16, 0, 0, false); }
    if (bot.x == botBefore) { return 1; }
    std::printf("[local-live-check] spawned=%u targets=%u shots=%zu movement=1\n", scene.GetSpawnCount(), bot.GetTargetCount(), fixture.scene.GetShotCount());

    // Two weapon slots of one peer can both assist the same accepted kill.
    bool checkedAssist = false;
    for (auto &enemy : scene.GetEnemies()) {
        auto &target = *enemy;
        if (target.combat.dead || target.combat.health <= 0 || !target.CanReceiveProjectile(0, kBrotherCombatId)) { continue; }
        // Keep autonomous shots out of this attribution assertion, without
        // changing the original enemy template or accepting zero-damage hits.
        fixture.scene.Clear();
        bot.vitals.stunMs = 100;
        const auto id = target.combat.id;
        ZCombatHit assist;
        assist.owner = kBrotherCombatId;
        assist.weapon = fixture.brotherModel.gunResource;
        assist.damage = target.combat.health * 0.1f;
        assist.part = 0;
        scene.ApplyHit(id, assist);
        session.Update(16, 0, 0, false);
        assist.weaponSlot = 1;
        scene.ApplyHit(id, assist);
        session.Update(16, 0, 0, false);
        if (enemy->assistMask[1] != 3 || target.combat.dead) { std::printf("[local-live-check] assist mask=%u result=%u hp=%.1f\n", enemy->assistMask[1], unsigned(target.combat.collisionResult), target.combat.health); continue; }
        const auto assistsBefore = scene.GetMultiplayerStatistics(1).total.assists;
        const auto peerXpBefore = scene.GetPeerExperience();
        const auto peerOreBefore = scene.GetMultiplayerStatistics(1).total.xplodium;
        const auto killsBefore = scene.GetMultiplayerStatistics(0).total.kills;
        ZCombatHit fatal = assist;
        fatal.owner = kPlayerCombatId;
        fatal.weapon = player.gunResource;
        fatal.damage = 100000;
        scene.ApplyHit(id, fatal);
        session.Update(16, 0, 0, false);
        if (scene.GetMultiplayerStatistics(1).total.assists != assistsBefore + 2 ||
            scene.GetMultiplayerStatistics(0).total.kills != killsBefore + 1 ||
            scene.GetPeerExperience() != peerXpBefore || scene.GetMultiplayerStatistics(1).total.xplodium != peerOreBefore) { return 1; }
        checkedAssist = true;
        break;
    }
    bot.vitals.stunMs = 0;
    if (!checkedAssist) { std::printf("[local-live-check] no accepted assist fixture\n"); return 1; }
    std::printf("[local-live-check] two-slot-assist=1 killer-reward-isolation=1\n");

    // Exercise the real shared session gate, including a pending wave overlay.
    session.SetHud(hud);
    hud->BeginLiveWave(scene.GetMultiplayerStatistics(0), scene.GetMultiplayerStatistics(1));
    const auto waitBefore = hud->LiveWaveRemaining();
    const float pausedPlayerX = scene.GetPlayer().x, pausedPlayerY = scene.GetPlayer().y;
    const float pausedBotX = bot.x, pausedBotY = bot.y;
    // Corpses may already have been removed; never assume a live front().
    std::vector<std::pair<float, float>> pausedEnemies;
    for (const auto &enemy : scene.GetEnemies()) { pausedEnemies.push_back({enemy->combat.x, enemy->combat.y}); }
    const auto shotsBeforePause = fixture.scene.GetShotCount();
    session.SetSuspended(true);
    for (unsigned tick = 0; tick < 100; ++tick) { session.Update(16, 1, 1, true); }
    if (scene.GetEnemies().size() != pausedEnemies.size()) { return 1; }
    if (scene.GetPlayer().x != pausedPlayerX || scene.GetPlayer().y != pausedPlayerY || bot.x != pausedBotX || bot.y != pausedBotY ||
        fixture.scene.GetShotCount() != shotsBeforePause ||
        hud->LiveWaveRemaining() != waitBefore) { return 1; }
    for (unsigned index = 0; index < pausedEnemies.size(); ++index) {
        if (scene.GetEnemies()[index]->combat.x != pausedEnemies[index].first ||
            scene.GetEnemies()[index]->combat.y != pausedEnemies[index].second) { return 1; }
    }
    session.SetSuspended(false);
    for (unsigned tick = 0; tick < 60; ++tick) { session.Update(16, -1, 0, false); }
    if (scene.GetPlayer().x == pausedPlayerX && scene.GetPlayer().y == pausedPlayerY) { std::printf("[live-wait-check] player blocked\n"); return 1; }
    if (bot.x == pausedBotX && bot.y == pausedBotY) { std::printf("[live-wait-check] peer blocked\n"); return 1; }
    session.SetHud(nullptr);
    hud->ResetNotices();
    std::printf("[live-wait-check] both-move=1 shop-freezes-actors-projectiles-timer=1\n");
    std::fflush(stdout);

    scene.UpdatePeerIndicator(16, bot.x + 200, bot.y + 200, 400, 400);
    if (scene.PeerIndicator() == nullptr || scene.PeerIndicator()->type != 4 + fixture.brotherModel.brotherIndex) { return 1; }
    scene.UpdatePeerIndicator(200, bot.x - 100, bot.y - 100, 400, 400);
    if (scene.PeerIndicator() != nullptr) { return 1; }

    // The pause probe injected a wave overlay while enemies were still alive.
    // Start a fresh real wave before testing LEVEL kill accounting; otherwise
    // kills during that synthetic wait have no corresponding wave transition.
    session.Restart(fixture.startX, fixture.startY, fixture.startFacing);

    ZCombatHit clearWave;
    clearWave.owner = kPlayerCombatId;
    clearWave.damage = 100000;
    for (unsigned elapsed = 0; elapsed < 120000 && scene.GetClearedWaves() < 2; elapsed += 16) {
        session.Update(16, 0, 0, false);
        for (auto &enemy : scene.GetEnemies()) {
            if (!enemy->combat.dead) { scene.ApplyHit(enemy->combat.id, clearWave); }
        }
    }
    if (scene.GetClearedWaves() < 2 || scene.GetInvalidSpawnCount() != 0) {
        std::printf("[local-live-check] wave failure cleared=%u invalid=%u state=%d wave=%d tutorial=%d spawned=%u alive=%zu paused=%d\n",
            scene.GetClearedWaves(), scene.GetInvalidSpawnCount(), session.GetLevel().GetStateId(), session.GetLevel().GetWave(),
            session.GetLevel().GetTutorialStep(), scene.GetSpawnCount(), scene.AliveCount(), session.GetLevel().IsPaused());
        return 1;
    }
    std::printf("[local-live-check] cleared-waves=%u\n", scene.GetClearedWaves());

    // Isolate damage/revive from new waves while retaining real actor scripts.
    scene.GetEnemies().clear();
    scene.GetPlayer().x = fixture.startX;
    scene.GetPlayer().y = fixture.startY;
    bot.x = scene.GetPlayer().x + 50;
    bot.y = scene.GetPlayer().y;
    if (!scene.Suicide()) { return 1; }
    for (unsigned elapsed = 0; elapsed < 8000 && !vitals.deathAnimationComplete; elapsed += 16) { scene.Update(16, 0, 0, false); }
    if (!vitals.deathAnimationComplete || session.IsFinished()) { return 1; }
    for (unsigned elapsed = 0; elapsed < 4000; elapsed += 16) { scene.Update(16, 0, 0, false); }
    if (!vitals.dead || scene.GetReviveProgress() < 0.35f || scene.GetReviveProgress() > 0.5f) {
        std::printf("[local-live-check] rescue-hold failed progress=%.4f distance=%.1f dead=%d\n", scene.GetReviveProgress(), std::hypot(bot.x-scene.GetPlayer().x,bot.y-scene.GetPlayer().y),vitals.dead); return 1;
    }
    if (scene.GetReviveEffectState() != 2) { return 1; }
    if (!CaptureRescueEffect(fixture, "live-revive-in-range.png")) { return 1; }
    // Outside radius, progress is retained without a phantom revive.
    const float progressBefore = scene.GetReviveProgress();
    // A test teleport must remain on the playable side of the authored walls.
    // The old fixed +200 X position was inside a wall on this map.
    bool placed = false;
    for (unsigned direction = 0; direction < 16; ++direction) {
        const float angle = direction * 3.14159265f / 8;
        const float x = scene.GetPlayer().x + 200 * std::cos(angle);
        const float y = scene.GetPlayer().y + 200 * std::sin(angle);
        if (!scene.CanWalkTo(scene.GetPlayer().x, scene.GetPlayer().y, x, y) ||
            !scene.CanWalkTo(x, y, scene.GetPlayer().x, scene.GetPlayer().y)) { continue; }
        bot.x = x; bot.y = y; placed = true; break;
    }
    if (!placed) { return 1; }
    std::printf("[local-live-check] rescue input move=%d walk=%d origin=%.1f,%.1f destination=%.1f,%.1f\n",
        fixture.brotherModel.CanMove(), scene.CanBrotherWalk(bot.x, bot.y, scene.GetPlayer().x, scene.GetPlayer().y),
        bot.x, bot.y, scene.GetPlayer().x, scene.GetPlayer().y);
    scene.Update(16, 0, 0, false);
    if (scene.GetReviveEffectState() != 1) { return 1; }
    fixture.scene.AdvanceAmbientEffects(250);
    if (!CaptureRescueEffect(fixture, "live-revive-waiting.png")) { return 1; }
    if (scene.GetReviveProgress() != progressBefore) {
        std::printf("[local-live-check] outside-radius moved %.1f,%.1f progress=%.4f before=%.4f\n", bot.x, bot.y, scene.GetReviveProgress(), progressBefore); return 1;
    }
    for (unsigned elapsed = 0; elapsed < 14000 && vitals.dead; elapsed += 16) { scene.Update(16, 0, 0, false); }
    if (vitals.dead || scene.GetReviveCount() != 1 || vitals.health != vitals.maximum) {
        std::printf("[local-live-check] rescue-return failed progress=%.4f distance=%.1f dead=%d count=%u\n",scene.GetReviveProgress(), std::hypot(bot.x-scene.GetPlayer().x,bot.y-scene.GetPlayer().y), vitals.dead,scene.GetReviveCount()); return 1;
    }
    for (unsigned elapsed = 0; elapsed < 7000; elapsed += 16) { scene.Update(16, 0, 0, false); }
    if (scene.GetReviveEffectState() != 0) { return 1; }
    if (!player.CanMove() || !player.CanShoot()) { return 1; }
    // A distant rescue must cross authored navigation portals, not only an
    // unobstructed 200-unit line. This catches a bot stopping before a corner.
    auto *rescuePath = fixture.loaded.GetPathLayer(session.GetLevel().GetPathLayer());
    bool distantRescue = false;
    if (rescuePath != nullptr) {
        const int destination = rescuePath->FindNode(scene.GetPlayer().x, scene.GetPlayer().y);
        for (unsigned nodeIndex = 0; nodeIndex < rescuePath->GetNodes().size(); ++nodeIndex) {
            const auto &node = rescuePath->GetNodes()[nodeIndex];
            if (node.locked || std::hypot(node.x - scene.GetPlayer().x, node.y - scene.GetPlayer().y) < 500 ||
                scene.CanBrotherWalk(node.x, node.y, scene.GetPlayer().x, scene.GetPlayer().y) ||
                !scene.CanBrotherWalk(node.x, node.y, node.x, node.y) ||
                rescuePath->FindNext(static_cast<int>(nodeIndex), destination) < 0) { continue; }
            bot.x = node.x; bot.y = node.y;
            if (!scene.Suicide()) { return 1; }
            for (unsigned elapsed = 0; elapsed < 60000 && vitals.dead; elapsed += 16) { scene.Update(16, 0, 0, false); }
            std::printf("[live-rescue-path] node=%u dead=%d distance=%.1f bot=%.1f,%.1f\n",
                nodeIndex, vitals.dead, std::hypot(bot.x - scene.GetPlayer().x, bot.y - scene.GetPlayer().y), bot.x, bot.y);
            if (vitals.dead) { return 1; }
            distantRescue = true;
            break;
        }
    }
    if (!distantRescue) { std::printf("[live-rescue-path] no blocked connected fixture\n"); return 1; }
    for (unsigned elapsed = 0; elapsed < 7000; elapsed += 16) { scene.Update(16, 0, 0, false); }
    bot.x = scene.GetPlayer().x + 50;
    bot.y = scene.GetPlayer().y;
    if (!fixture.brotherModel.StartDeath()) { return 1; }
    for (unsigned elapsed = 0; elapsed < 20000 && bot.vitals.dead; elapsed += 16) { scene.Update(16, 0, 0, false); }
    if (bot.vitals.dead || scene.GetReviveCount() != 3) { return 1; }
    if (!scene.Suicide() || !fixture.brotherModel.StartDeath()) { return 1; }
    for (unsigned elapsed = 0; elapsed < 8000 && !session.IsDeathComplete(); elapsed += 16) { scene.Update(16, 0, 0, false); }
    if (!session.IsFinished()) { return 1; }
    session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
    if (session.IsFinished() || vitals.dead || bot.vitals.dead || scene.GetReviveCount() != 0) { return 1; }
    std::printf("[local-live-check] both-revives=1 radius=1 both-down=1 restart=1\n");
    return 0;
}
