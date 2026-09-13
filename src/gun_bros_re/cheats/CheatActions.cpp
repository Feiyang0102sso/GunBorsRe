/** Host cheat actions shared by the menu and survival entry points. */
#include "gun_bros_re/cheats/CheatActions.h"
#include "gun_bros_re/cheats/CheatCodes.h"
#include "gun_bros_re/ui/MenuInternal.h"
#include "gun_bros_re/gameplay/SurvivalSession.h"
#include "gun_bros_re/gameplay/PowerupScene.h"
#include "gun_bros_re/data/NativeProfile.h"
#include <ctime>

namespace GameCheats {
bool AdvanceChallenges(CProfileManager &profile, CChallengeManager &challenges, std::uint32_t now) {
    if (!profile.nativeArchive) {
        std::printf("[cheat] chupdate requires a native profile\n");
        return true;
    }
    auto &archive = *profile.nativeArchive;
    if (!challenges.InitProgressData(*archive.toc, *archive.tables, profile, now)) { return false; }
    // User-requested host shortcut. The original saved cycle remains the seed;
    // changing it does not alter the system clock or the daily bonus tracker.
    const std::uint64_t seconds = (std::uint64_t(challenges.cycleDay) + 1) * 86400 - 36000;
    if (seconds > UINT32_MAX) {
        std::printf("[cheat] chupdate exceeds the original timestamp range\n");
        return true;
    }
    if (!challenges.InitProgressData(*archive.toc, *archive.tables, profile, static_cast<unsigned>(seconds))) { return false; }
    std::printf("[cheat] chupdate day=%u count=%zu\n", challenges.cycleDay, challenges.current.size());
    return true;
}

std::uint64_t ExperienceTarget(const std::string &command,
    const CPlayerProgress::Template &data, const CPlayerProgress &progress) {
    // player_progression.bt; CPlayer::AddExperience :101185 uses >= thresholds.
    unsigned level = progress.GetLevel();
    if (command == LevelUp) { level = std::min(level + Levels, data.GetMaximumLevel()); }
    else if (command == MaximumLevel) { level = data.GetMaximumLevel(); }
    else { return progress.GetExperience(); }
    return std::max(progress.GetExperience(), data.GetExperienceForLevel(level));
}

bool UnlockAllWaves(CProfileManager &profile) {
    if (!profile.nativeArchive || profile.nativeArchive->tables == nullptr) {
        std::printf("[cheat] unlock requires native survival level bindings\n");
        return false;
    }
    auto cleared = profile.clearedWaves;
    for (unsigned slot = 0; slot < cleared.size(); ++slot) {
        const GameObjectRef &ref = profile.nativeArchive->survivalLevels[slot];
        std::vector<std::uint8_t> bytes;
        if (!profile.nativeArchive->tables->ReadSectionResource(ref.packHash, GameSection::Level, ref.localIndex, bytes)) { return false; }
        CArrayInputStream input(bytes);
        CLevel::Template level;
        // level_template.bt; CLevel::Bind :121717. CMissionWaveStatus::AddWaves
        // :192830 only raises progress; SaveNativeProfile writes collection 1003.
        if (!level.Init(input) || input.Available() != 0) {
            std::printf("[cheat] invalid LEVEL pack=%u index=%u\n", ref.packHash, ref.localIndex);
            return false;
        }
        cleared[slot] = std::max(cleared[slot], static_cast<unsigned>(level.waveLimit));
    }
    profile.clearedWaves = cleared;
    return true;
}
}

namespace MenuDetail {
/** User-authorized cht advances the native elapsed-day accumulator. The saved
 * launch timestamp stays on the real clock, so restarting cannot underflow it. */
void AdvanceDailyDebugDay(CProfileManager &profile, const CDailyBonusTracking &daily, std::uint32_t now) {
    if (!profile.nativeArchive) { profile.dailyDayOffset += GameCheats::Days; return; }
    daily.RefreshUsageData(profile, now);
    profile.dailyConsecutiveSeconds += 86400 * GameCheats::Days;
    profile.dailyConsecutiveDays = profile.dailyConsecutiveSeconds / 86400 + 1;
}
}

bool ProcessMenuCheats(CWindow &window, CProfileManager &profile, MenuDetail::MenuState &state,
    const CDailyBonusTracking &daily, const std::filesystem::path &savePath,
    const CPlayerProgress::Template &progressData, CPlayerProgress &progress) {
#if GB_ENABLE_CHEATS
    for (std::string cheat = window.TakeCheatCode(); !cheat.empty(); cheat = window.TakeCheatCode()) {
        if (cheat == GameCheats::Money) {
            profile.coins += GameCheats::Coins;
            profile.warbucks += GameCheats::Warbucks;
            char message[256];
            std::snprintf(message, sizeof(message), GameCheats::MoneyMessage, GameCheats::Coins, GameCheats::Warbucks);
            state.message = message;
        }
        if (cheat == GameCheats::NextDay) {
            MenuDetail::AdvanceDailyDebugDay(profile, daily, static_cast<std::uint32_t>(MenuDetail::CurrentSeconds()));
            state.Navigate(24);
        }
        if (cheat == GameCheats::ToggleDebug) { GameHostSettings().debugMode = !GameHostSettings().debugMode; }
        if (cheat == GameCheats::ToggleConnection) { GameHostSettings().isConnected = !GameHostSettings().isConnected; }
        if (cheat == GameCheats::UpdateChallenges) {
            if (!GameCheats::AdvanceChallenges(profile, state.social.challenges, static_cast<unsigned>(MenuDetail::CurrentSeconds()))) { return false; }
            state.social.contentBound = false;
            state.social.selectedChallenge = 0;
        }
        if (cheat == GameCheats::UnlockWaves) {
            if (!GameCheats::UnlockAllWaves(profile)) { return false; }
            state.message = GameCheats::UnlockMessage;
        }
        if (cheat == GameCheats::LevelUp || cheat == GameCheats::MaximumLevel) {
            progress.SetExperience(GameCheats::ExperienceTarget(cheat, progressData, progress));
            profile.experience = progress.GetExperience();
            char message[256];
            std::snprintf(message, sizeof(message), GameCheats::LevelMessage, progress.GetLevel());
            state.message = message;
        }
        if (!profile.SaveToDisk(savePath)) { return false; }
        std::printf("[cheat] %s\n", cheat.c_str());
    }
#endif
    return true;
}

bool ApplyCombatCheat(const std::string &cheat, CombatScene &scene, PlayerVitals &vitals,
    PowerupScene &powerups, SurvivalSession &session, SurvivalGameContext *context, CombatCheatResult &result,
    const CPlayerProgress::Template &progressData, CPlayerProgress &progress) {
#if GB_ENABLE_CHEATS
    if (cheat == GameCheats::ToggleDebug) { GameHostSettings().debugMode = !GameHostSettings().debugMode; }
    if (cheat == GameCheats::ToggleConnection) { GameHostSettings().isConnected = !GameHostSettings().isConnected; }
    if (cheat == GameCheats::BrotherWeapon) { scene.RequestBrotherWeaponSwap(); }
    if (cheat == GameCheats::Suicide && !powerups.IsMovieActive() && scene.Suicide()) {
        result.resume = true;
        std::printf("[death] suicide started\n");
    }
    if (cheat == GameCheats::Boss) {
        result.resume = session.SkipToBoss();
        result.resetClock = true;
    }
    if (cheat == GameCheats::LevelUp || cheat == GameCheats::MaximumLevel) {
        const std::uint64_t target = GameCheats::ExperienceTarget(cheat, progressData, progress);
        // Keep runtime XP authoritative, including research sessions without a profile.
        // The normal level-up path preserves both brothers' health fractions.
        while (progress.GetExperience() < target) {
            const auto amount = std::min<std::uint64_t>(target - progress.GetExperience(), UINT32_MAX);
            scene.AddExperience(static_cast<unsigned>(amount));
        }
        if (context != nullptr) { context->profile.experience = progress.GetExperience(); }
    }
    if (context != nullptr) {
        if (cheat == GameCheats::UpdateChallenges) {
            context->profile.experience = progress.GetExperience();
            CChallengeManager challenges;
            if (!GameCheats::AdvanceChallenges(context->profile, challenges, static_cast<unsigned>(std::time(nullptr)))) { return false; }
            result.challengesUpdated = !challenges.current.empty();
        }
        if (cheat == GameCheats::Money) { context->profile.coins += GameCheats::Coins; context->profile.warbucks += GameCheats::Warbucks; }
        if (cheat == GameCheats::NextDay) { context->profile.dailyDayOffset += GameCheats::Days; }
        if (cheat == GameCheats::UnlockWaves) {
            if (!GameCheats::UnlockAllWaves(context->profile)) { return false; }
        }
        if (!context->SaveProfile()) { return false; }
    }
    std::printf("[cheat] %s invincible=%d\n", cheat.c_str(), vitals.invincible);
#endif
    return true;
}
