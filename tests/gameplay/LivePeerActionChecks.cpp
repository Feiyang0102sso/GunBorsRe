/** Test-peer commands must not affect a real remote peer or the local account. */
#include "gameplay/SurvivalChecks.h"
#include "gun_bros_re/cheats/CheatActions.h"
#include "gun_bros_re/cheats/CheatConfig.h"
#include "TestOutput.h"

int CheckLivePeerActions(SurvivalDeathFixture fixture, CResTOCManager &toc, PackTables &tables,
    PowerupScene &playerPowerups, PowerupScene &peerPowerups, CProfileManager &peerProfile) {
    auto &scene = fixture.scene;
    auto &session = fixture.session;
    auto &bot = fixture.brother;
    CPlayerProgress::Template progressData;
    CPlayerProgress progress;
    const auto cheatSavePath = std::filesystem::path(TestOutput::Path("runtime-cheat-save"));
    SurvivalGameContext cheatContext{peerProfile, cheatSavePath};
    auto command = [&](const char *code, CombatCheatResult &result) {
        return ApplyCombatCheat(code, scene, fixture.vitals, playerPowerups, session, &cheatContext, result, progressData, progress);
    };
    scene.SetTestBot(false);
    CombatCheatResult result;
    for (const char *code : {"brow", "brok", "bror", "bros", "brop"}) {
        if (!command(code, result) || bot.vitals.dead || result.botShop || result.botPowerup) { return 1; }
    }
    if (std::filesystem::exists(cheatSavePath)) {
        std::printf("[live-peer-actions] runtime cheat unexpectedly saved account\n");
        return 1;
    }
    scene.SetTestBot(true);
    if (!command("bros", result) || result.botShop != scene.IsLocalLive()) { return 1; }
    if (!command("brop", result) || result.botPowerup != scene.IsLocalLive()) { return 1; }
    if (!command("brok", result) || !bot.vitals.dead || fixture.vitals.dead) { return 1; }
    if (!command("bror", result)) { return 1; }
    for (unsigned elapsed = 0; elapsed < 10000 && bot.vitals.dead; elapsed += 16) { scene.Update(16, 0, 0, false); }
    if (bot.vitals.dead || bot.vitals.health != bot.vitals.maximum) { return 1; }
    if (!scene.IsLocalLive()) {
        std::printf("[live-peer-actions] solo-cheats=1 real-peer-guard=1\n"); return 0;
    }
    session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
    const unsigned playerConsumed = playerPowerups.consumed;
    if (!peerPowerups.UseAny(true)) { return 1; }
    for (unsigned elapsed = 0; elapsed < 15000 && peerPowerups.IsMovieActive(); elapsed += 16) { session.Update(16, 0, 0, false); }
    if (peerPowerups.IsMovieActive() || playerPowerups.consumed != playerConsumed || peerPowerups.failures != 0) { return 1; }
    session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
    std::vector<PowerupEntry> catalog;
    if (!LoadPowerupCatalog(toc, tables, catalog)) { return 1; }
    GameObjectRef revive;
    for (const auto &entry : catalog) {
        if (entry.data.field112 == 0) { continue; }
        revive = entry.resource; break;
    }
    if (revive.IsNull()) { return 1; }
    peerProfile.AddPowerup(revive, 1);
    const unsigned before = peerProfile.GetPowerupCount(revive);
    scene.SetAfterDeathAvailability(false, true);
    if (!scene.Suicide() || !scene.KillTestBot()) { return 1; }
    for (unsigned elapsed = 0; elapsed < 10000 && (!fixture.vitals.deathAnimationComplete || !bot.vitals.deathAnimationComplete); elapsed += 16) {
        scene.Update(16, 0, 0, false);
    }
    if (session.IsDeathComplete() || !scene.NeedsDeathChoice(1) || !peerPowerups.UseAfterDeathPowerup()) { return 1; }
    scene.FinishDeathChoice(1);
    if (session.IsDeathComplete()) { return 1; }
    for (unsigned elapsed = 0; elapsed < 15000 && peerPowerups.IsMovieActive(); elapsed += 16) { session.Update(16, 0, 0, false); }
    if (bot.vitals.dead || bot.vitals.health != bot.vitals.maximum || !fixture.vitals.dead ||
        peerProfile.GetPowerupCount(revive) + 1 != before || peerPowerups.failures != 0 || session.IsDeathComplete()) {
        std::printf("[live-peer-actions] revive item failed dead=%d active=%d failures=%u\n", bot.vitals.dead, peerPowerups.IsMovieActive(), peerPowerups.failures); return 1;
    }
    session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
    if (!command("stsuicide", result) || !fixture.vitals.dead || std::filesystem::exists(cheatSavePath)) { return 1; }
    std::printf("[live-peer-actions] runtime-cheats-no-account-save=1\n");
    std::printf("[live-peer-actions] live-cheats=1 real-peer-guard=1 powerup-owner=1 both-down-item-revive=1\n");
    return 0;
}
