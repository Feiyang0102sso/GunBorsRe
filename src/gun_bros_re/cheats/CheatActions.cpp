/** Host cheat actions shared by the menu and survival entry points. */
#include "gun_bros_re/cheats/CheatActions.h"
#include "gun_bros_re/cheats/CheatCodes.h"
#include "gun_bros_re/ui/ZMenuInternal.h"
#include "gun_bros_re/gameplay/CGame.h"
#include "gun_bros_re/ui/CPowerUpSelector.h"
#include "gun_bros_re/data/ZProfileStorage.h"
#include <ctime>

namespace GameCheats {
bool ApplyRefineryCheat(const std::string &command, CProfileManager &profile, std::int64_t now) {
    if (command == Xplodium) { profile.xplodium += XplodiumAmount; }
    if (command == AdvanceRefinery) {
        // Native record 1008 saves remaining time against a current checkpoint.
        // Keep the original start and total duration; only advance remaining work.
        for (auto &slot : profile.refinery.slots) {
            if (slot.state != 2) { continue; }
            slot.finishTime -= RefinerySkipSeconds;
            slot.finishTimeMs -= RefinerySkipSeconds * 1000;
        }
        profile.refinery.UpdateRefinement(now);
    }
    if (command != ToggleRefineryLocks) { return true; }
    if (!profile.nativeArchive) { return false; }
    CRefinementManager::Template data;
    auto &archive = *profile.nativeArchive;
    if (!LoadRefinementTemplate(*archive.toc, *archive.tables, data)) { return false; }
    bool unlock = false;
    for (unsigned index = 0; index < data.minutes.size(); ++index) {
        if (data.minutes[index] != 0 && profile.refinery.slots[index].state == 0) { unlock = true; }
    }
    for (unsigned index = 0; index < data.minutes.size(); ++index) {
        // Previously only paid chambers participated. The user extended this to
        // standard chambers too; BIG's zero-duration first slots remain intact.
        if (data.minutes[index] == 0) { continue; }
        auto &slot = profile.refinery.slots[index];
        if (unlock) {
            if (slot.state == 0) { slot.state = 1; }
        } else {
            // Cancelling a chamber returns its input, never its unclaimed yield.
            profile.xplodium += slot.amount;
            slot = CRefinementManager::CRefinementSlot{};
        }
    }
    std::printf("[cheat] refinery unlocked=%d\n", unlock);
    return true;
}

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
        if (!profile.nativeArchive->tables->ReadSectionResource(ref.packHash, ZGameSection::Level, ref.localIndex, bytes)) { return false; }
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

bool ProcessMenuCheats(ZWindow &window, CProfileManager &profile, MenuDetail::ZMenuState &state,
    const CDailyBonusTracking &daily, const std::filesystem::path &savePath,
    const CPlayerProgress::Template &progressData, CPlayerProgress &progress) {
    for (std::string cheat = window.TakeCheatCode(); !cheat.empty(); cheat = window.TakeCheatCode()) {
        if (!GameCheats::ApplyRefineryCheat(cheat, profile, MenuDetail::CurrentSeconds())) { return false; }
        if (cheat == GameCheats::ToggleRefineryLocks && state.refinery.refineryTransfer >= 0 &&
            profile.refinery.slots[state.refinery.refineryTransfer].state == 0) {
            state.refinery.refineryCancelTransfer = true;
        }
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
    return true;
}

bool ApplyCombatCheat(const std::string &cheat, CLevel &scene, ZPlayerVitals &vitals,
    CPowerUpSelector &powerups, CGame &session, ZSurvivalGameContext *context, CombatCheatResult &result,
    const CPlayerProgress::Template &progressData, CPlayerProgress &progress) {
    if (cheat == GameCheats::ToggleDebug) { GameHostSettings().debugMode = !GameHostSettings().debugMode; }
    if (cheat == GameCheats::ToggleConnection) { GameHostSettings().isConnected = !GameHostSettings().isConnected; }
    if (scene.HasLocalBot()) {
        if (cheat == GameCheats::BrotherWeapon) { scene.RequestBrotherWeaponSwap(); }
        if (cheat == GameCheats::BrotherKill) { scene.KillTestBot(); }
        if (cheat == GameCheats::BrotherRevive) { scene.ReviveTestBot(); }
        result.botShop = cheat == GameCheats::BrotherShop && scene.IsLocalLive();
        result.botPowerup = cheat == GameCheats::BrotherPowerup && scene.IsLocalLive();
    }
    if (cheat == GameCheats::Suicide && !powerups.GetPowerup().IsPresentationActive() && scene.Suicide()) {
        result.resume = true;
        std::printf("[death] suicide started\n");
    }
    if (cheat == GameCheats::Boss) {
        result.resume = session.StartBossSkip();
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
        if (!GameCheats::ApplyRefineryCheat(cheat, context->profile, static_cast<std::int64_t>(std::time(nullptr)))) { return false; }
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
        // Runtime-only cheats must not serialize every native account record
        // on the input thread. Wave/death progress has its normal save boundary.
        bool profileChanged = false;
        for (const char *command : {GameCheats::Money, GameCheats::NextDay, GameCheats::Xplodium,
            GameCheats::AdvanceRefinery, GameCheats::ToggleRefineryLocks, GameCheats::UpdateChallenges,
            GameCheats::UnlockWaves, GameCheats::LevelUp, GameCheats::MaximumLevel}) {
            if (cheat == command) { profileChanged = true; break; }
        }
        if (profileChanged && !context->SaveProfile()) { return false; }
    }
    std::printf("[cheat] %s invincible=%d\n", cheat.c_str(), vitals.invincible);
    return true;
}
