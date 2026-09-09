/**
 * @file CLevel.cpp
 * @brief A level: a map plus the script that drives it.
 */

#include "gun_bros/CLevel.h"

#include "gun_bros/CMap.h"

#include <cstdio>
#include <algorithm>

bool CLevel::SetIndicator(int objectId, unsigned type, std::uint64_t targetKey) {
    if (type >= 7 || m_indicators.size() >= 30 || m_world == nullptr) { return false; }
    CLevelIndicator indicator;
    indicator.objectId = objectId;
    indicator.targetKey = targetKey;
    indicator.type = type;
    bool found = false;
    if (targetKey == 0) { found = m_world->GetObjectPosition(objectId, indicator.x, indicator.y); }
    else { found = m_world->GetIndicatorTarget(targetKey, indicator.x, indicator.y); }
    if (!found) { return false; }
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
}

void CLevel::Bind(const Template &levelTemplate, CMap &map, IEnemySpawnWorld *world, int startWave) {
    SetLevelContext(this);
    m_template = &levelTemplate;
    m_map = &map;
    // CLevel snaps to 0.8 before executing OnLevelStart (:121002).
    map.GetCamera().Reset(0.8f);
    map.UnlockAllPathNodes();
    m_world = world;
    m_spawner.Bind(*this, world);
    m_timerMs = 0;
    m_timerFunction = -1;
    m_eventTimerMs = 0;
    m_objectLayer = -1;
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
    m_enemyLimit = 50;
    m_xplodiumMultiplierPercent = 100;
    m_playerCanMove = true;
    m_playerCanShoot = true;
    m_brotherCanShoot = true;
    m_tutorialStep = -1;
    if (m_tutorialEnabled) { m_tutorialStep = 0; }
    m_bossIntroSerial = 0;
    m_respawnPathLayer = -1;
    m_objectTimeScale = 1;
    m_statisticsGroup = 0;
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

void CLevel::Update(int deltaMs) {
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
            const PlacedObject &object = objects[index];
            if (m_spawnedObjects[index] || m_manualSpawnTags[object.spawnTag]) { continue; }
            if (object.objectType != static_cast<unsigned>(PlacedObjectType::Prop) &&
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
    const ScriptResourceRef &ref = m_template->script.GetResources()[index];
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
    const ScriptResourceRef &ref = m_template->script.GetResources()[index];
    out.packHash = ref.packHash;
    out.localIndex = static_cast<std::uint8_t>(ref.resourceId);
    return !out.IsNull() && out.localIndex != kNoLocalIndex;
}

void CLevel::OnEnemyKilled(int objectId, const GameObjectRef &enemy) {
    if (m_template == nullptr || m_cleared) {
        return;
    }
    ++m_statisticsKills[m_statisticsGroup];
    int resourceIndex = -1;
    const auto &resources = m_template->script.GetResources();
    for (std::size_t index = 0; index < resources.size(); ++index) {
        const ScriptResourceRef &ref = resources[index];
        if (ref.packHash == enemy.packHash && ref.resourceId == enemy.localIndex && ref.sectionOrType == 5) {
            resourceIndex = static_cast<int>(index);
            break;
        }
    }
    m_interpreter.CallExportFunction(5, static_cast<std::int16_t>(objectId),
        static_cast<std::int16_t>(resourceIndex));
}

void CLevel::SetWave(int wave) {
    // CGame initializes saved progress through CLevel::SetWave (:76837).
    if (wave < 0) { wave = 0; }
    if (m_template != nullptr && m_template->waveLimit > 0 && wave >= m_template->waveLimit) {
        wave = m_template->waveLimit - 1;
    }
    m_variables[0] = static_cast<std::int16_t>(wave);
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
