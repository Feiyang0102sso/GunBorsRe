#include "gun_bros_re/gameplay/brother/bot/ZLocalCoopBot.h"
/** Hard DM uses real PvP rules and Flow with virtual inventory; Easy stays limited. */
#include "gameplay/SurvivalChecks.h"
#include "gun_bros_re/gameplay/brother/bot/ZLocalPVPBot.h"
#include "engine/core/CStringToKey.h"

int CheckDeathmatchBotDifficulty(SurvivalDeathFixture fixture, CResTOCManager &toc, ZPackTables &tables,
    CMPMatch &match, CPowerUpSelector &powerups, CProfileManager &profile) {
    std::uint32_t powerupChoice = 0;
    auto &session = fixture.session;
    auto &scene = fixture.scene;
    auto &bot = static_cast<ZLocalPVPBot &>(fixture.brother);
    const auto previousInventory = profile.powerups;
    profile.powerups.clear();
    // Run the same real inventory/cooldown checks for both unlimited difficulties.
    for (const auto difficulty : {CMPMatch::BotLevel::Normal, CMPMatch::BotLevel::Hard}) {
        match.SetBotLevel(difficulty);
        session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
        if (!scene.RespawnDeathmatch(0, true) || !scene.RespawnDeathmatch(1, true)) { return 1; }
        fixture.vitals.invincible = true; bot.vitals.invincible = true;
        std::vector<ZPowerupEntry> catalog;
        std::vector<ZStoreEntry> store;
        std::vector<ZWeaponEntry> weapons;
        if (!LoadPowerupCatalog(toc, tables, catalog) || !LoadStoreCatalog(toc, tables, store)) { return 1; }
        if (!LoadWeaponCatalog(toc, tables, weapons)) { return 1; }
        const ZWeaponEntry *chosen[2]{};
        for (unsigned slot = 0; slot < 2; ++slot) {
            const auto &gun = scene.MatchGun(1, slot);
            for (const auto &weapon : weapons) {
                if (weapon.packHash == gun.packHash && weapon.ordinal == gun.localIndex) { chosen[slot] = &weapon; break; }
            }
            if (chosen[slot] == nullptr) { return 1; }
        }
        bot.Configure(42, *chosen[0], *chosen[1], difficulty);
        unsigned allowed = 0, excluded = 0;
        for (const auto &entry : catalog) {
            const CStoreItem *rule = nullptr;
            for (const auto &item : store) {
                if (item.data.type < 10 || item.data.type > 13 || item.data.objects.empty()) { continue; }
                const auto &reference = item.data.objects.front();
                if (reference.type == 17 && reference.object.packHash == entry.resource.packHash &&
                    reference.object.localIndex == entry.resource.localIndex) { rule = &item.data; break; }
            }
            if (rule == nullptr) { continue; }
            bool legal = !rule->IsExcludedFromGameType(2) && entry.data.field112 == 0;
            if (difficulty == CMPMatch::BotLevel::Normal) {
                const auto id = entry.resource.localIndex;
                legal = legal && entry.resource.packHash == CStringToKey("pack5") && (id == 1 || id == 8 || id == 9 || id == 13);
            }
            if (powerups.SelectResource(entry.resource) != legal) { return 1; }
            if (legal) {
                ++allowed;
                if (powerups.GetCount() == 0 || profile.GetPowerupCount(entry.resource) != 0) { return 1; }
            } else { ++excluded; }
        }
        // Drive the same timed decision method as the game loop, beyond Easy's two heals.
        for (unsigned use = 0; use < 5; ++use) {
            bot.vitals.health = 1;
            bot.UsePowerups(powerups);
            if (bot.vitals.health <= 1 || bot.WantsShop()) { return 1; }
            bot.vitals.health = 1;
            bot.UsePowerups(powerups);
            if (bot.vitals.health != 1) { return 1; }
            for (unsigned elapsed = 0; elapsed < 512; elapsed += 16) { session.Update(16, 0, 0, false); }
        }
        GameObjectRef grenade; grenade.packHash = CStringToKey("pack5"); grenade.localIndex = 13;
        for (unsigned use = 0; use < 3; ++use) {
            bool requested = false;
            const auto previousUses = match.GetLife(1).grenades;
            for (unsigned elapsed = 0; elapsed < 6000 && match.GetLife(1).grenades == previousUses; elapsed += 16) {
                if (!requested) { requested = ZLocalPVPBot::UseMatchConsumable(powerups, true); }
                session.Update(16, 0, 0, false);
            }
            if (!requested || match.GetLife(1).grenades != previousUses + 1 ||
                !powerups.SelectResource(grenade) || powerups.UseSelected()) { return 1; }
            const int cooldown = powerups.Cooldowns().at(grenade.localIndex);
            if (cooldown <= 0) { return 1; }
            for (int elapsed = 0; elapsed < cooldown + 1000; elapsed += 16) { session.Update(16, 0, 0, false); }
        }
        // The shared automatic path must select and execute a PvP item beyond Easy's set.
        bool usedOther = false;
        for (unsigned attempt = 0; attempt < 30 && !usedOther; ++attempt) {
            if (ZLocalCoopBot::UseAnyPowerup(powerups, powerupChoice)) {
                const auto id = powerups.GetSelected()->resource.localIndex;
                usedOther = id != 1 && id != 8 && id != 9 && id != 13;
            }
            for (unsigned elapsed = 0; elapsed < 1000; elapsed += 16) { session.Update(16, 0, 0, false); }
        }
        if (usedOther != (difficulty == CMPMatch::BotLevel::Hard) || !profile.powerups.empty() ||
            powerups.failures != 0 || match.CanShop(1) || match.EnterShop(1)) { return 1; }
        // Changing a DM rule cannot grant stock when the same peer host runs Live.
        powerups.SetDeathmatch(nullptr);
        if (!powerups.SelectResource(grenade) || powerups.GetCount() != 0 || powerups.UseSelected()) { return 1; }
        powerups.SetDeathmatch(&match);
        match.SetBotLevel(CMPMatch::BotLevel::Easy);
        bot.Configure(42, *chosen[0], *chosen[1], CMPMatch::BotLevel::Easy);
        session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
        if (!scene.RespawnDeathmatch(0, true) || !scene.RespawnDeathmatch(1, true) ||
            !powerups.SelectResource(grenade) || powerups.GetCount() != 0 || powerups.UseSelected()) { return 1; }
        std::printf("[dm-bot] level=%u allowed=%u excluded=%u heals=5 grenades=3 extra-item=%d cooldown=1 no-stock-write=1 no-shop=1\n",
            static_cast<unsigned>(difficulty), allowed, excluded, usedOther);
    }
    profile.powerups = previousInventory;
    // Reuse the production exit gate for a local final death, a double kill, and surrender.
    for (unsigned scenario = 0; scenario < 3; ++scenario) {
        session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
        if (!scene.RespawnDeathmatch(0, true) || !scene.RespawnDeathmatch(1, true) || session.IsDeathmatchFading()) { return 1; }
        if (scenario == 2) { match.Surrender(0); }
        else {
            for (unsigned peer = 0; peer <= scenario; ++peer) {
                while (match.Score(1 - peer) + 1 < match.Data().killLimit) {
                    if (!match.Kill(peer, 1 - peer) || !match.Respawn(peer, true)) { return 1; }
                }
            }
            if (!fixture.player.StartDeath()) { return 1; }
            if (scenario == 1 && !fixture.brotherModel.StartDeath()) { return 1; }
            session.Update(16, 0, 0, false);
        }
        if (!session.IsFinished() || session.IsReadyForResults()) { return 1; }
        const unsigned score0 = match.Score(0), score1 = match.Score(1);
        for (unsigned elapsed = 0; elapsed < 10000 && !session.IsReadyForResults(); elapsed += 16) {
            session.Update(16, 1, 1, true);
            if (session.IsDeathmatchFading() && scenario < 2 &&
                (!fixture.vitals.deathAnimationComplete || fixture.scene.HasActorBurst(kPlayerCombatId))) { return 1; }
        }
        if (!session.IsReadyForResults() || score0 != match.Score(0) || score1 != match.Score(1) ||
            (scenario == 1 && (!bot.vitals.deathAnimationComplete || match.GetResult() != CMPMatch::Result::Draw))) { return 1; }
    }
    std::printf("[dm-ending] local-final=1 double-final=1 surrender=1 restart-reset=1\n");
    return 0;
}
