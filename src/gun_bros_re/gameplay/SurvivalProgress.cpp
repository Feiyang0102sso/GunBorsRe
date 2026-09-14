#include "gun_bros_re/gameplay/MapWorldInternal.h"
#include "gun_bros_re/data/LocalBotFriend.h"
using namespace MapDetail;
bool SaveSurvivalProgress(SurvivalGameContext *context, const CPlayerProgress &progress,
    const CombatScene &scene, const CLevel &level, std::uint64_t &accountedXplodium, bool missionEnded ) {
    if (context == nullptr || !context->persistProgress) { return true; }
    CProfileManager &profile = context->profile;
    if (context->tutorial) {
        const int step = level.GetTutorialStep();
        if (step == -1) { profile.tutorialCompleted = true; profile.tutorialSteps |= 128; }
        else if (step < 8) { profile.tutorialSteps |= 1u << step; }
    }
    const unsigned wave = level.GetWave();
    profile.stat42Bits |= level.GetStat42Bits();
    profile.experience = progress.GetExperience();
    profile.xplodium += scene.GetXplodium() - accountedXplodium;
    accountedXplodium = scene.GetXplodium();
    if (context->hordeStart >= 0) {
        if (profile.nativeArchive) {
            if (!RecordNativeMissionWaves(profile, context->missionLevel, wave, scene.GetWavePerfectResults())) { return false; }
            if (missionEnded && !RecordNativeMissionScore(profile, context->mission, scene.GetScore())) { return false; }
        }
        const unsigned index = static_cast<unsigned>(context->hordeStart);
        profile.hordeBestKills[index] = std::max(profile.hordeBestKills[index], scene.GetTotalKills());
        profile.hordeBestWave[index] = std::max(profile.hordeBestWave[index], wave);
        profile.hordeBestScore[index] = std::max(profile.hordeBestScore[index], scene.GetScore());
    } else {
        profile.clearedWaves[context->planet] = std::max(profile.clearedWaves[context->planet], wave);
        const auto &perfectResults = scene.GetWavePerfectResults();
        if (wave >= perfectResults.size()) {
            const unsigned firstWave = wave - static_cast<unsigned>(perfectResults.size());
            for (unsigned index = 0; index < perfectResults.size() && firstWave + index < 500; ++index) {
                if (perfectResults[index]) { profile.perfectedWaves[context->planet].set(firstWave + index); }
            }
        }
        if (scene.GetTotalKills() < context->accountedKills) { context->accountedKills = 0; }
        profile.enemyKills[context->planet] += scene.GetTotalKills() - context->accountedKills;
    }
    context->accountedKills = scene.GetTotalKills();
    for (const auto &entry : scene.GetWeaponProgress()) {
        const std::uint64_t key = (static_cast<std::uint64_t>(entry.resource.packHash) << 8) | entry.resource.localIndex;
        unsigned &credited = context->accountedWeaponExperience[key];
        if (entry.experience < credited) { credited = 0; }
        profile.AddWeaponExperience(entry.resource, entry.experience - credited, entry.maximum);
        credited = entry.experience;
    }
    context->result.kills = scene.GetTotalKills();
    context->result.live = scene.IsLocalLive();
    for (unsigned peer = 0; peer < 2; ++peer) { context->result.peers[peer] = scene.GetMultiplayerStatistics(peer).total; }
    if (scene.IsLocalLive() && context->botFriend != nullptr) {
        context->result.peerName = context->botFriend->name;
        auto &bot = context->botFriend->profile;
        bot.experience = scene.GetPeerExperience();
        const auto ore = scene.GetMultiplayerStatistics(1).total.xplodium;
        if (ore < context->accountedPeerXplodium) { context->accountedPeerXplodium = 0; }
        bot.xplodium += ore - context->accountedPeerXplodium;
        context->accountedPeerXplodium = ore;
        if (!context->botFriend->Save()) { return false; }
    }
    context->result.horde = context->hordeStart >= 0;
    context->result.score = scene.GetScore();
    context->result.bestKillStreak = scene.GetBestKillStreak();
    context->result.stopwatchMs = level.GetStopwatchTime();
    context->result.wavesPerRevolution = level.GetWavesPerRevolution();
    context->result.waveLimit = level.GetWaveLimit();
    if (context->hordeStart >= 0) { context->result.highScore = profile.hordeBestScore[context->hordeStart]; }
    context->result.wave = wave;
    context->result.waves = scene.GetClearedWaves();
    context->result.perfectWaves = scene.GetPerfectWaves();
    context->result.xplodium = scene.GetXplodium();
    context->result.experience = progress.GetExperience() - context->startingExperience;
    context->result.casualties = scene.GetCasualties();
    context->result.weapons = scene.GetWeaponProgress();
    return profile.SaveToDisk(context->savePath);
}
