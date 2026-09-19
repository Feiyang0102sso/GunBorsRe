/** CGame session host implementation; original ownership follows game.cpp.
 * SDL/GL submission and borrowed desktop resources are host adaptations.
 */
#include "gun_bros_re/gameplay/game/CGameRuntime.h"
using namespace MapDetail;

bool CGame::Session::OpenShop(unsigned peer, bool exempt) {
    if (launch.deathmatch && !match.CanShop(peer)) { return false; }
    if (launch.deathmatch) {
        if (peer == 1) {
            if (botShop.Request(1, window.GetTicksMs(), 0)) {
                matchShopCounted = false;
                matchShopPurchased = false;
            }
        } else if (liveShop.Request(0, window.GetTicksMs(), 0, UINT32_MAX)) {
            survivalHud.ResetSelector(scene.IsMatchSpawnPending(0));
        }
        itemChoice = false;
        return true;
    }
    if (launch.localLive) {
        if (session.IsBossSkipActive()) { return false; }
        if (liveShop.RequestForWave(peer, window.GetTicksMs(), session.GetLevel().GetWave(), scene.IsRescuePending(),
                                    exempt)) {
            itemChoice = false;
            accumulator = 0;
            matchShopCounted = false;
            matchShopPurchased = false;
            return true;
        }
        return false;
    } else if (peer == 0) {
        shopOpen = true;
        itemChoice = false;
        accumulator = 0;
    }
    return true;
}
void CGame::Session::CloseShop() {
    // ResumeActionCallback :94386 completes both first entry and respawn.
    if (launch.deathmatch && match.GetResult() == CMPMatch::Result::Playing) {
        if (scene.IsMatchSpawnPending(0)) {
            if (!scene.RespawnDeathmatch(0, true)) { ++runtimeFailures; }
        } else if (vitals.dead && vitals.deathAnimationComplete) {
            if (!scene.RespawnDeathmatch(0, false, true)) { ++runtimeFailures; }
        }
    }
    if (deathShop) {
        scene.FinishDeathChoice(0);
        deathShop = false;
    }
    if (launch.localLive || launch.deathmatch) { liveShop.Close(0); }
    survivalHud.ResetSelector();
    shopOpen = false;
    itemChoice = false;
}

int CGame::Session::UpdateShop() {
    if (launch.deathmatch && !session.IsFinished()) {
        // DeathMatchIntroSequenceCallback :90035 opens the selector after the banner.
        if (survivalHud.TakeDeathmatchIntroCompletion() && !session.IsFinished()) {
            survivalHud.ResetSelector(true);
            liveShop.Request(0, frameTicks, 0, match.Data().respawnSeconds * 1000);
            // The local Bot has already chosen its two guns. Each peer
            // enters independently; an unfinished player loadout stays absent.
            if (scene.IsMatchSpawnPending(1) && !scene.RespawnDeathmatch(1, true)) { ++runtimeFailures; }
        }
        const bool wasShopping = liveShop.Active();
        liveShop.Update(frameTicks);
        if (wasShopping && !liveShop.Active()) { CloseShop(); }
        botShop.Update(frameTicks);
        const auto &life = match.GetLife(0);
        // OnPlayerKilled opens the normal DM selector after the burst.
        if (life.dead && vitals.deathAnimationComplete && deathShopSerial != life.serial &&
            match.GetResult() == CMPMatch::Result::Playing) {
            deathShopSerial = life.serial;
            liveShop.Close(0);
            survivalHud.ResetSelector();
            unsigned selectorMs = 1;
            if (life.respawnMs > 1000) { selectorMs = life.respawnMs - 1000; }
            liveShop.Request(0, frameTicks, 0, selectorMs);
            itemChoice = false;
        }
        shopOpen = liveShop.Visible(frameTicks);
        if (botShop.Visible(frameTicks) && !matchShopCounted) {
            if (!match.EnterShop(1)) {
                botShop.Close(1);
            } else {
                matchShopCounted = true;
            }
        }
        if (botShop.Visible(frameTicks) && !matchShopPurchased) {
            // Shop decisions use the same catalog, price, balance and level checks as the player.
            while (const auto *item = ZLocalPVPBot::ChoosePurchase(matchStore, *peerProfile, peerProgress.GetLevel(),
                                                                   match.GetLife(1))) {
                if (peerProfile->AcquireItem(item->data, peerProgress.GetLevel()) != ZPurchaseResult::Purchased) {
                    break;
                }
                std::printf("[deathmatch] bot purchased powerup=%u\n", item->data.objects.front().object.localIndex);
            }
            matchShopPurchased = true;
            if (launch.botFriend != nullptr && gameContext != nullptr && gameContext->persistProgress &&
                !launch.botFriend->Save()) {
                return 1;
            }
        }
        if (botShop.Visible(frameTicks) && botShop.Remaining(frameTicks) < 7500) { botShop.Close(1); }
        if (!paused && !botShop.Active() && !brother.vitals.dead && !session.IsFinished()) {
            deathmatchBot->UsePowerups(peerPowerups);
            if (deathmatchBot->WantsShop() && match.CanShop(1) &&
                ZLocalPVPBot::ChoosePurchase(matchStore, *peerProfile, peerProgress.GetLevel(), match.GetLife(1)) !=
                    nullptr) {
                OpenShop(1);
                deathmatchBot->OnShopAttempt();
            }
        }
    }
    if (launch.localLive) {
        scene.SetAfterDeathAvailability(powerups.HasAfterDeathPowerup(), peerPowerups.HasAfterDeathPowerup());
        if (scene.IsRescuePending() && liveShop.Active()) {
            // A death can arrive during the request delay or from a cheat.
            liveShop.Close(liveShop.Owner());
            deathShop = false;
            itemChoice = false;
            survivalHud.ResetSelector();
        }
        const bool wasActive = liveShop.Active();
        liveShop.Update(frameTicks);
        if (wasActive && !liveShop.Active() && deathShop) {
            scene.FinishDeathChoice(liveShop.Owner());
            deathShop = false;
        }
        if (!liveShop.Active() && !powerups.GetPowerup().IsPresentationActive() &&
            !peerPowerups.GetPowerup().IsPresentationActive()) {
            for (unsigned peer = 0; peer < 2; ++peer) {
                const CBrother::Vitals *down = &vitals;
                if (peer == 1) { down = &brother.vitals; }
                if (scene.NeedsDeathChoice(peer) && down->deathAnimationComplete) {
                    if (OpenShop(peer, true)) { deathShop = true; }
                    break;
                }
            }
        }
        shopOpen = liveShop.Visible(frameTicks);
        if (shopOpen && liveShop.Owner() == 1) {
            const unsigned browsingMs = ZLiveShopSession::LimitMs - liveShop.Remaining(frameTicks);
            survivalHud.BrowseRemoteShop(localBot->ShopSelection(browsingMs));
            const auto *item = survivalHud.SelectedItem();
            if (!deathShop && item != nullptr &&
                localBot->ShouldBuyShopItem(browsingMs,
                                            peerProfile->GetPowerupCount(item->data.objects.front().object))) {
                const auto purchase = peerProfile->AcquireItem(item->data, peerProgress.GetLevel());
                if (purchase == ZPurchaseResult::Purchased) {
                    std::printf("[local-live] peer purchased powerup=%u\n",
                                item->data.objects.front().object.localIndex);
                    if (launch.botFriend != nullptr && !launch.botFriend->Save()) { return 1; }
                }
            }
            if (deathShop && liveShop.Remaining(frameTicks) < 8000 && peerPowerups.UseAfterDeathPowerup()) {
                scene.FinishDeathChoice(1);
                liveShop.Close(1);
                shopOpen = false;
                deathShop = false;
            }
        }
        if (!paused && !liveShop.Active() && !session.IsTransitioning() && !brother.vitals.dead &&
            !scene.IsRescuePending() && !session.IsBossSkipActive() && !powerups.GetPowerup().IsPresentationActive() &&
            !peerPowerups.GetPowerup().IsPresentationActive()) {
            localBot->AdvanceActions(menuElapsed);
            if (localBot->TakePowerupRequest()) { localBot->UsePowerup(peerPowerups); }
            if (!peerPowerups.GetPowerup().IsPresentationActive() && localBot->TakeShopRequest()) { OpenShop(1); }
        }
    }
    return -1;
}
