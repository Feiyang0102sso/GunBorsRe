/** @file CGame.cpp
 * @brief Coordinate the active CLevel with HUD and session presentation.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/CGame.h"
#include "gun_bros_re/ui/CInputPad.h"
#include "gun_bros_re/data/Mission.h"
#include <cstdio>

CGame::CGame(CLevel &level, CMap &map,
    const std::vector<CEnemy::Template> &catalog) : m_level(level), m_map(map) {
    m_level.AttachRuntime(*this, catalog);
}

bool CGame::Load(CResTOCManager &toc, ZPackTables &tables, std::uint32_t mapPack, unsigned mapIndex,
    const GameObjectRef *selectedLevel, bool archive) {
    m_archive = archive;
    m_level.SetArchive(archive);
    m_toc = &toc;
    GameObjectRef requested;
    if (selectedLevel != nullptr) { requested = *selectedLevel; }
    // Original Mission -> LEVEL -> TILELAYER chain, never script-size heuristics.
    // Mission::Init :164402; CLevel::Template::Init :114770, corresponding BT.
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC *pack = toc.GetPack(packIndex);
        if (selectedLevel != nullptr && pack->GetPackHash() != selectedLevel->packHash) { continue; }
        ZGameSection section = ZGameSection::Mission;
        if (selectedLevel != nullptr) { section = ZGameSection::Level; }
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(section);
        for (unsigned index = 0; index < count; ++index) {
            if (selectedLevel != nullptr && index != selectedLevel->localIndex) { continue; }
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack->GetPackHash(), section, index, payload)) { return false; }
            GameObjectRef level = requested;
            if (selectedLevel == nullptr) {
                CArrayInputStream missionStream(payload);
                Mission mission;
                if (!mission.Init(missionStream) || missionStream.Available() != 0) { return false; }
                if (mission.type != 1) { continue; }
                level = mission.level;
                if (!tables.ReadSectionResource(level.packHash, ZGameSection::Level, level.localIndex, payload)) { return false; }
            }
            CArrayInputStream stream(payload);
            CLevel::Template candidate;
            if (!candidate.Init(stream) || stream.Available() != 0) { return false; }
            if (candidate.mapRef.packHash == mapPack && candidate.mapRef.localIndex == mapIndex) {
                if (!requested.IsNull() && selectedLevel == nullptr &&
                    (requested.packHash != level.packHash || requested.localIndex != level.localIndex)) {
                    std::printf("[survival] ambiguous retail LEVEL for map=%u:%u\n", mapPack, mapIndex);
                    return false;
                }
                requested = level;
                m_levelReference = level;
                m_template = std::move(candidate);
                if (selectedLevel != nullptr) {
                    std::printf("[survival] selected explicit LEVEL %u:%u for MAP %u:%u\n", level.packHash, level.localIndex, mapPack, mapIndex);
                    return m_level.PreloadEnemies(m_map.GetRequirements(), m_template.script);
                }
            }
        }
    }
    if (!requested.IsNull() && selectedLevel == nullptr) {
        std::printf("[survival] selected retail Mission LEVEL %u:%u for MAP %u:%u\n", requested.packHash, requested.localIndex, mapPack, mapIndex);
        return m_level.PreloadEnemies(m_map.GetRequirements(), m_template.script);
    }
    std::printf("[survival] no retail survival level for requested map\n");
    return false;
}

void CGame::Restart(float x, float y, float facingDegrees) {
    if (m_bossSkipActive) { FinishBossSkip(); }
    m_suspended = false;
    m_matchEndingHoldMs = 0;
    m_matchFading = false;
    m_challengeSessionEnded = false;
    m_level.ResetWorld(x, y, facingDegrees);
    m_transitionMs = 1200; // Desktop intro duration; original completion event retained.
    m_transitionDuration = 1200;
    if (m_horde) { m_transitionMs = 0; } // BOKOR owns its five-second intro timer.
    // Seed before Bind: export 0 already rolls for the pack12 babe. The
    // original's clock-seeded stream keeps running across level starts, so
    // each restart moves this seed on rather than repeating the same rolls.
    if (m_hasScriptRandomSeed) {
        m_scriptRandomSeed = m_scriptRandomSeed * 1664525u + 1013904223u;
        m_level.SetScriptRandomSeed(m_scriptRandomSeed);
    }
    m_level.Bind(m_template, m_map, nullptr, m_startWave);
    m_bossIntroSerial = m_level.GetBossIntroSerial();
    if (m_bossIntroSerial > 0) { m_transitionMs = 2000; m_transitionDuration = 2000; }
    m_bossWave = m_bossIntroSerial > 0;
    if (m_hud != nullptr && m_match == nullptr) {
        m_transitionMs = 0;
        unsigned wave = m_level.GetRealWave() + 1;
        if (m_horde && m_level.GetWavesPerRevolution() > 0) { wave = m_level.GetWave() / m_level.GetWavesPerRevolution() + 1; }
        m_hud->BeginLevel(wave, m_horde, m_bossWave);
    }
    m_level.RefreshCamera();
    UpdateDialog(0);
    if (m_match != nullptr) {
        m_transitionMs = 0;
        m_level.HandleEvent(2);
        if (!m_level.StartDeathmatch()) { m_level.RecordInvalidSpawn(); }
        if (m_hud != nullptr) { m_hud->BeginDeathmatch(m_match->Data().killLimit); }
    }
    for (const CLayerPathLink &path : m_map.GetPathLinkLayers()) {
        std::printf("[survival] path layer=%u nodes=%zu selected=%d\n", path.GetLayerIndex(), path.GetNodes().size(), m_level.GetPathLayer());
    }
}

void CGame::OnWaveCleared(unsigned perfectRewardPercent) {
    const bool perfect = !m_level.GetWavePerfectResults().empty() &&
        m_level.GetWavePerfectResults().back();
    SubmitChallenges(false, true);
    // CGame::OnWaveCleared :76246 only shows this sequence for game type 1.
    if (m_hud != nullptr && !m_horde) {
        m_hud->OnWaveClear(m_level.GetRealWave() + 1,
            perfect, perfectRewardPercent, m_bossWave);
        if (m_level.IsLocalLive() && m_level.GetWave() + 1 < m_level.GetWaveLimit()) {
            m_hud->BeginLiveWave(m_level.GetMultiplayerStatistics(0), m_level.GetMultiplayerStatistics(1));
        }
    }
    m_bossWave = false;
}

bool CGame::IsTransitioning() const {
    if (m_match != nullptr) { return false; }
    if (m_hud != nullptr) { return m_hud->HasInterstitial(); }
    return m_transitionMs > 0;
}

unsigned CGame::GetTransitionElapsed() const {
    if (m_hud != nullptr) { return m_hud->NoticeTime(); }
    return m_transitionDuration - m_transitionMs;
}

void CGame::CompleteDialog() {
    m_dialogText.clear();
    m_level.CompleteDialog();
    UpdateDialog(0);
}

void CGame::UpdateDialog(int deltaMs) {
    if (m_level.IsDialogCloseRequested() && m_dialogHud != nullptr) { m_dialogHud->ClearDialog(false); }
    if (m_dialogSerial != m_level.GetDialogSerial()) {
        m_dialogSerial = m_level.GetDialogSerial();
        m_dialogText.clear();
        m_dialogBound = false;
        if (m_dialogHud != nullptr) { m_dialogHud->ClearDialog(true); }
        CGameAssetRef resource;
        if (m_toc != nullptr && m_level.GetStringResource(m_level.GetDialogResource(), resource)) {
            m_dialogText = ReadGameString(*m_toc, resource);
            std::printf("[campaign-dialog] %s\n", m_dialogText.c_str());
            if (m_dialogHud != nullptr) { m_dialogBound = m_dialogHud->ShowDialog(m_dialogText, m_level.DoesDialogAutoClose(), m_level.GetDialogArrow()); }
            if (!m_dialogBound) {
                std::printf("[dialog] original Movie binding failed resource=%d\n", m_level.GetDialogResource());
            }
        }
    }
    if (m_level.GetDialogResource() < 0) { m_dialogText.clear(); return; }
    // Desktop reading duration; original movie/text-box pagination remains
    // separate research. Native argument three means automatic close, not pause.
    // The historical estimate above is superseded by CDialogPopup playback.
    if (m_dialogHud != nullptr) {
        m_dialogHud->UpdateDialog(static_cast<unsigned>(deltaMs));
        if (m_dialogBound && m_dialogHud->IsDialogDone()) { CompleteDialog(); }
    }
}

bool CGame::IsReadyForResults() const {
    if (!IsFinished()) { return false; }
    if (m_match == nullptr) { return true; }
    if (!m_matchFading) { return false; }
    if (m_hud == nullptr) { return true; }
    return m_hud->IsDeathmatchWrapUpComplete();
}

void CGame::Update(int deltaMs, float moveX, float moveY, bool fire) {
    if (m_hud != nullptr) {
        for (const auto &name : m_level.TakePowerupUseMessages()) { m_hud->OnDeathmatchPowerup(name); }
    }
    if (deltaMs <= 0 || m_suspended) { return; }
    if (m_match != nullptr && IsFinished()) {
        if (!m_matchFading) {
            if (!m_level.AdvanceDeathmatchEnding(deltaMs)) { return; }
            m_matchEndingHoldMs += deltaMs;
            if (m_matchEndingHoldMs < MatchEndingHoldMs) { return; }
            m_matchFading = true;
            std::printf("[deathmatch] death presentation complete; starting result fade\n");
        }
        if (m_hud != nullptr) { m_hud->AdvanceDeathmatchWrapUp(deltaMs); }
        return;
    }
    UpdateDialog(deltaMs);
    if (m_hud != nullptr) { m_hud->Advance(deltaMs); }
    const bool waitingForLiveWave = m_hud != nullptr && m_hud->LiveWaveRemaining() != 0;
    if (m_hud != nullptr && !waitingForLiveWave) {
        // InterstitialSequenceCallback :86336 emits LEVEL event 2 only after
        // the last authored Movie completes. BOKOR keeps its script clock alive.
        if (m_hud->TakeInterstitialCompletion()) {
            if (m_level.IsLocalLive()) { m_level.ClearWaveStatistics(); }
            m_level.HandleEvent(2);
        }
        // CGame::Update :76581 keeps CLevel::Update running under the Movie.
        // The script's object multiplier supplies slow motion, not a pause.
    }
    if (m_hud == nullptr && m_transitionMs > 0) {
        m_transitionMs -= deltaMs;
        if (m_transitionMs <= 0) { m_level.HandleEvent(2); }
    }
    const int previousWave = m_level.GetWave();
    m_level.Update(deltaMs, moveX, moveY, fire, !waitingForLiveWave);
    if (m_hud == nullptr && m_level.GetWave() != previousWave && !m_level.IsCleared()) { m_transitionMs = 1200; m_transitionDuration = 1200; }
    if (m_hud != nullptr && m_horde && m_level.GetWave() != previousWave && !m_level.IsCleared()) {
        // CGame::OnLevelStart :75385 names Horde rounds with GetRevolution.
        // Its Movie callback releases the script's slow-motion intermission.
        const int divisor = m_level.GetWavesPerRevolution();
        if (divisor > 0) { m_hud->BeginLevel(m_level.GetWave() / divisor + 1, true, false); }
    }
    if (m_bossIntroSerial != m_level.GetBossIntroSerial()) {
        m_bossIntroSerial = m_level.GetBossIntroSerial();
        // OnBossWaveStart uses GLU_MOVIE_WAVE_CLEARED and its real 2000 ms
        // completion callback before releasing the next scripted state.
        m_transitionMs = 2000;
        m_transitionDuration = 2000;
        m_bossWave = true;
        if (m_hud != nullptr) {
            m_transitionMs = 0;
            m_hud->BeginLevel(m_level.GetRealWave() + 1, m_horde, true);
        }
    }
}

void CGame::UpdateAfterDeath(int deltaMs) {
    if (m_suspended) { return; }
    if (m_hud != nullptr) { m_hud->Advance(deltaMs); }
    m_level.UpdateAfterDeath(deltaMs);
}

// CGame submits wave deltas before the HUD's common interstitial sequence.
bool CGame::SubmitChallenges(bool ended, bool waveCleared) {
    if (!m_challenges || m_challengeSessionEnded) { return true; }
    m_challengeProfile->experience = m_level.GetExperience();
    CChallengeManager::Session data;
    data.level = m_levelReference;
    data.guns = m_challengeProfile->configuration.guns;
    data.gameType = 1;
    if (m_level.IsLocalLive()) { data.gameType = 2; }
    // Horde is a mission type, not the cooperative GameType=2 requirement.
    data.wave = m_level.GetWave() + 1; // Original +0x4BEEC is the global wave, not modulo revolution.
    data.waveCleared = waveCleared;
    data.perfect = !m_level.GetWavePerfectResults().empty() && m_level.GetWavePerfectResults().back();
    data.ended = ended;
    data.kills = m_level.TakeChallengeKills();
    data.powerups = m_level.TakeChallengePowerups();
    m_challenges->UpdateFromLevelSession(data, *m_challengeWeapons, *m_challengeProfile);
    m_challengeSessionEnded = ended;
    return m_challenges->StoreProgress(*m_challengeProfile);
}
