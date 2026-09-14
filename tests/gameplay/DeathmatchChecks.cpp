/** BIG match validation and authoritative life budget regressions. */
#include "gun_bros_re/gameplay/CMPMatch.h"
#include "gun_bros_re/data/PlanetCatalog.h"
#include "gun_bros_re/gameplay/DeathmatchBot.h"
#include <cstdio>
#include "gun_bros_re/gameplay/CPickup.h"
#include "engine/core/CStringToKey.h"

int RunDeathmatchDataCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    // Resolve the death-drop native actions from the original pickup scripts.
    for (unsigned index = 0; index < 2; ++index) {
        std::vector<std::uint8_t> bytes;
        if (!tables.ReadSectionResource(CStringToKey("pack5"), GameSection::Pickup, index, bytes)) { return 1; }
        CArrayInputStream input(bytes);
        CPickup::Template data;
        if (!data.Init(input)) { return 1; }
        CPickup pickup;
        pickup.SetDeathmatch(true); pickup.Bind(data);
        if (!pickup.Collect()) { return 1; }
        for (const auto &action : pickup.TakeActions()) {
            std::printf("[deathmatch-drop] pickup=%u action=%u amount=%d\n", index, static_cast<unsigned>(action.kind), action.amount);
        }
    }
    std::vector<StoreEntry> store;
    if (!LoadStoreCatalog(toc, tables, store)) { return 1; }
    CProfileManager shopper;
    CMPMatch::Life life;
    if (DeathmatchBot::ChoosePurchase(store, shopper, 99, life) != nullptr) { return 1; }
    shopper.coins = 100000; shopper.warbucks = 100000;
    unsigned purchases = 0;
    while (const auto *item = DeathmatchBot::ChoosePurchase(store, shopper, 99, life)) {
        if (++purchases > 4 || shopper.AcquireItem(item->data, 99) != PurchaseResult::Purchased) { return 1; }
    }
    if (purchases != 4 || shopper.statistics[12] != 4) { return 1; }
    shopper.powerups.clear(); life.grenades = 2; life.healthPacks = 2;
    if (DeathmatchBot::ChoosePurchase(store, shopper, 99, life) != nullptr) { return 1; }
    std::vector<CMPMatch::Entry> entries;
    if (!LoadMPMatches(toc, tables, entries) || entries.size() != 5) { return 1; }
    const unsigned originalHealth[] = {120, 250, 280, 360, 470};
    for (unsigned index = 0; index < entries.size(); ++index) {
        const auto &entry = entries[index];
        if (entry.data.health != originalHealth[index] || entry.data.killLimit != 3 || entry.data.seconds != 0) { return 1; }
        CMPMatch match;
        match.Bind(entry.data, 42);
        int previous = -1;
        for (unsigned roll = 0; roll < 10000; ++roll) {
            const int picked = match.ChoosePickup();
            if (picked < 0 || picked == previous || picked >= static_cast<int>(entry.data.pickups.size())) { return 1; }
            previous = picked;
        }
        if (!match.EnterShop(1) || !match.EnterShop(1) || match.EnterShop(1)) { return 1; }
        for (bool grenade : {false, true}) {
            for (unsigned use = 0; use < 2; ++use) {
                if (!match.CanUse(1, grenade)) { return 1; }
                match.CommitUse(1, grenade);
            }
            if (match.CanUse(1, grenade)) { return 1; }
        }
        for (unsigned use = 0; use < 5; ++use) {
            if (!match.EnterShop(0) || !match.CanUse(0, true)) { return 1; }
            match.CommitUse(0, true);
        }
        if (!match.Kill(1, 0) || match.Kill(1, 0) || match.CanShop(1)) { return 1; }
        match.Update(entry.data.respawnSeconds * 1000 - 1);
        if (match.Respawn(1)) { return 1; }
        match.Update(1);
        if (!match.Respawn(1) || !match.CanShop(1) || !match.CanUse(1, true) || match.GetLife(1).serial != 1) { return 1; }
        for (unsigned kill = 1; kill < entry.data.killLimit; ++kill) {
            if (!match.Kill(1, 0)) { return 1; }
            match.Update(entry.data.respawnSeconds * 1000);
            if (kill + 1 < entry.data.killLimit && !match.Respawn(1)) { return 1; }
        }
        if (match.GetResult() != CMPMatch::Result::PlayerWon || match.Respawn(1)) { return 1; }
        auto timed = entry.data;
        timed.seconds = 1;
        match.Bind(timed, 1);
        match.Update(1000);
        if (match.GetResult() != CMPMatch::Result::Draw) { return 1; }
        match.Restart();
        if (!match.Kill(0, -1) || match.Score(1) != 1) { return 1; }
        timed.killLimit = 1;
        match.Bind(timed, 1);
        if (!match.Kill(0, 1) || !match.Kill(1, 0)) { return 1; }
        match.Update(16);
        if (match.GetResult() != CMPMatch::Result::Draw) { return 1; }
        std::printf("[deathmatch-check] tier=%u draws=10000 budgets=1 respawn=1 score=1 time=1\n", index);
        for (const auto difficulty : {CMPMatch::BotLevel::Normal, CMPMatch::BotLevel::Hard}) {
            match.Bind(entry.data, 42);
            match.SetBotLevel(difficulty);
            for (unsigned use = 0; use < 10; ++use) {
                if (match.CanShop(1) || match.EnterShop(1) || !match.CanShop(0) ||
                    !match.CanUse(1, true) || !match.CanUse(1, false)) { return 1; }
                match.CommitUse(1, true); match.CommitUse(1, false);
            }
            if (!match.Kill(1, 0) || match.CanUse(1, false) || !match.Respawn(1, true)) { return 1; }
            match.Restart();
            if (!match.HasUnlimitedBotPowerups() || match.HasHardBot() != (difficulty == CMPMatch::BotLevel::Hard) ||
                match.CanShop(1) || !match.CanUse(1, true)) { return 1; }
        }
    }
    std::vector<PlanetEntry> planets;
    if (!LoadPlanetCatalog(toc, tables, planets)) { return 1; }
    for (const auto &planet : planets) {
        const auto &ref = planet.data.object12;
        if (ref.IsNull()) { continue; }
        std::vector<std::uint8_t> bytes;
        if (!tables.ReadSectionResource(ref.packHash, GameSection::Mission, ref.localIndex, bytes)) { return 1; }
        CArrayInputStream stream(bytes);
        Mission mission;
        if (!mission.Init(stream) || stream.Available() != 0) { return 1; }
        std::printf("[deathmatch-check] planet=%s extra=%08x:%u type=%u value64=%u value66=%u level=%08x:%u\n",
            ReadGameString(toc, planet.data.name).c_str(), ref.packHash, ref.localIndex, mission.type,
            mission.value64, mission.value66, mission.level.packHash, mission.level.localIndex);
    }
    return 0;
}
