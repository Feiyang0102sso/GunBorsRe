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
#include <chrono>

// CBrother constructor :139098; CPlayer::Move uses the full radius for triggers.
constexpr float kBrotherTriggerRadius = 22.0f;
// Host cheat bounds, not resource timings. Normal gameplay never uses these.
constexpr int kBossSkipStepMs = 16;
constexpr int kBossSkipLimitMs = 600000;

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

void SurvivalSession::Restart(float x, float y, float facingDegrees) {
    m_scene.Reset();
    if (m_powerups != nullptr) { m_powerups->Reset(); }
    if (m_pickups != nullptr) { m_pickups->Reset(); }
    if (m_props != nullptr) { m_props->Reset(); }
    m_scene.playerX = x;
    m_scene.playerY = y;
    // CBrother::Spawn :135887 writes one spawn angle to the player and the
    // AI brother alike.
    m_scene.facing = facingDegrees;
    m_scene.ResetBrotherPosition(x, y, facingDegrees);
    m_spawnSerial = 0;
    m_kills = 0;
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

/**
 * Where a rule-driven spawn appears.
 *
 * CEnemySpawner::GetSpawnPoint :146098 picks between two rules. With an
 * explicit node list (DisableAllNodes + EnableNode) it is
 * GetSpawnPointSpecific :146576: one of the listed nodes, uniformly at random,
 * with no other test. Otherwise it is GetSpawnPointOffScreen :146112, which
 * hands CLayerPathLink::GetSpawnLocation :166819 the player's position
 * (GetSpawnSource :147224) and an offscreen filter built from the camera
 * rectangle grown by ten units on each side (SetupSpawnFilter :147283).
 *
 * GetSpawnLocation walks every node, drops the locked ones and the ones the
 * filter rejects for being on screen, and feeds the rest to a DistanceList
 * :167261 that keeps the five nearest to the player. One of those five is then
 * chosen at random -- so enemies arrive from just outside the view, never in
 * the player's face, and never from the far side of the map.
 *
 * No node qualifying is an ordinary outcome: the original spawns nothing that
 * tick and the rule tries again on the next one.
 */
int SurvivalSession::ChooseSpawnNode(const ILayerPath &path) {
    const auto &nodes = path.GetNodes();
    const CEnemySpawner &spawner = m_level.GetSpawner();
    if (!spawner.AllNodesEnabled()) {
        const auto &enabled = spawner.GetEnabledNodes();
        if (enabled.empty()) { return -1; }
        const int pick = m_level.RandomInteger(0, static_cast<std::int16_t>(enabled.size() - 1));
        return enabled[pick];
    }
    const float left = m_cameraLeft - 10;
    const float top = m_cameraTop - 10;
    const float right = left + m_cameraWidth + 20;
    const float bottom = top + m_cameraHeight + 20;
    // {node index, squared distance to the player}, nearest first.
    std::vector<std::pair<int, float>> nearest;
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const ILayerPath::Node &node = nodes[index];
        if (node.locked) { continue; }
        const bool onScreen = m_cameraWidth > 0 && m_cameraHeight > 0 &&
            node.x >= left && node.x <= right && node.y >= top && node.y <= bottom;
        if (onScreen) { continue; }
        const float dx = m_scene.playerX - node.x;
        const float dy = m_scene.playerY - node.y;
        const float distance = dx * dx + dy * dy;
        std::size_t position = 0;
        while (position < nearest.size() && nearest[position].second <= distance) { ++position; }
        if (position >= 5) { continue; }
        nearest.insert(nearest.begin() + position, {static_cast<int>(index), distance});
        if (nearest.size() > 5) { nearest.pop_back(); }
    }
    if (nearest.empty()) { return -1; }
    const int pick = m_level.RandomInteger(0, static_cast<std::int16_t>(nearest.size() - 1));
    return nearest[pick].first;
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
    if (nodeIndex < 0) { nodeIndex = ChooseSpawnNode(*path); }
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) { return false; }
    CombatEnemy *actor = m_scene.Spawn(entryIndex, nodes[nodeIndex].x, nodes[nodeIndex].y);
    if (actor == nullptr) { return false; }
    if (objectId < 0) {
        const float distance = std::hypot(m_scene.playerX - nodes[nodeIndex].x, m_scene.playerY - nodes[nodeIndex].y);
        if (m_closestSpawnDistance < 0 || distance < m_closestSpawnDistance) { m_closestSpawnDistance = distance; }
        if (m_cameraWidth > 0 && nodes[nodeIndex].x >= m_cameraLeft && nodes[nodeIndex].y >= m_cameraTop &&
            nodes[nodeIndex].x <= m_cameraLeft + m_cameraWidth && nodes[nodeIndex].y <= m_cameraTop + m_cameraHeight) {
            ++m_onScreenSpawns;
        }
    }
    actor->objectId = objectId;
    // CLevel::AddObject :116890 attaches the enemy direction marker.
    m_level.SetIndicator(objectId, 0, actor->model.enemy.combat.id);
    return true;
}

void SurvivalSession::StartObjectLayer(int layer) {
    if (m_props != nullptr) { m_props->StartLayer(layer); }
}

bool SurvivalSession::SpawnMapObject(const PlacedObject &object, int objectId) {
    if (object.objectType == static_cast<unsigned>(PlacedObjectType::Prop)) {
        if (m_props == nullptr) { return false; }
        return m_props->Spawn(m_level.GetObjectLayer(), objectId);
    }
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
        actor->mapPlaced = true;
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
        // Each reference is checked independently. Some authored entries share
        // a WAV, so checking them in one tick would correctly coalesce them.
        m_effects->BeginAudioFrame();
        const std::size_t before = m_effects->GetSoundCueCount();
        const std::int16_t argument = static_cast<std::int16_t>(index);
        m_level.FunctionResolver(20, &argument, 1);
        if (m_effects->GetSoundCueCount() != before + 1) { ++failures; }
        ++checked;
    }
    std::printf("[level-sound-check] references=%u failures=%u\n", checked, failures);
    return failures;
}

unsigned SurvivalSession::CheckTriggerRoutes(float startX, float startY, float startFacing) {
    unsigned failures = 0, tested = 0;
    for (unsigned layerIndex = 0; layerIndex < m_map.GetCollisionLayerCount(); ++layerIndex) {
        const auto &layer = m_map.GetCollisionLayer(layerIndex);
        if (static_cast<int>(layer.GetLayerIndex()) != m_level.GetTriggerLayer()) { continue; }
        const auto &geometry = layer.GetCollision();
        std::vector<unsigned> groups;
        for (const auto &edge : geometry.GetEdges()) {
            if (std::find(groups.begin(), groups.end(), edge.group) != groups.end()) { continue; }
            bool reached = false;
            const auto &a = geometry.GetVertices()[edge.firstVertex];
            const auto &b = geometry.GetVertices()[edge.secondVertex];
            const float length = std::hypot(b.x - a.x, b.y - a.y);
            if (length == 0) { continue; }
            const float nx = -(b.y - a.y) / length, ny = (b.x - a.x) / length;
            for (int side : {-1, 1}) {
                Restart(startX, startY, startFacing);
                // Let the original intro complete before supplying movement.
                for (unsigned tick = 0; tick < 250; ++tick) { Update(16, 0, 0, false); }
                m_scene.playerX = (a.x + b.x) * 0.5f + nx * 45 * side;
                m_scene.playerY = (a.y + b.y) * 0.5f + ny * 45 * side;
                const unsigned before = m_level.GetTriggerCount();
                for (unsigned tick = 0; tick < 45; ++tick) { Update(16, -nx * side, -ny * side, false); }
                reached = m_level.GetTriggerCount() > before;
                if (reached) { break; }
            }
            std::printf("[map-trigger-check] layer=%u group=%u reached=%d position=%.1f,%.1f\n",
                layer.GetLayerIndex(), edge.group, reached, m_scene.playerX, m_scene.playerY);
            if (reached) { groups.push_back(edge.group); ++tested; }
        }
        std::vector<unsigned> expected;
        for (const auto &edge : geometry.GetEdges()) {
            if (std::find(expected.begin(), expected.end(), edge.group) == expected.end()) { expected.push_back(edge.group); }
        }
        if (groups.size() != expected.size()) { ++failures; }
    }
    Restart(startX, startY, startFacing);
    std::printf("[map-trigger-check] groups=%u failures=%u\n", tested, failures);
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

std::uint64_t SurvivalSession::ResolveIndicatorTarget(int objectId) const {
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

bool SurvivalSession::GetIndicatorTarget(std::uint64_t key, float &x, float &y) const {
    if ((key >> 32) == 2) {
        return m_props != nullptr && m_props->GetIndicatorTarget(static_cast<unsigned>(key), x, y);
    }
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

void SurvivalSession::UpdateMapInteractions(float previousX, float previousY) {
    if (m_archive) {
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
    }
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

bool SurvivalSession::SkipToBoss() {
    PlayerVitals &player = m_scene.GetPlayerVitals();
    if (m_archive || m_horde || m_level.GetTutorialStep() >= 0 || player.dead ||
        m_level.IsCleared() || m_level.IsPaused() ||
        (m_powerups != nullptr && m_powerups->IsMovieActive())) {
        std::printf("[stboss] unavailable in current mode or presentation\n");
        return false;
    }
    if (m_level.HasLargeEnemyHealthBars()) {
        std::printf("[stboss] boss already active\n");
        return false;
    }
    const auto started = std::chrono::steady_clock::now();
    const unsigned introSerial = m_level.GetBossIntroSerial();
    const bool playerInvincible = player.invincible;
    PlayerVitals *brother = m_scene.GetBrotherVitals();
    bool brotherInvincible = false;
    if (brother != nullptr) { brotherInvincible = brother->invincible; brother->invincible = true; }
    player.invincible = true;
    // The original LEVEL consumes this flag in its next Boss probability roll.
    // Remaining spawns must still die: resetting the spawner strands kill quotas.
    *m_level.VariableResolver(5) = 1;
    if (m_effects != nullptr) { m_effects->SetPaused(true); }
    int elapsed = 0;
    unsigned defeated = 0;
    while (elapsed < kBossSkipLimitMs && m_level.GetBossIntroSerial() == introSerial && !m_level.IsCleared()) {
        for (auto &actor : m_scene.enemies) {
            CEnemy &enemy = actor->model.enemy;
            if (!actor->mapPlaced && enemy.CanReceiveProjectile(0, kPlayerCombatId)) {
                enemy.Damage(enemy.combat.health);
                ++defeated;
            }
        }
        // Retire skipped projectiles/audio before each tick. The last tick's
        // real Boss spawn cues survive, so its authored entrance plays normally.
        if (m_effects != nullptr) { m_effects->Clear(); }
        Update(kBossSkipStepMs, 0, 0, false);
        elapsed += kBossSkipStepMs;
    }
    player.invincible = playerInvincible;
    if (brother != nullptr) { brother->invincible = brotherInvincible; }
    if (m_effects != nullptr) { m_effects->SetPaused(false); }
    *m_level.VariableResolver(5) = 0;
    const bool success = m_level.GetBossIntroSerial() != introSerial;
    const auto wallMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    std::printf("[stboss] started=%d defeated=%u simulated-ms=%d wall-ms=%lld wave=%d state=%d\n",
        success, defeated, elapsed, static_cast<long long>(wallMs), m_level.GetWave(), m_level.GetStateId());
    return success;
}

void SurvivalSession::Update(int deltaMs, float moveX, float moveY, bool fire) {
    if (deltaMs <= 0) { return; }
    // CLevel::Update :121255 advances the active powerup before its pause
    // gate. Keep presentation time alive without advancing actors or spawns.
    if (m_powerups != nullptr && m_powerups->IsMovieActive()) {
        m_powerups->Update(deltaMs);
        return;
    }
    if (m_level.IsPaused()) { return; }
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
        // CGame::Update :76581 keeps CLevel::Update running under the Movie.
        // The script's object multiplier supplies slow motion, not a pause.
    }
    if (m_originalHud == nullptr && m_transitionMs > 0) {
        m_transitionMs -= deltaMs;
        if (m_transitionMs <= 0) { m_level.HandleEvent(2); }
    }
    const int previousWave = m_level.GetWave();
    m_level.Update(deltaMs);
    m_scene.SetPathLayer(m_level.GetPathLayer());
    const float previousX = m_scene.playerX;
    const float previousY = m_scene.playerY;
    if (!m_level.CanPlayerMove()) { moveX = 0; moveY = 0; }
    if (!m_level.CanPlayerShoot()) { fire = false; }
    m_scene.Update(deltaMs, moveX, moveY, fire);
    // CPlayer::Move checks triggers in every mode, including retail survival.
    UpdateMapInteractions(previousX, previousY);
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

void SurvivalSession::UpdateAfterDeath(int deltaMs) {
    if (m_powerups != nullptr && m_powerups->IsMovieActive()) {
        m_powerups->Update(deltaMs);
        return;
    }
    if (m_originalHud != nullptr) { m_originalHud->Advance(deltaMs); }
    // Finish existing attacks and camera effects without advancing new waves.
    m_map.GetCamera().Update(deltaMs);
    m_scene.Update(deltaMs, 0, 0, false);
    if (m_powerups != nullptr) { m_powerups->Update(deltaMs); }
    UpdateCamera(deltaMs);
}
