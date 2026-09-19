/** CGame session host implementation; original ownership follows game.cpp.
 * SDL/GL submission and borrowed desktop resources are host adaptations.
 */
#include "gun_bros_re/gameplay/game/CGameRuntime.h"
using namespace MapDetail;

int CGame::Session::Notify(ZGameObserver::FramePhase phase) {
    if (launch.observer == nullptr) { return -1; }
    return launch.observer->OnFrame(phase, frame);
}
int CGame::Session::Notify(ZGameObserver::Stage phase) {
    if (launch.observer == nullptr) { return -1; }
    return launch.observer->OnStage(phase, *this);
}

int CGame::Session::Run() {
    // Ready observers may have advanced the level or changed the equipped items.
    leftPowerup = powerups.GetEquipped(0);
    rightPowerup = powerups.GetEquipped(1);
    lastSavedTutorialStep = session.GetLevel().GetTutorialStep();
    if (launch.deathmatch && !LoadStoreCatalog(toc, tables, matchStore)) { return 1; }
    if (launch.deathmatch && !survivalHud.ConfigureDeathmatch(matches[launch.matchIndex].data.stores)) { return 1; }
    if (const int result = Notify(ZGameObserver::FramePhase::Begin); result >= 0) { return result; }
    previous = window.GetTicksMs();
    menuTicks = previous;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    std::printf("[game] input ready (bindings: gameplay/game/ZGameKeys.h)\n");
    if (const int result = Notify(ZGameObserver::Stage::LoopStarting); result >= 0) { return result; }
    while (window.PumpEvents()) {
        if (const int result = Notify(ZGameObserver::FramePhase::Starting); result >= 0) { return result; }
        frameTicks = window.GetTicksMs();
        menuElapsed = static_cast<unsigned>(frameTicks - menuTicks);
        int result = UpdateShop();
        if (result >= 0) { return result; }
        result = ProcessInput();
        if (result == -3) { break; }
        if (result >= 0) { return result; }
        result = Update();
        if (result == -3) { break; }
        if (result >= 0) { return result; }
        if (const int result = Notify(ZGameObserver::FramePhase::Updated); result >= 0) { return result; }
        result = Draw();
        if (result >= 0) { return result; }
        window.Present();
        if (result == -2) { continue; }
        if (const int result = Notify(ZGameObserver::FramePhase::Presented); result >= 0) { return result; }
    }
    return Finish();
}

int CGame::Session::Finish() {
    if (!session.SubmitChallenges(true)) { return 1; }
    if (launch.deathmatch && gameContext != nullptr) {
        if (match.GetResult() == CMPMatch::Result::Playing) { match.Surrender(0); }
        auto &result = gameContext->result;
        result.live = true;
        result.deathmatch = true;
        result.matchResult = static_cast<unsigned>(match.GetResult());
        result.matchKillLimit = match.Data().killLimit;
        result.matchTimeLimitSeconds = match.Data().seconds;
        result.score = scene.GetScore();
        result.bestKillStreak = scene.GetBestKillStreak();
        result.peerName = brotherName;
        for (unsigned peer = 0; peer < 2; ++peer) {
            result.peers[peer] = scene.GetMultiplayerStatistics(peer).total;
            result.peers[peer].kills = match.Score(peer);
            result.peers[peer].deaths = match.GetLife(peer).serial;
            if (match.GetLife(peer).dead) { ++result.peers[peer].deaths; }
        }
        if (launch.botFriend != nullptr && gameContext->persistProgress && !launch.botFriend->Save()) { return 1; }
    }
    if (!CGame::SaveProgress(gameContext, progress, session.GetLevel(), accountedXplodium)) { return 1; }
    return 0;
}
