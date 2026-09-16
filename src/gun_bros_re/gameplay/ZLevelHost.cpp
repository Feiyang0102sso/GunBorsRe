/** @file ZLevelHost.cpp
 * @brief Keep level templates stable and deliver actor/HUD callbacks explicitly.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/ZLevelHost.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/ui/CInputPad.h"
#include "gun_bros_re/data/Mission.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// CBrother constructor :139098; CPlayer::Move uses the full radius for triggers.
constexpr float kBrotherTriggerRadius = 22.0f;

ZLevelHost::ZLevelHost(ZCombatWorld &scene, CMap &map,
    const std::vector<ZEnemyTemplateData> &catalog) : m_scene(scene), m_map(map), m_catalog(catalog) {
    m_scene.SetLevel(&m_level);
}

bool ZLevelHost::Load(CResTOCManager &toc, ZPackTables &tables, std::uint32_t mapPack, unsigned mapIndex,
    const GameObjectRef *selectedLevel, bool archive) {
    m_archive = archive;
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
                    return m_scene.PreloadEnemies(m_map.GetRequirements(), m_template.script);
                }
            }
        }
    }
    if (!requested.IsNull() && selectedLevel == nullptr) {
        std::printf("[survival] selected retail Mission LEVEL %u:%u for MAP %u:%u\n", requested.packHash, requested.localIndex, mapPack, mapIndex);
        return m_scene.PreloadEnemies(m_map.GetRequirements(), m_template.script);
    }
    std::printf("[survival] no retail survival level for requested map\n");
    return false;
}

void ZLevelHost::Restart(float x, float y, float facingDegrees) {
    if (m_bossSkipActive) { FinishBossSkip(); }
    m_suspended = false;
    m_matchEndingHoldMs = 0;
    m_matchFading = false;
    m_scene.Reset();
    m_challengeSessionEnded = false;
    if (m_powerups != nullptr) { m_powerups->Reset(); }
    if (m_peerPowerups != nullptr) { m_peerPowerups->Reset(); }
    if (m_pickups != nullptr) { m_pickups->Reset(); }
    if (m_props != nullptr) { m_props->Reset(); }
    m_scene.GetPlayer().x = x;
    m_scene.GetPlayer().y = y;
    // CBrother::Spawn :135887 writes one spawn angle to the player and the
    // AI brother alike.
    m_scene.GetPlayer().facing = facingDegrees;
    m_scene.ResetBrotherPosition(x, y, facingDegrees);
    m_spawnSerial = 0;
    m_closestSpawnDistance = -1;
    m_onScreenSpawns = 0;
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
    m_level.Bind(m_template, m_map, this, m_startWave);
    m_bossIntroSerial = m_level.GetBossIntroSerial();
    if (m_bossIntroSerial > 0) { m_transitionMs = 2000; m_transitionDuration = 2000; }
    m_bossWave = m_bossIntroSerial > 0;
    if (m_hud != nullptr && m_match == nullptr) {
        m_transitionMs = 0;
        unsigned wave = m_level.GetRealWave() + 1;
        if (m_horde && m_level.GetWavesPerRevolution() > 0) { wave = m_level.GetWave() / m_level.GetWavesPerRevolution() + 1; }
        m_hud->BeginLevel(wave, m_horde, m_bossWave);
    }
    UpdateCamera();
    UpdateDialog(0);
    if (m_match != nullptr) {
        m_transitionMs = 0;
        m_level.HandleEvent(2);
        m_scene.SetPathLayer(m_level.GetPathLayer());
        if (!m_scene.StartDeathmatch()) { ++m_scene.invalidSpawns; }
        if (m_hud != nullptr) { m_hud->BeginDeathmatch(m_match->Data().killLimit); }
    }
    for (const CLayerPathLink &path : m_map.GetPathLinkLayers()) {
        std::printf("[survival] path layer=%u nodes=%zu selected=%d\n", path.GetLayerIndex(), path.GetNodes().size(), m_level.GetPathLayer());
    }
}

bool ZLevelHost::SpawnEnemy(const GameObjectRef &enemy, int layerIndex, int nodeIndex, int objectId) {
    const bool hasAuthoredRoute = layerIndex >= 0 && nodeIndex >= 0;
    std::size_t entryIndex = 0;
    while (entryIndex < m_catalog.size()) {
        if (m_catalog[entryIndex].packHash == enemy.packHash && m_catalog[entryIndex].ordinal == enemy.localIndex) { break; }
        ++entryIndex;
    }
    if (entryIndex == m_catalog.size()) { return false; }
    // CEnemySpawner::GetSpawnPointOffScreen :146112 uses CMap's current path
    // when no override is selected. Reset clears that override before Lava 0's
    // final single spawn; treating -1 as a literal link layer dropped the enemy.
    if (layerIndex < 0) { layerIndex = m_level.GetPathLayer(); }
    ILayerPath *path = m_map.GetPathLayer(layerIndex);
    if (path == nullptr || path->GetNodes().empty()) { return false; }
    const auto &nodes = path->GetNodes();
    if (nodeIndex < 0) { nodeIndex = m_level.GetSpawner().GetSpawnPoint(*path, m_scene.GetPlayer().x, m_scene.GetPlayer().y,
        m_cameraLeft, m_cameraTop, m_cameraWidth, m_cameraHeight); }
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) { return false; }
    ZCombatEnemy *actor = m_scene.Spawn(entryIndex, nodes[nodeIndex].x, nodes[nodeIndex].y);
    if (actor == nullptr) { return false; }
    if (objectId < 0) {
        const float distance = std::hypot(m_scene.GetPlayer().x - nodes[nodeIndex].x, m_scene.GetPlayer().y - nodes[nodeIndex].y);
        if (m_closestSpawnDistance < 0 || distance < m_closestSpawnDistance) { m_closestSpawnDistance = distance; }
        if (m_cameraWidth > 0 && nodes[nodeIndex].x >= m_cameraLeft && nodes[nodeIndex].y >= m_cameraTop &&
            nodes[nodeIndex].x <= m_cameraLeft + m_cameraWidth && nodes[nodeIndex].y <= m_cameraTop + m_cameraHeight) {
            ++m_onScreenSpawns;
        }
    }
    actor->objectId = objectId;
    // SpawnEnemyPath -> SetPath :147015; a spawn override alone is not a route.
    if (hasAuthoredRoute) { actor->model.enemy.SetPath(path); }
    // CLevel::AddObject :116890 attaches the enemy direction marker.
    m_level.SetIndicator(objectId, 0, actor->model.enemy.combat.id);
    return true;
}

void ZLevelHost::StartObjectLayer(int layer) {
    if (m_props != nullptr) { m_props->StartLayer(layer); }
}

bool ZLevelHost::SpawnMapObject(const ZPlacedObject &object, int objectId) {
    if (object.objectType == static_cast<unsigned>(ZPlacedObjectType::Prop)) {
        if (m_props == nullptr) { return false; }
        return m_props->Spawn(m_level.GetObjectLayer(), objectId);
    }
    if (object.objectType == static_cast<unsigned>(ZPlacedObjectType::Pickup)) {
        GameObjectRef pickup;
        pickup.packHash = object.packHash;
        pickup.localIndex = object.localIndex;
        return SpawnPickupAt(pickup, object.x, object.y, objectId);
    }
    // Static props and players are already loaded by the map host.
    if (object.objectType != static_cast<unsigned>(ZPlacedObjectType::Enemy)) { return true; }
    for (unsigned index = 0; index < m_catalog.size(); ++index) {
        if (m_catalog[index].packHash != object.packHash || m_catalog[index].ordinal != object.localIndex) { continue; }
        ZCombatEnemy *actor = m_scene.Spawn(index, object.x, object.y);
        if (actor == nullptr) { return false; }
        actor->objectId = objectId;
        actor->mapPlaced = true;
        if (object.pathLayer != 255) { actor->model.enemy.SetPath(m_map.GetPathLayer(object.pathLayer)); }
        m_level.SetIndicator(objectId, 0, actor->model.enemy.combat.id);
        actor->model.enemy.combat.facing = static_cast<float>(object.facing);
        std::printf("[survival] placed enemy id=%d tag=%u item=%u path=%u facing=%d\n",
            objectId, object.spawnTag, object.localIndex, object.pathLayer, object.facing);
        return true;
    }
    return false;
}

void ZLevelHost::SendEnemyMessage(int objectId, int message) {
    for (const auto &actor : m_scene.enemies) {
        if (actor->objectId == objectId) {
            const unsigned before = actor->model.enemy.GetStateId();
            actor->model.enemy.HandleMessage(message);
            if (before != actor->model.enemy.GetStateId()) {
                std::printf("[map-enemy] id=%d message=%d state=%u->%u\n",
                    objectId, message, before, actor->model.enemy.GetStateId());
            }
            return;
        }
    }
}

void ZLevelHost::SetEnemyPortal(int enemyId, int propId) {
    for (const auto &actor : m_scene.enemies) {
        if (actor->objectId != enemyId) { continue; }
        // CEnemy::SetPortal :67225 resets the previous activation state.
        actor->model.enemy.combat.portalObjectId = propId;
        actor->model.enemy.combat.portalActive = false;
        return;
    }
}

void ZLevelHost::SendPropMessage(int objectId, int message) {
    if (m_props != nullptr) { m_props->SendMessage(objectId, message); }
}

void ZLevelHost::OnWaveCleared(unsigned perfectRewardPercent) {
    const unsigned previousPerfect = m_scene.GetPerfectWaves();
    m_scene.OnWaveCleared(perfectRewardPercent);
    SubmitChallenges(false, true);
    // CGame::OnWaveCleared :76246 only shows this sequence for game type 1.
    if (m_hud != nullptr && !m_horde) {
        m_hud->OnWaveClear(m_level.GetRealWave() + 1,
            m_scene.GetPerfectWaves() > previousPerfect, perfectRewardPercent, m_bossWave);
        if (m_scene.IsLocalLive() && m_level.GetWave() + 1 < m_level.GetWaveLimit()) {
            m_hud->BeginLiveWave(m_scene.GetMultiplayerStatistics(0), m_scene.GetMultiplayerStatistics(1));
        }
    }
    m_bossWave = false;
}

bool ZLevelHost::IsTransitioning() const {
    if (m_match != nullptr) { return false; }
    if (m_hud != nullptr) { return m_hud->HasInterstitial(); }
    return m_transitionMs > 0;
}

unsigned ZLevelHost::GetTransitionElapsed() const {
    if (m_hud != nullptr) { return m_hud->NoticeTime(); }
    return m_transitionDuration - m_transitionMs;
}

void ZLevelHost::PlayLevelSound(const GameObjectRef &sound) {
    if (m_effects == nullptr) { return; }
    ZGunCue cue;
    cue.kind = ZGunCue::Kind::Sound;
    cue.resource = sound;
    m_effects->Emit(cue, 0, 0, 0, 0);
}

bool ZLevelHost::SpawnPickup(const GameObjectRef &pickup, int layer, int node, int objectId, bool nearby) {
    if (m_pickups == nullptr) { return false; }
    if (layer < 0) { layer = m_level.GetPathLayer(); }
    ILayerPath *path = m_map.GetPathLayer(layer);
    if (path == nullptr || path->GetNodes().empty()) { return false; }
    const auto &nodes = path->GetNodes();
    if (nearby) { node = path->FindNearest(m_scene.GetPlayer().x, m_scene.GetPlayer().y); }
    if (node < 0) {
        // Retain authored nodes. Exact original free-position scoring is a
        // separate navigation task, shared with enemy spawn selection.
        for (unsigned attempt = 0; attempt < nodes.size(); ++attempt) {
            const unsigned candidate = (m_spawnSerial + attempt) % nodes.size();
            if (!nodes[candidate].locked) { node = candidate; m_spawnSerial = candidate + 1; break; }
        }
    }
    if (node < 0 || node >= static_cast<int>(nodes.size())) { return false; }
    return SpawnPickupAt(pickup, nodes[node].x, nodes[node].y, objectId);
}

bool ZLevelHost::SpawnPickupAt(const GameObjectRef &pickup, float x, float y, int objectId) {
    if (m_pickups == nullptr) { return false; }
    if (!m_pickups->Spawn(pickup, x, y, objectId)) { return false; }
    // CEnemySpawner::SpawnPickup :146349 marks the particular pickup instance.
    m_level.SetIndicator(objectId, 1, (1ULL << 32) | m_pickups->spawned);
    return true;
}

bool ZLevelHost::SpawnMPMatchPickup(const GameObjectRef &pickup, int layer) {
    if (m_match == nullptr || m_pickups == nullptr) { return false; }
    ILayerPath *path = m_map.GetPathLayer(layer);
    if (path == nullptr || path->GetNodes().empty()) { return false; }
    const auto &nodes = path->GetNodes();
    const unsigned start = static_cast<unsigned>(m_level.RandomInteger(0, static_cast<std::int16_t>(nodes.size() - 1)));
    for (unsigned offset = 0; offset < nodes.size(); ++offset) {
        const auto &node = nodes[(start + offset) % nodes.size()];
        if (node.locked || !m_scene.CanBrotherWalk(node.x, node.y, node.x, node.y)) { continue; }
        float nearestX = 0, nearestY = 0;
        if (m_pickups->FindNearest(node.x, node.y, nearestX, nearestY) && std::hypot(nearestX - node.x, nearestY - node.y) < m_scene.GetPlayerRadius() * 2) { continue; }
        const int candidate = m_match->ChoosePickup();
        if (candidate < 0) { return false; }
        return SpawnPickupAt(pickup, node.x, node.y, CMPMatch::PickupIdBase + candidate);
    }
    return false;
}

std::uint64_t ZLevelHost::ResolveIndicatorTarget(int objectId) const {
    // Native 49 binds an existing map object once; later enemies may reuse its ID.
    if (m_props != nullptr) {
        const unsigned key = m_props->ResolveIndicatorTarget(objectId);
        if (key != 0) { return (2ULL << 32) | key; }
    }
    for (const auto &actor : m_scene.enemies) {
        if (actor->objectId == objectId && !actor->model.enemy.combat.dead && !actor->model.enemy.combat.removed) {
            return actor->model.enemy.combat.id;
        }
    }
    return 0;
}

bool ZLevelHost::GetIndicatorTarget(std::uint64_t key, float &x, float &y) const {
    if ((key >> 32) == 2) {
        return m_props != nullptr && m_props->GetIndicatorTarget(static_cast<unsigned>(key), x, y);
    }
    if ((key >> 32) == 1) {
        return m_pickups != nullptr && m_pickups->GetIndicatorTarget(static_cast<unsigned>(key), x, y);
    }
    ZCombatEnemy *actor = m_scene.Find(static_cast<ZCombatId>(key));
    if (actor == nullptr || actor->model.enemy.combat.dead || actor->model.enemy.combat.removed) { return false; }
    x = actor->model.enemy.combat.x; y = actor->model.enemy.combat.y; return true;
}

bool ZLevelHost::GetObjectPosition(int objectId, float &x, float &y) const {
    for (const auto &actor : m_scene.enemies) {
        if (actor->objectId != objectId || actor->model.enemy.combat.dead || actor->model.enemy.combat.removed) { continue; }
        x = actor->model.enemy.combat.x;
        y = actor->model.enemy.combat.y;
        return true;
    }
    if (m_pickups != nullptr && m_pickups->GetObjectPosition(objectId, x, y)) { return true; }
    return m_props != nullptr && m_props->GetObjectPosition(objectId, x, y);
}

int ZLevelHost::CountEnemySlots(const GameObjectRef *enemy) const {
    int count = 0;
    for (const auto &actor : m_scene.enemies) {
        if (actor->model.enemy.combat.removed) { continue; }
        if (enemy == nullptr || (actor->data->packHash == enemy->packHash && actor->data->ordinal == enemy->localIndex)) { ++count; }
    }
    return count;
}

int ZLevelHost::CountEnemies(const GameObjectRef *enemy, int objectId) const {
    int count = 0;
    for (const auto &actor : m_scene.enemies) {
        const ZEnemyCombat &state = actor->model.enemy.combat;
        if (state.dead || state.removed) { continue; }
        if (objectId >= 0 && actor->objectId != objectId) { continue; }
        if (enemy == nullptr || (actor->data->packHash == enemy->packHash && actor->data->ordinal == enemy->localIndex)) { ++count; }
    }
    return count;
}

void ZLevelHost::CompleteDialog() {
    m_dialogText.clear();
    m_level.CompleteDialog();
    UpdateDialog(0);
}

void ZLevelHost::UpdateDialog(int deltaMs) {
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

void ZLevelHost::UpdateMapInteractions(float previousX, float previousY) {
    if (m_scene.IsMatchSpawnPending(0)) { return; }
    if (m_archive) {
        const float scaleRatio = 0.8f / m_map.GetCamera().GetScale();
        const float viewWidth = m_viewWidth * scaleRatio;
        const float viewHeight = m_viewHeight * scaleRatio;
        float left = m_scene.GetPlayer().x - viewWidth * 0.5f;
        float top = m_scene.GetPlayer().y - viewHeight * 0.5f;
        const ZMapRectangle bounds = m_map.GetVisibleBounds();
        if (!bounds.IsEmpty()) {
            if (bounds.width <= viewWidth) { left = bounds.x + (bounds.width - viewWidth) * 0.5f; }
            else { left = std::clamp(left, static_cast<float>(bounds.x), bounds.x + bounds.width - viewWidth); }
            if (bounds.height <= viewHeight) { top = bounds.y + (bounds.height - viewHeight) * 0.5f; }
            else { top = std::clamp(top, static_cast<float>(bounds.y), bounds.y + bounds.height - viewHeight); }
        }
        m_level.UpdateProximitySpawns(left, top, viewWidth, viewHeight);
    }
    for (unsigned index = 0; index < m_map.GetCollisionLayerCount(); ++index) {
        const CLayerCollision &layer = m_map.GetCollisionLayer(index);
        if (static_cast<int>(layer.GetLayerIndex()) != m_level.GetTriggerLayer()) { continue; }
        const auto &geometry = layer.GetCollision();
        float nearest = 2;
        int group = -1;
        for (const ZCollisionEdge &edge : geometry.GetEdges()) {
            if (!edge.enabled) { continue; }
            const float fraction = CombatGeometry::EdgeFraction(previousX, previousY,
                m_scene.GetPlayer().x - previousX, m_scene.GetPlayer().y - previousY,
                // CBrother constructor :139098 stores 22.0; CPlayer::Move
                // :100866 passes the full radius to TestTrigger, while wall
                // resolution uses half. Trigger regions remain authored BIG data.
                geometry.GetVertices()[edge.firstVertex], geometry.GetVertices()[edge.secondVertex], kBrotherTriggerRadius);
            if (fraction < nearest) { nearest = fraction; group = edge.group; }
        }
        if (group >= 0 && nearest <= 1) { m_level.OnTrigger(group); }
        break;
    }
}

bool ZLevelHost::IsReadyForResults() const {
    if (!IsFinished()) { return false; }
    if (m_match == nullptr) { return true; }
    if (!m_matchFading) { return false; }
    if (m_hud == nullptr) { return true; }
    return m_hud->IsDeathmatchWrapUpComplete();
}

void ZLevelHost::Update(int deltaMs, float moveX, float moveY, bool fire) {
    if (m_hud != nullptr && m_powerups != nullptr) {
        for (const auto &name : m_powerups->TakeUseMessages()) { m_hud->OnDeathmatchPowerup(name); }
    }
    if (deltaMs <= 0 || m_suspended) { return; }
    if (m_match != nullptr && IsFinished()) {
        if (!m_matchFading) {
            if (!m_scene.AdvanceDeathmatchEnding(deltaMs)) { return; }
            m_matchEndingHoldMs += deltaMs;
            if (m_matchEndingHoldMs < MatchEndingHoldMs) { return; }
            m_matchFading = true;
            std::printf("[deathmatch] death presentation complete; starting result fade\n");
        }
        if (m_hud != nullptr) { m_hud->AdvanceDeathmatchWrapUp(deltaMs); }
        return;
    }
    // CLevel::Update :121255 advances the active powerup before its pause
    // gate. Keep presentation time alive without advancing actors or spawns.
    if (m_peerPowerups != nullptr && m_peerPowerups->IsMovieActive()) {
        m_peerPowerups->Update(deltaMs);
        return;
    }
    if (m_powerups != nullptr && m_powerups->IsMovieActive()) {
        m_powerups->Update(deltaMs);
        return;
    }
    if (IsDeathComplete()) { UpdateAfterDeath(deltaMs); return; }
    if (m_level.IsPaused()) { return; }
    const int worldDeltaMs = m_level.TransformWorldElapseMS(deltaMs);
    m_map.GetCamera().Update(worldDeltaMs);
    UpdateDialog(deltaMs);
    if (m_hud != nullptr) { m_hud->Advance(deltaMs); }
    // CLevel::Update :121325 keeps both player objects alive during the
    // 15-second script wait. CInputPad does not block movement sticks.
    // Continue through the shared actor/event path below. Returning after
    // scene.Update lost deaths and callbacks when its next tick cleared them.
    const bool waitingForLiveWave = m_hud != nullptr && m_hud->LiveWaveRemaining() != 0;
    if (m_level.IsCleared()) {
        m_scene.Update(worldDeltaMs, 0, 0, false);
        UpdateCamera(worldDeltaMs);
        return;
    }
    if (m_hud != nullptr && !waitingForLiveWave) {
        // InterstitialSequenceCallback :86336 emits LEVEL event 2 only after
        // the last authored Movie completes. BOKOR keeps its script clock alive.
        if (m_hud->TakeInterstitialCompletion()) {
            if (m_scene.IsLocalLive()) { m_scene.ClearWaveStatistics(); }
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
    m_level.CheckForCameraChange(m_scene.GetPlayer().x, m_scene.GetPlayer().y);
    if (!waitingForLiveWave) { m_level.Update(deltaMs); }
    m_scene.SetPathLayer(m_level.GetPathLayer());
    const float previousX = m_scene.GetPlayer().x;
    const float previousY = m_scene.GetPlayer().y;
    if (!m_level.CanPlayerMove()) { moveX = 0; moveY = 0; }
    if (!m_level.CanPlayerShoot()) { fire = false; }
    m_scene.Update(worldDeltaMs, moveX, moveY, fire);
    // CPlayer::Move checks triggers in every mode, including retail survival.
    UpdateMapInteractions(previousX, previousY);
    if (m_powerups != nullptr) { m_powerups->Update(deltaMs); }
    if (m_peerPowerups != nullptr) { m_peerPowerups->Update(deltaMs); }
    if (m_props != nullptr) { m_props->Update(worldDeltaMs); }
    if (m_pickups != nullptr) {
        for (const ZPickupSpawn &spawn : m_scene.pickupSpawns) { SpawnPickupAt(spawn.resource, spawn.x, spawn.y, 0); }
        m_pickups->Update(worldDeltaMs, m_scene, *m_effects);
        for (const ZPickupCollection &pickup : m_pickups->collections) {
            if (m_match != nullptr && pickup.objectId >= CMPMatch::PickupIdBase &&
                !m_scene.CollectMatchWeapon(pickup.peer, pickup.objectId - CMPMatch::PickupIdBase)) { ++m_scene.invalidSpawns; }
            m_level.OnPickupCollected(pickup.objectId, pickup.resource);
        }
    }
    for (std::uint8_t event : m_scene.levelEvents) {
        // Enemy/prop events belong to this mode's authored LEVEL script.
        m_level.HandleEvent(event);
    }
    if (m_match != nullptr) { m_scene.UpdateDeathmatch(deltaMs); }
    for (const auto &event : m_scene.teleports) { m_level.OnEnemyTeleport(event.objectId, event.enemy); }
    // Deliver only after the scene update, so callbacks may safely spawn actors.
    for (const ZCombatDeath &death : m_scene.deaths) {
        m_level.OnEnemyKilled(death.objectId, death.enemy);
    }
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
    UpdateCamera(worldDeltaMs);
}

unsigned ZLevelHost::GetPowerupCount(unsigned localIndex) const {
    if (m_powerups == nullptr) { return 0; }
    return m_powerups->GetCount(localIndex);
}

void ZLevelHost::UpdateCamera(int deltaMs) {
    const ZMapRectangle bounds = m_map.GetVisibleBounds();
    const float scale = 0.8f / m_map.GetCamera().GetScale();
    // Before the first local spawn, retain the default camera established at load.
    // The original respawn callback restores player following after confirmation.
    if (!m_scene.IsMatchSpawnPending(0) || !m_map.GetCamera().HasPosition()) {
        m_map.GetCamera().UpdatePosition(m_scene.GetPlayer().x, m_scene.GetPlayer().y, bounds.x, bounds.y,
            bounds.width, bounds.height, m_viewWidth * scale, m_viewHeight * scale);
    }
    m_scene.SetViewCenter(m_map.GetCamera().GetX(), m_map.GetCamera().GetY());
    const float width = m_viewWidth * scale;
    const float height = m_viewHeight * scale;
    // The same rectangle CCamera::GetBounds hands CBullet::CanBeCulled :60583
    // and the offscreen spawn filter (SetupSpawnFilter :147283).
    m_scene.SetViewSize(width, height);
    m_cameraLeft = m_map.GetCamera().GetX() - width * 0.5f;
    m_cameraTop = m_map.GetCamera().GetY() - height * 0.5f;
    m_cameraWidth = width;
    m_cameraHeight = height;
    // Same camera-scaled 25/25/100 margins as CLevelIndicator::Init :191653.
    const float viewportFactor = std::min(m_viewWidth / 480.0f, m_viewHeight / 320.0f);
    const float margin = 25 * viewportFactor * scale;
    const float bottomMargin = 100 * viewportFactor * scale;
    m_level.UpdateIndicators(deltaMs, m_map.GetCamera().GetX() - width * 0.5f + margin,
        m_map.GetCamera().GetY() - height * 0.5f + margin,
        width - 2 * margin, height - margin - bottomMargin);
}

void ZLevelHost::UpdateAfterDeath(int deltaMs) {
    if (m_suspended) { return; }
    // HP zero starts the animation; native 1 ends normal level updates.
    if (!IsDeathComplete()) { Update(deltaMs, 0, 0, false); return; }
    if (m_powerups != nullptr && m_powerups->IsMovieActive()) {
        m_powerups->Update(deltaMs);
        return;
    }
    if (m_hud != nullptr) { m_hud->Advance(deltaMs); }
    // Finish existing attacks and camera effects without advancing new waves.
    m_map.GetCamera().Update(deltaMs);
    m_scene.Update(deltaMs, 0, 0, false);
    if (m_powerups != nullptr) { m_powerups->Update(deltaMs); }
    UpdateCamera(deltaMs);
}

// CGame submits wave deltas before the HUD's common interstitial sequence.
bool ZLevelHost::SubmitChallenges(bool ended, bool waveCleared) {
    if (!m_challenges || m_challengeSessionEnded) { return true; }
    m_challengeProfile->experience = m_scene.GetExperience();
    CChallengeManager::Session data;
    data.level = m_levelReference;
    data.guns = m_challengeProfile->configuration.guns;
    data.gameType = 1;
    if (m_scene.IsLocalLive()) { data.gameType = 2; }
    // Horde is a mission type, not the cooperative GameType=2 requirement.
    data.wave = m_level.GetWave() + 1; // Original +0x4BEEC is the global wave, not modulo revolution.
    data.waveCleared = waveCleared;
    data.perfect = !m_scene.GetWavePerfectResults().empty() && m_scene.GetWavePerfectResults().back();
    data.ended = ended;
    data.kills = m_scene.TakeChallengeKills();
    data.powerups = m_scene.TakeChallengePowerups();
    m_challenges->UpdateFromLevelSession(data, *m_challengeWeapons, *m_challengeProfile);
    m_challengeSessionEnded = ended;
    return m_challenges->StoreProgress(*m_challengeProfile);
}
