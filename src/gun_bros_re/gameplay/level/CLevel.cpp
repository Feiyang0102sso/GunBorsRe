/**
 * @file CLevel.cpp
 * @brief A level: a map plus the script that drives it.
 */

#include "gun_bros_re/gameplay/level/CLevel.h"

#include "gun_bros_re/gameplay/CGame.h"
#include "gun_bros_re/gameplay/CMPMatch.h"
#include "gun_bros_re/gameplay/CMap.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
#include "gun_bros_re/gameplay/ZPickupScene.h"
#include "gun_bros_re/ui/CPowerUpSelector.h"
#include "gun_bros_re/gameplay/ZPropWorld.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/data/CFriendPowerManager.h"
#include "gun_bros_re/data/ZStoreCatalog.h"

#include <cstdio>
#include <algorithm>
#include <cmath>

// CBrother constructor :139098; CPlayer::Move uses the full radius for triggers.
constexpr float kBrotherTriggerRadius = 22.0f;
// CEffectLayer::AddTextEffect :66884 has twenty fixed text-effect slots.
constexpr unsigned kTextEffectCapacity = 20;

void CLevel::FocusCameraOnEnemy(float x, float y) {
    if (m_map == nullptr) { return; }
    m_map->GetCamera().SetCameraMode(2);
    m_map->GetCamera().SetTarget(x, y);
}

bool CLevel::SetIndicator(int objectId, unsigned type, std::uint64_t targetKey) {
    if (type >= 7 || m_indicators.size() >= 30 || m_world == nullptr) { return false; }
    CLevelIndicator indicator;
    // CLevel::FunctionResolver native 49 :117927 resolves once, then Init
    // retains the target instance. A drawing-order ID search is not equivalent.
    if (targetKey == 0) { targetKey = m_world->ResolveIndicatorTarget(objectId); }
    indicator.objectId = objectId;
    indicator.targetKey = targetKey;
    indicator.type = type;
    bool found = false;
    if (targetKey == 0) { found = m_world->GetObjectPosition(objectId, indicator.x, indicator.y); }
    else { found = m_world->GetIndicatorTarget(targetKey, indicator.x, indicator.y); }
    if (!found) { return false; }
    // Native 49 :117927 looks the object up by ILevelObject::GetID -- the
    // object's index in its layer. A script that never assigns its own
    // variable asks for index 0, so log what each marker actually bound to.
    if (type != 0) {
        std::printf("[indicator] object=%d type=%u target=%llu at %.0f,%.0f\n",
            objectId, type, static_cast<unsigned long long>(targetKey), indicator.x, indicator.y);
    }
    m_indicators.push_back(indicator);
    return true;
}

void CLevel::RemoveIndicator(int objectId) {
    for (CLevelIndicator &indicator : m_indicators) {
        if (indicator.objectId == objectId) { indicator.FadeOut(); return; }
    }
}

void CLevel::UpdateIndicators(int deltaMs, float left, float top, float width, float height) {
    for (std::size_t index = 0; index < m_indicators.size();) {
        CLevelIndicator &indicator = m_indicators[index];
        indicator.Update(deltaMs);
        bool found = false;
        if (m_world != nullptr) {
            if (indicator.targetKey == 0) { found = m_world->GetObjectPosition(indicator.objectId, indicator.x, indicator.y); }
            else { found = m_world->GetIndicatorTarget(indicator.targetKey, indicator.x, indicator.y); }
        }
        if (!found) { indicator.FadeOut(); }
        // Original markers retire once the target enters the inner viewport.
        if (indicator.x > left && indicator.x < left + width && indicator.y > top && indicator.y < top + height) { indicator.FadeOut(); }
        if (indicator.IsDone()) { m_indicators.erase(m_indicators.begin() + index); }
        else { ++index; }
    }
}

CLevel::Template::Template() : wavesPerRevolution(0), waveLimit(0), perfectWaveRewardPercent(0) {}

bool CLevel::Template::Init(CArrayInputStream &stream) {
    mapRef.Init(stream);
    script.Load(stream);
    wavesPerRevolution = stream.ReadUInt16();
    waveLimit = stream.ReadUInt16();
    perfectWaveRewardPercent = stream.ReadUInt16();

    return !stream.Overran();
}

CLevel::CLevel() : m_template(nullptr), m_map(nullptr), m_unimplementedCalls(0) {
    for (std::uint32_t i = 0; i < kLevelVariableCount; ++i) {
        m_variables[i] = 0;
    }
    for (auto &enemy : m_enemyMultipliers) {
        for (float &multiplier : enemy) { multiplier = 1; }
    }
}

void CLevel::AttachRuntime(CGame &game, const std::vector<ZEnemyTemplateData> &catalog) {
    m_game = &game;
    m_catalog = &catalog;
    m_objects.SetLevel(this);
}

void CLevel::SetViewSize(float width, float height) {
    m_viewWidth = width;
    m_viewHeight = height;
}

void CLevel::ResetWorld(float x, float y, float facingDegrees) {
    if (m_playerModel == nullptr) { return; }
    Reset();
    if (m_powerups != nullptr) { ResetPowerup(*m_powerups); }
    if (m_peerPowerups != nullptr) { ResetPowerup(*m_peerPowerups); }
    if (m_pickups != nullptr) { m_pickups->Reset(); }
    if (m_props != nullptr) { m_props->Reset(); }
    GetPlayer().x = x;
    GetPlayer().y = y;
    // CBrother::Spawn :135887 writes one spawn angle to both brothers.
    GetPlayer().facing = facingDegrees;
    ResetBrotherPosition(x, y, facingDegrees);
    m_spawnSerial = 0;
    m_closestSpawnDistance = -1;
    m_onScreenSpawns = 0;
    BeginCombatFrame();
}

void CLevel::Bind(const Template &levelTemplate, CMap &map, ZLevelWorld *world, int startWave) {
    SetLevelContext(this);
    m_template = &levelTemplate;
    m_map = &map;
    // CLevel snaps to 0.8 before executing OnLevelStart (:121002).
    map.GetCamera().Reset(0.8f);
    map.UnlockAllPathNodes();
    m_world = world;
    if (m_world == nullptr && m_playerModel != nullptr) { m_world = this; }
    m_spawner.Bind(*this, m_world);
    m_timerMs = 0;
    m_timerFunction = -1;
    m_eventTimerMs = 0;
    m_objectLayer = -1;
    m_cameraEntered.assign(map.GetCameraLayerCount(), false);
    m_spawnedObjects.clear();
    m_indicators.clear();
    for (bool &manual : m_manualSpawnTags) { manual = false; }
    m_pathLayer = -1;
    m_triggerLayer = -1;
    m_triggerCount = 0;
    m_dialogResource = -1;
    m_dialogCloseRequested = false;
    m_nextLevel = {};
    ++m_dialogSerial;
    for (bool &enabled : m_triggerEnabled) { enabled = true; }
    for (int &pause : m_triggerPauseMs) { pause = 0; }
    m_cleared = false;
    m_stopwatchMs = 0;
    m_stopwatchRunning = false;
    m_brotherLabelVisible = false;
    m_brotherLabelAlpha = 0;
    // CLevelObjectPool::Clear :145780 preserves the configured capacity.
    m_xplodiumMultiplierPercent = 100;
    m_playerCanMove = true;
    m_playerCanShoot = true;
    m_brotherCanShoot = true;
    m_tutorialStep = -1;
    if (m_tutorialEnabled) { m_tutorialStep = 0; }
    m_bossIntroSerial = 0;
    m_respawnPathLayer = -1;
    m_objectTimeScale = 1;
    m_worldTimeScale = 1;
    m_statisticsGroup = 0;
    m_kills = 0;
    for (unsigned &kills : m_statisticsKills) { kills = 0; }
    m_stat42Bits = 0;
    m_unimplementedCalls = 0;
    for (std::int16_t &variable : m_variables) {
        variable = 0;
    }
    // CLevel::Init :121799: waves per revolution. Variable 4 is offset 311032,
    // not the separate final-wave field at 311028 (VariableResolver :114371).
    m_variables[2] = static_cast<std::int16_t>(levelTemplate.wavesPerRevolution);
    SetWave(startWave);
    for (float &multiplier : m_globalEnemyMultipliers) {
        multiplier = 1;
    }
    for (auto &enemy : m_enemyMultipliers) {
        for (float &multiplier : enemy) {
            multiplier = 1;
        }
    }

    if (!levelTemplate.script.IsPresent()) {
        return;
    }

    m_interpreter.SetScript(levelTemplate.script, *this);
    m_paused = false;
    m_interpreter.CallExportFunction(kLevelExportOnLevelStart);
}

std::int16_t CLevel::FunctionResolver(std::uint8_t function, const std::int16_t *arguments,
                                      std::uint8_t argumentCount) {
    int first = 0;
    int second = 0;
    if (argumentCount > 0) { first = arguments[0]; }
    if (argumentCount > 1) { second = arguments[1]; }
    switch (function) {
    case 0: m_timerMs = first * 1000; m_timerFunction = second; return 0;
    case 3:
        if (m_objectLayer != first) { m_spawnedObjects.clear(); }
        m_objectLayer = first;
        // SetObjectLayer :91935 calls CLayerObject::OnStart :126249 immediately.
        if (m_world != nullptr) { m_world->StartObjectLayer(first); }
        return 0;
    case 4: m_pathLayer = first; return 0;
    case 7: m_triggerLayer = first; return 0;
    case 8:
    case 9:
        if (first >= 0 && first < 32) { m_triggerEnabled[first] = function == 9; }
        return 0;
    case 10:
        if (first == 1 && m_world != nullptr) {
            int objectId = -1;
            if (argumentCount > 1) { objectId = second; }
            return static_cast<std::int16_t>(m_world->CountEnemies(nullptr, objectId));
        }
        return 0;
    case 12: m_cleared = true; return 0;
    case 14:
        if (m_map != nullptr) { m_map->GetCamera().SetScale(first / 256.0f); }
        return 0;
    case 15:
        if (second >= 0 && second < 256) { m_manualSpawnTags[second] = first == 1; }
        return 0;
    case 16: SpawnMapObjects(first); return 0;
    case 20: {
        // Original :117599 resolves SOUNDEFFECT, whose asset points to WAV.
        GameObjectRef sound;
        if (GetResource(first, sound) && m_world != nullptr) { m_world->PlayLevelSound(sound); }
        return 0;
    }
    case 22: GetResource(first, m_nextLevel); return 0;
    case 23: SpawnMapObjects(-1, first); return 0;
    case 26:
        if (first == 1 && argumentCount >= 3 && m_world != nullptr) {
            m_world->SendEnemyMessage(second, arguments[2]);
        }
        if (first == 2 && argumentCount >= 3 && m_world != nullptr) {
            m_world->SendPropMessage(second, arguments[2]);
        }
        return 0;
    case 17:
    case 18:
        if (m_map != nullptr) {
            ILayerPath *path = m_map->GetPathLayer(first);
            if (path != nullptr) { path->SetNodeLocked(second, function == 17); }
        }
        return 0;
    case 5: m_worldTimeScale = first / 256.0f; return 0; // FunctionResolver :117510.
    case 27: m_eventTimerMs = first * 1000 / 256; return 0;
    case 28:
    case 29:
    case 30:
    case 31:
    case 32:
        if (m_map != nullptr) {
            ILayerPath *path = m_map->GetPathLayer(m_pathLayer);
            if (path == nullptr) { return 0; }
            if (function == 28 || function == 29) { path->PropogateNodeLock(first, second, function == 28); }
            if (function == 30 && argumentCount >= 3) { path->UnlockNodesBetween(first, second, arguments[2]); }
            if (function == 31 || function == 32) { path->SetAllNodesLocked(function == 31); }
        }
        return 0;
    case 33: return 0; // Desktop path searches read current locks without a distance-map cache.
    case 34:
        if (first >= 0 && first < 32) { m_triggerPauseMs[first] = second * 1000 / 256; }
        return 0;
    case 35: m_timerMs = first; m_timerFunction = second; return 0;
    case 40:
        m_dialogResource = first;
        m_dialogArrow = static_cast<unsigned>(second);
        m_dialogCloseRequested = false;
        m_dialogAutoClose = argumentCount >= 3 && arguments[2] != 0;
        ++m_dialogSerial;
        return 0;
    case 47:
        if (m_cleared) { return 0; }
        if (m_world != nullptr) { m_world->OnWaveCleared(m_template->perfectWaveRewardPercent); }
        ++m_variables[0];
        std::printf("[level] wave cleared; next=%d\n", m_variables[0] + 1);
        std::fflush(stdout); // Keep long-running wave checks observable under redirected output.
        if (m_template != nullptr && m_template->waveLimit > 0 && m_variables[0] >= m_template->waveLimit) {
            m_cleared = true;
            m_spawner.Reset();
            std::printf("[level] survival complete: %d waves\n", m_variables[0]);
        }
        return 0;
    case 52: m_timerMs = 0; m_timerFunction = -1; return 0;
    case 48:
        // CLevel::FunctionResolver :117883 resolves enemy and prop object IDs.
        if (m_world != nullptr) { m_world->SetEnemyPortal(first, second); }
        return 0;
    case 49: SetIndicator(first, static_cast<unsigned>(second)); return 0;
    case 50:
        // Shipped scripts pass one object ID. The decompiler's a3[1] in this
        // arm cannot describe those calls; use the single supplied argument.
        RemoveIndicator(first);
        return 0;
    case 39: m_map->GetCamera().Shake(first * 1000 / 256); return 0;
    case 74: m_map->GetCamera().SetCameraMode(first); return 0;
    case 75: m_map->GetCamera().SetTarget(static_cast<float>(first), static_cast<float>(second)); return 0;
    case 76: m_map->GetCamera().SetTargetToPlayer(); return 0;
    // Original CBrother words 1006/1004 gate movement and firing respectively.
    case 77: m_playerCanMove = first != 0; return 0;
    case 78: m_playerCanShoot = first != 0; return 0;
    case 79: ++m_bossIntroSerial; return 0;
    // CLevel::RespawnPlayerForDeathMatch :115007 uses this path's farthest node.
    // Retail scripts also set it; single-player never invokes that respawn path.
    case 80: m_respawnPathLayer = first; return 0;
    // CPlayerStatistics::SetStatBit :220857 accepts bit positions 0..31.
    case 82:
        if (first >= 0 && first < 32) { m_stat42Bits |= 1u << first; }
        return 0;
    case 53:
        if (m_variables[2] > 0) { return m_variables[0] % m_variables[2]; }
        return 0;
    case 54:
        if (argumentCount >= 3 && first >= 0 && first < 32 && second >= 0 && second < 5) {
            m_enemyMultipliers[first][second] = arguments[2] / 256.0f;
        }
        return 0;
    case 55:
        if (first >= 0 && first < 5) { m_globalEnemyMultipliers[first] = second / 256.0f; }
        return 0;
    // CPlayer::SetXplodiumMultiplier :100302 stores a percentage, not 8.8.
    case 56: m_xplodiumMultiplierPercent = first; return 0;
    case 57: m_xplodiumMultiplierPercent += first; return 0;
    // CLevel::FunctionResolver :118039-118089. These are host clocks and
    // presentation switches, not network gates or arbitrary gameplay bonuses.
    case 58: m_objectTimeScale = first / 256.0f; return 0;
    // CEnemySpawner::GetNumFreeEnemies :147230 consumes the same pool limit.
    case 59: m_enemyLimit = std::min(50u, static_cast<unsigned>(first)); return 0;
    case 60: m_stopwatchMs = 0; m_stopwatchRunning = false; return 0;
    case 61: m_stopwatchRunning = true; return 0;
    case 62: m_stopwatchRunning = false; return 0;
    case 63: m_stopwatchRunning = first != 0; return 0;
    // CLevel::FunctionResolver :118073 writes pause byte +275620.
    case 64: m_paused = true; return 0;
    case 65: m_paused = false; return 0;
    case 66: m_brotherLabelVisible = true; return 0;
    case 67: m_brotherLabelVisible = false; return 0;
    // :119883 uses this byte in the enemy-statistic key, independently of XP.
    case 68: m_statisticsGroup = static_cast<std::uint8_t>(first); return 0;
    // CBrotherAI::Update :139437 gates targeting with byte 3144.
    case 69: m_brotherCanShoot = first != 0; return 0;
    case 71: TutorialAdvance(); return 0;
    case 72: m_tutorialStep = -1; return 0;
    case 73:
        if (m_world != nullptr) { return static_cast<std::int16_t>(m_world->GetPowerupCount(first)); }
        return 0;
    case 70:
        // ClearChapterPlayback requests an exit; CGame delivers event 4 later
        // when the popup finishes. Never reenter the script resolver here.
        if (m_dialogResource >= 0) { m_dialogCloseRequested = true; }
        return 0;
    default: break;
    }
    if (function == kLevelFunctionSetCameraLayer) {
        SetCameraLayer(arguments, argumentCount);
        return 0;
    }

    if (function == kLevelFunctionSetCollisionLayer) {
        SetCollisionLayer(arguments, argumentCount);
        return 0;
    }
    if (function == 6 && m_map != nullptr && argumentCount > 0) {
        if (!m_map->SetBulletCollisionLayer(arguments[0])) {
            std::printf("[level] invalid bullet collision layer %d\n", arguments[0]);
        } else {
            std::printf("[level] setBulletCollisionLayer( %d )\n", arguments[0]);
        }
        return 0;
    }

    if (function == kLevelFunctionSetTileLayerSpeed) {
        SetTileLayerSpeed(arguments, argumentCount);
        return 0;
    }

    m_unimplementedCalls += 1;
    std::printf("[level] function %u, %u args:", function, argumentCount);
    for (std::uint8_t i = 0; i < argumentCount; ++i) {
        std::printf(" %d", arguments[i]);
    }
    std::printf(" -- not implemented\n");
    return 0;
}

int CLevel::TransformWorldElapseMS(int deltaMs) const {
    if (deltaMs <= 0) { return 0; }
    return std::max(1, static_cast<int>(deltaMs * m_worldTimeScale));
}

void CLevel::UpdateScript(int deltaMs) {
    if (deltaMs <= 0 || m_cleared) {
        return;
    }
    if (m_stopwatchRunning) { m_stopwatchMs += deltaMs; }
    // CLevel::Update :121477 fades the brother's name at 0.002 per ms.
    if (m_brotherLabelVisible) { m_brotherLabelAlpha = std::min(1.0f, m_brotherLabelAlpha + deltaMs * 0.002f); }
    else { m_brotherLabelAlpha = std::max(0.0f, m_brotherLabelAlpha - deltaMs * 0.002f); }
    for (int &pause : m_triggerPauseMs) {
        pause -= deltaMs;
        if (pause < 0) { pause = 0; }
    }
    if (m_timerMs > 0) {
        m_timerMs -= deltaMs;
        if (m_timerMs <= 0) {
            m_timerMs = 0;
            if (m_timerFunction >= 0) {
                m_interpreter.CallFunctionDirect(static_cast<std::uint8_t>(m_timerFunction));
            }
        }
    }
    if (m_eventTimerMs > 0) {
        m_eventTimerMs -= deltaMs;
        if (m_eventTimerMs <= 0) {
            m_eventTimerMs = 0;
            m_interpreter.HandleEvent(4, 0);
        }
    }
    m_spawner.Update(deltaMs);
}

void CLevel::Update(int deltaMs) {
    if (m_playerModel == nullptr) {
        UpdateScript(deltaMs);
        return;
    }
    Update(deltaMs, 0, 0, false, true);
}

void CLevel::Update(int deltaMs, float moveX, float moveY, bool fire, bool advanceScript) {
    if (deltaMs <= 0 || m_playerModel == nullptr || m_map == nullptr) { return; }
    // CLevel::Update :121255 advances the active powerup before its pause gate.
    if (m_peerPowerups != nullptr && m_peerPowerups->IsPresentationActive()) {
        UpdatePowerup(*m_peerPowerups, deltaMs);
        return;
    }
    if (m_powerups != nullptr && m_powerups->IsPresentationActive()) {
        UpdatePowerup(*m_powerups, deltaMs);
        return;
    }
    if (IsDeathComplete()) {
        UpdateAfterDeath(deltaMs);
        return;
    }
    if (m_paused) { return; }

    const int worldDeltaMs = TransformWorldElapseMS(deltaMs);
    m_map->GetCamera().Update(worldDeltaMs);
    if (m_cleared) {
        Update(worldDeltaMs, 0, 0, false);
        UpdateCamera(worldDeltaMs);
        return;
    }

    CheckForCameraChange(GetPlayer().x, GetPlayer().y);
    if (advanceScript) { UpdateScript(deltaMs); }
    const float previousX = GetPlayer().x;
    const float previousY = GetPlayer().y;
    if (!m_playerCanMove) { moveX = 0; moveY = 0; }
    if (!m_playerCanShoot) { fire = false; }
    Update(worldDeltaMs, moveX, moveY, fire);
    UpdateMapInteractions(previousX, previousY);

    if (m_powerups != nullptr) { UpdatePowerup(*m_powerups, deltaMs); }
    if (m_peerPowerups != nullptr) { UpdatePowerup(*m_peerPowerups, deltaMs); }
    if (m_props != nullptr) { m_props->Update(worldDeltaMs); }
    if (m_pickups != nullptr && m_effectSprites != nullptr) {
        for (const PendingPickup &spawn : m_pendingPickups) {
            SpawnPickupAt(spawn.resource, spawn.x, spawn.y, 0);
        }
        m_pickups->Update(worldDeltaMs, *this);
        for (const ZPickupCollection &pickup : m_pickups->collections) {
            if (m_match != nullptr && pickup.objectId >= CMPMatch::PickupIdBase &&
                !CollectMatchWeapon(pickup.peer, pickup.objectId - CMPMatch::PickupIdBase)) {
                RecordInvalidSpawn();
            }
            OnPickupCollected(pickup.objectId, pickup.resource);
        }
    }
    for (std::uint8_t event : m_pendingLevelEvents) { HandleEvent(event); }
    if (m_match != nullptr) { UpdateDeathmatch(deltaMs); }
    for (const PendingTeleport &event : m_pendingTeleports) { OnEnemyTeleport(event.objectId, event.enemy); }
    UpdateCamera(worldDeltaMs);
}

void CLevel::UpdateAfterDeath(int deltaMs) {
    if (deltaMs <= 0 || m_playerModel == nullptr || m_map == nullptr) { return; }
    if (!IsDeathComplete()) {
        Update(deltaMs, 0, 0, false, true);
        return;
    }
    if (m_powerups != nullptr && m_powerups->IsPresentationActive()) {
        UpdatePowerup(*m_powerups, deltaMs);
        return;
    }
    m_map->GetCamera().Update(deltaMs);
    Update(deltaMs, 0, 0, false);
    if (m_powerups != nullptr) { UpdatePowerup(*m_powerups, deltaMs); }
    UpdateCamera(deltaMs);
}

bool CLevel::IsDeathComplete() const {
    if (IsPowerupMovieActive()) { return false; }
    return m_playerModel != nullptr && IsTeamDeathComplete();
}

bool CLevel::IsPowerupMovieActive() const {
    if (m_powerups != nullptr && m_powerups->IsPresentationActive()) { return true; }
    return m_peerPowerups != nullptr && m_peerPowerups->IsPresentationActive();
}

std::vector<std::string> CLevel::TakePowerupUseMessages() {
    if (m_powerups == nullptr) { return {}; }
    if (m_powerups->m_selector == nullptr) { return {}; }
    std::vector<std::string> messages;
    messages.swap(m_powerups->m_selector->m_useMessages);
    return messages;
}

bool CLevel::OnTrigger(int group) {
    if (m_cleared || group < 0 || group >= 32 || !m_triggerEnabled[group] || m_triggerPauseMs[group] > 0) { return false; }
    // OnTrigger :116252; Init :120973 enables exactly 32 groups.
    ++m_triggerCount;
    m_interpreter.CallExportFunction(6, static_cast<std::int16_t>(group));
    std::printf("[level] trigger group=%d\n", group);
    return true;
}

void CLevel::UpdateProximitySpawns(float left, float top, float width, float height) {
    if (m_map == nullptr || m_world == nullptr || m_cleared) { return; }
    // CLayerObject::Update :126177 expands the current camera by 200 per side.
    for (unsigned layerIndex = 0; layerIndex < m_map->GetObjectLayerCount(); ++layerIndex) {
        const CLayerObject &layer = m_map->GetObjectLayer(layerIndex);
        if (static_cast<int>(layer.GetLayerIndex()) != m_objectLayer) { continue; }
        const auto &objects = layer.GetObjects();
        m_spawnedObjects.resize(objects.size(), false);
        for (unsigned index = 0; index < objects.size(); ++index) {
            const ZPlacedObject &object = objects[index];
            if (m_spawnedObjects[index] || m_manualSpawnTags[object.spawnTag]) { continue; }
            if (object.objectType != static_cast<unsigned>(ZPlacedObjectType::Prop) &&
                (object.x < left - 200 || object.x > left + width + 200 ||
                 object.y < top - 200 || object.y > top + height + 200)) { continue; }
            if (m_world->SpawnMapObject(object, index)) { m_spawnedObjects[index] = true; }
        }
    }
}

void CLevel::SpawnMapObjects(int tag, int objectId) {
    if (m_map == nullptr || m_world == nullptr) { return; }
    for (unsigned layerIndex = 0; layerIndex < m_map->GetObjectLayerCount(); ++layerIndex) {
        const CLayerObject &layer = m_map->GetObjectLayer(layerIndex);
        if (static_cast<int>(layer.GetLayerIndex()) != m_objectLayer) { continue; }
        const auto &objects = layer.GetObjects();
        m_spawnedObjects.resize(objects.size(), false);
        for (unsigned index = 0; index < objects.size(); ++index) {
            if (m_spawnedObjects[index]) { continue; }
            if (objectId >= 0 && static_cast<int>(index) != objectId) { continue; }
            if (tag >= 0 && objects[index].spawnTag != tag) { continue; }
            // Original IDs are the flattened map-object indices (:126401).
            if (m_world->SpawnMapObject(objects[index], index)) { m_spawnedObjects[index] = true; }
        }
    }
}

bool CLevel::GetStringResource(int index, CGameAssetRef &out) const {
    if (m_template == nullptr || index < 0 || index >= static_cast<int>(m_template->script.GetResources().size())) { return false; }
    const ZScriptResourceRef &ref = m_template->script.GetResources()[index];
    if (ref.sectionOrType != 254) { return false; }
    out.packHash = ref.packHash;
    out.assetId = ref.resourceId;
    return true;
}

void CLevel::CompleteDialog() {
    if (m_dialogResource < 0) { return; }
    m_dialogResource = -1;
    m_dialogCloseRequested = false;
    // CGame::Update :76636 forwards popup completion through class 4 event 4.
    HandleEvent(4);
}

void CLevel::TutorialAdvance() {
    if (m_tutorialStep < 0) { return; }
    ++m_tutorialStep;
    std::printf("[tutorial] step=%d\n", m_tutorialStep);
    // Native 71 :118108 increments the shared variable before export ten.
    m_interpreter.CallExportFunction(10);
}

bool CLevel::GetResource(int index, GameObjectRef &out) const {
    if (m_template == nullptr || index < 0 || index >= static_cast<int>(m_template->script.GetResources().size())) {
        return false;
    }
    const ZScriptResourceRef &ref = m_template->script.GetResources()[index];
    out.packHash = ref.packHash;
    out.localIndex = static_cast<std::uint8_t>(ref.resourceId);
    return !out.IsNull() && out.localIndex != kNoLocalIndex;
}

void CLevel::OnEnemyKilled(int objectId, const GameObjectRef &enemy) {
    // Count delivered deaths once, including the event that clears the level.
    ++m_kills;
    if (m_template == nullptr || m_cleared) {
        return;
    }
    ++m_statisticsKills[m_statisticsGroup];
    int resourceIndex = -1;
    const auto &resources = m_template->script.GetResources();
    for (std::size_t index = 0; index < resources.size(); ++index) {
        const ZScriptResourceRef &ref = resources[index];
        if (ref.packHash == enemy.packHash && ref.resourceId == enemy.localIndex && ref.sectionOrType == 5) {
            resourceIndex = static_cast<int>(index);
            break;
        }
    }
    m_interpreter.CallExportFunction(5, static_cast<std::int16_t>(objectId),
        static_cast<std::int16_t>(resourceIndex));
}

void CLevel::BeginCombatFrame() {
    m_pendingLevelEvents.clear();
    m_pendingTeleports.clear();
    m_pendingPickups.clear();
}

void CLevel::QueueEnemyTeleport(int objectId, const GameObjectRef &enemy) {
    m_pendingTeleports.push_back({objectId, enemy});
}

void CLevel::QueuePickupSpawn(const GameObjectRef &pickup, float x, float y) {
    m_pendingPickups.push_back({pickup, x, y});
}

void CLevel::ResetCombatProgress() {
    m_weaponProgress.clear();
    m_casualties.clear();
    m_challengeKills.clear();
    m_challengePowerups.clear();
    m_score = 0;
    m_killStreak = 0;
    m_bestKillStreak = 0;
    m_waveXplodium = m_actor.GetXplodium();
    m_waveHits = 0;
    m_lastWaveBonus = 0;
    m_perfectWaves = 0;
    m_clearedWaves = 0;
    m_wavePerfectResults.clear();
}

std::vector<CChallengeManager::Kill> CLevel::TakeChallengeKills() {
    std::vector<CChallengeManager::Kill> result = std::move(m_challengeKills);
    m_challengeKills.clear();
    return result;
}

std::vector<GameObjectRef> CLevel::TakeChallengePowerups() {
    std::vector<GameObjectRef> result = std::move(m_challengePowerups);
    m_challengePowerups.clear();
    return result;
}

void CLevel::AddMatchScore(unsigned points, unsigned streak) {
    m_score = static_cast<unsigned>(std::min<std::uint64_t>(3000000000ULL,
        static_cast<std::uint64_t>(m_score) + points));
    m_killStreak = streak;
    m_bestKillStreak = std::max(m_bestKillStreak, m_killStreak);
}

void CLevel::CreditWeaponProgress(const GameObjectRef &weapon,
    unsigned experience, unsigned masteryLimit) {
    for (ZWeaponCombatProgress &entry : m_weaponProgress) {
        if (entry.resource.packHash == weapon.packHash &&
            entry.resource.localIndex == weapon.localIndex) {
            entry.experience += experience;
            return;
        }
    }
    m_weaponProgress.push_back({weapon, experience, masteryLimit});
}

void CLevel::RewardEnemy(const ZCombatEnemy &actor) {
    if (m_actor.GetProgress() == nullptr) { return; }
    // CLevel::OnEnemyKilled :119609: offset 912 is XP, 876 is Xplodium.
    // Multiplier attribute 2 is Xplodium; attribute 3 is XP. Both round up.
    const GameObjectRef &ref = actor.model.enemy.combat.templateRef;
    const unsigned experience = static_cast<unsigned>(std::ceil(actor.data->experienceReward *
        GetEnemyMultiplier(ref, 3) * PlayerArmorMultiplier(*m_playerModel, 3) *
        CFriendPowerManager::Multiplier(m_playerModel->friendCount, 5)));
    const bool playerKill = actor.model.enemy.combat.pendingHit.owner == kPlayerCombatId;
    if (m_localLive) {
        const ZCombatId owner = actor.model.enemy.combat.pendingHit.owner;
        if (owner == kPlayerCombatId || owner == kBrotherCombatId) {
            unsigned killer = 0;
            if (owner == kBrotherCombatId) { killer = 1; }
            ZMultiplayerStatistics &statistics = m_multiplayer[killer];
            ++statistics.wave.kills;
            ++statistics.total.kills;
            ++statistics.streak;
            statistics.total.bestStreak = std::max(statistics.total.bestStreak, statistics.streak);
            const unsigned other = 1 - killer;
            unsigned assistExperience = experience;
            if (other == 1 && m_brotherModel != nullptr) {
                assistExperience = static_cast<unsigned>(std::ceil(actor.data->experienceReward *
                    GetEnemyMultiplier(ref, 3) * PlayerArmorMultiplier(*m_brotherModel, 3) *
                    CFriendPowerManager::Multiplier(m_brotherModel->friendCount, 5)));
            }
            unsigned assisted = 0;
            for (unsigned slot = 0; slot < 2; ++slot) {
                if ((actor.assistMask[other] & (1u << slot)) == 0) { continue; }
                ++assisted;
                ++m_multiplayer[other].wave.assists;
                ++m_multiplayer[other].total.assists;
                CreditAssistMastery(other, slot, assistExperience);
            }
            if (assisted == 0) {
                unsigned slot = m_playerModel->gunSlot;
                if (other == 1) { slot = m_brotherWeaponSlot; }
                // OnEnemyKilledByBro grants mastery without a numerical assist.
                CreditAssistMastery(other, slot, assistExperience);
            }
            if (killer == 1 && m_brotherModel != nullptr) {
                const unsigned peerExperience = static_cast<unsigned>(std::ceil(actor.data->experienceReward *
                    GetEnemyMultiplier(ref, 3) * PlayerArmorMultiplier(*m_brotherModel, 3) *
                    CFriendPowerManager::Multiplier(m_brotherModel->friendCount, 5)));
                const unsigned peerXplodium = static_cast<unsigned>(std::ceil(actor.data->xplodiumReward *
                    GetEnemyMultiplier(ref, 2) * PlayerArmorMultiplier(*m_brotherModel, 4) *
                    CFriendPowerManager::Multiplier(m_brotherModel->friendCount, 6)));
                AddPeerExperience(peerExperience);
                AddPeerXplodium(peerXplodium);
                if (!actor.model.enemy.combat.pendingHit.weapon.IsNull()) {
                    CreditAssistMastery(1,
                        actor.model.enemy.combat.pendingHit.weaponSlot, peerExperience);
                }
            }
        }
    }

    bool counted = false;
    for (ZEnemyCasualty &entry : m_casualties) {
        if (entry.resource.packHash == ref.packHash && entry.resource.localIndex == ref.localIndex) {
            ++entry.count;
            counted = true;
            break;
        }
    }
    if (!counted) { m_casualties.push_back({ref, 1, actor.data->owner}); }

    const ZCombatHit &hit = actor.model.enemy.combat.pendingHit;
    // Original CLevel::OnEnemyKilled :119912, six-byte statistic key.
    if (playerKill || hit.owner == kBrotherCombatId) {
        bool recorded = false;
        for (CChallengeManager::Kill &kill : m_challengeKills) {
            if (kill.enemy.packHash == ref.packHash && kill.enemy.localIndex == ref.localIndex &&
                kill.bullet.packHash == hit.bullet.packHash && kill.bullet.localIndex == hit.bullet.localIndex &&
                kill.group == m_statisticsGroup && kill.critical == hit.critical && kill.player == playerKill) {
                ++kill.count;
                recorded = true;
                break;
            }
        }
        if (!recorded) {
            m_challengeKills.push_back({ref, hit.bullet, m_statisticsGroup, 1, hit.critical, playerKill});
        }
    }
    if (playerKill && !hit.weapon.IsNull()) {
        bool credited = false;
        for (ZWeaponCombatProgress &entry : m_weaponProgress) {
            if (entry.resource.packHash == hit.weapon.packHash &&
                entry.resource.localIndex == hit.weapon.localIndex) {
                entry.experience += experience;
                credited = true;
                break;
            }
        }
        if (!credited) {
            m_weaponProgress.push_back({hit.weapon, experience, hit.weaponMasteryLimit});
        }
    }

    if (m_horde) {
        // CLevel::OnEnemyKilled :119655-119802: bro kills score once, player
        // kills twice at the current streak multiplier and advance that streak.
        std::uint64_t points = static_cast<std::uint64_t>(experience) * (m_killStreak + 1);
        if (playerKill) {
            points *= 2;
            ++m_killStreak;
        }
        m_bestKillStreak = std::max(m_bestKillStreak, m_killStreak);
        m_score = static_cast<unsigned>(std::min<std::uint64_t>(3000000000ULL, m_score + points));
        if (playerKill) { AddExperience(experience); }
    } else if (!m_localLive || playerKill) {
        AddExperience(experience);
    }

    // CLevel::OnEnemyKilled VA0x950AC captures the projected position once.
    if ((!m_localLive || playerKill) && m_experienceTexts.size() < kTextEffectCapacity) {
        const CEnemy::CombatState &enemy = actor.model.enemy.combat;
        ExperienceText text;
        text.amount = experience;
        text.x = static_cast<float>(static_cast<int>((enemy.x - m_textViewX) * m_textScaleX));
        text.y = static_cast<float>(static_cast<int>((enemy.y - m_textViewY) * m_textScaleY));
        m_experienceTexts.push_back(text);
    }
    if (hit.owner == kPlayerCombatId) {
        const unsigned xplodium = static_cast<unsigned>(std::ceil(actor.data->xplodiumReward *
            GetEnemyMultiplier(ref, 2) * PlayerArmorMultiplier(*m_playerModel, 4) *
            CFriendPowerManager::Multiplier(m_playerModel->friendCount, 6)));
        AddXplodium(xplodium);
    }
}

void CLevel::ResolveWaveReward(unsigned perfectRewardPercent) {
    if (m_playerModel != nullptr && m_playerModel->weapon != nullptr) {
        m_playerModel->weapon->brother.OnWaveCleared();
    }
    if (m_brotherModel != nullptr) { m_brotherModel->weapon->brother.OnWaveCleared(); }
    m_lastWaveBonus = 0;
    m_wavePerfectResults.push_back(m_vitals->hits == m_waveHits);
    // CLevel::OnWaveCleared :116980 uses integer percentage and grants at least one.
    if (m_vitals->hits == m_waveHits) {
        m_lastWaveBonus = std::max<std::uint64_t>(1,
            (m_actor.GetXplodium() - m_waveXplodium) * perfectRewardPercent / 100);
        const std::uint64_t before = m_actor.GetXplodium();
        AddXplodium(static_cast<unsigned>(m_lastWaveBonus));
        m_lastWaveBonus = m_actor.GetXplodium() - before;
        ++m_perfectWaves;
    }
    ++m_clearedWaves;
    std::printf("[progress] wave reward=%llu percent=%u hits=%u perfect=%u/%u\n",
        m_lastWaveBonus, perfectRewardPercent, m_vitals->hits - m_waveHits,
        m_perfectWaves, m_clearedWaves);
    m_waveXplodium = m_actor.GetXplodium();
    m_waveHits = m_vitals->hits;
    if (m_localLive && m_brother != nullptr) {
        ZMultiplayerStatistics &peer = m_multiplayer[1];
        if (m_brother->vitals.hits == peer.previousHits) {
            const std::uint64_t bonus = std::max<std::uint64_t>(1,
                peer.wave.xplodium * perfectRewardPercent / 100);
            AddPeerXplodium(static_cast<unsigned>(bonus));
            peer.wave.perfectWaves = 1;
            ++peer.total.perfectWaves;
        }
        if (m_wavePerfectResults.back()) {
            m_multiplayer[0].wave.perfectWaves = 1;
            ++m_multiplayer[0].total.perfectWaves;
        }
        peer.previousHits = m_brother->vitals.hits;
    }
}

void CLevel::SetWave(int wave) {
    // CGame initializes saved progress through CLevel::SetWave (:76837).
    if (wave < 0) { wave = 0; }
    if (m_template != nullptr && m_template->waveLimit > 0 && wave >= m_template->waveLimit) {
        wave = m_template->waveLimit - 1;
    }
    m_variables[0] = static_cast<std::int16_t>(wave);
}

static bool ContainsCameraPoint(const ZMapRectangle &bounds, int x, int y) {
    return bounds.width != 0 && bounds.height != 0 && x >= bounds.x && y >= bounds.y &&
        x <= bounds.x + bounds.width && y <= bounds.y + bounds.height;
}

void CLevel::CheckForCameraChange(float playerX, float playerY) {
    if (m_map == nullptr || m_cleared) { return; }
    const auto *current = m_map->GetCurrentCameraLayer();
    if (current == nullptr) { return; }
    // CLevel::CheckForCameraChange :116091 consumes the second rectangle,
    // emits export 7 with the whole-map layer index, and lets Flow select the
    // camera. Within the current region, other regions fire only on entry.
    const int x = static_cast<int>(playerX);
    const int y = static_cast<int>(playerY);
    const bool inCurrent = ContainsCameraPoint(current->GetSecondaryBounds(), x, y);
    for (unsigned index = 0; index < m_map->GetCameraLayerCount(); ++index) {
        const auto &camera = m_map->GetCameraLayer(index);
        const bool inside = ContainsCameraPoint(camera.GetSecondaryBounds(), x, y);
        if (inside && &camera != current && (!inCurrent || !m_cameraEntered[index])) {
            m_interpreter.CallExportFunction(7, static_cast<std::int16_t>(camera.GetLayerIndex()));
        }
        m_cameraEntered[index] = inside;
    }
}

void CLevel::OnEnemyTeleport(int objectId, const GameObjectRef &enemy) {
    if (m_template == nullptr || m_cleared) { return; }
    int resourceIndex = -1;
    const auto &resources = m_template->script.GetResources();
    for (std::size_t index = 0; index < resources.size(); ++index) {
        const auto &ref = resources[index];
        if (ref.packHash == enemy.packHash && ref.resourceId == enemy.localIndex && ref.sectionOrType == 5) {
            resourceIndex = static_cast<int>(index);
            break;
        }
    }
    // CLevel::OnEnemyTeleport :118252 calls export 9, independently of kills.
    RemoveIndicator(objectId);
    m_interpreter.CallExportFunction(9, static_cast<std::int16_t>(objectId), static_cast<std::int16_t>(resourceIndex));
    std::printf("[level] enemy teleported id=%d resource=%d\n", objectId, resourceIndex);
}

void CLevel::OnPickupCollected(int objectId, const GameObjectRef &pickup) {
    if (m_template == nullptr || m_cleared) { return; }
    int resourceIndex = -1;
    const auto &resources = m_template->script.GetResources();
    for (std::size_t index = 0; index < resources.size(); ++index) {
        const auto &ref = resources[index];
        if (ref.packHash == pickup.packHash && ref.resourceId == pickup.localIndex && ref.sectionOrType == 12) {
            resourceIndex = static_cast<int>(index);
            break;
        }
    }
    // OnPickupCollected :118390 forwards placed ID and LEVEL resource index.
    m_interpreter.CallExportFunction(4, static_cast<std::int16_t>(objectId), static_cast<std::int16_t>(resourceIndex));
}

void CLevel::OnDeathmatchKill(float x, float y) {
    // OnPlayerKilled :118700 invokes export 11 with the victim's position.
    if (m_template != nullptr && IsDeathmatch()) {
        m_interpreter.CallExportFunction(11, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), 0);
    }
}

void CLevel::OnPropEvent(int objectId, const GameObjectRef &prop, bool entered) {
    if (m_template == nullptr) { return; }
    int resourceIndex = -1;
    const auto &resources = m_template->script.GetResources();
    for (unsigned index = 0; index < resources.size(); ++index) {
        const auto &resource = resources[index];
        if (resource.sectionOrType == 19 && resource.packHash == prop.packHash && resource.resourceId == prop.localIndex) {
            resourceIndex = index;
            break;
        }
    }
    // Original CLevel OnPropEntered/Destroyed :118202/:118216.
    std::uint8_t function = 3;
    if (entered) { function = 8; }
    m_interpreter.CallExportFunction(function, static_cast<std::int16_t>(objectId), static_cast<std::int16_t>(resourceIndex));
}

float CLevel::GetEnemyMultiplier(int enemy, int attribute) const {
    if (attribute < 0 || attribute >= 5) {
        return 1;
    }
    float multiplier = m_globalEnemyMultipliers[attribute];
    if (enemy >= 0 && enemy < 32) {
        multiplier *= m_enemyMultipliers[enemy][attribute];
    }
    return multiplier;
}

float CLevel::GetEnemyMultiplier(const GameObjectRef &enemy, int attribute) const {
    // Original lookup defaults to resource zero when the type is not listed.
    int resourceIndex = 0;
    if (m_template != nullptr) {
        const auto &resources = m_template->script.GetResources();
        for (unsigned index = 0; index < resources.size(); ++index) {
            const auto &resource = resources[index];
            if (resource.sectionOrType == 5 && resource.packHash == enemy.packHash && resource.resourceId == enemy.localIndex) {
                resourceIndex = static_cast<int>(index);
                break;
            }
        }
    }
    return GetEnemyMultiplier(resourceIndex, attribute);
}

std::int16_t *CLevel::VariableResolver(std::uint8_t variable) {
    if (variable >= kLevelVariableCount) {
        return nullptr;
    }

    return &m_variables[variable];
}

void CLevel::SetCameraLayer(const std::int16_t *arguments, std::uint8_t argumentCount) {
    if (m_map == nullptr || argumentCount < 1) {
        return;
    }

    // The argument indexes the whole layer stack, not the camera layers alone.
    const std::int16_t layerIndex = arguments[0];
    if (layerIndex < 0 ||
        !m_map->SetCameraLayer(static_cast<std::uint32_t>(layerIndex))) {
        std::printf("[level] setCameraLayer( %d ) names no camera layer\n", layerIndex);
        return;
    }

    std::printf("[level] setCameraLayer( %d )\n", layerIndex);
}

void CLevel::SetCollisionLayer(const std::int16_t *arguments,
                               std::uint8_t argumentCount) {
    if (m_map == nullptr || argumentCount < 1) {
        return;
    }

    // Like the camera selector, this indexes the complete map layer stack.
    const std::int16_t layerIndex = arguments[0];
    if (layerIndex < 0 ||
        !m_map->SetCollisionLayer(static_cast<std::uint32_t>(layerIndex))) {
        std::printf("[level] setCollisionLayer( %d ) names no collision layer\n",
                    layerIndex);
        return;
    }

    std::printf("[level] setCollisionLayer( %d )\n", layerIndex);
}

void CLevel::SetTileLayerSpeed(const std::int16_t *arguments, std::uint8_t argumentCount) {
    if (m_map == nullptr || argumentCount < 3) {
        return;
    }

    // The layer index counts tile layers only: the original walks the map's
    // whole layer stack and skips everything whose type is not zero, which is
    // the same list CMap keeps.
    const std::int16_t layerOrdinal = arguments[0];
    if (layerOrdinal < 0 ||
        static_cast<std::uint32_t>(layerOrdinal) >= m_map->GetTileLayerCount()) {
        std::printf("[level] setTileLayerSpeed names tile layer %d; the map has %u\n",
                    layerOrdinal, m_map->GetTileLayerCount());
        return;
    }

    const float speedX = static_cast<float>(arguments[1]) * kLevelSpeedArgumentUnit;
    const float speedY = static_cast<float>(arguments[2]) * kLevelSpeedArgumentUnit;

    m_map->GetTileLayer(static_cast<std::uint32_t>(layerOrdinal)).SetSpeed(speedX, speedY);

    std::printf("[level] setTileLayerSpeed( %d, %.4f, %.4f )\n", layerOrdinal, speedX,
                speedY);
}

bool CLevel::SpawnEnemy(const GameObjectRef &enemy, int layerIndex, int nodeIndex, int objectId) {
    if (m_playerModel == nullptr || m_map == nullptr || m_catalog == nullptr) { return false; }
    const bool hasAuthoredRoute = layerIndex >= 0 && nodeIndex >= 0;
    std::size_t entryIndex = 0;
    while (entryIndex < m_catalog->size()) {
        const ZEnemyTemplateData &entry = (*m_catalog)[entryIndex];
        if (entry.packHash == enemy.packHash && entry.ordinal == enemy.localIndex) { break; }
        ++entryIndex;
    }
    if (entryIndex == m_catalog->size()) { return false; }
    if (layerIndex < 0) { layerIndex = m_pathLayer; }
    ILayerPath *path = m_map->GetPathLayer(layerIndex);
    if (path == nullptr || path->GetNodes().empty()) { return false; }
    const auto &nodes = path->GetNodes();
    if (nodeIndex < 0) {
        nodeIndex = m_spawner.GetSpawnPoint(*path, GetPlayer().x, GetPlayer().y,
            m_cameraLeft, m_cameraTop, m_cameraWidth, m_cameraHeight);
    }
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) { return false; }
    ZCombatEnemy *actor = Spawn(entryIndex, nodes[nodeIndex].x, nodes[nodeIndex].y);
    if (actor == nullptr) { return false; }
    if (objectId < 0) {
        const float distance = std::hypot(GetPlayer().x - nodes[nodeIndex].x,
            GetPlayer().y - nodes[nodeIndex].y);
        if (m_closestSpawnDistance < 0 || distance < m_closestSpawnDistance) {
            m_closestSpawnDistance = distance;
        }
        if (m_cameraWidth > 0 && nodes[nodeIndex].x >= m_cameraLeft && nodes[nodeIndex].y >= m_cameraTop &&
            nodes[nodeIndex].x <= m_cameraLeft + m_cameraWidth && nodes[nodeIndex].y <= m_cameraTop + m_cameraHeight) {
            ++m_onScreenSpawns;
        }
    }
    actor->objectId = objectId;
    if (hasAuthoredRoute) { actor->model.enemy.SetPath(path); }
    SetIndicator(objectId, 0, actor->model.enemy.combat.id);
    return true;
}

void CLevel::StartObjectLayer(int layer) {
    if (m_props != nullptr) { m_props->StartLayer(layer); }
}

bool CLevel::SpawnMapObject(const ZPlacedObject &object, int objectId) {
    if (m_playerModel == nullptr || m_map == nullptr || m_catalog == nullptr) { return false; }
    if (object.objectType == static_cast<unsigned>(ZPlacedObjectType::Prop)) {
        if (m_props == nullptr) { return false; }
        return m_props->Spawn(m_objectLayer, objectId);
    }
    if (object.objectType == static_cast<unsigned>(ZPlacedObjectType::Pickup)) {
        GameObjectRef pickup;
        pickup.packHash = object.packHash;
        pickup.localIndex = object.localIndex;
        return SpawnPickupAt(pickup, object.x, object.y, objectId);
    }
    if (object.objectType != static_cast<unsigned>(ZPlacedObjectType::Enemy)) { return true; }
    for (unsigned index = 0; index < m_catalog->size(); ++index) {
        const ZEnemyTemplateData &entry = (*m_catalog)[index];
        if (entry.packHash != object.packHash || entry.ordinal != object.localIndex) { continue; }
        ZCombatEnemy *actor = Spawn(index, object.x, object.y);
        if (actor == nullptr) { return false; }
        actor->objectId = objectId;
        actor->mapPlaced = true;
        if (object.pathLayer != 255) { actor->model.enemy.SetPath(m_map->GetPathLayer(object.pathLayer)); }
        SetIndicator(objectId, 0, actor->model.enemy.combat.id);
        actor->model.enemy.combat.facing = static_cast<float>(object.facing);
        std::printf("[survival] placed enemy id=%d tag=%u item=%u path=%u facing=%d\n",
            objectId, object.spawnTag, object.localIndex, object.pathLayer, object.facing);
        return true;
    }
    return false;
}

void CLevel::SendEnemyMessage(int objectId, int message) {
    if (m_playerModel == nullptr) { return; }
    for (const auto &actor : GetEnemies()) {
        if (actor->objectId != objectId) { continue; }
        const unsigned before = actor->model.enemy.GetStateId();
        actor->model.enemy.HandleMessage(message);
        if (before != actor->model.enemy.GetStateId()) {
            std::printf("[map-enemy] id=%d message=%d state=%u->%u\n",
                objectId, message, before, actor->model.enemy.GetStateId());
        }
        return;
    }
}

void CLevel::SetEnemyPortal(int enemyId, int propId) {
    if (m_playerModel == nullptr) { return; }
    for (const auto &actor : GetEnemies()) {
        if (actor->objectId != enemyId) { continue; }
        actor->model.enemy.combat.portalObjectId = propId;
        actor->model.enemy.combat.portalActive = false;
        return;
    }
}

void CLevel::SendPropMessage(int objectId, int message) {
    if (m_props != nullptr) { m_props->SendMessage(objectId, message); }
}

bool CLevel::IsActivePortal(int propId) const {
    if (m_world != nullptr && m_world != this) { return m_world->IsActivePortal(propId); }
    return m_props != nullptr && m_props->IsActivePortal(propId);
}

void CLevel::PlayLevelSound(const GameObjectRef &sound) {
    if (m_effectSprites == nullptr) { return; }
    ZGunCue cue;
    cue.kind = ZGunCue::Kind::Sound;
    cue.resource = sound;
    Emit(cue, 0, 0, 0, 0);
}

void CLevel::OnWaveCleared(unsigned perfectRewardPercent) {
    if (m_playerModel != nullptr) { ResolveWaveReward(perfectRewardPercent); }
    if (m_game != nullptr) { m_game->OnWaveCleared(perfectRewardPercent); }
}

bool CLevel::SpawnPickup(const GameObjectRef &pickup, int layer, int node, int objectId, bool nearby) {
    if (m_playerModel == nullptr || m_map == nullptr || m_pickups == nullptr) { return false; }
    if (layer < 0) { layer = m_pathLayer; }
    ILayerPath *path = m_map->GetPathLayer(layer);
    if (path == nullptr || path->GetNodes().empty()) { return false; }
    const auto &nodes = path->GetNodes();
    if (nearby) { node = path->FindNearest(GetPlayer().x, GetPlayer().y); }
    if (node < 0) {
        for (unsigned attempt = 0; attempt < nodes.size(); ++attempt) {
            const unsigned candidate = (m_spawnSerial + attempt) % nodes.size();
            if (!nodes[candidate].locked) {
                node = candidate;
                m_spawnSerial = candidate + 1;
                break;
            }
        }
    }
    if (node < 0 || node >= static_cast<int>(nodes.size())) { return false; }
    return SpawnPickupAt(pickup, nodes[node].x, nodes[node].y, objectId);
}

bool CLevel::SpawnPickupAt(const GameObjectRef &pickup, float x, float y, int objectId) {
    if (m_pickups == nullptr || !m_pickups->Spawn(pickup, x, y, objectId)) { return false; }
    SetIndicator(objectId, 1, (1ULL << 32) | m_pickups->spawned);
    return true;
}

bool CLevel::SpawnMPMatchPickup(const GameObjectRef &pickup, int layer) {
    if (m_match == nullptr || m_playerModel == nullptr || m_map == nullptr || m_pickups == nullptr) { return false; }
    ILayerPath *path = m_map->GetPathLayer(layer);
    if (path == nullptr || path->GetNodes().empty()) { return false; }
    const auto &nodes = path->GetNodes();
    const unsigned start = static_cast<unsigned>(RandomInteger(0, static_cast<std::int16_t>(nodes.size() - 1)));
    for (unsigned offset = 0; offset < nodes.size(); ++offset) {
        const auto &node = nodes[(start + offset) % nodes.size()];
        if (node.locked || !CanBrotherWalk(node.x, node.y, node.x, node.y)) { continue; }
        float nearestX = 0;
        float nearestY = 0;
        if (m_pickups->FindNearest(node.x, node.y, nearestX, nearestY) &&
            std::hypot(nearestX - node.x, nearestY - node.y) < GetPlayerRadius() * 2) {
            continue;
        }
        const int candidate = m_match->ChoosePickup();
        if (candidate < 0) { return false; }
        return SpawnPickupAt(pickup, node.x, node.y, CMPMatch::PickupIdBase + candidate);
    }
    return false;
}

std::uint64_t CLevel::ResolveIndicatorTarget(int objectId) const {
    if (m_playerModel == nullptr) { return 0; }
    if (m_props != nullptr) {
        const unsigned key = m_props->ResolveIndicatorTarget(objectId);
        if (key != 0) { return (2ULL << 32) | key; }
    }
    for (const auto &actor : GetEnemies()) {
        if (actor->objectId == objectId && !actor->model.enemy.combat.dead && !actor->model.enemy.combat.removed) {
            return actor->model.enemy.combat.id;
        }
    }
    return 0;
}

bool CLevel::GetIndicatorTarget(std::uint64_t key, float &x, float &y) const {
    if (m_playerModel == nullptr) { return false; }
    if ((key >> 32) == 2) {
        return m_props != nullptr && m_props->GetIndicatorTarget(static_cast<unsigned>(key), x, y);
    }
    if ((key >> 32) == 1) {
        return m_pickups != nullptr && m_pickups->GetIndicatorTarget(static_cast<unsigned>(key), x, y);
    }
    const ZCombatEnemy *actor = Find(static_cast<ZCombatId>(key));
    if (actor == nullptr || actor->model.enemy.combat.dead || actor->model.enemy.combat.removed) { return false; }
    x = actor->model.enemy.combat.x;
    y = actor->model.enemy.combat.y;
    return true;
}

bool CLevel::GetObjectPosition(int objectId, float &x, float &y) const {
    if (m_playerModel == nullptr) { return false; }
    for (const auto &actor : GetEnemies()) {
        if (actor->objectId != objectId || actor->model.enemy.combat.dead || actor->model.enemy.combat.removed) { continue; }
        x = actor->model.enemy.combat.x;
        y = actor->model.enemy.combat.y;
        return true;
    }
    if (m_pickups != nullptr && m_pickups->GetObjectPosition(objectId, x, y)) { return true; }
    return m_props != nullptr && m_props->GetObjectPosition(objectId, x, y);
}

int CLevel::CountEnemySlots(const GameObjectRef *enemy) const {
    if (m_playerModel == nullptr) { return 0; }
    int count = 0;
    for (const auto &actor : GetEnemies()) {
        if (actor->model.enemy.combat.removed) { continue; }
        if (enemy == nullptr || (actor->data->packHash == enemy->packHash && actor->data->ordinal == enemy->localIndex)) {
            ++count;
        }
    }
    return count;
}

int CLevel::CountEnemies(const GameObjectRef *enemy, int objectId) const {
    if (m_playerModel == nullptr) { return 0; }
    int count = 0;
    for (const auto &actor : GetEnemies()) {
        const CEnemy::CombatState &state = actor->model.enemy.combat;
        if (state.dead || state.removed) { continue; }
        if (objectId >= 0 && actor->objectId != objectId) { continue; }
        if (enemy == nullptr || (actor->data->packHash == enemy->packHash && actor->data->ordinal == enemy->localIndex)) {
            ++count;
        }
    }
    return count;
}

void CLevel::UpdateMapInteractions(float previousX, float previousY) {
    if (m_playerModel == nullptr || m_map == nullptr || IsMatchSpawnPending(0)) { return; }
    if (m_archive) {
        const float scaleRatio = 0.8f / m_map->GetCamera().GetScale();
        const float viewWidth = m_viewWidth * scaleRatio;
        const float viewHeight = m_viewHeight * scaleRatio;
        float left = GetPlayer().x - viewWidth * 0.5f;
        float top = GetPlayer().y - viewHeight * 0.5f;
        const ZMapRectangle bounds = m_map->GetVisibleBounds();
        if (!bounds.IsEmpty()) {
            if (bounds.width <= viewWidth) { left = bounds.x + (bounds.width - viewWidth) * 0.5f; }
            else { left = std::clamp(left, static_cast<float>(bounds.x), bounds.x + bounds.width - viewWidth); }
            if (bounds.height <= viewHeight) { top = bounds.y + (bounds.height - viewHeight) * 0.5f; }
            else { top = std::clamp(top, static_cast<float>(bounds.y), bounds.y + bounds.height - viewHeight); }
        }
        UpdateProximitySpawns(left, top, viewWidth, viewHeight);
    }
    for (unsigned index = 0; index < m_map->GetCollisionLayerCount(); ++index) {
        const CLayerCollision &layer = m_map->GetCollisionLayer(index);
        if (static_cast<int>(layer.GetLayerIndex()) != m_triggerLayer) { continue; }
        const auto &geometry = layer.GetCollision();
        float nearest = 2;
        int group = -1;
        for (const ZCollisionEdge &edge : geometry.GetEdges()) {
            if (!edge.enabled) { continue; }
            const float fraction = CombatGeometry::EdgeFraction(previousX, previousY,
                GetPlayer().x - previousX, GetPlayer().y - previousY,
                geometry.GetVertices()[edge.firstVertex], geometry.GetVertices()[edge.secondVertex],
                kBrotherTriggerRadius);
            if (fraction < nearest) {
                nearest = fraction;
                group = edge.group;
            }
        }
        if (group >= 0 && nearest <= 1) { OnTrigger(group); }
        break;
    }
}

unsigned CLevel::GetPowerupCount(unsigned localIndex) const {
    if (m_powerups == nullptr) { return 0; }
    if (m_powerups->m_selector == nullptr) { return 0; }
    return m_powerups->m_selector->GetCount(localIndex);
}

void CLevel::UpdateCamera(int deltaMs) {
    if (m_playerModel == nullptr || m_map == nullptr) { return; }
    const ZMapRectangle bounds = m_map->GetVisibleBounds();
    const float scale = 0.8f / m_map->GetCamera().GetScale();
    if (!IsMatchSpawnPending(0) || !m_map->GetCamera().HasPosition()) {
        m_map->GetCamera().UpdatePosition(GetPlayer().x, GetPlayer().y,
            bounds.x, bounds.y, bounds.width, bounds.height, m_viewWidth * scale, m_viewHeight * scale);
    }
    SetViewCenter(m_map->GetCamera().GetX(), m_map->GetCamera().GetY());
    const float width = m_viewWidth * scale;
    const float height = m_viewHeight * scale;
    if (m_effectSprites != nullptr) {
        SetViewBounds(GetViewCenterX(), GetViewCenterY(), width, height);
    }
    m_cameraLeft = m_map->GetCamera().GetX() - width * 0.5f;
    m_cameraTop = m_map->GetCamera().GetY() - height * 0.5f;
    m_cameraWidth = width;
    m_cameraHeight = height;
    const float viewportFactor = std::min(m_viewWidth / 480.0f, m_viewHeight / 320.0f);
    const float margin = 25 * viewportFactor * scale;
    const float bottomMargin = 100 * viewportFactor * scale;
    UpdateIndicators(deltaMs, m_map->GetCamera().GetX() - width * 0.5f + margin,
        m_map->GetCamera().GetY() - height * 0.5f + margin,
        width - 2 * margin, height - margin - bottomMargin);
}
