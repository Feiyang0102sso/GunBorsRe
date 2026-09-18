#include "gun_bros_re/debug/Capture.h"
#include "gameplay/SurvivalChecks.h"
#include "TestOutput.h"
using namespace MapDetail;

int CheckSurvivalRewards(SurvivalRewardsFixture fixture) {
    auto & checkFailures = fixture.checkFailures;
    auto & check = fixture.check;
    auto & toc = fixture.toc;
    auto & tables = fixture.tables;
    auto & enemies = fixture.enemies;
    auto & vitals = fixture.vitals;
    auto & progressData = fixture.progressData;
    auto & window = fixture.window;
    auto & survivalHud = fixture.survivalHud;
    auto & program = fixture.program;
    auto & loaded = fixture.loaded;
    auto & player = fixture.player;
    auto & scene = fixture.scene;

    if (check) {
        // A separate world exercises empty-wave minimums and damage rejection
        // without putting fixture currency into the actual survival/profile run.
        CLevel rewardProbe(toc, tables, program);
        rewardProbe.BindCombat(enemies, player, vitals, loaded.GetResources().playerTemplate->GetGameScale());
        CLevel::Template percentageTemplate;
        CMap percentageMap;
        rewardProbe.Bind(percentageTemplate, percentageMap);
        rewardProbe.Reset();
        rewardProbe.ResolveWaveReward(10);
        rewardProbe.ResolveWaveReward(100);
        if (rewardProbe.GetXplodium() != 2 || rewardProbe.GetPerfectWaves() != 2) { ++checkFailures; }
        ZCombatHit wound;
        wound.ownerType = 1;
        wound.damage = 0.25f;
        rewardProbe.ApplyHit(kPlayerCombatId, wound);
        rewardProbe.ResolveWaveReward(10);
        if (rewardProbe.GetLastWaveBonus() != 0 || rewardProbe.GetXplodium() != 2 ||
            rewardProbe.GetPerfectWaves() != 2 || rewardProbe.GetClearedWaves() != 3) { ++checkFailures; }
        rewardProbe.ResolveWaveReward(10);
        if (rewardProbe.GetLastWaveBonus() != 1 || rewardProbe.GetXplodium() != 3) { ++checkFailures; }
        // Render an actual credited reward, not a percentage estimate or a made-up HUD bonus.
        ZInputPadState rewardState;
        rewardState.health = rewardState.maximumHealth = vitals.maximum;
        rewardState.debugMap = "REWARD REGRESSION";
        rewardState.xplodium = rewardProbe.GetXplodium();
        rewardState.perfectBonus = rewardProbe.GetLastWaveBonus();
        rewardState.perfectWaves = rewardProbe.GetPerfectWaves();
        rewardState.clearedWaves = rewardProbe.GetClearedWaves();
        rewardState.lastWavePerfect = rewardProbe.GetWavePerfectResults().back();
        const ZHostSettings previousSettings = GameHostSettings();
        GameHostSettings().debugMode = true;
        GameHostSettings().drawDebugInfo = true;
        survivalHud.ResetNotices();
        survivalHud.OnWaveClear(4, true, 10, false);
        // Reach the second authored movie, then capture its readable middle.
        for (unsigned tick = 0; tick < 2000 && survivalHud.NoticeCount() == 2; ++tick) { survivalHud.Advance(16); }
        survivalHud.Advance(500);
        if (survivalHud.NoticeCount() != 1) { ++checkFailures; }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!survivalHud.Draw(rewardState) ||
            !Capture::SaveFrame(window, TestOutput::Path("perfect-wave-credited-bonus.png"))) { ++checkFailures; }
        survivalHud.ResetNotices();
        GameHostSettings() = previousSettings;
        std::printf("[survival-check] minimum/previous-bonus/damage/next-wave failures=%u\n", checkFailures);
        CPlayerProgress pickupProgress;
        pickupProgress.Bind(progressData);
        rewardProbe.SetPlayerProgress(&pickupProgress);
        CRefinementManager::Template pickupRefinement;
        if (!LoadRefinementTemplate(toc, tables, pickupRefinement)) { return 1; }
        CProfileManager pickupProfile;
        pickupProfile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), pickupRefinement);
        if (!rewardProbe.InitPickups(toc, tables, program, &pickupProfile)) { return 1; }
        GameObjectRef pickupRef;
        pickupRef.packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
        pickupRef.localIndex = 2;
        vitals.health = 1;
        rewardProbe.SpawnPickupAt(pickupRef, rewardProbe.GetPlayer().x, rewardProbe.GetPlayer().y);
        rewardProbe.UpdatePickups(16);
        if (vitals.health != vitals.maximum) { ++checkFailures; }
        pickupRef.localIndex = 0;
        rewardProbe.SpawnPickupAt(pickupRef, rewardProbe.GetPlayer().x, rewardProbe.GetPlayer().y);
        pickupRef.localIndex = 1;
        rewardProbe.SpawnPickupAt(pickupRef, rewardProbe.GetPlayer().x, rewardProbe.GetPlayer().y);
        pickupRef.localIndex = 7;
        rewardProbe.SpawnPickupAt(pickupRef, rewardProbe.GetPlayer().x, rewardProbe.GetPlayer().y);
        rewardProbe.UpdatePickups(16);
        rewardProbe.UpdatePickups(16);
        GameObjectRef grenade = pickupRef;
        grenade.localIndex = 13;
        if (pickupProgress.GetExperience() != 500 || rewardProbe.GetXplodium() != 153 ||
            rewardProbe.GetPickupCollectedCount() != 4 || rewardProbe.GetPickupCount() != 0 || rewardProbe.GetPickupFailureCount() != 0 ||
            pickupProfile.GetPowerupCount(grenade) != 1) { ++checkFailures; }
        CProfileManager restoredPickupProfile;
        restoredPickupProfile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), pickupRefinement);
        if (!pickupProfile.SaveToDisk(TestOutput::Path("pickup-profile-check.dat")) ||
            !restoredPickupProfile.LoadFromDisk(TestOutput::Path("pickup-profile-check.dat")) ||
            restoredPickupProfile.GetPowerupCount(grenade) != 1) { ++checkFailures; }
        std::printf("[pickup-check] health/experience/xplodium/grenade/save-once failures=%u\n", checkFailures);
        rewardProbe.Bind(percentageTemplate, percentageMap);
        const std::uint64_t beforePercentage = rewardProbe.GetXplodium();
        const std::int16_t percentage = 105;
        rewardProbe.FunctionResolver(56, &percentage, 1);
        for (unsigned award = 0; award < 20; ++award) { rewardProbe.AddXplodium(1); }
        if (rewardProbe.GetXplodium() != beforePercentage + 21) { ++checkFailures; }
        const std::int16_t increment = 95;
        rewardProbe.FunctionResolver(57, &increment, 1);
        rewardProbe.AddXplodium(1);
        if (rewardProbe.GetXplodium() != beforePercentage + 23) { ++checkFailures; }
        std::printf("[xplodium-check] fractional-carry=1 set-add-percent=1 failures=%u\n", checkFailures);
        // Three real enemy deaths distinguish player streak growth from a bro
        // assist. Expected points are 2E + 4E + 3E; only the first two grant XP.
        rewardProbe.Reset();
        pickupProgress.Bind(progressData);
        rewardProbe.SetPlayerProgress(&pickupProgress);
        rewardProbe.SetHorde(true);
        rewardProbe.Bind(percentageTemplate, percentageMap);
        vitals.invincible = true;
        unsigned expectedExperience = 0;
        for (unsigned death = 0; death < 3; ++death) {
            CEnemy *target = rewardProbe.Spawn(0, 600, 350);
            if (target == nullptr) { ++checkFailures; break; }
            const unsigned experience = static_cast<unsigned>(std::ceil(target->data->experienceReward * player.GetArmorMultiplier(3)));
            if (death == 0) { expectedExperience = experience; }
            ZCombatHit hit;
            hit.owner = kPlayerCombatId;
            if (death == 2) { hit.owner = kBrotherCombatId; }
            hit.ownerType = 0;
            hit.damage = 1000000;
            hit.applyArmorAttack = false;
            const ZCombatId targetId = target->combat.id;
            for (unsigned tick = 0; tick < 300 && !target->deathReported; ++tick) {
                rewardProbe.ApplyHit(targetId, hit);
                rewardProbe.Update(16, 0, 0, false);
            }
            if (!target->deathReported) { ++checkFailures; }
            const auto &texts = rewardProbe.GetExperienceTexts();
            if (texts.empty() || texts.back().amount != experience) { ++checkFailures; }
            std::printf("[xp-text-check] death=%u amount=%u visible=%zu failures=%u\n",
                death, experience, texts.size(), checkFailures);
        }
        if (expectedExperience == 0 || rewardProbe.GetScore() != expectedExperience * 9 ||
            rewardProbe.GetKillStreak() != 2 || pickupProgress.GetExperience() != expectedExperience * 2) { ++checkFailures; }
        std::printf("[horde-score-check] enemy-xp=%u points=%u expected=%u streak=%u xp=%llu failures=%u\n",
            expectedExperience, rewardProbe.GetScore(), expectedExperience * 9,
            rewardProbe.GetKillStreak(), pickupProgress.GetExperience(), checkFailures);
        // A standard-mode real death must draw the original XP string, float
        // in screen space, fade, expire and stay absent after restart.
        rewardProbe.Reset();
        rewardProbe.SetHorde(false);
        rewardProbe.SetTextView(400, 100, 2, 1.5f);
        CEnemy *xpTarget = rewardProbe.Spawn(0, 600, 350);
        if (xpTarget == nullptr) { return 1; }
        ZCombatHit xpHit;
        xpHit.owner = kPlayerCombatId;
        xpHit.ownerType = 0;
        xpHit.damage = 1000000;
        xpHit.applyArmorAttack = false;
        for (unsigned tick = 0; tick < 300 && !xpTarget->deathReported; ++tick) {
            rewardProbe.ApplyHit(xpTarget->combat.id, xpHit);
            rewardProbe.Update(16, 0, 0, false);
        }
        if (!xpTarget->deathReported || rewardProbe.GetExperienceTexts().size() != 1) { return 1; }
        const auto bornText = rewardProbe.GetExperienceTexts().front();
        const auto &deadState = xpTarget->combat;
        if (bornText.amount != expectedExperience || bornText.x != int((deadState.x - 400) * 2) ||
            bornText.y != int((deadState.y - 100) * 1.5f) || bornText.alpha != 1) { ++checkFailures; }
        rewardProbe.SetTextView(900, 700, 4, 3);
        rewardProbe.UpdateExperienceTexts(0);
        if (rewardProbe.GetExperienceTexts().front().y != bornText.y) { ++checkFailures; }
        int frameWidth = 0, frameHeight = 0;
        window.GetDrawableSize(frameWidth, frameHeight);
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(frameWidth) * frameHeight * 4);
        std::uint64_t previousLight = 0;
        for (unsigned stage = 0; stage < 3; ++stage) {
            if (stage > 0) { rewardProbe.UpdateExperienceTexts(1000); }
            if (stage == 1) {
                const auto &text = rewardProbe.GetExperienceTexts().front();
                if (text.x != bornText.x || text.y != bornText.y - 100 || text.alpha != 0.5f) { ++checkFailures; }
            }
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!survivalHud.DrawExperienceTexts(rewardProbe.GetExperienceTexts(), false)) { ++checkFailures; }
            // Inspect the isolated text before Capture adds the global presentation overlay.
            glReadPixels(0, 0, frameWidth, frameHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            if (!Capture::SaveFrame(window, TestOutput::Path("xp-text-") + std::to_string(stage * 1000) + ".png")) { ++checkFailures; }
            std::uint64_t light = 0;
            for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
                light += pixels[pixel] + pixels[pixel + 1] + pixels[pixel + 2];
            }
            if (stage == 0 && light == 0) { ++checkFailures; }
            if (stage == 1 && (light == 0 || light >= previousLight)) { ++checkFailures; }
            if (stage == 2 && (light != 0 || !rewardProbe.GetExperienceTexts().empty())) { ++checkFailures; }
            previousLight = light;
            std::printf("[xp-text-render-check] time=%u light=%llu failures=%u\n", stage * 1000, light, checkFailures);
        }
        rewardProbe.Update(16, 0, 0, false);
        if (!rewardProbe.GetExperienceTexts().empty()) { ++checkFailures; }
        rewardProbe.Reset();
        if (!rewardProbe.GetExperienceTexts().empty()) { ++checkFailures; }
        vitals.invincible = false;
    }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}
