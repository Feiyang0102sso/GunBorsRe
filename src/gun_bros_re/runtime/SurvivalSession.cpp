/** @file SurvivalSession.cpp
 * @brief Keep level templates stable and deliver actor/HUD callbacks explicitly.
 */
#define NOMINMAX
#include "runtime/SurvivalSession.h"
#include "runtime/CombatGeometry.h"
#include "runtime/StoreCatalog.h"
#include "runtime/SurvivalHud.h"
#include "gun_bros/Mission.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

SurvivalSession::SurvivalSession(CombatScene &scene, CMap &map,
    const std::vector<EnemyTemplateData> &catalog) : m_scene(scene), m_map(map), m_catalog(catalog) {
    m_scene.SetLevel(&m_level);
}

bool SurvivalSession::Load(CResTOCManager &toc, PackTables &tables, std::uint32_t mapPack, unsigned mapIndex,
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
        GameSection section = GameSection::Mission;
        if (selectedLevel != nullptr) { section = GameSection::Level; }
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
                if (!tables.ReadSectionResource(level.packHash, GameSection::Level, level.localIndex, payload)) { return false; }
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
                m_template = std::move(candidate);
                if (selectedLevel != nullptr) {
                    std::printf("[survival] selected explicit LEVEL %u:%u for MAP %u:%u\n", level.packHash, level.localIndex, mapPack, mapIndex);
                    return true;
                }
            }
        }
    }
    if (!requested.IsNull() && selectedLevel == nullptr) {
        std::printf("[survival] selected retail Mission LEVEL %u:%u for MAP %u:%u\n", requested.packHash, requested.localIndex, mapPack, mapIndex);
        return true;
    }
    std::printf("[survival] no retail survival level for requested map\n");
    return false;
}

void SurvivalSession::Restart(float x, float y) {
    m_scene.Reset();
    if (m_powerups != nullptr) { m_powerups->Reset(); }
    if (m_pickups != nullptr) { m_pickups->Reset(); }
    if (m_props != nullptr) { m_props->Reset(); }
    m_scene.playerX = x;
    m_scene.playerY = y;
    m_scene.ResetBrotherPosition(x, y);
    m_spawnSerial = 0;
    m_kills = 0;
    m_transitionMs = 1200; // Desktop intro duration; original completion event retained.
    m_transitionDuration = 1200;
    if (m_horde) { m_transitionMs = 0; } // BOKOR owns its five-second intro timer.
    m_level.Bind(m_template, m_map, this, m_startWave);
    m_bossIntroSerial = m_level.GetBossIntroSerial();
    if (m_bossIntroSerial > 0) { m_transitionMs = 2000; m_transitionDuration = 2000; }
    m_bossWave = m_bossIntroSerial > 0;
    if (m_originalHud != nullptr) {
        m_transitionMs = 0;
        unsigned wave = m_level.GetRealWave() + 1;
        if (m_horde && m_level.GetWavesPerRevolution() > 0) { wave = m_level.GetWave() / m_level.GetWavesPerRevolution() + 1; }
        m_originalHud->BeginOriginalLevel(wave, m_horde, m_bossWave);
    }
    UpdateCamera();
    UpdateDialog(0);
    for (const CLayerPathLink &path : m_map.GetPathLinkLayers()) {
        std::printf("[survival] path layer=%u nodes=%zu selected=%d\n", path.GetLayerIndex(), path.GetNodes().size(), m_level.GetPathLayer());
    }
}

bool SurvivalSession::SpawnEnemy(const GameObjectRef &enemy, int layerIndex, int nodeIndex, int objectId) {
    std::size_t entryIndex = 0;
    while (entryIndex < m_catalog.size()) {
        if (m_catalog[entryIndex].packHash == enemy.packHash && m_catalog[entryIndex].ordinal == enemy.localIndex) { break; }
        ++entryIndex;
    }
    if (entryIndex == m_catalog.size()) { return false; }
    CLayerPathLink *path = m_map.GetPathLinkLayer(layerIndex);
    if (path == nullptr || path->GetNodes().empty()) { return false; }
    const auto &nodes = path->GetNodes();
    if (nodeIndex < 0) {
        // Cycle through authored spawn nodes; exact original free-node scoring
        // is still being researched. Never invent positions beyond the map.
        const auto &enabled = m_level.GetSpawner().GetEnabledNodes();
        for (unsigned attempt = 0; attempt < nodes.size(); ++attempt) {
            const unsigned candidate = (m_spawnSerial + attempt) % nodes.size();
            if (nodes[candidate].locked) { continue; }
            if (!m_level.GetSpawner().AllNodesEnabled() &&
                std::find(enabled.begin(), enabled.end(), static_cast<int>(candidate)) == enabled.end()) { continue; }
            nodeIndex = static_cast<int>(candidate);
            m_spawnSerial = candidate + 1;
            break;
        }
    }
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) { return false; }
    CombatEnemy *actor = m_scene.Spawn(entryIndex, nodes[nodeIndex].x, nodes[nodeIndex].y);
    if (actor == nullptr) { return false; }
    actor->objectId = objectId;
    // CLevel::AddObject :116890 attaches the enemy direction marker.
    m_level.SetIndicator(objectId, 0, actor->model.enemy.combat.id);
    return true;
}

bool SurvivalSession::SpawnMapObject(const PlacedObject &object, int objectId) {
    if (object.objectType == static_cast<unsigned>(PlacedObjectType::Pickup)) {
        GameObjectRef pickup;
        pickup.packHash = object.packHash;
        pickup.localIndex = object.localIndex;
        return SpawnPickupAt(pickup, object.x, object.y, objectId);
    }
    // Static props and players are already loaded by the map host.
    if (object.objectType != static_cast<unsigned>(PlacedObjectType::Enemy)) { return true; }
    for (unsigned index = 0; index < m_catalog.size(); ++index) {
        if (m_catalog[index].packHash != object.packHash || m_catalog[index].ordinal != object.localIndex) { continue; }
        CombatEnemy *actor = m_scene.Spawn(index, object.x, object.y);
        if (actor == nullptr) { return false; }
        actor->objectId = objectId;
        m_level.SetIndicator(objectId, 0, actor->model.enemy.combat.id);
        actor->model.enemy.combat.facing = static_cast<float>(object.facing);
        std::printf("[survival] placed enemy id=%d tag=%u item=%u path=%u facing=%d\n",
            objectId, object.spawnTag, object.localIndex, object.pathLayer, object.facing);
        return true;
    }
    return false;
}

void SurvivalSession::SendEnemyMessage(int objectId, int message) {
    for (const auto &actor : m_scene.enemies) {
        if (actor->objectId == objectId) {
            actor->model.enemy.HandleMessage(message);
            return;
        }
    }
}

void SurvivalSession::SendPropMessage(int objectId, int message) {
    if (m_props != nullptr) { m_props->SendMessage(objectId, message); }
}

void SurvivalSession::OnWaveCleared(unsigned perfectRewardPercent) {
    const unsigned previousPerfect = m_scene.GetPerfectWaves();
    m_scene.OnWaveCleared(perfectRewardPercent);
    // CGame::OnWaveCleared :76246 only shows this sequence for game type 1.
    if (m_originalHud != nullptr && !m_horde) {
        m_originalHud->OnOriginalWaveClear(m_level.GetRealWave() + 1,
            m_scene.GetPerfectWaves() > previousPerfect, perfectRewardPercent, m_bossWave);
    }
    m_bossWave = false;
}

bool SurvivalSession::IsTransitioning() const {
    if (m_originalHud != nullptr) { return m_originalHud->HasInterstitial(); }
    return m_transitionMs > 0;
}

unsigned SurvivalSession::GetTransitionElapsed() const {
    if (m_originalHud != nullptr) { return m_originalHud->NoticeTime(); }
    return m_transitionDuration - m_transitionMs;
}

void SurvivalSession::PlayLevelSound(const GameObjectRef &sound) {
    if (m_effects == nullptr) { return; }
    GunCue cue;
    cue.kind = GunCue::Kind::Sound;
    cue.resource = sound;
    m_effects->Emit(cue, 0, 0, 0, 0);
}

unsigned SurvivalSession::CheckLevelSounds() {
    if (m_effects == nullptr) { return 1; }
    unsigned failures = 0, checked = 0;
    const auto &resources = m_template.script.GetResources();
    for (unsigned index = 0; index < resources.size(); ++index) {
        if (resources[index].sectionOrType != static_cast<unsigned>(GameSection::SoundEffect) - 1) { continue; }
        const std::size_t before = m_effects->GetSoundCueCount();
        const std::int16_t argument = static_cast<std::int16_t>(index);
        m_level.FunctionResolver(20, &argument, 1);
        if (m_effects->GetSoundCueCount() != before + 1) { ++failures; }
        ++checked;
    }
    std::printf("[level-sound-check] references=%u failures=%u\n", checked, failures);
    return failures;
}

bool SurvivalSession::SpawnPickup(const GameObjectRef &pickup, int layer, int node, int objectId, bool nearby) {
    if (m_pickups == nullptr) { return false; }
    if (layer < 0) { layer = m_level.GetPathLayer(); }
    ILayerPath *path = m_map.GetPathLayer(layer);
    if (path == nullptr || path->GetNodes().empty()) { return false; }
    const auto &nodes = path->GetNodes();
    if (nearby) { node = path->FindNearest(m_scene.playerX, m_scene.playerY); }
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

bool SurvivalSession::SpawnPickupAt(const GameObjectRef &pickup, float x, float y, int objectId) {
    if (m_pickups == nullptr) { return false; }
    if (!m_pickups->Spawn(pickup, x, y, objectId)) { return false; }
    // CEnemySpawner::SpawnPickup :146349 marks the particular pickup instance.
    m_level.SetIndicator(objectId, 1, (1ULL << 32) | m_pickups->spawned);
    return true;
}

bool SurvivalSession::GetIndicatorTarget(std::uint64_t key, float &x, float &y) const {
    if ((key >> 32) == 1) {
        return m_pickups != nullptr && m_pickups->GetIndicatorTarget(static_cast<unsigned>(key), x, y);
    }
    CombatEnemy *actor = m_scene.Find(static_cast<CombatId>(key));
    if (actor == nullptr || actor->model.enemy.combat.dead || actor->model.enemy.combat.removed) { return false; }
    x = actor->model.enemy.combat.x; y = actor->model.enemy.combat.y; return true;
}

bool SurvivalSession::GetObjectPosition(int objectId, float &x, float &y) const {
    for (const auto &actor : m_scene.enemies) {
        if (actor->objectId != objectId || actor->model.enemy.combat.dead || actor->model.enemy.combat.removed) { continue; }
        x = actor->model.enemy.combat.x;
        y = actor->model.enemy.combat.y;
        return true;
    }
    if (m_pickups != nullptr && m_pickups->GetObjectPosition(objectId, x, y)) { return true; }
    return m_props != nullptr && m_props->GetObjectPosition(objectId, x, y);
}

int SurvivalSession::CountEnemies(const GameObjectRef *enemy, int objectId) const {
    int count = 0;
    for (const auto &actor : m_scene.enemies) {
        const EnemyCombat &state = actor->model.enemy.combat;
        if (state.dead || state.removed) { continue; }
        if (objectId >= 0 && actor->objectId != objectId) { continue; }
        if (enemy == nullptr || (actor->data->packHash == enemy->packHash && actor->data->ordinal == enemy->localIndex)) { ++count; }
    }
    return count;
}

void SurvivalSession::CompleteDialog() {
    m_dialogText.clear();
    m_level.CompleteDialog();
    UpdateDialog(0);
}

void SurvivalSession::UpdateDialog(int deltaMs) {
    if (m_level.IsDialogCloseRequested()) { CompleteDialog(); return; }
    if (m_dialogSerial != m_level.GetDialogSerial()) {
        m_dialogSerial = m_level.GetDialogSerial();
        m_dialogElapsedMs = 0;
        m_dialogText.clear();
        CGameAssetRef resource;
        if (m_toc != nullptr && m_level.GetStringResource(m_level.GetDialogResource(), resource)) {
            m_dialogText = ReadGameString(*m_toc, resource);
            std::printf("[campaign-dialog] %s\n", m_dialogText.c_str());
        }
    }
    if (m_level.GetDialogResource() < 0) { m_dialogText.clear(); return; }
    m_dialogElapsedMs += deltaMs;
    // Desktop reading duration; original movie/text-box pagination remains
    // separate research. Native argument three means automatic close, not pause.
    const int readingMs = 2000 + static_cast<int>(m_dialogText.size()) * 40;
    if (m_level.DoesDialogAutoClose() && m_dialogElapsedMs >= readingMs) { CompleteDialog(); }
}

void SurvivalSession::UpdateArchiveMap(float previousX, float previousY) {
    const float scaleRatio = 0.8f / m_map.GetCamera().GetScale();
    const float viewWidth = m_viewWidth * scaleRatio;
    const float viewHeight = m_viewHeight * scaleRatio;
    float left = m_scene.playerX - viewWidth * 0.5f;
    float top = m_scene.playerY - viewHeight * 0.5f;
    const MapRectangle bounds = m_map.GetVisibleBounds();
    if (!bounds.IsEmpty()) {
        if (bounds.width <= viewWidth) { left = bounds.x + (bounds.width - viewWidth) * 0.5f; }
        else { left = std::clamp(left, static_cast<float>(bounds.x), bounds.x + bounds.width - viewWidth); }
        if (bounds.height <= viewHeight) { top = bounds.y + (bounds.height - viewHeight) * 0.5f; }
        else { top = std::clamp(top, static_cast<float>(bounds.y), bounds.y + bounds.height - viewHeight); }
    }
    m_level.UpdateProximitySpawns(left, top, viewWidth, viewHeight);
    for (unsigned index = 0; index < m_map.GetCollisionLayerCount(); ++index) {
        const CLayerCollision &layer = m_map.GetCollisionLayer(index);
        if (static_cast<int>(layer.GetLayerIndex()) != m_level.GetTriggerLayer()) { continue; }
        const auto &geometry = layer.GetCollision();
        float nearest = 2;
        int group = -1;
        for (const CollisionEdge &edge : geometry.GetEdges()) {
            if (!edge.enabled) { continue; }
            const float fraction = CombatGeometry::EdgeFraction(previousX, previousY,
                m_scene.playerX - previousX, m_scene.playerY - previousY,
                geometry.GetVertices()[edge.firstVertex], geometry.GetVertices()[edge.secondVertex], m_scene.GetPlayerRadius());
            if (fraction < nearest) { nearest = fraction; group = edge.group; }
        }
        if (group >= 0 && nearest <= 1) { m_level.OnTrigger(group); }
        break;
    }
}

void SurvivalSession::Update(int deltaMs, float moveX, float moveY, bool fire) {
    if (deltaMs <= 0) { return; }
    m_map.GetCamera().Update(deltaMs);
    UpdateDialog(deltaMs);
    if (m_originalHud != nullptr) { m_originalHud->Advance(deltaMs); }
    if (m_level.IsCleared()) {
        m_scene.Update(deltaMs, 0, 0, false);
        UpdateCamera(deltaMs);
        return;
    }
    if (m_originalHud != nullptr) {
        // InterstitialSequenceCallback :86336 emits LEVEL event 2 only after
        // the last authored Movie completes. BOKOR keeps its script clock alive.
        if (m_originalHud->TakeInterstitialCompletion()) { m_level.HandleEvent(2); }
        if (m_originalHud->HasInterstitial() && !m_horde) { UpdateCamera(deltaMs); return; }
    }
    if (m_originalHud == nullptr && m_transitionMs > 0) {
        m_transitionMs -= deltaMs;
        if (m_transitionMs <= 0) { m_level.HandleEvent(2); }
        if (!m_horde) { UpdateCamera(deltaMs); return; }
    }
    const int previousWave = m_level.GetWave();
    m_level.Update(deltaMs);
    m_scene.SetPathLayer(m_level.GetPathLayer());
    const float previousX = m_scene.playerX;
    const float previousY = m_scene.playerY;
    if (!m_level.CanPlayerMove()) { moveX = 0; moveY = 0; }
    if (!m_level.CanPlayerShoot()) { fire = false; }
    m_scene.Update(deltaMs, moveX, moveY, fire);
    if (m_archive) { UpdateArchiveMap(previousX, previousY); }
    if (m_powerups != nullptr) { m_powerups->Update(deltaMs); }
    if (m_props != nullptr) { m_props->Update(deltaMs); }
    if (m_pickups != nullptr) {
        for (const PickupSpawn &spawn : m_scene.pickupSpawns) { SpawnPickupAt(spawn.resource, spawn.x, spawn.y, 0); }
        m_pickups->Update(deltaMs, m_scene, *m_effects);
        for (const PickupCollection &pickup : m_pickups->collections) {
            m_level.OnPickupCollected(pickup.objectId, pickup.resource);
        }
    }
    for (std::uint8_t event : m_scene.levelEvents) { m_level.HandleEvent(event); }
    // Deliver only after the scene update, so callbacks may safely spawn actors.
    for (const CombatDeath &death : m_scene.deaths) {
        m_level.OnEnemyKilled(death.objectId, death.enemy);
        ++m_kills;
    }
    if (m_originalHud == nullptr && m_level.GetWave() != previousWave && !m_level.IsCleared()) { m_transitionMs = 1200; m_transitionDuration = 1200; }
    if (m_originalHud != nullptr && m_horde && m_level.GetWave() != previousWave && !m_level.IsCleared()) {
        // CGame::OnLevelStart :75385 names Horde rounds with GetRevolution.
        // Its Movie callback releases the script's slow-motion intermission.
        const int divisor = m_level.GetWavesPerRevolution();
        if (divisor > 0) { m_originalHud->BeginOriginalLevel(m_level.GetWave() / divisor + 1, true, false); }
    }
    if (m_bossIntroSerial != m_level.GetBossIntroSerial()) {
        m_bossIntroSerial = m_level.GetBossIntroSerial();
        // OnBossWaveStart uses GLU_MOVIE_WAVE_CLEARED and its real 2000 ms
        // completion callback before releasing the next scripted state.
        m_transitionMs = 2000;
        m_transitionDuration = 2000;
        m_bossWave = true;
        if (m_originalHud != nullptr) {
            m_transitionMs = 0;
            m_originalHud->BeginOriginalLevel(m_level.GetRealWave() + 1, m_horde, true);
        }
    }
    UpdateCamera(deltaMs);
}

unsigned SurvivalSession::GetPowerupCount(unsigned localIndex) const {
    if (m_powerups == nullptr) { return 0; }
    return m_powerups->GetCount(localIndex);
}

void SurvivalSession::UpdateCamera(int deltaMs) {
    const MapRectangle bounds = m_map.GetVisibleBounds();
    const float scale = 0.8f / m_map.GetCamera().GetScale();
    m_map.GetCamera().UpdatePosition(m_scene.playerX, m_scene.playerY, bounds.x, bounds.y,
        bounds.width, bounds.height, m_viewWidth * scale, m_viewHeight * scale);
    m_scene.SetViewCenter(m_map.GetCamera().GetX(), m_map.GetCamera().GetY());
    const float width = m_viewWidth * scale;
    const float height = m_viewHeight * scale;
    // The original reserves 25 screen units at top/sides and 100 at bottom.
    m_level.UpdateIndicators(deltaMs, m_map.GetCamera().GetX() - width * 0.5f + width * 20 / 1024,
        m_map.GetCamera().GetY() - height * 0.5f + height * 20 / 768,
        width * 984 / 1024, height * 668 / 768);
}

void SurvivalSession::UpdateAfterDeath(int deltaMs) {
    if (m_originalHud != nullptr) { m_originalHud->Advance(deltaMs); }
    // Finish existing attacks and camera effects without advancing new waves.
    m_map.GetCamera().Update(deltaMs);
    m_scene.Update(deltaMs, 0, 0, false);
    if (m_powerups != nullptr) { m_powerups->Update(deltaMs); }
    UpdateCamera(deltaMs);
}
