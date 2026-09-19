#include "gun_bros_re/gameplay/game/CGameFlow.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/data/ZProfileStorage.h"
#include "gun_bros_re/gameplay/multiplayer/bot/ZLocalBotFriend.h"
// CGame::OnWaveCleared :76192 / UpdatePostGameStats :75868 orchestrate these writes.
// Payloads continue through saves/save_payloads.bt and the existing profile clients.
bool CGameFlow::UpdatePlayerProgress(const CPlayerProgress &progress, const CLevel &level,
                                     std::uint64_t &accountedXplodium, bool missionEnded) {
    if (!persistProgress) { return true; }
    // Deathmatch consumes account inventory but never advances survival waves.
    if (level.IsDeathmatch()) {
        profile.experience = progress.GetExperience();
        if (level.GetXplodium() < accountedXplodium) { accountedXplodium = 0; }
        profile.xplodium += level.GetXplodium() - accountedXplodium;
        accountedXplodium = level.GetXplodium();
        const unsigned kills = level.GetMultiplayerStatistics(0).total.kills;
        if (kills < accountedKills) { accountedKills = 0; }
        // CLevel::OnPlayerKilled :118693 increments DEATHMATCH_KILLS (37).
        profile.statistics[37] += kills - accountedKills;
        accountedKills = kills;
        if (botFriend != nullptr) {
            auto &bot = botFriend->profile;
            bot.experience = level.GetPeerExperience();
            const auto ore = level.GetMultiplayerStatistics(1).total.xplodium;
            if (ore < accountedPeerXplodium) { accountedPeerXplodium = 0; }
            bot.xplodium += ore - accountedPeerXplodium;
            accountedPeerXplodium = ore;
            const unsigned peerKills = level.GetMultiplayerStatistics(1).total.kills;
            if (peerKills < accountedPeerKills) { accountedPeerKills = 0; }
            bot.statistics[37] += peerKills - accountedPeerKills;
            accountedPeerKills = peerKills;
            if (!botFriend->Save()) { return false; }
        }
        return SaveProfile();
    }
    if (tutorial) {
        const int step = level.GetTutorialStep();
        if (step == -1) {
            profile.tutorialCompleted = true;
            profile.tutorialSteps |= 128;
        } else if (step < 8) {
            profile.tutorialSteps |= 1u << step;
        }
    }
    const unsigned wave = level.GetWave();
    profile.stat42Bits |= level.GetStat42Bits();
    profile.experience = progress.GetExperience();
    profile.xplodium += level.GetXplodium() - accountedXplodium;
    accountedXplodium = level.GetXplodium();
    if (hordeStart >= 0) {
        if (profile.nativeArchive) {
            if (!RecordMissionWaves(profile, missionLevel, wave, level.GetWavePerfectResults())) { return false; }
            if (missionEnded && !RecordMissionScore(profile, mission, level.GetScore())) { return false; }
        }
        const unsigned index = static_cast<unsigned>(hordeStart);
        profile.hordeBestKills[index] = std::max(profile.hordeBestKills[index], level.GetTotalKills());
        profile.hordeBestWave[index] = std::max(profile.hordeBestWave[index], wave);
        profile.hordeBestScore[index] = std::max(profile.hordeBestScore[index], level.GetScore());
    } else {
        profile.clearedWaves[planet] = std::max(profile.clearedWaves[planet], wave);
        const auto &perfectResults = level.GetWavePerfectResults();
        if (wave >= perfectResults.size()) {
            const unsigned firstWave = wave - static_cast<unsigned>(perfectResults.size());
            for (unsigned index = 0; index < perfectResults.size() && firstWave + index < 500; ++index) {
                if (perfectResults[index]) { profile.perfectedWaves[planet].set(firstWave + index); }
            }
        }
        if (level.GetTotalKills() < accountedKills) { accountedKills = 0; }
        profile.enemyKills[planet] += level.GetTotalKills() - accountedKills;
    }
    accountedKills = level.GetTotalKills();
    for (const auto &entry : level.GetWeaponProgress()) {
        const std::uint64_t key =
            (static_cast<std::uint64_t>(entry.resource.packHash) << 8) | entry.resource.localIndex;
        unsigned &credited = accountedWeaponExperience[key];
        if (entry.experience < credited) { credited = 0; }
        profile.AddWeaponExperience(entry.resource, entry.experience - credited, entry.maximum);
        credited = entry.experience;
    }
    result.kills = level.GetTotalKills();
    result.live = level.IsLocalLive();
    for (unsigned peer = 0; peer < 2; ++peer) { result.peers[peer] = level.GetMultiplayerStatistics(peer).total; }
    if (level.IsLocalLive() && botFriend != nullptr) {
        result.peerName = botFriend->name;
        auto &bot = botFriend->profile;
        bot.experience = level.GetPeerExperience();
        const auto ore = level.GetMultiplayerStatistics(1).total.xplodium;
        if (ore < accountedPeerXplodium) { accountedPeerXplodium = 0; }
        bot.xplodium += ore - accountedPeerXplodium;
        accountedPeerXplodium = ore;
        if (!botFriend->Save()) { return false; }
    }
    result.horde = hordeStart >= 0;
    result.score = level.GetScore();
    result.bestKillStreak = level.GetBestKillStreak();
    result.stopwatchMs = level.GetStopwatchTime();
    result.wavesPerRevolution = level.GetWavesPerRevolution();
    result.waveLimit = level.GetWaveLimit();
    if (hordeStart >= 0) { result.highScore = profile.hordeBestScore[hordeStart]; }
    result.wave = wave;
    result.waves = level.GetClearedWaves();
    result.perfectWaves = level.GetPerfectWaves();
    result.xplodium = level.GetXplodium();
    result.experience = progress.GetExperience() - startingExperience;
    result.casualties = level.GetCasualties();
    result.weapons = level.GetWeaponProgress();
    return profile.SaveToDisk(savePath);
}
