/** @file CombatScene.cpp
 * @brief Actor targeting, continuous projectile collision and scene lifecycle.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/CombatScene.h"
#include "gun_bros_re/gameplay/CFlock.h"
#include "gun_bros_re/gameplay/CombatGeometry.h"
#include "engine/core/CMatrix4d.h"
#include "engine/graphics/CMeshCamera.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/data/StoreCatalog.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#if GB_ENABLE_TESTS
#include "gameplay/PerformanceProbe.h"
#endif

namespace {
constexpr float kRadians = 3.14159265f / 180;
constexpr float kPlayerSpeed = 220;
constexpr float kSpawnDistance = 280;
constexpr int kCorpseLimitMs = 10000;
// CEffectLayer::AddTextEffect :66884 and TextEffect::Update :67086.
constexpr unsigned kTextEffectCapacity = 20;
constexpr unsigned kTextEffectLifetimeMs = 2000;
constexpr float kTextEffectRisePerSecond = 100;

bool Skipped(CombatId id, const std::vector<CombatId> &ids) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

using CombatGeometry::CircleFraction;
using CombatGeometry::EdgeFraction;

void Transform(const float *matrix, float localX, float localY, float localZ,
    float &x, float &y, float &z) {
    x = matrix[0] * localX + matrix[1] * localY + matrix[2] * localZ + matrix[3];
    y = matrix[4] * localX + matrix[5] * localY + matrix[6] * localZ + matrix[7];
    z = matrix[8] * localX + matrix[9] * localY + matrix[10] * localZ + matrix[11];
}
}

bool LoadInitialPlayerHealth(CResTOCManager &toc, PackTables &tables, float &health) {
    CPlayerProgress::Template progress;
    if (!LoadPlayerProgress(toc, tables, progress)) { return false; }
    // Progress is indexed by the displayed level. Entry zero is a sentinel;
    // a new player starts at level one.
    health = static_cast<float>(progress.health[1]);
    std::printf("[combat] initial player health %.0f from PLAYER_PROGRESS\n", health);
    return true;
}

void CombatScene::SetPlayerProgress(CPlayerProgress *progress) {
    m_progress = progress;
    if (progress != nullptr) { m_vitals.maximum = progress->GetHealth(); }
}

void CombatScene::AddExperience(unsigned amount) {
    if (m_progress == nullptr) { return; }
    const float fraction = m_vitals.health / m_vitals.maximum;
    if (!m_progress->AddExperience(amount)) { return; }
    // CPlayer::AddExperience (:101250) preserves the current health fraction.
    m_vitals.maximum = m_progress->GetHealth();
    m_vitals.health = m_vitals.maximum * fraction;
    if (m_brother != nullptr) {
        const float brotherFraction = m_brother->vitals.health / m_brother->vitals.maximum;
        m_brother->vitals.maximum = m_progress->GetHealth();
        m_brother->vitals.health = m_brother->vitals.maximum * brotherFraction;
    }
    std::printf("[progress] level-up=%u health=%.1f/%.1f xp=%llu\n",
        m_progress->GetLevel(), m_vitals.health, m_vitals.maximum, m_progress->GetExperience());
}

void CombatScene::RewardEnemy(const CombatEnemy &actor) {
    if (m_progress == nullptr || m_level == nullptr) { return; }
    // CLevel::OnEnemyKilled (:119609): offset 912 is XP, 876 is Xplodium.
    // Multiplier attribute 2 is Xplodium; attribute 3 is XP. Both round UP.
    const GameObjectRef &ref = actor.model.enemy.combat.templateRef;
    const unsigned experience = static_cast<unsigned>(std::ceil(actor.data->experienceReward *
        m_level->GetEnemyMultiplier(ref, 3) * PlayerArmorMultiplier(m_player, 3)));
    const bool playerKill = actor.model.enemy.combat.pendingHit.owner == kPlayerCombatId;
    bool counted = false;
    for (auto &entry : m_casualties) {
        if (entry.resource.packHash == ref.packHash && entry.resource.localIndex == ref.localIndex) {
            ++entry.count; counted = true; break;
        }
    }
    if (!counted) { m_casualties.push_back({ref, 1, actor.data->owner}); }
    const CombatHit &hit = actor.model.enemy.combat.pendingHit;
    if (playerKill && !hit.weapon.IsNull()) {
        bool credited = false;
        for (auto &entry : m_weaponProgress) {
            if (entry.resource.packHash == hit.weapon.packHash && entry.resource.localIndex == hit.weapon.localIndex) {
                entry.experience += experience; credited = true; break;
            }
        }
        if (!credited) { m_weaponProgress.push_back({hit.weapon, experience, hit.weaponMasteryLimit}); }
    }
    if (m_horde) {
        // CLevel::OnEnemyKilled :119655-119802: bro kills score once, player
        // kills twice at the current streak multiplier and advance that streak.
        std::uint64_t points = static_cast<std::uint64_t>(experience) * (m_killStreak + 1);
        if (playerKill) { points *= 2; ++m_killStreak; }
        // CLevel::GetBestKillStreak retains the session maximum across hits.
        m_bestKillStreak = std::max(m_bestKillStreak, m_killStreak);
        m_score = static_cast<unsigned>(std::min<std::uint64_t>(3000000000ULL, m_score + points));
        if (playerKill) { AddExperience(experience); }
    } else { AddExperience(experience); }
    // CLevel::OnEnemyKilled VA0x950AC passes the awarded XP to the STR
    // formatter, then projects enemy position (+828/+832) once. The text
    // survives the corpse and does not follow later camera/player movement.
    if (m_experienceTexts.size() < kTextEffectCapacity) {
        const auto &enemy = actor.model.enemy.combat;
        ExperienceText text;
        text.amount = experience;
        text.x = static_cast<float>(static_cast<int>((enemy.x - m_textViewX) * m_textScaleX));
        text.y = static_cast<float>(static_cast<int>((enemy.y - m_textViewY) * m_textScaleY));
        m_experienceTexts.push_back(text);
    }
    if (actor.model.enemy.combat.pendingHit.owner == kPlayerCombatId) {
        const unsigned xplodium = static_cast<unsigned>(std::ceil(actor.data->xplodiumReward *
            m_level->GetEnemyMultiplier(ref, 2) * PlayerArmorMultiplier(m_player, 4)));
        AddXplodium(xplodium);
    }
}

void CombatScene::UpdateExperienceTexts(int deltaMs) {
    if (deltaMs <= 0) { return; }
    for (auto text = m_experienceTexts.begin(); text != m_experienceTexts.end();) {
        text->elapsedMs += static_cast<unsigned>(deltaMs);
        if (text->elapsedMs >= kTextEffectLifetimeMs) {
            text = m_experienceTexts.erase(text);
            continue;
        }
        text->alpha = 1 - static_cast<float>(text->elapsedMs) / kTextEffectLifetimeMs;
        text->y -= kTextEffectRisePerSecond * deltaMs / 1000.0f;
        ++text;
    }
}

void CombatScene::AddHealth(unsigned amount) {
    if (!m_vitals.dead) { m_vitals.health = std::min(m_vitals.maximum, m_vitals.health + amount); }
}

void CombatScene::AddXplodium(unsigned amount) {
    // CPlayer::AddXplodium :101116 keeps hundredths between individual grants.
    // Rounding every small pickup separately loses the later-wave bonus.
    unsigned percent = 100;
    if (m_level != nullptr) { percent = static_cast<unsigned>(std::max(0, m_level->GetXplodiumMultiplierPercent())); }
    const std::uint64_t scaled = static_cast<std::uint64_t>(amount) * percent + m_xplodiumRemainder;
    m_xplodium += scaled / 100;
    m_xplodiumRemainder = static_cast<unsigned>(scaled % 100);
}

unsigned CombatScene::GetTotalKills() const {
    unsigned total = kills;
    // Dead actors can retain their original corpse animation for ten seconds.
    // Saving must include them before their render objects are retired.
    for (const auto &actor : enemies) { total += actor->model.enemy.combat.deathCount; }
    return total;
}

float CombatScene::GetEnemyTimeScale() const {
    if (m_level == nullptr) { return 1; }
    return m_level->GetObjectTimeScale();
}

bool CombatScene::TouchesPickup(float x, float y) const {
    // CPickup::Bind :99937 sets its fixed collision radius to 10.
    // CBrother::TestCollisions :138154 excludes GetBrotherType() == 1;
    // CBrotherAI::GetBrotherType :139638 returns 1 for the AI companion.
    return !m_vitals.dead && CircleFraction(playerX, playerY,
        m_previousPlayerX - playerX, m_previousPlayerY - playerY, x, y, m_playerRadius + 10) <= 1;
}

void CombatScene::OnWaveCleared(unsigned perfectRewardPercent) {
    if (m_player.weapon != nullptr) { m_player.weapon->brother.OnWaveCleared(); }
    if (m_brotherModel != nullptr) { m_brotherModel->weapon->brother.OnWaveCleared(); }
    m_lastWaveBonus = 0;
    m_wavePerfectResults.push_back(m_vitals.hits == m_waveHits);
    // CLevel::OnWaveCleared (:116980): integer percentage, at least one.
    // Count accepted damage contacts even in the invincible test pilot.
    if (m_vitals.hits == m_waveHits) {
        m_lastWaveBonus = std::max<std::uint64_t>(1,
            (m_xplodium - m_waveXplodium) * perfectRewardPercent / 100);
        const std::uint64_t before = m_xplodium;
        AddXplodium(static_cast<unsigned>(m_lastWaveBonus));
        m_lastWaveBonus = m_xplodium - before;
        ++m_perfectWaves;
    }
    ++m_clearedWaves;
    std::printf("[progress] wave reward=%llu percent=%u hits=%u perfect=%u/%u\n",
        m_lastWaveBonus, perfectRewardPercent, m_vitals.hits - m_waveHits,
        m_perfectWaves, m_clearedWaves);
    // The next wave excludes this wave's bonus from its reward basis.
    m_waveXplodium = m_xplodium;
    m_waveHits = m_vitals.hits;
}

CombatScene::CombatScene(PackTables &tables, const CShaderProgram &program,
    const std::vector<EnemyTemplateData> &catalog, PlayerModel &player,
    PlayerVitals &vitals, WeaponEffects &effects, float playerGameScale)
    : m_tables(tables), m_program(program), m_catalog(catalog), m_player(player),
      m_vitals(vitals), m_effects(effects), m_playerGameScale(playerGameScale) {
    m_effects.SetCombatWorld(this);
}

void CombatScene::SetMap(CMap &map, const CCollisionData &collision, WeaponCollision &weaponCollision,
    float cameraScale, float playerRadius) {
    m_map = &map;
    m_collision = &collision;
    m_weaponCollision = &weaponCollision;
    m_cameraScale = cameraScale;
    m_playerRadius = playerRadius;
    const MapRectangle bounds = map.GetCameraExtent();
    m_left = bounds.x + playerRadius;
    m_top = bounds.y + playerRadius;
    m_right = bounds.x + bounds.width - playerRadius;
    m_bottom = bounds.y + bounds.height - playerRadius;
}

void CombatScene::ResolveMovement(float previousX, float previousY, float &x, float &y, float radius, bool player) const {
    // CEnemy::UpdatePathFinder (:70142) advances on its navigation path;
    // TestCollisions (:73079) tests bullets/player, not the player's wall
    // circle resolver. Applying that resolver again can block authored portals.
    if (m_collision != nullptr && player) {
        const CollisionPoint position = m_collision->ResolveCircleMovement(
            CollisionPoint(previousX, previousY), CollisionPoint(x - previousX, y - previousY), radius);
        x = position.x;
        y = position.y;
    }
    // Camera bounds constrain the player. Authored enemy spawn nodes can be
    // outside the visible rectangle and must remain there until they enter.
    if (player || m_map == nullptr) {
        x = std::clamp(x, m_left, m_right);
        y = std::clamp(y, m_top, m_bottom);
    }
}

bool CombatScene::HasClearPath(float x, float y, float targetX, float targetY, float radius) const {
    if (m_collision == nullptr) { return true; }
    const auto &vertices = m_collision->GetVertices();
    for (const CollisionEdge &edge : m_collision->GetEdges()) {
        if (!edge.enabled) { continue; }
        if (EdgeFraction(x, y, targetX - x, targetY - y, vertices[edge.firstVertex], vertices[edge.secondVertex], radius) < 1) {
            return false;
        }
    }
    return true;
}

bool CombatScene::CanWalkTo(float x, float y, float targetX, float targetY) const {
    const float distance = std::hypot(targetX - x, targetY - y);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / 4)));
    const float dx = (targetX - x) / steps, dy = (targetY - y) / steps;
    for (int step = 0; step < steps; ++step) {
        const float previousX = x, previousY = y;
        x += dx; y += dy;
        ResolveMovement(previousX, previousY, x, y, m_playerRadius);
    }
    return std::hypot(targetX - x, targetY - y) < 1;
}

void CombatScene::ResolveBrotherForce(float previousX, float previousY, float &x, float &y) {
    ResolveMovement(previousX, previousY, x, y, m_playerRadius);
}

void CombatScene::UpdateNavigation(CombatEnemy &actor, int deltaMs) {
#if GB_ENABLE_TESTS
    PerformanceProbe::Scope timing(PerformanceProbe::counters.navigationMs);
#endif
    EnemyCombat &state = actor.model.enemy.combat;
    if (m_map == nullptr || state.behaviour != 0 || state.dead) {
        state.hasNavigationTarget = false;
        return;
    }
    actor.navigationTimer -= deltaMs;
    if (actor.navigationTimer > 0 && state.hasNavigationTarget &&
        std::hypot(state.navigationX - state.x, state.navigationY - state.y) > 10) { return; }
    actor.navigationTimer = 240;
    state.hasNavigationTarget = false;
    const float radius = actor.model.enemy.GetPart(0).radius * m_cameraScale;
    if (HasClearPath(state.x, state.y, state.targetX, state.targetY, radius)) { return; }
    ILayerPath *path = m_map->GetPathLayer(m_pathLayer);
    if (path == nullptr) { return; }
    const int start = path->FindNode(state.x, state.y);
    const int destination = path->FindNode(state.targetX, state.targetY);
    const auto &nodes = path->GetNodes();
    if (start < 0 || destination < 0) { return; }
    int next = path->FindNext(start, destination);
    if (next < 0) { return; }
    const int adjacent = next;
    // Skip centres only when the actual collision sweep has a clear corridor.
    for (int lookAhead = 0; lookAhead < 8 && next != destination; ++lookAhead) {
        const int farther = path->FindNext(next, destination);
        if (farther < 0 || farther == next ||
            !HasClearPath(state.x, state.y, nodes[farther].x, nodes[farther].y, radius)) { break; }
        next = farther;
    }
    state.hasNavigationTarget = true;
    state.navigationX = nodes[next].x;
    state.navigationY = nodes[next].y;
    if (next == adjacent && next != start) {
        path->GetConnectionPoint(start, next, state.navigationX, state.navigationY);
    }
}

void CombatScene::Reset() {
    m_flockEnemies.clear();
    m_experienceTexts.clear();
    m_weaponProgress.clear();
    m_casualties.clear();
    m_xplodiumRemainder = 0;
    m_hasViewCenter = false;
    m_score = 0;
    m_killStreak = 0;
    m_bestKillStreak = 0;
    m_effects.Clear();
    enemies.clear();
    deaths.clear();
    teleports.clear();
    levelEvents.clear();
    pickupSpawns.clear();
    m_pendingSpawns.clear();
    m_vitals.Reset();
    m_player.powerups = {};
    m_autoAim.Reset();
    if (m_brotherModel != nullptr) { m_brotherModel->powerups = {}; }
    m_waveXplodium = m_xplodium;
    m_waveHits = 0;
    m_lastWaveBonus = 0;
    m_perfectWaves = 0;
    m_clearedWaves = 0;
    m_wavePerfectResults.clear();
    if (m_player.weapon != nullptr) {
        // Reset the script and gun state as well as health. The replacement
        // copies templates before retiring the old equipment.
        if (EquipPlayerWeapon(m_tables, m_player.weapon->playerScript,
            m_player.ActiveWeapon().data, "arena reset", m_player)) {
            CreatePlayerBuffers(m_player, m_program);
        }
    }
    m_playerForceMs = 0;
    if (m_brotherModel != nullptr) {
        m_brother->Reset(playerX, playerY, facing);
        if (EquipPlayerWeapon(m_tables, m_brotherModel->weapon->playerScript,
            m_brotherModel->weapon->data, "brother reset", *m_brotherModel)) {
            CreatePlayerBuffers(*m_brotherModel, m_program);
            m_brotherWeaponSlot = 0;
        }
    }
    playerX = 600;
    playerY = 650;
    m_previousPlayerX = playerX;
    m_previousPlayerY = playerY;
    m_player.weapon->brother.SetLevelContext(m_level);
    facing = 0;
    damageDealt = 0;
    lastDamage = 0;
    hits = 0;
    kills = 0;
    spawned = 0;
    invalidSpawns = 0;
}

bool CombatScene::PreloadEnemies(const RequirementList &requirements, const CScript &levelScript) {
    // CMap::GetRequirements :92483 and RequirementList::Add :191754/191802.
    // Only immutable meshes/textures enter this cache; no Flow export runs.
    std::vector<ScriptResourceRef> pending = levelScript.GetResources();
    for (const auto &entry : requirements.objects) {
        ScriptResourceRef ref;
        ref.packHash = entry.object.packHash;
        ref.sectionOrType = entry.objectType;
        ref.resourceId = entry.object.localIndex;
        pending.push_back(ref);
    }
    unsigned loaded = 0;
    for (std::size_t index = 0; index < pending.size(); ++index) {
        const auto ref = pending[index];
        if (ref.sectionOrType != static_cast<unsigned>(GameSection::Enemy) - 1 || ref.resourceId == 255) { continue; }
        const std::uint64_t key = (static_cast<std::uint64_t>(ref.packHash) << 32) | ref.resourceId;
        if (m_enemyModelCache.entries.count(key) != 0) { continue; }
        const EnemyTemplateData *data = nullptr;
        for (const auto &entry : m_catalog) {
            if (entry.packHash == ref.packHash && entry.ordinal == ref.resourceId) { data = &entry; break; }
        }
        if (data == nullptr || !PreloadEnemyModel(m_tables, *data, m_program, m_enemyModelCache)) {
            std::printf("[preload] enemy=%08x:%u failed\n", ref.packHash, ref.resourceId);
            return false;
        }
        ++loaded;
        const auto &dependencies = data->script.GetResources();
        pending.insert(pending.end(), dependencies.begin(), dependencies.end());
    }
    // Runtime counters measure misses during gameplay, separately from preload.
    m_enemyModelCache.hits = 0;
    m_enemyModelCache.misses = 0;
    std::printf("[preload] enemy-templates=%u map-references=%zu\n", loaded, requirements.objects.size());
    return true;
}

CombatEnemy *CombatScene::Spawn(std::size_t entry, float x, float y) {
#if GB_ENABLE_TESTS
    PerformanceProbe::Scope timing(PerformanceProbe::counters.spawnMs);
    if (PerformanceProbe::enabled) { ++PerformanceProbe::counters.spawns; }
#endif
    if (entry >= m_catalog.size()) { return nullptr; }
    std::unique_ptr<CombatEnemy> actor(new CombatEnemy());
    actor->data = &m_catalog[entry];
    actor->model.enemy.SetLevelContext(m_level);
    EnemyCombat &state = actor->model.enemy.combat;
    state.templateRef.packHash = actor->data->packHash;
    state.templateRef.localIndex = static_cast<std::uint8_t>(actor->data->ordinal);
    state.enabled = actor->data->script.IsPresent();
    state.id = m_nextId++;
    state.randomState = static_cast<std::uint32_t>(state.id * 7919);
    actor->model.enemy.SetRandomSeed(state.randomState);
    state.x = std::clamp(x, 70.0f, kArenaWidth - 70);
    state.y = std::clamp(y, 150.0f, kArenaHeight - 70);
    if (m_map != nullptr) {
        state.x = x;
        state.y = y;
    }
    state.previousX = state.x;
    state.previousY = state.y;
    if (!LoadEnemyModel(m_tables, *actor->data, true, &m_program, EnemySpawnMode::Level, actor->model, &m_enemyModelCache)) {
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
    const float scale = PlayerModelWorldScale(m_player, m_playerGameScale, m_cameraScale);
    BuildPlayerGameMatrix(identity, playerX, playerY, scale, facing, matrix);
}

void CombatScene::SetBrother(PlayerModel *model, CBrotherAI *brother) {
    m_brotherModel = model;
    m_brother = brother;
}

void CombatScene::SetBrotherWeapons(const CScript &script, const CGun::Template &pistol, const CGun::Template &rifle) {
    m_brotherScript = &script;
    m_brotherWeapons[0] = &pistol;
    m_brotherWeapons[1] = &rifle;
    m_brotherWeaponSlot = 0;
}

bool CombatScene::RequestBrotherWeaponSwap() {
    if (m_brother == nullptr || m_brotherModel == nullptr || m_brotherScript == nullptr || m_brother->vitals.dead) { return false; }
    return m_brotherModel->weapon->brother.OnSwapGun();
}

void CombatScene::ResolvePlayerMovement(float previousX, float previousY, float &x, float &y) const {
    // CPlayer::Move :100691-100862: bounds, enemy bodies, then map/prop edges.
    x = std::clamp(x, m_left, m_right);
    y = std::clamp(y, m_top, m_bottom);
    const CBrother &player = m_player.weapon->brother;
    if (!player.CanPassEnemies()) {
        for (const auto &actor : enemies) {
            const CEnemy &enemy = actor->model.enemy;
            if (!enemy.CanCollideWithPlayer()) { continue; }
            const EnemyCombat &state = enemy.combat;
            float offsetX, offsetY;
            EnemyRotationOffset(enemy, actor->data->gameScale, offsetX, offsetY);
            const CollisionPoint enemyPrevious(state.previousX + offsetX, state.previousY + offsetY);
            const CollisionPoint enemyCurrent(state.x + offsetX, state.y + offsetY);
            const float moveX = x - previousX, moveY = y - previousY;
            const float towardEnemy = moveX * (enemyPrevious.x - x) + moveY * (enemyPrevious.y - y);
            if (towardEnemy <= 0) { continue; }
            float fraction = 0;
            // Native 0.8 is a body allowance, not a resource radius or model scale.
            if (CombatGeometry::CircleCircle({previousX, previousY}, {x, y}, player.GetRadius(),
                enemyPrevious, enemyCurrent, enemy.GetPart(0).radius * 0.8f, fraction)) {
                x = previousX + moveX * fraction;
                y = previousY + moveY * fraction;
            }
        }
    }
    ResolveMovement(previousX, previousY, x, y, m_playerRadius);
}

bool CombatScene::SwapBrotherWeapon() {
    if (m_brotherScript == nullptr || m_brotherModel == nullptr || m_brother->vitals.dead) { return true; }
    const unsigned next = 1 - m_brotherWeaponSlot;
    m_effects.RetireOwner(kBrotherCombatId);
    // CBrother native 3 changes the gun while the same body/script continues
    // the swap sequence. Reuse the two stable banks used by the local player.
    if (m_brotherModel->uiOtherWeapon == nullptr) {
        if (!PreparePlayerUIWeapon(m_tables, *m_brotherWeapons[1], "AI brother swap", *m_brotherModel) ||
            !CreatePlayerBuffers(*m_brotherModel, m_program)) { return false; }
    }
    SelectPlayerUIWeapon(*m_brotherModel, next == 0);
    m_brotherWeaponSlot = next;
    std::printf("[brother] weapon-slot=%u\n", next);
    return true;
}

void CombatScene::ResetBrotherPosition(float x, float y, float facingDegrees) {
    if (m_brother == nullptr) { return; }
    // The host used to reset both actors onto the same point. Pick an open
    // nearby position through the actual collision path, including on restart.
    constexpr float directions[][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1},
        {-0.7071f, -0.7071f}, {0.7071f, -0.7071f}, {-0.7071f, 0.7071f}, {0.7071f, 0.7071f}};
    const float distance = m_playerRadius * 4;
    for (const auto &direction : directions) {
        const float targetX = x + direction[0] * distance;
        const float targetY = y + direction[1] * distance;
        if (!CanWalkTo(x, y, targetX, targetY)) { continue; }
        m_brother->Reset(targetX, targetY, facingDegrees);
        return;
    }
    // Extremely tight authored spawn areas still have a deterministic fallback.
    m_brother->Reset(x, y, facingDegrees);
}

void CombatScene::BrotherMatrix(float *matrix) const {
    float identity[16];
    Matrix4dIdentity(identity);
    const float scale = PlayerModelWorldScale(*m_brotherModel, m_playerGameScale, m_cameraScale);
    BuildPlayerGameMatrix(identity, m_brother->x, m_brother->y, scale, m_brother->facing, matrix);
}

CombatId CombatScene::FindBrotherTarget(float x, float y, float radius) {
    CombatId nearest = 0;
    for (const auto &actor : enemies) {
        float targetX = 0;
        float targetY = 0;
        const CombatId id = actor->model.enemy.combat.id;
        if (!GetBrotherTarget(id, targetX, targetY)) { continue; }
        const float distance = std::hypot(targetX - x, targetY - y);
        if (distance < radius) { radius = distance; nearest = id; }
    }
    return nearest;
}

bool CombatScene::GetBrotherTarget(CombatId id, float &x, float &y) {
    CombatEnemy *actor = Find(id);
    if (actor == nullptr) { return false; }
    const CEnemy &enemy = actor->model.enemy;
    if (!enemy.combat.enabled || !enemy.combat.targetable ||
        !enemy.CanReceiveProjectile(0, kBrotherCombatId)) { return false; }
    x = enemy.combat.x;
    y = enemy.combat.y;
    return true;
}

bool CombatScene::GetBrotherWaypoint(float x, float y, float targetX, float targetY,
    float &waypointX, float &waypointY) {
    waypointX = targetX;
    waypointY = targetY;
    if (HasClearPath(x, y, targetX, targetY, m_playerRadius)) { return true; }
    if (m_map == nullptr) { return false; }
    ILayerPath *path = m_map->GetPathLayer(m_pathLayer);
    if (path == nullptr) { return false; }
    const int start = path->FindNode(x, y);
    const int destination = path->FindNode(targetX, targetY);
    if (start < 0 || destination < 0) { return false; }
    const int next = path->FindNext(start, destination);
    if (next < 0) { return false; }
    waypointX = path->GetNodes()[next].x;
    waypointY = path->GetNodes()[next].y;
    if (next != start) { path->GetConnectionPoint(start, next, waypointX, waypointY); }
    if (std::hypot(waypointX - x, waypointY - y) < 5) {
        waypointX = path->GetNodes()[next].x;
        waypointY = path->GetNodes()[next].y;
    }
    return true;
}

void CombatScene::EnemyMatrix(const CombatEnemy &actor, float *matrix) const {
    float identity[16];
    Matrix4dIdentity(identity);
    const EnemyCombat &state = actor.model.enemy.combat;
    const float scale = EnemyModelWorldScale(actor.model, actor.data->gameScale, m_cameraScale) * state.scaleFactor;
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
            EnemyModelWorldScale(actor.model, actor.data->gameScale, m_cameraScale) * state.scaleFactor, 0, base);
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
    if (id == kPlayerCombatId && part < 0) {
        x = playerX; y = playerY; z = 0; direction = facing - 90;
        return !m_vitals.dead;
    }
    if (id == kBrotherCombatId && m_brotherModel != nullptr) {
        if (part < 0) {
            x = m_brother->x; y = m_brother->y; z = 0; direction = m_brother->facing - 90;
            return !m_brother->vitals.dead;
        }
        if (m_brother->vitals.dead || !m_brotherModel->weapon->gun.IsShooting()) { return false; }
        MeshBoneTransform muzzle;
        if (!GetPlayerMuzzle(*m_brotherModel, part, node, muzzle)) { return false; }
        float matrix[16];
        BrotherMatrix(matrix);
        Transform(matrix, muzzle.posX, muzzle.posY, muzzle.posZ, x, y, z);
        direction = m_brother->facing - 90;
        return true;
    }
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
    EnemyCollisionCircle(enemy, actor.data->gameScale, part, x, y, radius);
}

CombatTrace CombatScene::Trace(const CombatHit &hit, float x, float y, float dx, float dy,
    float radius, const std::vector<CombatId> &skipTargets) {
    CombatTrace result;
    float nearest = 2;
    if (m_props != nullptr) {
        result = m_props->Trace(hit, x, y, dx, dy, radius, skipTargets);
        if (result.target != 0) { nearest = result.fraction; }
    }
    if (hit.ownerType == 1 && hit.owner != kPlayerCombatId && !m_vitals.dead && !Skipped(kPlayerCombatId, skipTargets)) {
        float moveX = playerX - m_previousPlayerX, moveY = playerY - m_previousPlayerY;
        if ((hit.flags & 0x100) != 0) { moveX = 0; moveY = 0; }
        const float fraction = CircleFraction(x, y, dx - moveX, dy - moveY,
            playerX - moveX, playerY - moveY, m_playerRadius + radius);
        if (fraction <= 1 && fraction < nearest) {
            nearest = fraction;
            result.target = kPlayerCombatId; result.fraction = nearest;
            result.normalX = x + dx * nearest - playerX;
            result.normalY = y + dy * nearest - playerY;
        }
    }
    if (hit.ownerType == 1 && m_brother != nullptr && !m_brother->vitals.dead && !Skipped(kBrotherCombatId, skipTargets)) {
        float moveX = m_brother->x - m_brother->previousX;
        float moveY = m_brother->y - m_brother->previousY;
        if ((hit.flags & 0x100) != 0) { moveX = 0; moveY = 0; }
        const float fraction = CircleFraction(x, y, dx - moveX, dy - moveY,
            m_brother->x - moveX, m_brother->y - moveY, m_playerRadius + radius);
        if (fraction <= 1 && fraction < nearest) {
            nearest = fraction;
            result = {kBrotherCombatId, fraction, -1, -1,
                x + dx * fraction - m_brother->x, y + dy * fraction - m_brother->y};
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

std::vector<CombatScene::HealthBar> CombatScene::EnemyHealthBars(float viewportScale) const {
    // CLevel::DrawEnemyHealthBars :120454: native 30x4 and inset 1.
    // BIG controls visibility and the larger-bar flag; it has no size table.
    // Correction: mem+311032 is LEVEL variable 4 (the script's boss-wave
    // flag), NOT the revolution. Camera mem+0 is min(width/480,height/320),
    // NOT SnapScale/GetScale. These sizes are already physical screen pixels.
    float waveScale = 1;
    if (m_level != nullptr && m_level->HasLargeEnemyHealthBars()) { waveScale = 2; }
    const float width = int(30 * viewportScale * waveScale);
    const float height = int(4 * viewportScale * waveScale);
    const float border = int(viewportScale * waveScale);
    std::vector<HealthBar> bars;
    for (const auto &actor : enemies) {
        const CEnemy &enemy = actor->model.enemy;
        const EnemyCombat &state = enemy.combat;
        if (!state.enabled || state.removed || state.dead || state.health <= 0 ||
            state.maxHealth <= 0 || state.variables[15] == 0) { continue; }
        const CMesh *body = enemy.GetPart(0).controller.GetAnimation().GetMesh();
        if (body == nullptr) { continue; }
        const float scale = body->GetBounds().inverseExtent * actor->data->gameScale;
        bool first = true;
        float left = 0, right = 0, top = 0;
        for (unsigned part = 0; part < enemy.GetPartCount(); ++part) {
            const CMesh *mesh = enemy.GetPart(part).controller.GetAnimation().GetMesh();
            if (mesh == nullptr) { continue; }
            const MeshBounds &bounds = mesh->GetBounds();
            const int extent = int(std::max(std::abs(bounds.maxX - bounds.minX),
                std::abs(bounds.maxY - bounds.minY)) * scale);
            if (extent == 0) { continue; }
            const float partLeft = int(bounds.centerX) - extent / 2;
            const float partTop = int(bounds.centerY) - extent / 2;
            if (first) { left = partLeft; right = partLeft + extent; top = partTop; first = false; }
            else { left = std::min(left, partLeft); right = std::max(right, partLeft + extent); top = std::min(top, partTop); }
        }
        if (first) { continue; }
        // GetBounds :67485 adds a native 20-unit margin. The damage pulse is
        // cos((remainingMs/1000+1)*pi/2)*-50 added to original red 0xC80000.
        const float bright = std::cos((state.healthBarFlashMs * 0.001f + 1) * 3.14159265f * 0.5f) * -50;
        // Preserve the world-space top centre until projection. Width is in
        // screen pixels and must not become a camera-scaled world offset.
        bars.push_back({state.x + (left + right) * 0.5f,
            state.y + top - 20, width, height, border,
            std::min(1.0f, state.health / state.maxHealth), (200 + bright) / 255});
    }
    return bars;
}

bool CombatScene::Suicide() {
    m_player.weapon->brother.SetLevelContext(m_level);
    return m_player.weapon->brother.StartDeath();
}

HitResult CombatScene::ApplyHit(CombatId target, const CombatHit &hit) {
    if (target == kBrotherCombatId && m_brotherModel != nullptr) {
        if (hit.ownerType != 1) { return HitResult::Ignored; }
        m_brotherModel->weapon->brother.SetLevelContext(m_level);
        const float reduction = PlayerArmorMultiplier(*m_brotherModel, 0) - 1;
        float damage = hit.damage;
        if (hit.splash && hit.percentDamage) { damage *= m_brother->vitals.maximum * 0.01f; }
        return m_brotherModel->weapon->brother.ReceiveDamage(std::max(0.0f, damage * (1 - reduction)));
    }
    if (target == kPlayerCombatId) {
        if (hit.ownerType != 1 || m_player.weapon == nullptr) { return HitResult::Ignored; }
        m_player.weapon->brother.SetLevelContext(m_level);
        // CBrother::Damage (:136667): add slot percentages, then reduce the
        // incoming amount. Defence does not increase the player's max health.
        const float reduction = PlayerArmorMultiplier(m_player, 0) - 1.0f;
        // CBrother::OnSplashDamage :135359 interprets native 23 as a percent
        // of maximum health before the ordinary armor / frenzy reductions.
        float damage = hit.damage;
        if (hit.splash && hit.percentDamage) { damage *= m_vitals.maximum * 0.01f; }
        damage = std::max(0.0f, damage * (1.0f - reduction));
        const unsigned hitsBefore = m_vitals.hits;
        const HitResult result = m_player.weapon->brother.ReceiveDamage(damage);
        // OnPlayerDamaged :115914 resets the streak on accepted damage only.
        if (m_vitals.hits != hitsBefore) { m_killStreak = 0; }
        return result;
    }
    CombatHit adjusted = hit;
    if (hit.applyArmorAttack && hit.owner == kPlayerCombatId) {
        adjusted.damage *= PlayerArmorMultiplier(m_player, 1);
    }
    if (hit.applyArmorAttack && hit.owner == kBrotherCombatId && m_brotherModel != nullptr) {
        adjusted.damage *= PlayerArmorMultiplier(*m_brotherModel, 1);
    }
    CombatEnemy *actor = Find(target);
    if (actor == nullptr) {
        if (m_props != nullptr) { return m_props->ApplyHit(target, adjusted); }
        return HitResult::Ignored;
    }
    return actor->model.enemy.ReceiveHit(adjusted);
}

float CombatScene::GetDamageMultiplier(CombatId owner, float fallback) const {
    if (m_level == nullptr) { return fallback; }
    for (const auto &actor : enemies) {
        if (actor->model.enemy.combat.id == owner) {
            return m_level->GetEnemyMultiplier(actor->model.enemy.combat.templateRef, 0);
        }
    }
    return fallback;
}

float CombatScene::GetProjectilePowerupMultiplier(CombatId owner) const {
    if (owner == kPlayerCombatId && m_player.weapon) { return m_player.weapon->brother.GetProjectilePowerupMultiplier(); }
    if (owner == kBrotherCombatId && m_brotherModel != nullptr && m_brotherModel->weapon) {
        return m_brotherModel->weapon->brother.GetProjectilePowerupMultiplier();
    }
    return 1;
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
    if (m_props != nullptr) { m_props->Splash(hit, radius); }
    std::vector<CombatId> targets;
    if (hit.ownerType == 1 && !m_vitals.dead) { targets.push_back(kPlayerCombatId); }
    if (hit.ownerType == 1 && m_brother != nullptr && !m_brother->vitals.dead) { targets.push_back(kBrotherCombatId); }
    for (const auto &actor : enemies) {
        if (actor->model.enemy.CanReceiveProjectile(hit.ownerType, hit.owner)) { targets.push_back(actor->model.enemy.combat.id); }
    }
    for (CombatId id : targets) {
        float x = playerX, y = playerY;
        if (id == kBrotherCombatId) { x = m_brother->x; y = m_brother->y; }
        CombatEnemy *actor = Find(id);
        if (actor != nullptr) { x = actor->model.enemy.combat.x; y = actor->model.enemy.combat.y; }
        const float dx = x - hit.x, dy = y - hit.y;
        const float distance = std::hypot(dx, dy);
        // CLevel includes the target's collision radius in the blast test.
        // Testing only its centre drops explosions at the surface of big units.
        float targetRadius = m_playerRadius;
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
        if (force > 0 && forceMs > 0 && distance > 0 && actor == nullptr) {
            // CBrother::OnSplashDamage :135359 uses SetForce over time.
            // CEnemy::OnSplashDamage :67945 does not apply positional force.
            // In particular, a full-size map must never clamp to Arena bounds.
            ApplyBrotherForce(id, dx / distance * force, dy / distance * force, forceMs);
        }
    }
}

void CombatScene::ApplyBrotherForce(CombatId target, float x, float y, int durationMs) {
    if (target == kBrotherCombatId && m_brotherModel != nullptr) {
        if (m_brotherModel->weapon->brother.BeginKnockback(durationMs)) {
            m_brother->SetForce(x, y, durationMs);
        }
    } else if (target == kPlayerCombatId) {
        if (m_player.weapon != nullptr && m_player.weapon->brother.BeginKnockback(durationMs)) {
            m_playerForceX = x;
            m_playerForceY = y;
            m_playerForceMs = durationMs;
        }
    }
}

void CombatScene::SpawnFromProjectile(const GameObjectRef &resource, const CombatHit &hit) {
    for (std::size_t i = 0; i < m_catalog.size(); ++i) {
        if (m_catalog[i].packHash == resource.packHash && m_catalog[i].ordinal == resource.localIndex) {
            m_pendingSpawns.push_back({i, hit.x, hit.y, hit.spawnObjectId, hit.forceSpawn});
            return;
        }
    }
    ++invalidSpawns;
    std::printf("[combat] missing spawn resource %08x:%u\n", resource.packHash, resource.localIndex);
}

void CombatScene::FinishSpawns() {
    std::vector<PendingSpawn> pending;
    pending.swap(m_pendingSpawns);
    for (const PendingSpawn &spawn : pending) {
        CombatEnemy *actor = Spawn(spawn.entry, spawn.x, spawn.y);
        if (actor != nullptr) { actor->objectId = spawn.objectId; }
    }
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
        if (action.kind == EnemyAction::Kind::LevelEvent) {
            levelEvents.push_back(static_cast<std::uint8_t>(action.slot));
        } else if (action.kind == EnemyAction::Kind::Teleported) {
            teleports.push_back({actor.objectId, state.templateRef});
        } else if (action.kind == EnemyAction::Kind::Shake) {
            if (m_map != nullptr) { m_map->GetCamera().Shake(action.durationMs); }
        } else if (action.kind == EnemyAction::Kind::TurretActive) {
            // CEnemy native 71 :72744 selects the local player when offline.
            m_player.weapon->brother.SetTurretIsActive(action.slot != 0);
            std::printf("[turret] actor=%llu active=%d\n", static_cast<unsigned long long>(state.id), action.slot != 0);
        } else if (action.kind == EnemyAction::Kind::SpawnPickup) {
            pickupSpawns.push_back({action.resource, x, y});
        } else if (action.kind == EnemyAction::Kind::Bullet) {
            if (action.slot != 1) { direction = action.direction - 90; }
            m_effects.SpawnProjectile(action.resource, x, y, z, direction,
                action.speed, state.id, ownerType, action.part, action.node);
        } else if (action.kind == EnemyAction::Kind::Stun) {
            if (ownerType == 1 && std::hypot(playerX - x, playerY - y) < action.radius) {
                m_player.weapon->brother.Stun(action.durationMs);
            }
            if (ownerType == 1 && m_brother != nullptr && std::hypot(m_brother->x - x, m_brother->y - y) < action.radius) {
                m_brotherModel->weapon->brother.Stun(action.durationMs);
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
            hit.x = x; hit.y = y; hit.direction = direction;
            hit.damage = action.damage * GetDamageMultiplier(state.id);
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
    // Equipment changes create a new script host; reconnect before input.
    m_player.weapon->brother.SetLevelContext(m_level);
    if (deltaMs <= 0) { return; }
    m_effects.BeginAudioFrame();
    UpdateExperienceTexts(deltaMs);
    deaths.clear();
    teleports.clear();
    levelEvents.clear();
    pickupSpawns.clear();
    m_previousPlayerX = playerX;
    m_previousPlayerY = playerY;
    if (!m_vitals.dead && m_vitals.stunMs == 0) {
        const float length = std::hypot(moveX, moveY);
        if (length > 0 && m_player.weapon->brother.CanMove()) {
            const float speed = kPlayerSpeed * PlayerArmorMultiplier(m_player, 2) * m_player.weapon->brother.GetFrenzyMultiplier(2) *
                m_player.ActiveWeapon().gun.GetMasterySpeedMod() * 0.01f;
            playerX += moveX / length * speed * deltaMs * 0.001f;
            playerY += moveY / length * speed * deltaMs * 0.001f;
        }
        // CPlayer::UpdateShooting :101312 only targets while the fire stick is
        // active. Desktop mouse-held fire supplies that intent; idle never fires.
        if (m_player.weapon->brother.IsAutoFire()) {
            if (shoot) { shoot = m_autoAim.Update(deltaMs, playerX, playerY, facing, *this); }
            else { m_autoAim.ClearTarget(facing); }
        } else if (m_autoAim.GetTarget() != 0) { m_autoAim.ClearTarget(facing); }
        SetPlayerInput(m_player, length > 0, shoot);
    }
    const float forceSeconds = m_player.weapon->brother.GetKnockbackStepSeconds(deltaMs);
    AdvancePlayer(m_player, deltaMs);
    if (m_playerForceMs > 0 && !m_vitals.dead) {
        playerX += m_playerForceX * forceSeconds;
        playerY += m_playerForceY * forceSeconds;
        m_playerForceMs = std::max(0, m_playerForceMs - deltaMs);
    }
    ResolvePlayerMovement(m_previousPlayerX, m_previousPlayerY, playerX, playerY);
    if (m_brotherModel != nullptr) {
        m_brotherModel->weapon->brother.SetLevelContext(m_level);
#if GB_ENABLE_TESTS
        PerformanceProbe::Scope timing(PerformanceProbe::counters.brotherMs);
#endif
        m_brother->SetShootingAllowed(m_level == nullptr || m_level->CanBrotherShoot());
        m_brother->Update(deltaMs, m_brotherModel->weapon->brother, *this,
            playerX, playerY, PlayerArmorMultiplier(*m_brotherModel, 2) * m_brotherModel->weapon->brother.GetFrenzyMultiplier(2));
        if (m_brother->TakeWeaponSwapRequest()) { RequestBrotherWeaponSwap(); }
        AdvancePlayer(*m_brotherModel, deltaMs);
        if (m_brotherModel->weapon->brother.TakeWeaponSwap() && !SwapBrotherWeapon()) { ++invalidSpawns; }
    }
    // CLevel::Update :121318 refreshes CFlock before object movement.
    // AddObject/RemoveObject maintain membership, including unremoved corpses.
    m_flockEnemies.clear();
    for (auto &actor : enemies) {
        auto &state = actor->model.enemy.combat;
        if (state.enabled && !state.removed) { m_flockEnemies.push_back(&state); }
    }
    {
#if GB_ENABLE_TESTS
        PerformanceProbe::Scope timing(PerformanceProbe::counters.flockMs);
        if (PerformanceProbe::disableFlock) {
            for (auto *state : m_flockEnemies) { state->flockX = 0; state->flockY = 0; }
        } else
#endif
        { CFlock::RefreshFlock(m_flockEnemies); }
    }
    for (auto &actor : enemies) {
        CEnemy &enemy = actor->model.enemy;
        EnemyCombat &state = enemy.combat;
        if (!state.enabled || state.removed) { continue; }
#if GB_ENABLE_TESTS
        PerformanceProbe::Scope timing(PerformanceProbe::counters.enemyMs);
#endif
        int enemyDeltaMs = deltaMs;
        // TransformObjectElapseMS :114279 leaves dead actors and player shots
        // at normal speed; live enemies use the script's Q8 time multiplier.
        if (m_level != nullptr && !state.dead) {
            enemyDeltaMs = std::max(1, static_cast<int>(std::lround(deltaMs * m_level->GetObjectTimeScale())));
        }
        SelectTarget(*actor);
        UpdateNavigation(*actor, enemyDeltaMs);
        enemy.Update(enemyDeltaMs);
        for (std::uint32_t part = 0; part < enemy.GetPartCount(); ++part) {
            for (const MoveSoundRef &sound : enemy.GetPart(part).controller.TakeSounds()) {
                GameObjectRef resource;
                resource.packHash = sound.packHash;
                resource.localIndex = sound.localIndex;
                m_effects.PlayMoveSound(resource);
            }
        }
        ResolveMovement(state.previousX, state.previousY, state.x, state.y,
            enemy.GetPart(0).radius * m_cameraScale, false);
        actor->contactTimer = std::max(0, actor->contactTimer - enemyDeltaMs);
        actor->brotherContactTimer = std::max(0, actor->brotherContactTimer - enemyDeltaMs);
        // TestCollisions :73178-73194 uses both location histories and the full
        // 22-unit brother radius, not the smaller map-wall resolution radius.
        float contactFraction = 0;
        if (m_brother != nullptr && !m_brother->vitals.dead && !state.dead &&
            state.variables[16] != 1 && state.targetType != 2 && state.variables[12] > 0 &&
            state.variables[13] > 0 && actor->brotherContactTimer == 0 &&
            CombatGeometry::CircleCircle({m_brother->previousX, m_brother->previousY},
                {m_brother->x, m_brother->y}, m_brotherModel->weapon->brother.GetRadius(),
                {state.previousX, state.previousY}, {state.x, state.y}, enemy.GetPart(0).radius, contactFraction)) {
            CombatHit contact;
            contact.owner = state.id;
            contact.ownerType = 1;
            contact.damage = state.variables[17] * GetDamageMultiplier(state.id);
            ApplyHit(kBrotherCombatId, contact);
            const float angle = (state.facing - 90) * kRadians;
            ApplyBrotherForce(kBrotherCombatId, std::cos(angle) * state.variables[12],
                std::sin(angle) * state.variables[12], state.variables[13]);
            actor->brotherContactTimer = state.variables[13];
            enemy.TriggerEvent(8);
        }
        if (!state.dead && state.variables[16] != 1 && state.targetType != 2 && !m_vitals.dead &&
            state.variables[12] > 0 && state.variables[13] > 0 &&
            actor->contactTimer == 0 && CombatGeometry::CircleCircle({m_previousPlayerX, m_previousPlayerY},
                {playerX, playerY}, m_player.weapon->brother.GetRadius(),
                {state.previousX, state.previousY}, {state.x, state.y}, enemy.GetPart(0).radius, contactFraction)) {
            if (state.variables[17] > 0) {
                CombatHit contact;
                contact.owner = state.id;
                contact.ownerType = 1;
                contact.damage = state.variables[17] * GetDamageMultiplier(state.id);
                ApplyHit(kPlayerCombatId, contact);
            }
            const float angle = (state.facing - 90) * kRadians;
            ApplyBrotherForce(kPlayerCombatId, std::cos(angle) * state.variables[12],
                std::sin(angle) * state.variables[12], state.variables[13]);
            actor->contactTimer = state.variables[13];
            enemy.TriggerEvent(8);
        }
        Actions(*actor);
    }
    float matrix[16];
    if (m_brotherModel != nullptr) {
        BrotherMatrix(matrix);
        m_effects.EmitBrother(*m_brotherModel, matrix, m_brother->facing, kBrotherCombatId, m_weaponCollision);
    }
    PlayerMatrix(matrix);
    {
#if GB_ENABLE_TESTS
        PerformanceProbe::Scope timing(PerformanceProbe::counters.effectsMs);
#endif
        m_effects.Update(m_player, matrix, facing, deltaMs, m_weaponCollision);
    }
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
        if (state.dead && !enemies[i]->deathReported) {
            RewardEnemy(*enemies[i]);
            CombatDeath death;
            death.objectId = enemies[i]->objectId;
            death.enemy.packHash = enemies[i]->data->packHash;
            death.enemy.localIndex = static_cast<std::uint8_t>(enemies[i]->data->ordinal);
            deaths.push_back(death);
            enemies[i]->deathReported = true;
        }
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
