/** @file CampaignDoorChecks.cpp
 * @brief Walk through the authored Lava 3 entrance gate using the real session.
 */
#include "gameplay/CampaignDoorChecks.h"
#include "gameplay/SurvivalStudy.h"
#include "gun_bros_re/debug/DebugMaps.h"
#include "gun_bros_re/gameplay/SurvivalRuntime.h"
#include "gun_bros_re/gameplay/MapWorldInternal.h"

enum class CampaignCheck { Doors, Targets, Progression, Rescue, Portal, Cache };

static int RunCampaignCheck(unsigned levelIndex, CampaignCheck check) {
    const std::string big = (Paths::Root() / Paths::BigDirectory).u8string();
    CResTOCManager toc;
    if (!toc.Init(big, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    std::vector<MissionEntry> missions;
    if (!LoadMissionCatalog(toc, tables, missions)) { return 1; }
    for (const auto &mission : missions) {
        // Original pack2 Mission 14 -> LEVEL 3 enables object 5 on level start.
        if (tables.GetPackName(mission.resource.packHash) != "pack2" ||
            mission.data.level.packHash != CStringToKey("pack2") || mission.data.level.localIndex != levelIndex) { continue; }
        std::vector<std::uint8_t> bytes;
        if (!tables.ReadSectionResource(mission.data.level.packHash, GameSection::Level, mission.data.level.localIndex, bytes)) { return 1; }
        CArrayInputStream input(bytes);
        CLevel::Template level;
        if (!level.Init(input) || input.Available() != 0) { return 1; }
        SurvivalLaunch launch;
        launch.bigDirectory = big;
        launch.packShortName = tables.GetPackName(level.mapRef.packHash);
        launch.mapIndex = level.mapRef.localIndex;
        launch.archiveMission = &mission;
        SurvivalDevelopment development;
        development.campaignDoorCheck = check == CampaignCheck::Doors;
        development.campaignTargetCheck = check == CampaignCheck::Targets;
        development.campaignProgressionCheck = check == CampaignCheck::Progression;
        development.campaignRescueCheck = check == CampaignCheck::Rescue;
        development.campaignPortalCheck = check == CampaignCheck::Portal;
        development.campaignCacheCheck = check == CampaignCheck::Cache;
        return RunSurvivalSession(launch, &development);
    }
    return 1;
}

int RunCampaignDoorCheck() { return RunCampaignCheck(3, CampaignCheck::Doors); }
int RunCampaignProgressionCheck() {
    if (RunCampaignCheck(0, CampaignCheck::Progression) != 0) { return 1; }
    return RunCampaignCheck(3, CampaignCheck::Progression);
}
int RunCampaignRescueCheck() { return RunCampaignCheck(4, CampaignCheck::Rescue); }
int RunCampaignPortalCheck() { return RunCampaignCheck(5, CampaignCheck::Portal); }
int RunCampaignCacheCheck() { return RunCampaignCheck(0, CampaignCheck::Cache); }
int RunCampaignLava2Check() {
    // This archive LEVEL has no Mission entry. Use the browser's standalone
    // LEVEL path instead of quietly substituting a different Mission.
    DebugMapSelection selection;
    selection.pack = "pack2";
    selection.map = 2;
    selection.level.packHash = CStringToKey("pack2");
    selection.level.localIndex = 2;
    SurvivalLaunch launch;
    launch.bigDirectory = (Paths::Root() / Paths::BigDirectory).u8string();
    launch.packShortName = selection.pack;
    launch.mapIndex = selection.map;
    launch.debugMap = &selection;
    SurvivalDevelopment development;
    development.campaignProgressionCheck = true;
    return RunSurvivalSession(launch, &development);
}
int RunCampaignTargetCheck() {
    if (RunCampaignCheck(4, CampaignCheck::Targets) != 0) { return 1; }
    return RunCampaignCheck(0, CampaignCheck::Targets);
}

static void RecordCacheCollections(const PickupScene &pickups, bool (&collected)[3]) {
    for (const auto &pickup : pickups.collections) {
        if (pickup.objectId >= 84 && pickup.objectId <= 86) { collected[pickup.objectId - 84] = true; }
    }
}

int CheckCampaignCache(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session, PickupScene &pickups) {
    scene.GetPlayerVitals().invincible = true;
    bool cacheCollected[3]{};
    for (const auto &prop : map.props) {
        if (prop.objectId != 70 || prop.runtime == nullptr) { continue; }
        const auto &vertices = prop.runtime->GetCollision(true).GetVertices();
        if (vertices.empty()) { return 1; }
        float centerX = 0, centerY = 0;
        for (const auto &point : vertices) { centerX += point.x; centerY += point.y; }
        centerX = prop.x + centerX / vertices.size();
        centerY = prop.y + centerY / vertices.size();
        scene.playerX = centerX + 100;
        scene.playerY = centerY;
        CombatHit hit;
        hit.owner = kPlayerCombatId;
        hit.ownerType = 0;
        hit.damage = 100;
        const auto trace = scene.Trace(hit, centerX + 100, centerY, -200, 0, 0, {});
        std::printf("[campaign-cache-check] wall hp=%.0f traced=%llu\n", prop.runtime->GetHealth(),
            static_cast<unsigned long long>(trace.target));
        if (trace.target == 0) { return 1; }
        // Damage amount alone must not open it. DestroyWall is bit 0 in the
        // original collision attributes; the PROP script decides the result.
        scene.ApplyHit(trace.target, hit);
        if (prop.runtime->GetStateId() != 0) { return 1; }
        hit.flags = 1;
        scene.ApplyHit(trace.target, hit);
        if (prop.runtime->GetStateId() != 1) { return 1; }
        float passageY = 0;
        float normalX = 0, normalY = 0;
        unsigned passagePoints = 0;
        for (const auto &edge : prop.runtime->GetCollision(false).GetEdges()) {
            if (edge.group != 1) { continue; }
            const auto &points = prop.runtime->GetCollision(false).GetVertices();
            if (edge.enabled) { return 1; }
            passageY += points[edge.firstVertex].y + points[edge.secondVertex].y;
            normalX += points[edge.secondVertex].y - points[edge.firstVertex].y;
            normalY += points[edge.firstVertex].x - points[edge.secondVertex].x;
            passagePoints += 2;
        }
        if (passagePoints == 0) { return 1; }
        const float normalLength = std::hypot(normalX, normalY);
        if (normalLength == 0) { return 1; }
        normalX /= normalLength;
        normalY /= normalLength;
        if (normalX < 0) { normalX = -normalX; normalY = -normalY; }
        scene.playerX = centerX + normalX * 100;
        scene.playerY = prop.y + passageY / passagePoints + normalY * 100;
        for (int elapsed = 0; elapsed < 12000 && scene.playerX >= centerX - 80; elapsed += 16) {
            session.Update(16, -normalX, -normalY, false);
            RecordCacheCollections(pickups, cacheCollected);
        }
        std::printf("[campaign-cache-check] passage xy=%.0f,%.0f wall-x=%.0f target-y=%.0f state=%u\n",
            scene.playerX, scene.playerY, centerX, prop.y + passageY / passagePoints, prop.runtime->GetStateId());
        if (scene.playerX >= centerX - 30) {
            for (const auto &edge : map.collisionScene.GetEdges()) {
                if (!edge.enabled) { continue; }
                const auto &a = map.collisionScene.GetVertices()[edge.firstVertex];
                const auto &b = map.collisionScene.GetVertices()[edge.secondVertex];
                if (scene.playerX < std::min(a.x, b.x) - 30 || scene.playerX > std::max(a.x, b.x) + 30 ||
                    scene.playerY < std::min(a.y, b.y) - 30 || scene.playerY > std::max(a.y, b.y) + 30) { continue; }
                std::printf("[campaign-cache-check] nearby collision %.0f,%.0f -> %.0f,%.0f group=%u\n", a.x, a.y, b.x, b.y, edge.group);
            }
        }
        if (scene.playerX >= centerX - 30 || session.IsFinished()) { return 1; }
        break;
    }
    for (unsigned layerIndex = 0; layerIndex < map.map.GetObjectLayerCount(); ++layerIndex) {
        const auto &objects = map.map.GetObjectLayer(layerIndex).GetObjects();
        for (unsigned objectId = 84; objectId <= 86 && objectId < objects.size(); ++objectId) {
            const auto &object = objects[objectId];
            if (object.objectType != static_cast<unsigned>(PlacedObjectType::Pickup)) { return 1; }
            for (int elapsed = 0; elapsed < 10000 && !cacheCollected[objectId - 84]; elapsed += 16) {
                const float dx = object.x - scene.playerX;
                const float dy = object.y - scene.playerY;
                const float distance = std::hypot(dx, dy);
                if (distance < 1) { session.Update(16, 0, 0, false); }
                else { session.Update(16, dx / distance, dy / distance, false); }
                RecordCacheCollections(pickups, cacheCollected);
            }
            if (!cacheCollected[objectId - 84]) { return 1; }
        }
    }
    unsigned collected = 0;
    for (bool present : cacheCollected) { if (present) { ++collected; } }
    std::printf("[campaign-cache-check] collected=%u finished=%d failures=%d\n", collected, session.IsFinished(), collected != 3);
    return collected != 3;
}

int CheckCampaignPortal(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session) {
    scene.GetPlayerVitals().invincible = true;
    CProp *portal = nullptr;
    float portalX = 0, portalY = 0;
    for (const auto &prop : map.props) {
        if (prop.objectId != 0) { continue; }
        portal = prop.runtime.get();
        portalX = prop.x;
        portalY = prop.y;
    }
    if (portal == nullptr || portal->GetStateId() != 0) { return 1; }
    auto &level = session.GetLevel();
    // Clear actual waves; neither set a wave number nor enable the portal.
    for (int elapsed = 0; elapsed < 240000 && level.GetWave() < 10; elapsed += 16) {
        for (const auto &actor : scene.enemies) {
            const auto &enemy = actor->model.enemy.combat;
            if (enemy.dead || enemy.removed) { continue; }
            CombatHit hit;
            hit.owner = kPlayerCombatId;
            hit.ownerType = 0;
            hit.damage = 100000.0f;
            hit.splash = true;
            scene.ApplyHit(enemy.id, hit);
        }
        session.Update(16, 0, 0, false);
        if (level.GetWave() < 10 && portal->GetStateId() != 0) { return 1; }
    }
    if (level.GetWave() != 10 || portal->GetStateId() != 1 || session.IsFinished()) { return 1; }
    std::printf("[campaign-portal-check] wave=%d portal enabled, mission still active\n", level.GetWave());
    scene.playerX = portalX;
    scene.playerY = portalY;
    for (int elapsed = 0; elapsed < 1000; elapsed += 16) { session.Update(16, 0, 0, false); }
    scene.playerX = portalX + 150;
    for (int elapsed = 0; elapsed < 2500; elapsed += 16) { session.Update(16, 0, 0, false); }
    if (session.IsFinished() || portal->GetStateId() != 1) { return 1; }
    scene.playerX = portalX;
    for (int elapsed = 0; elapsed < 4000 && !session.IsFinished(); elapsed += 16) { session.Update(16, 0, 0, false); }
    std::printf("[campaign-portal-check] leave cancels, return completes finished=%d failures=%d\n", session.IsFinished(), !session.IsFinished());
    return !session.IsFinished();
}

static int CheckLaterRescue(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session,
    int cameraLayer, int platformId, unsigned requiredRescues) {
    // Visit the authored camera entry rectangle; the camera export configures
    // this area's enemy rules and number of refugees. Never invoke it directly.
    for (unsigned index = 0; index < map.map.GetCameraLayerCount(); ++index) {
        const auto &camera = map.map.GetCameraLayer(index);
        if (camera.GetLayerIndex() != static_cast<unsigned>(cameraLayer)) { continue; }
        const auto &bounds = camera.GetSecondaryBounds();
        scene.playerX = bounds.x + bounds.width * 0.5f;
        scene.playerY = bounds.y + bounds.height * 0.5f;
        session.Update(16, 0, 0, false);
    }
    float platformX = 0, platformY = 0;
    if (!session.GetObjectPosition(platformId, platformX, platformY)) { return 1; }
    unsigned rescued = 0;
    for (unsigned attempt = 0; attempt < requiredRescues; ++attempt) {
        scene.playerX = platformX + 150;
        scene.playerY = platformY;
        session.Update(16, 0, 0, false);
        scene.playerX = platformX;
        scene.playerY = platformY;
        const unsigned before = rescued;
        for (int elapsed = 0; elapsed < 30000; elapsed += 16) {
            for (const auto &actor : scene.enemies) {
                const auto &enemy = actor->model.enemy.combat;
                if (enemy.templateRef.packHash == CStringToKey("pack1") && enemy.templateRef.localIndex == 14) {
                    if (enemy.dead) { return 1; }
                    continue;
                }
                if (enemy.dead || enemy.removed) { continue; }
                CombatHit hit;
                hit.owner = kPlayerCombatId;
                hit.ownerType = 0;
                hit.damage = 100000.0f;
                hit.splash = true;
                scene.ApplyHit(enemy.id, hit);
            }
            session.Update(16, 0, 0, false);
            rescued += static_cast<unsigned>(scene.teleports.size());
            if (rescued > before) { break; }
        }
    }
    std::printf("[campaign-rescue-check] platform=%d rescued=%u expected=%u failures=%d\n",
        platformId, rescued, requiredRescues, rescued != requiredRescues);
    return rescued != requiredRescues;
}

int CheckCampaignRescue(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session) {
    scene.GetPlayerVitals().invincible = true;
    // Stand inside the first authored platform. The original LEVEL script
    // must spawn its refugee and open gate 42 after the teleport callback.
    for (const auto &prop : map.props) {
        if (prop.objectId != 84 || prop.runtime == nullptr) { continue; }
        scene.playerX = prop.x;
        scene.playerY = prop.y;
        break;
    }
    bool refugeeSeen = false;
    bool refugeeDied = false;
    bool interrupted = false;
    unsigned initialGateState = 0;
    for (const auto &prop : map.props) {
        if (prop.objectId == 42 && prop.runtime != nullptr) { initialGateState = prop.runtime->GetStateId(); }
    }
    for (int elapsed = 0; elapsed < 30000; elapsed += 16) {
        session.Update(16, 0, 0, false);
        bool shouldInterrupt = false;
        for (const auto &actor : scene.enemies) {
            if (actor->objectId != 200) { continue; }
            refugeeSeen = true;
            refugeeDied = refugeeDied || actor->model.enemy.combat.dead;
            if (!interrupted && actor->model.enemy.GetStateId() == 6) { shouldInterrupt = true; }
            if (elapsed % 5008 == 0) {
                const auto &enemy = actor->model.enemy;
                std::printf("[campaign-rescue-check] refugee state=%u xy=%.0f,%.0f dead=%d removed=%d\n",
                    enemy.GetInterpreter().GetStateId(), enemy.combat.x, enemy.combat.y, enemy.combat.dead, enemy.combat.removed);
            }
        }
        if (shouldInterrupt) {
            // Do not hold an actor-vector iterator across scene updates.
            const float platformX = scene.playerX;
            scene.playerX += 150;
            for (int wait = 0; wait < 4000; wait += 16) { session.Update(16, 0, 0, false); }
            bool waiting = false;
            for (const auto &actor : scene.enemies) {
                if (actor->objectId == 200 && actor->model.enemy.GetStateId() == 7) { waiting = true; }
            }
            if (!waiting || session.CountEnemies(nullptr, 200) != 1) { return 1; }
            scene.playerX = platformX;
            interrupted = true;
            std::printf("[campaign-rescue-check] leaving pad interrupted teleport; returning\n");
        }
        for (const auto &prop : map.props) {
            if (prop.objectId == 42 && prop.runtime != nullptr && prop.runtime->GetStateId() != initialGateState &&
                refugeeSeen && !refugeeDied && session.CountEnemies(nullptr, 200) == 0) {
                std::printf("[campaign-rescue-check] first rescue gate opened refugee=%d failures=%d\n", refugeeSeen, !refugeeSeen);
                if (CheckLaterRescue(map, scene, session, 8, 86, 2) != 0) { return 1; }
                if (CheckLaterRescue(map, scene, session, 13, 85, 3) != 0) { return 1; }
                for (int wait = 0; wait < 4000 && !session.IsFinished(); wait += 16) { session.Update(16, 0, 0, false); }
                std::printf("[campaign-rescue-check] all rescued finished=%d failures=%d\n", session.IsFinished(), !session.IsFinished());
                return !session.IsFinished();
            }
        }
    }
    std::printf("[campaign-rescue-check] first rescue stalled refugee=%d failures=1\n", refugeeSeen);
    return 1;
}

int CheckCampaignProgression(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session, unsigned mapIndex) {
    scene.GetPlayerVitals().invincible = true;
    if (mapIndex == 0) {
        if (CheckCampaignTargets(map, scene, session) != 0) { return 1; }
    } else if (mapIndex == 3) {
        if (CheckCampaignDoorPassage(map, scene, session) != 0) { return 1; }
        // Lava 3's authored pickup 113 starts the final placed-enemy group.
        // Exercise collection and the death export, not a direct state jump.
        for (unsigned index = 0; index < map.map.GetObjectLayerCount(); ++index) {
            const auto &layer = map.map.GetObjectLayer(index);
            if (layer.GetLayerIndex() != static_cast<unsigned>(session.GetLevel().GetObjectLayer())) { continue; }
            const auto &objects = layer.GetObjects();
            if (objects.size() <= 113 || objects[113].objectType != static_cast<unsigned>(PlacedObjectType::Pickup)) { return 1; }
            scene.playerX = objects[113].x;
            scene.playerY = objects[113].y;
        }
        session.Update(16, 0, 0, false);
        if (session.GetLevel().GetStateId() != 3) { return 1; }
    } else if (mapIndex == 2) {
        // Observe the untouched spawn first: a placed preview is not proof
        // that the LEVEL has created a live actor or fired its start trigger.
        std::printf("[campaign-lava2-check] initial state=%d alive=%d triggers=%u xy=%.0f,%.0f\n",
            session.GetLevel().GetStateId(), session.CountEnemies(nullptr, -1),
            session.GetLevel().GetTriggerCount(), scene.playerX, scene.playerY);
        for (int elapsed = 0; elapsed < 3000; elapsed += 16) { session.Update(16, 0, 0, false); }
        std::printf("[campaign-lava2-check] idle start state=%d alive=%d triggers=%u xy=%.0f,%.0f\n",
            session.GetLevel().GetStateId(), session.CountEnemies(nullptr, -1),
            session.GetLevel().GetTriggerCount(), scene.playerX, scene.playerY);
        // Enter the original start trigger; never invoke the LEVEL export.
        for (unsigned index = 0; index < map.map.GetCollisionLayerCount(); ++index) {
            const auto &layer = map.map.GetCollisionLayer(index);
            if (static_cast<int>(layer.GetLayerIndex()) != session.GetLevel().GetTriggerLayer()) { continue; }
            const auto &geometry = layer.GetCollision();
            for (const auto &edge : geometry.GetEdges()) {
                if (edge.group != 0) { continue; }
                const auto &first = geometry.GetVertices()[edge.firstVertex];
                const auto &second = geometry.GetVertices()[edge.secondVertex];
                scene.playerX = (first.x + second.x) * 0.5f;
                scene.playerY = (first.y + second.y) * 0.5f;
                session.Update(16, 0, -1, false);
                break;
            }
        }
        if (session.GetLevel().GetStateId() != 1) { return 1; }
        // Allow the authored entrance to finish before applying lethal damage.
        for (int elapsed = 0; elapsed < 30000; elapsed += 16) { session.Update(16, 0, 0, false); }
        std::printf("[campaign-lava2-check] entrance after 30 seconds: state=%d alive=%d\n",
            session.GetLevel().GetStateId(), session.CountEnemies(nullptr, -1));
    }
    auto &level = session.GetLevel();
    int previousState = -1;
    bool finalEnemySeen = false;
    // Defeat actors through their own hit/death handlers; never advance LEVEL
    // state or call its death export from the test. Virtual time is bounded.
    for (int elapsed = 0; elapsed < 120000; elapsed += 16) {
        for (auto &actor : scene.enemies) {
            auto &enemy = actor->model.enemy;
            if (enemy.combat.templateRef.packHash == CStringToKey("pack1") && enemy.combat.templateRef.localIndex == 17) {
                finalEnemySeen = true;
            }
            if (enemy.combat.dead || enemy.combat.removed) { continue; }
            if (mapIndex == 3 || mapIndex == 2) { finalEnemySeen = true; }
            CombatHit hit;
            hit.owner = kPlayerCombatId;
            hit.ownerType = 0;
            hit.damage = 100000.0f;
            hit.part = 0;
            hit.splash = true;
            scene.ApplyHit(enemy.combat.id, hit);
        }
        session.Update(16, 0, 0, false);
        if (previousState != level.GetStateId()) {
            previousState = level.GetStateId();
            std::printf("[campaign-progression-check] elapsed=%d state=%d kills=%u alive=%d spawned=%u final=%d cleared=%d\n",
                elapsed, previousState, scene.GetTotalKills(), session.CountEnemies(nullptr, -1),
                level.GetSpawner().GetSpawnCount(), finalEnemySeen, level.IsCleared());
        }
        if (level.IsCleared()) {
            const float previousY = scene.playerY;
            session.Update(100, 0, 1, false);
            const bool finished = session.IsFinished();
            const int failures = !finalEnemySeen || !finished;
            std::printf("[campaign-progression-check] MAP %u complete final=%d moved=%.1f finished=%d failures=%d\n",
                mapIndex, finalEnemySeen, scene.playerY - previousY, finished, failures);
            return failures;
        }
    }
    std::printf("[campaign-progression-check] stalled state=%d kills=%u alive=%d spawned=%u final=%d failures=1\n",
        level.GetStateId(), scene.GetTotalKills(), session.CountEnemies(nullptr, -1), level.GetSpawner().GetSpawnCount(), finalEnemySeen);
    for (const auto &actor : scene.enemies) {
        const auto &enemy = actor->model.enemy.combat;
        if (!enemy.dead && !enemy.removed) {
            std::printf("[campaign-progression-check] remaining object=%d ref=%08x:%u hp=%.0f xy=%.0f,%.0f\n",
                actor->objectId, enemy.templateRef.packHash, enemy.templateRef.localIndex, enemy.health, enemy.x, enemy.y);
        }
    }
    return 1;
}

int CheckCampaignTargets(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session) {
    scene.GetPlayerVitals().invincible = true;
    // Bring the authored turret into view so CLevel can spawn its placed object.
    for (unsigned layerIndex = 0; layerIndex < map.map.GetObjectLayerCount(); ++layerIndex) {
        const auto &layer = map.map.GetObjectLayer(layerIndex);
        if (layer.GetLayerIndex() != static_cast<unsigned>(session.GetLevel().GetObjectLayer())) { continue; }
        for (const auto &object : layer.GetObjects()) {
            if (object.objectType == static_cast<unsigned>(PlacedObjectType::Enemy) &&
                object.packHash == CStringToKey("pack1") && object.localIndex == 16) {
                scene.playerX = object.x;
                scene.playerY = object.y + 140.0f;
            }
        }
    }
    for (int elapsed = 0; elapsed < 1000; elapsed += 16) { session.Update(16, 0, 0, false); }
    // MAP 4 turret: trace the same segments as player projectiles, then deliver
    // damage through CombatScene so the real ENEMY hit script decides the result.
    for (auto &actor : scene.enemies) {
        auto &enemy = actor->model.enemy;
        if (enemy.combat.templateRef.packHash != CStringToKey("pack1") || enemy.combat.templateRef.localIndex != 16) { continue; }
        CombatHit hit;
        hit.owner = kPlayerCombatId;
        hit.ownerType = 0;
        hit.damage = 1000;
        const auto trace = scene.Trace(hit, enemy.combat.x, enemy.combat.y + 140, 0, -280, 2, {});
        std::printf("[campaign-target-check] turret hp=%.0f edges=%zu parts=%u traced=%llu target=%llu\n",
            enemy.combat.health, enemy.combat.collision.GetEdges().size(), enemy.GetPartCount(),
            static_cast<unsigned long long>(trace.target), static_cast<unsigned long long>(enemy.combat.id));
        if (trace.target != enemy.combat.id) { return 1; }
        hit.part = trace.part;
        hit.edge = trace.edge;
        scene.ApplyHit(trace.target, hit);
        std::printf("[campaign-target-check] turret hp-after=%.0f dead=%d\n", enemy.combat.health, enemy.combat.dead);
        return enemy.combat.health > 0;
    }
    // MAP 0: destroy the authored control object 75, then physically cross gate 41.
    bool switchHit = false;
    for (auto &prop : map.props) {
        if (!prop.active || prop.objectId != 75 || prop.runtime == nullptr) { continue; }
        const auto &vertices = prop.runtime->GetCollision(true).GetVertices();
        if (vertices.empty()) { return 1; }
        float centerX = 0, centerY = 0;
        for (const auto &vertex : vertices) { centerX += vertex.x; centerY += vertex.y; }
        centerX = prop.x + centerX / vertices.size();
        centerY = prop.y + centerY / vertices.size();
        CombatHit hit;
        hit.owner = kPlayerCombatId;
        hit.ownerType = 0;
        hit.damage = 1000;
        const auto trace = scene.Trace(hit, centerX, centerY + 100, 0, -200, 2, {});
        if (trace.target == 0) { return 1; }
        const auto before = prop.runtime->GetStateId();
        scene.ApplyHit(trace.target, hit);
        if (prop.runtime->GetStateId() == before) { return 1; }
        switchHit = true;
    }
    if (!switchHit) { return 1; }
    for (int elapsed = 0; elapsed < 2000; elapsed += 16) { session.Update(16, 0, 0, false); }
    for (auto &prop : map.props) {
        if (!prop.active || prop.objectId != 41 || prop.runtime == nullptr) { continue; }
        scene.playerX = prop.x;
        scene.playerY = prop.y + 140;
        for (int elapsed = 0; elapsed < 3000; elapsed += 16) {
            session.Update(16, 0, -1, false);
            if (scene.playerY < prop.y - 60) {
                std::printf("[campaign-target-check] MAP 0 switch destroyed / gate crossed failures=0\n");
                return 0;
            }
        }
    }
    return 1;
}

int CheckCampaignDoorPassage(MapDetail::LoadedMap &map, CombatScene &scene, SurvivalSession &session) {
    bool entranceCrossed = false;
    for (auto &prop : map.props) {
        if (prop.objectId != 5 || prop.objectLayer != static_cast<unsigned>(session.GetLevel().GetObjectLayer())) { continue; }
        if (!prop.active || prop.runtime == nullptr) { return 1; }
        scene.GetPlayerVitals().invincible = true;
        const float destinationY = prop.y + 160;
        std::printf("[campaign-door-check] gate=%08x:%u id=%d at=%.1f,%.1f player=%.1f,%.1f state=%u entry=%d\n",
            prop.sprite->resource.packHash, prop.sprite->resource.localIndex, prop.objectId, prop.x, prop.y,
            scene.playerX, scene.playerY, prop.runtime->GetStateId(), prop.runtime->ChecksEntry());
        unsigned previousState = prop.runtime->GetStateId();
        for (int elapsed = 0; elapsed < 12000; elapsed += 16) {
            float moveX = prop.x - scene.playerX;
            float moveY = destinationY - scene.playerY;
            const float distance = std::hypot(moveX, moveY);
            if (distance > 1) { moveX /= distance; moveY /= distance; }
            session.Update(16, moveX, moveY, false);
            if (previousState != prop.runtime->GetStateId()) {
                previousState = prop.runtime->GetStateId();
                std::printf("[campaign-door-check] elapsed=%d gate-state=%u player=%.1f,%.1f\n", elapsed, previousState, scene.playerX, scene.playerY);
            }
            if (scene.playerY >= destinationY - 20) {
                std::printf("[campaign-door-check] crossed gate elapsed=%d failures=0\n", elapsed);
                entranceCrossed = true;
                break;
            }
        }
        if (entranceCrossed) { break; }
        std::printf("[campaign-door-check] blocked gate-state=%u entry=%d player=%.1f,%.1f target=%.1f failures=1\n",
            prop.runtime->GetStateId(), prop.runtime->ChecksEntry(), scene.playerX, scene.playerY, destinationY);
        return 1;
    }
    if (!entranceCrossed) { return 1; }
    // LEVEL 3 export 6 must keep gate 4 locked before pickup object 114.
    bool lockedGateChecked = false;
    for (auto &prop : map.props) {
        if (prop.objectId != 4 || prop.objectLayer != static_cast<unsigned>(session.GetLevel().GetObjectLayer())) { continue; }
        if (!prop.active || prop.runtime == nullptr) { return 1; }
        scene.playerX = prop.x;
        scene.playerY = prop.y + 140;
        const unsigned before = session.GetLevel().GetTriggerCount();
        for (int elapsed = 0; elapsed < 2000; elapsed += 16) {
            session.Update(16, 0, -1, false);
            if (scene.playerY < prop.y - 60) {
                std::printf("[campaign-door-check] crossed locked gate without key failures=1\n");
                return 1;
            }
        }
        const unsigned triggers = session.GetLevel().GetTriggerCount() - before;
        if (triggers == 0) { return 1; }
        std::printf("[campaign-door-check] gate locked without key triggers=%u failures=0\n", triggers);
        lockedGateChecked = true;
        break;
    }
    if (!lockedGateChecked) { return 1; }
    // Isolate the authored pickup -> trigger -> gate chain from route planning.
    // Each stage starts at a real map location; collection and triggers still
    // run through SurvivalSession::Update, never direct native/export calls.
    bool placedAtKey = false;
    for (unsigned index = 0; index < map.map.GetObjectLayerCount(); ++index) {
        const auto &layer = map.map.GetObjectLayer(index);
        if (layer.GetLayerIndex() != static_cast<unsigned>(session.GetLevel().GetObjectLayer())) { continue; }
        const auto &objects = layer.GetObjects();
        if (objects.size() <= 114 || objects[114].objectType != static_cast<unsigned>(PlacedObjectType::Pickup)) { return 1; }
        scene.playerX = objects[114].x;
        scene.playerY = objects[114].y;
        placedAtKey = true;
    }
    if (!placedAtKey) { return 1; }
    session.Update(16, 0, 0, false);
    for (auto &prop : map.props) {
        if (prop.objectId != 4 || prop.objectLayer != static_cast<unsigned>(session.GetLevel().GetObjectLayer())) { continue; }
        if (!prop.active || prop.runtime == nullptr) { return 1; }
        scene.playerX = prop.x;
        scene.playerY = prop.y + 140;
        const unsigned before = session.GetLevel().GetTriggerCount();
        for (int elapsed = 0; elapsed < 6000; elapsed += 16) {
            session.Update(16, 0, -1, false);
            if (scene.playerY < prop.y - 60) {
                std::printf("[campaign-door-check] key/trigger/gate crossed state=%u triggers=%u failures=0\n",
                    prop.runtime->GetStateId(), session.GetLevel().GetTriggerCount() - before);
                return 0;
            }
        }
        std::printf("[campaign-door-check] key/trigger/gate blocked state=%u triggers=%u player=%.1f,%.1f gate=%.1f,%.1f failures=1\n",
            prop.runtime->GetStateId(), session.GetLevel().GetTriggerCount() - before, scene.playerX, scene.playerY, prop.x, prop.y);
        return 1;
    }
    return 1;
}
