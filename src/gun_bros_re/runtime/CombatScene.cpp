/** @file CombatScene.cpp
 * @brief Actor targeting, continuous projectile collision and scene lifecycle.
 */
#define NOMINMAX
#include "runtime/CombatScene.h"
#include "engine/CMatrix4d.h"
#include "gun_bros/CMeshCamera.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kRadians = 3.14159265f / 180;
constexpr float kPlayerSpeed = 220;
constexpr float kSpawnDistance = 280;
constexpr int kCorpseLimitMs = 10000;

bool Skipped(CombatId id, const std::vector<CombatId> &ids) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

// Earliest point where a moving circle overlaps a stationary one.
float CircleFraction(float x, float y, float dx, float dy, float cx, float cy, float radius) {
    const float ox = x - cx, oy = y - cy;
    const float c = ox * ox + oy * oy - radius * radius;
    if (c <= 0) { return 0; }
    const float a = dx * dx + dy * dy;
    if (a <= 0) { return 2; }
    const float b = ox * dx + oy * dy;
    const float discriminant = b * b - a * c;
    if (discriminant < 0) { return 2; }
    const float fraction = (-b - std::sqrt(discriminant)) / a;
    if (fraction < 0 || fraction > 1) { return 2; }
    return fraction;
}

// A swept circle against a finite edge is its strip plus both endpoint caps.
float EdgeFraction(float x, float y, float dx, float dy,
    const CollisionPoint &a, const CollisionPoint &b, float radius) {
    float nearest = std::min(CircleFraction(x, y, dx, dy, a.x, a.y, radius),
        CircleFraction(x, y, dx, dy, b.x, b.y, radius));
    const float ex = b.x - a.x, ey = b.y - a.y;
    const float length = std::hypot(ex, ey);
    if (length <= 0) { return nearest; }
    const float nx = -ey / length, ny = ex / length;
    const float distance = (x - a.x) * nx + (y - a.y) * ny;
    const float velocity = dx * nx + dy * ny;
    for (int side = -1; side <= 1; side += 2) {
        float fraction = 0;
        if (std::abs(distance) > radius) {
            if (std::abs(velocity) < 0.00001f) { continue; }
            fraction = (side * radius - distance) / velocity;
        }
        if (fraction < 0 || fraction > 1) { continue; }
        const float along = ((x + dx * fraction - a.x) * ex +
            (y + dy * fraction - a.y) * ey) / length;
        if (along >= 0 && along <= length) { nearest = std::min(nearest, fraction); }
    }
    return nearest;
}

void Transform(const float *matrix, float localX, float localY, float localZ,
    float &x, float &y, float &z) {
    x = matrix[0] * localX + matrix[1] * localY + matrix[2] * localZ + matrix[3];
    y = matrix[4] * localX + matrix[5] * localY + matrix[6] * localZ + matrix[7];
    z = matrix[8] * localX + matrix[9] * localY + matrix[10] * localZ + matrix[11];
}
}

bool LoadInitialPlayerHealth(CResTOCManager &toc, PackTables &tables, float &health) {
    for (std::uint32_t p = 0; p < toc.GetPackCount(); ++p) {
        // Progress is a singleton data table, not a script-spawned object;
        // OBJECT_SCRIPT_COUNTS can report zero while its section exists.
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(toc.GetPack(p)->GetPackHash(), GameSection::PlayerProgress, 0, payload)) { continue; }
        CArrayInputStream stream(payload);
        const unsigned xpCount = stream.ReadUInt16();
        stream.Skip(xpCount * 4);
        const unsigned healthCount = stream.ReadUInt16();
        if (healthCount < 2) { continue; }
        // Progress is indexed by the displayed level. Entry zero is a sentinel;
        // a new player starts at level one.
        stream.ReadUInt32();
        health = static_cast<float>(static_cast<std::int16_t>(stream.ReadUInt32()));
        if (!stream.Overran() && health > 0) {
            std::printf("[combat] initial player health %.0f from PLAYER_PROGRESS\n", health);
            return true;
        }
    }
    std::printf("[combat] initial player health missing\n");
    return false;
}

CombatScene::CombatScene(PackTables &tables, const CShaderProgram &program,
    const std::vector<EnemyTemplateData> &catalog, PlayerModel &player,
    PlayerVitals &vitals, WeaponEffects &effects, float playerGameScale)
    : m_tables(tables), m_program(program), m_catalog(catalog), m_player(player),
      m_vitals(vitals), m_effects(effects), m_playerGameScale(playerGameScale) {
    m_effects.SetCombatWorld(this);
}

void CombatScene::Reset() {
    m_effects.Clear();
    enemies.clear();
    m_pendingSpawns.clear();
    m_vitals.Reset();
    if (m_player.weapon != nullptr) {
        // Reset the script and gun state as well as health. The replacement
        // copies templates before retiring the old equipment.
        if (EquipPlayerWeapon(m_tables, m_player.weapon->playerScript,
            m_player.weapon->data, "arena reset", m_player)) {
            CreatePlayerBuffers(m_player, m_program);
        }
    }
    m_playerForceMs = 0;
    playerX = 600;
    playerY = 650;
    m_previousPlayerX = playerX;
    m_previousPlayerY = playerY;
    facing = 0;
    damageDealt = 0;
    lastDamage = 0;
    hits = 0;
    kills = 0;
    spawned = 0;
    invalidSpawns = 0;
}

CombatEnemy *CombatScene::Spawn(std::size_t entry, float x, float y) {
    if (entry >= m_catalog.size()) { return nullptr; }
    std::unique_ptr<CombatEnemy> actor(new CombatEnemy());
    actor->data = &m_catalog[entry];
    EnemyCombat &state = actor->model.enemy.combat;
    state.enabled = actor->data->script.IsPresent();
    state.id = m_nextId++;
    state.randomState = static_cast<std::uint32_t>(state.id * 7919);
    actor->model.enemy.SetRandomSeed(state.randomState);
    state.x = std::clamp(x, 70.0f, kArenaWidth - 70);
    state.y = std::clamp(y, 150.0f, kArenaHeight - 70);
    state.previousX = state.x;
    state.previousY = state.y;
    if (!LoadEnemyModel(m_tables, *actor->data, true, &m_program, EnemySpawnMode::Level, actor->model)) {
        ++invalidSpawns;
        return nullptr;
    }
    CombatEnemy *result = actor.get();
    enemies.push_back(std::move(actor));
    ++spawned;
    SelectTarget(*result);
    return result;
}

CombatEnemy *CombatScene::SpawnNearby(std::size_t entry) {
    float radius = 40;
    if (entry < m_catalog.size()) { radius = std::max(radius, static_cast<float>(m_catalog[entry].radius116)); }
    for (int attempt = 0; attempt < 120; ++attempt) {
        const float angle = (spawned * 137.5f + attempt * 137.5f) * kRadians;
        const float distance = kSpawnDistance + (attempt % 5) * 55;
        const float x = playerX + std::sin(angle) * distance;
        const float y = playerY - std::cos(angle) * distance;
        if (x < radius + 20 || x > kArenaWidth - radius - 20 ||
            y < 145 + radius || y > kArenaHeight - radius - 20) { continue; }
        bool free = true;
        for (const auto &actor : enemies) {
            const EnemyCombat &state = actor->model.enemy.combat;
            const float otherRadius = std::max(35.0f, actor->model.enemy.GetPart(0).radius);
            if (!state.removed && std::hypot(x - state.x, y - state.y) < radius + otherRadius + 15) {
                free = false;
                break;
            }
        }
        if (free) { return Spawn(entry, x, y); }
    }
    std::printf("[arena] no free spawn position\n");
    return nullptr;
}

CombatEnemy *CombatScene::Find(CombatId id) {
    for (auto &actor : enemies) {
        if (actor->model.enemy.combat.id == id) { return actor.get(); }
    }
    return nullptr;
}

std::size_t CombatScene::AliveCount() const {
    std::size_t count = 0;
    for (const auto &actor : enemies) {
        const EnemyCombat &state = actor->model.enemy.combat;
        if (!state.dead && !state.removed) { ++count; }
    }
    return count;
}

void CombatScene::PlayerMatrix(float *matrix) const {
    float identity[16];
    Matrix4dIdentity(identity);
    const float scale = PlayerModelWorldScale(m_player, m_playerGameScale, 1);
    BuildPlayerGameMatrix(identity, playerX, playerY, scale, facing, matrix);
}

void CombatScene::EnemyMatrix(const CombatEnemy &actor, float *matrix) const {
    float identity[16];
    Matrix4dIdentity(identity);
    const EnemyCombat &state = actor.model.enemy.combat;
    const float scale = EnemyModelWorldScale(actor.model, actor.data->gameScale, 1) * state.scaleFactor;
    BuildEnemyGameMatrix(actor.model, identity, state.x, state.y, scale, state.facing, matrix);
}

void CombatScene::PartMatrix(const CombatEnemy &actor, int index, float *matrix) const {
    float base[16];
    EnemyMatrix(actor, base);
    const EnemyPart &part = actor.model.enemy.GetPart(index);
    if (!part.followsFacing) {
        float identity[16];
        Matrix4dIdentity(identity);
        const EnemyCombat &state = actor.model.enemy.combat;
        BuildEnemyGameMatrix(actor.model, identity, state.x, state.y,
            EnemyModelWorldScale(actor.model, actor.data->gameScale, 1) * state.scaleFactor, 0, base);
    }
    MeshPart placement;
    placement.extraAngleDegrees = part.extraAngleDegrees;
    placement.extraAxisX = part.extraAxisX;
    placement.extraAxisY = part.extraAxisY;
    placement.extraAxisZ = part.extraAxisZ;
    if (part.boneIndex >= 0) {
        actor.model.enemy.GetPart(0).controller.GetAnimation().GetNodeAt(part.boneIndex, placement.attachment);
    }
    MeshCameraBuildPartMatrix(placement, base, matrix);
}

bool CombatScene::Anchor(CombatId id, int part, int node, float &x, float &y, float &z, float &direction) {
    CombatEnemy *actor = Find(id);
    if (actor == nullptr || actor->model.enemy.combat.removed || actor->model.enemy.combat.dead) { return false; }
    CEnemy &enemy = actor->model.enemy;
    x = enemy.combat.x;
    y = enemy.combat.y;
    z = 0;
    direction = enemy.combat.facing - 90;
    if (part < 0 || part >= static_cast<int>(enemy.GetPartCount()) || node < 0) { return true; }
    MeshBoneTransform bone;
    if (!enemy.GetPart(part).controller.GetAnimation().GetNodeAt(node, bone)) { return true; }
    float matrix[16];
    PartMatrix(*actor, part, matrix);
    Transform(matrix, bone.posX, bone.posY, bone.posZ, x, y, z);
    MeshPart nodePlacement;
    nodePlacement.attachment = bone;
    float nodeMatrix[16];
    MeshCameraBuildPartMatrix(nodePlacement, matrix, nodeMatrix);
    const float forwardX = -nodeMatrix[1];
    const float forwardY = -nodeMatrix[5];
    if (std::hypot(forwardX, forwardY) > 0.0001f) {
        direction = std::atan2(forwardY, forwardX) / kRadians;
    }
    return true;
}

void CombatScene::SelectTarget(CombatEnemy &actor) {
    CEnemy &enemy = actor.model.enemy;
    if (enemy.combat.targetType != 2) {
        enemy.SetTarget(kPlayerCombatId, playerX, playerY, !m_vitals.dead);
        return;
    }
    CombatEnemy *nearest = nullptr;
    float distance = 100000;
    for (auto &other : enemies) {
        const CEnemy &target = other->model.enemy;
        if (!target.combat.enabled || !target.combat.targetable ||
            !target.CanReceiveProjectile(0, enemy.combat.id)) { continue; }
        const float current = std::hypot(target.combat.x - enemy.combat.x, target.combat.y - enemy.combat.y);
        if (current < distance) { distance = current; nearest = other.get(); }
    }
    if (nearest != nullptr) {
        const EnemyCombat &target = nearest->model.enemy.combat;
        enemy.SetTarget(target.id, target.x, target.y, true);
    } else { enemy.SetTarget(0, enemy.combat.x, enemy.combat.y, false); }
}

void CombatScene::EnemyCircle(const CombatEnemy &actor, int part, float &x, float &y, float &radius) const {
    const CEnemy &enemy = actor.model.enemy;
    const EnemyCombat &state = enemy.combat;
    x = state.x;
    y = state.y;
    radius = enemy.GetPart(part).radius * state.scaleFactor;
    const CMesh *mesh = enemy.GetPart(0).controller.GetAnimation().GetMesh();
    if (mesh == nullptr) { return; }
    // CMesh::GetRotationOffset shifts the circular hurtbox with the root mesh.
    const MeshBounds &bounds = mesh->GetBounds();
    const float cosine = std::cos(state.facing * kRadians);
    const float sine = std::sin(state.facing * kRadians);
    const float scale = bounds.inverseExtent * actor.data->gameScale * state.scaleFactor;
    x += (bounds.centerX * cosine + bounds.centerY * sine) * scale;
    y += ((bounds.centerY * cosine - bounds.centerX * sine) * 0.8660254f - bounds.centerZ * 0.5f) * scale;
}

CombatTrace CombatScene::Trace(const CombatHit &hit, float x, float y, float dx, float dy,
    float radius, const std::vector<CombatId> &skipTargets) {
    CombatTrace result;
    float nearest = 2;
    if (hit.ownerType == 1 && hit.owner != kPlayerCombatId && !m_vitals.dead && !Skipped(kPlayerCombatId, skipTargets)) {
        float moveX = playerX - m_previousPlayerX, moveY = playerY - m_previousPlayerY;
        if ((hit.flags & 0x100) != 0) { moveX = 0; moveY = 0; }
        nearest = CircleFraction(x, y, dx - moveX, dy - moveY,
            playerX - moveX, playerY - moveY, kPlayerCollisionRadius + radius);
        if (nearest <= 1) {
            result.target = kPlayerCombatId; result.fraction = nearest;
            result.normalX = x + dx * nearest - playerX;
            result.normalY = y + dy * nearest - playerY;
        }
    }
    for (auto &actor : enemies) {
        CEnemy &enemy = actor->model.enemy;
        EnemyCombat &state = enemy.combat;
        if (!state.enabled || !enemy.CanReceiveProjectile(hit.ownerType, hit.owner) ||
            Skipped(state.id, skipTargets)) { continue; }
        const auto &edges = state.collision.GetEdges();
        const auto &vertices = state.collision.GetVertices();
        if (!edges.empty()) {
            const float cosine = std::cos(state.facing * kRadians), sine = std::sin(state.facing * kRadians);
            // Authored collision vertices already use world units; only runtime
            // scaling and actor rotation apply, not mesh normalisation or tilt.
            for (std::size_t e = 0; e < edges.size(); ++e) {
                const CollisionEdge &edge = edges[e];
                if (!edge.enabled || edge.firstVertex >= vertices.size() || edge.secondVertex >= vertices.size()) { continue; }
                const CollisionPoint &a = vertices[edge.firstVertex], &b = vertices[edge.secondVertex];
                CollisionPoint first(state.x + (a.x * cosine - a.y * sine) * state.scaleFactor,
                    state.y + (a.x * sine + a.y * cosine) * state.scaleFactor);
                CollisionPoint second(state.x + (b.x * cosine - b.y * sine) * state.scaleFactor,
                    state.y + (b.x * sine + b.y * cosine) * state.scaleFactor);
                const float fraction = EdgeFraction(x, y, dx, dy, first, second, radius);
                if (fraction < nearest) {
                    nearest = fraction;
                    // The script sees the authored edge group, not its array index.
                    result = {state.id, fraction, 0, edge.group, first.y - second.y, second.x - first.x};
                }
            }
            continue;
        }
        for (std::uint32_t p = 0; p < enemy.GetPartCount(); ++p) {
            const EnemyPart &part = enemy.GetPart(p);
            if (part.radius <= 0 || !part.visible) { continue; }
            float cx = 0, cy = 0, partRadius = 0;
            EnemyCircle(*actor, p, cx, cy, partRadius);
            float moveX = state.x - state.previousX, moveY = state.y - state.previousY;
            if ((hit.flags & 0x100) != 0) { moveX = 0; moveY = 0; }
            const float fraction = CircleFraction(x, y, dx - moveX, dy - moveY,
                cx - moveX, cy - moveY, radius + partRadius);
            if (fraction < nearest) {
                nearest = fraction;
                result = {state.id, fraction, static_cast<int>(p), -1,
                    x + dx * fraction - cx + moveX * (1 - fraction),
                    y + dy * fraction - cy + moveY * (1 - fraction)};
            }
        }
    }
    return result;
}

HitResult CombatScene::ApplyHit(CombatId target, const CombatHit &hit) {
    if (target == kPlayerCombatId) {
        if (hit.ownerType != 1 || m_player.weapon == nullptr) { return HitResult::Ignored; }
        return m_player.weapon->brother.ReceiveDamage(hit.damage);
    }
    CombatEnemy *actor = Find(target);
    if (actor == nullptr) { return HitResult::Ignored; }
    return actor->model.enemy.ReceiveHit(hit);
}

bool CombatScene::FindTarget(const CombatHit &hit, float radius, float &x, float &y) {
    bool found = false;
    if (hit.ownerType == 1 && !m_vitals.dead && std::hypot(hit.x - playerX, hit.y - playerY) < radius) {
        x = playerX; y = playerY; return true;
    }
    for (auto &actor : enemies) {
        CEnemy &enemy = actor->model.enemy;
        if (!enemy.combat.targetable || !enemy.CanReceiveProjectile(hit.ownerType, hit.owner)) { continue; }
        const float distance = std::hypot(enemy.combat.x - hit.x, enemy.combat.y - hit.y);
        if (distance < radius) { radius = distance; x = enemy.combat.x; y = enemy.combat.y; found = true; }
    }
    return found;
}

void CombatScene::Splash(const CombatHit &hit, float radius, float coneDegrees, float force, int forceMs) {
    std::vector<CombatId> targets;
    if (hit.ownerType == 1 && !m_vitals.dead) { targets.push_back(kPlayerCombatId); }
    for (const auto &actor : enemies) {
        if (actor->model.enemy.CanReceiveProjectile(hit.ownerType, hit.owner)) { targets.push_back(actor->model.enemy.combat.id); }
    }
    for (CombatId id : targets) {
        float x = playerX, y = playerY;
        CombatEnemy *actor = Find(id);
        if (actor != nullptr) { x = actor->model.enemy.combat.x; y = actor->model.enemy.combat.y; }
        const float dx = x - hit.x, dy = y - hit.y;
        const float distance = std::hypot(dx, dy);
        // CLevel includes the target's collision radius in the blast test.
        // Testing only its centre drops explosions at the surface of big units.
        float targetRadius = kPlayerCollisionRadius;
        if (actor != nullptr) {
            targetRadius = actor->model.enemy.GetPart(0).radius * actor->model.enemy.combat.scaleFactor;
        }
        if (distance > radius + targetRadius) { continue; }
        const float angle = std::atan2(dy, dx) / kRadians;
        const float difference = std::remainder(angle - hit.direction, 360.0f);
        if (coneDegrees < 360 && std::abs(difference) > coneDegrees * 0.5f) { continue; }
        CombatHit splash = hit;
        splash.part = 0;
        // OnSplashDamage still goes through class 6 event 2. The separate
        // splash flag tells the script which shield/part rules to apply.
        splash.splash = true;
        splash.part = -1;
        ApplyHit(id, splash);
        if (force > 0 && distance > 0) {
            const float travel = force * forceMs * 0.001f;
            x = std::clamp(x + dx / distance * travel, 40.0f, kArenaWidth - 40);
            y = std::clamp(y + dy / distance * travel, 150.0f, kArenaHeight - 40);
            if (actor != nullptr) { actor->model.enemy.combat.x = x; actor->model.enemy.combat.y = y; }
            else { playerX = x; playerY = y; }
        }
    }
}

void CombatScene::SpawnFromProjectile(const GameObjectRef &resource, const CombatHit &hit) {
    for (std::size_t i = 0; i < m_catalog.size(); ++i) {
        if (m_catalog[i].packHash == resource.packHash && m_catalog[i].ordinal == resource.localIndex) {
            m_pendingSpawns.push_back({i, hit.x, hit.y});
            return;
        }
    }
    ++invalidSpawns;
    std::printf("[combat] missing spawn resource %08x:%u\n", resource.packHash, resource.localIndex);
}

void CombatScene::FinishSpawns() {
    std::vector<PendingSpawn> pending;
    pending.swap(m_pendingSpawns);
    for (const PendingSpawn &spawn : pending) { Spawn(spawn.entry, spawn.x, spawn.y); }
}

void CombatScene::Actions(CombatEnemy &actor) {
    CEnemy &enemy = actor.model.enemy;
    const EnemyCombat &state = enemy.combat;
    for (const EnemyAction &action : enemy.TakeActions()) {
        float x = state.x, y = state.y, z = 0, direction = state.facing - 90;
        // Death effects still use the final pose, even though the actor is no
        // longer a valid continuous beam/effect anchor.
        if (!state.dead) { Anchor(state.id, action.part, action.node, x, y, z, direction); }
        int ownerType = 1;
        if (state.targetType == 2) { ownerType = 0; }
        if (action.kind == EnemyAction::Kind::Bullet) {
            if (action.slot != 1) { direction = action.direction - 90; }
            m_effects.SpawnProjectile(action.resource, x, y, z, direction,
                action.speed, state.id, ownerType, action.part, action.node);
        } else if (action.kind == EnemyAction::Kind::Stun) {
            if (ownerType == 1 && std::hypot(playerX - x, playerY - y) < action.radius) {
                m_player.weapon->brother.Stun(action.durationMs);
            }
        } else if (action.kind == EnemyAction::Kind::CollisionResolved) {
            m_effects.ResolveHit(action.projectile, action.result);
        } else if (action.kind == EnemyAction::Kind::RemoveBullet) {
            m_effects.RemoveOldestProjectile(state.id);
        } else if (action.kind == EnemyAction::Kind::Broadcast) {
            for (auto &other : enemies) {
                if (other.get() != &actor && !other->model.enemy.combat.dead) {
                    other->model.enemy.TriggerEvent(static_cast<std::uint8_t>(action.slot));
                }
            }
        } else if (action.kind == EnemyAction::Kind::Splash || action.kind == EnemyAction::Kind::SpawnEnemy) {
            CombatHit hit;
            hit.owner = state.id;
            hit.ownerType = ownerType;
            hit.x = x; hit.y = y; hit.direction = direction; hit.damage = action.damage;
            if (action.kind == EnemyAction::Kind::Splash) { Splash(hit, action.radius, 360, action.force, action.durationMs); }
            else { SpawnFromProjectile(action.resource, hit); }
        } else {
            GunCue cue;
            cue.resource = action.resource;
            cue.kind = GunCue::Kind::Effect;
            if (action.kind == EnemyAction::Kind::Sound) { cue.kind = GunCue::Kind::Sound; }
            if (action.kind == EnemyAction::Kind::LoopSound) { cue.kind = GunCue::Kind::LoopSound; }
            if (action.kind == EnemyAction::Kind::StopSound) { cue.kind = GunCue::Kind::StopSound; }
            if (action.kind == EnemyAction::Kind::LinkedEffect) { cue.kind = GunCue::Kind::Trail; }
            if (action.kind == EnemyAction::Kind::StopEffect) { cue.kind = GunCue::Kind::StopTrail; }
            if (action.kind == EnemyAction::Kind::Shake || action.kind == EnemyAction::Kind::Reward) { continue; }
            m_effects.Emit(cue, x, y, z, direction, state.id, action.slot, action.part, action.node);
        }
    }
}

void CombatScene::Update(int deltaMs, float moveX, float moveY, bool shoot) {
    if (deltaMs <= 0) { return; }
    m_previousPlayerX = playerX;
    m_previousPlayerY = playerY;
    if (!m_vitals.dead && m_vitals.stunMs == 0) {
        const float length = std::hypot(moveX, moveY);
        if (length > 0) {
            playerX = std::clamp(playerX + moveX / length * kPlayerSpeed * deltaMs * 0.001f, 35.0f, kArenaWidth - 35);
            playerY = std::clamp(playerY + moveY / length * kPlayerSpeed * deltaMs * 0.001f, 150.0f, kArenaHeight - 35);
        }
        SetPlayerInput(m_player, length > 0, shoot);
    }
    AdvancePlayer(m_player, deltaMs);
    if (m_playerForceMs > 0 && !m_vitals.dead) {
        const float seconds = std::min(deltaMs, m_playerForceMs) * 0.001f;
        playerX = std::clamp(playerX + m_playerForceX * seconds, 35.0f, kArenaWidth - 35);
        playerY = std::clamp(playerY + m_playerForceY * seconds, 150.0f, kArenaHeight - 35);
        m_playerForceMs = std::max(0, m_playerForceMs - deltaMs);
    }
    for (auto &actor : enemies) {
        CEnemy &enemy = actor->model.enemy;
        EnemyCombat &state = enemy.combat;
        if (!state.enabled || state.removed) { continue; }
        SelectTarget(*actor);
        enemy.Update(deltaMs);
        for (std::uint32_t part = 0; part < enemy.GetPartCount(); ++part) {
            for (const GameObjectRef &sound : enemy.GetPart(part).controller.TakeSounds()) {
                m_effects.PlayMoveSound(sound);
            }
        }
        state.x = std::clamp(state.x, 35.0f, kArenaWidth - 35);
        state.y = std::clamp(state.y, 150.0f, kArenaHeight - 35);
        actor->contactTimer = std::max(0, actor->contactTimer - deltaMs);
        if (!state.dead && state.variables[16] != 1 && state.targetType != 2 && !m_vitals.dead &&
            state.variables[12] > 0 && state.variables[13] > 0 &&
            actor->contactTimer == 0 && std::hypot(state.x - playerX, state.y - playerY) < enemy.GetPart(0).radius + kPlayerCollisionRadius) {
            if (state.variables[17] > 0) { m_player.weapon->brother.ReceiveDamage(static_cast<float>(state.variables[17])); }
            const float angle = (state.facing - 90) * kRadians;
            m_playerForceX = std::cos(angle) * state.variables[12];
            m_playerForceY = std::sin(angle) * state.variables[12];
            m_playerForceMs = state.variables[13];
            actor->contactTimer = state.variables[13];
            enemy.TriggerEvent(8);
        }
        Actions(*actor);
    }
    float matrix[16];
    PlayerMatrix(matrix);
    m_effects.Update(m_player, matrix, facing, deltaMs);
    for (auto &actor : enemies) {
        Actions(*actor);
        EnemyCombat &state = actor->model.enemy.combat;
        if (state.dead) {
            actor->corpseMs += deltaMs;
            if (actor->corpseMs > kCorpseLimitMs) { state.removed = true; }
        }
    }
    FinishSpawns();
    // Accumulate completed actor statistics before erasing removed instances.
    for (std::size_t i = 0; i < enemies.size();) {
        EnemyCombat &state = enemies[i]->model.enemy.combat;
        if (state.hitFlash > 0) { lastDamage = state.lastDamage; }
        if (state.removed) {
            m_effects.RetireOwner(state.id);
            kills += state.deathCount;
            hits += state.hitCount;
            damageDealt += state.totalDamage;
            enemies.erase(enemies.begin() + i);
        } else { ++i; }
    }
}
