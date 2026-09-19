/** Reproduce reported Haven actor/shot penetration through the actual session. */
#include "gun_bros_re/host/ZGameObserver.h"
#include "gun_bros_re/gameplay/map/CMapResources.h"
#include "gun_bros_re/gameplay/game/CGameRuntime.h"
#include "gun_bros_re/debug/Capture.h"
#include "gun_bros_re/debug/PerformanceProbe.h"
#include "TestOutput.h"
#include "engine/core/CStringToKey.h"
#include <cstdio>
#include <limits>
#include <set>

namespace {
class HavenMuzzleObserver final : public ZGameObserver {
public:
    int OnResources(Resources resources) override {
        m_map = &resources.loaded;
        m_player = &resources.player;
        for (std::size_t index = 0; index < resources.enemies.size(); ++index) {
            const auto &enemy = resources.enemies[index];
            if (enemy.packHash == CStringToKey("pack1") && enemy.ordinal == 17) {
                m_enemyIndex = index;
                return -1;
            }
        }
        return 1;
    }
    int OnStage(Stage phase, CGame::Session &state) override {
        if (phase != Stage::LoopStarting) { return -1; }
        auto &scene = state.scene;
        auto &collision = m_map->GetResources().weaponCollision;
        scene.SetViewBounds(1400, 450, 3000, 3000);
        // Minimized from frame 1246 of the real wave-45 replay below.
        auto *owner = scene.Spawn(m_enemyIndex, 1502.26f, 455.26f);
        if (owner == nullptr) { return 1; }
        GameObjectRef bullet;
        bullet.packHash = CStringToKey("pack1");
        bullet.localIndex = 2;
        const float blocked = Collision::SegmentFraction(owner->combat.x, owner->combat.y,
            1547.24f - owner->combat.x, 377.61f - owner->combat.y, &collision.walls);
        if (blocked >= 1) { return 1; }
        scene.SpawnProjectile(bullet, 1547.24f, 377.61f, 0, -60, 300, owner->combat.id, 1);
        float matrix[16];
        scene.PlayerMatrix(matrix);
        scene.Update(*m_player, matrix, 0, 16, &collision);
        const auto blockedCount = scene.GetBulletCount();
        // Same original projectile/owner, but both ends are on open ground.
        owner->combat.x = 1550;
        owner->combat.y = 500;
        const float clear = Collision::SegmentFraction(1550, 500, 50, 0, &collision.walls);
        if (clear < 1) { return 1; }
        scene.SpawnProjectile(bullet, 1600, 500, 0, 0, 300, owner->combat.id, 1);
        scene.Update(*m_player, matrix, 0, 16, &collision);
        const auto clearCount = scene.GetBulletCount();
        std::printf("[haven-muzzle-check] blocked-fraction=%.5f blocked-alive=%zu clear-alive=%zu\n",
            blocked, blockedCount, clearCount);
        if (blockedCount != 0 || clearCount != 1) { return 1; }
        return 0;
    }
private:
    CMap *m_map = nullptr;
    CBrother *m_player = nullptr;
    std::size_t m_enemyIndex = 0;
};

bool Inside(const CProp &prop, const CCollisionData &shape, float x, float y) {
    x -= prop.x;
    y -= prop.y;
    bool inside = false;
    for (const auto &edge : shape.GetEdges()) {
        if (!edge.enabled) { continue; }
        const auto &a = shape.GetVertices()[edge.firstVertex];
        const auto &b = shape.GetVertices()[edge.secondVertex];
        if ((a.y > y) == (b.y > y)) { continue; }
        const float crossing = a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y);
        if (x < crossing) { inside = !inside; }
    }
    return inside;
}

float Depth(const CProp &prop, const CCollisionData &shape, float x, float y) {
    x -= prop.x;
    y -= prop.y;
    float nearest = std::numeric_limits<float>::max();
    for (const auto &edge : shape.GetEdges()) {
        if (!edge.enabled) { continue; }
        const auto &a = shape.GetVertices()[edge.firstVertex];
        const auto &b = shape.GetVertices()[edge.secondVertex];
        const float dx = b.x - a.x, dy = b.y - a.y;
        const float squared = dx * dx + dy * dy;
        float fraction = 0;
        if (squared > 0) { fraction = std::clamp(((x - a.x) * dx + (y - a.y) * dy) / squared, 0.0f, 1.0f); }
        nearest = std::min(nearest, std::hypot(x - a.x - dx * fraction, y - a.y - dy * fraction));
    }
    return nearest;
}

class HavenCollisionObserver final : public ZGameObserver {
public:
    explicit HavenCollisionObserver(bool artillery) : m_artillery(artillery) {}
    void Configure(const CGame::Launch &, Options &options) override {
        options.realtime = false;
        options.mouseAim = false;
        options.levelSeed = 5489;
        options.showCollisions = true;
        options.exitOnCompletion = false;
    }
    int OnResources(Resources resources) override {
        m_map = &resources.loaded;
        resources.vitals.invincible = true;
        return -1;
    }
    int OnStage(Stage phase, CGame::Session &state) override {
        if (phase == Stage::LoopStarting) {
            // Position from the user's Haven screenshot, not production data.
            state.scene.GetPlayer().x = 1222.4f;
            state.scene.GetPlayer().y = 626.8f;
            if (m_artillery) {
                state.scene.GetPlayer().x = 1539.5f;
                state.scene.GetPlayer().y = 410.2f;
            }
            const auto *body = m_map->GetCurrentCollisionLayer();
            const auto *shots = m_map->GetCurrentBulletCollisionLayer();
            std::printf("[haven-collision] layers body=%d shots=%d props=%zu\n",
                body ? body->GetLayerIndex() : -1, shots ? shots->GetLayerIndex() : -1,
                m_map->GetResources().props.size());
        }
        return -1;
    }
    int OnFrame(FramePhase phase, Frame &frame) override {
        if (phase == FramePhase::BeforeSimulation) {
            frame.accumulator = 16;
            frame.vitals.invincible = true;
            frame.suppressFire = true;
            frame.moveX = 0;
            frame.moveY = 0;
        }
        if (phase == FramePhase::AfterSimulation) {
            ++m_frames;
            std::set<Collision::ObjectId> insideThisFrame;
            m_peakEnemies = std::max(m_peakEnemies, frame.scene.GetEnemies().size());
            for (const auto &item : frame.scene.GetBulletRenderItems()) {
                const CBullet &shot = *item.bullet;
                if (shot.beam || shot.ownerType != 1 || !m_seenShots.insert(shot.id).second) { continue; }
                const auto *owner = frame.scene.Find(shot.owner);
                if (owner == nullptr) { continue; }
                const CCollisionData *shape = &m_map->GetResources().weaponCollision.walls;
                if ((shot.flags & 0x20) != 0) { shape = &m_map->GetResources().weaponCollision.terrain; }
                const float fraction = Collision::SegmentFraction(owner->combat.x, owner->combat.y,
                    shot.x - owner->combat.x, shot.y - owner->combat.y, shape);
                if (fraction < 1) {
                    ++m_birthCrossings;
                    std::printf("[haven-birth-through] frame=%u bullet=%08x:%u owner=%08x:%u radius=%.1f owner-xy=%.2f,%.2f shot-xy=%.2f,%.2f fraction=%.5f\n",
                        m_frames, shot.source.resource.packHash, shot.source.resource.localIndex,
                        owner->combat.templateRef.packHash, owner->combat.templateRef.localIndex,
                        owner->GetPart(0).radius, owner->combat.x, owner->combat.y, shot.x, shot.y, fraction);
                }
            }
            for (const auto &prop : m_map->GetResources().props) {
                if (!prop.active || prop.IsRemoved()) { continue; }
                for (const auto &actor : frame.scene.GetEnemies()) {
                    const auto &state = actor->combat;
                    if (state.dead || state.removed || !state.enabled) { continue; }
                    if (!Inside(prop, prop.GetCollision(), state.x, state.y)) { continue; }
                    insideThisFrame.insert(state.id);
                    const float depth = Depth(prop, prop.GetCollision(), state.x, state.y);
                    if (depth > m_maxDepth) {
                        m_maxDepth = depth;
                        m_deepestX = state.x;
                        m_deepestY = state.y;
                    }
                    if (!m_actors.insert(state.id).second) { continue; }
                    std::printf("[haven-actor-inside] frame=%u enemy=%08x:%u id=%llu pos=%.2f,%.2f previous=%.2f,%.2f radius=%.2f behaviour=%d prop=%08x:%u origin=%.1f,%.1f\n",
                        m_frames, state.templateRef.packHash, state.templateRef.localIndex,
                        static_cast<unsigned long long>(state.id), state.x, state.y, state.previousX, state.previousY,
                        actor->GetPart(0).radius, state.behaviour, prop.resources->resource.packHash,
                        prop.resources->resource.localIndex, prop.x, prop.y);
                    std::printf("[haven-actor-path] nav=%.3f,%.3f flock=%.3f,%.3f target=%.3f,%.3f\n",
                        state.navigationX, state.navigationY, state.flockX, state.flockY, state.targetX, state.targetY);
                    for (const auto &point : prop.GetCollision().GetVertices()) {
                        std::printf("[haven-body-vertex] %.2f,%.2f\n", prop.x + point.x, prop.y + point.y);
                    }
                    for (const auto &point : prop.GetCollision(true).GetVertices()) {
                        std::printf("[haven-shot-vertex] %.2f,%.2f\n", prop.x + point.x, prop.y + point.y);
                    }
                    const auto *path = dynamic_cast<const CLayerPathMesh *>(frame.scene.GetNavigationPath());
                    if (path != nullptr) {
                        const int cell = path->FindNode(state.x, state.y);
                        std::printf("[haven-actor-cell] cell=%d\n", cell);
                        if (cell >= 0) {
                            for (const auto index : path->GetQuads()[cell].vertices) {
                                const auto &point = path->GetVertices()[index];
                                std::printf("[haven-path-vertex] %u %.2f,%.2f\n", index, point.x, point.y);
                            }
                        }
                    }
                }
                for (const auto &item : frame.scene.GetBulletRenderItems()) {
                    const CBullet &shot = *item.bullet;
                    if (shot.beam || shot.ownerType != 1 || !Inside(prop, prop.GetCollision(), shot.x, shot.y)) { continue; }
                    if (!m_shots.insert(shot.id).second) { continue; }
                    std::printf("[haven-shot-inside] frame=%u bullet=%08x:%u id=%llu pos=%.2f,%.2f flags=%x bullet-shape-inside=%d prop=%08x:%u\n",
                        m_frames, shot.source.resource.packHash, shot.source.resource.localIndex,
                        static_cast<unsigned long long>(shot.id), shot.x, shot.y, shot.flags,
                        Inside(prop, prop.GetCollision(true), shot.x, shot.y),
                        prop.resources->resource.packHash, prop.resources->resource.localIndex);
                    const auto *owner = frame.scene.Find(shot.owner);
                    if (owner != nullptr) {
                        std::printf("[haven-shot-owner] enemy=%08x:%u radius=%.2f pos=%.2f,%.2f\n",
                            owner->combat.templateRef.packHash, owner->combat.templateRef.localIndex,
                            owner->GetPart(0).radius, owner->combat.x, owner->combat.y);
                    }
                }
            }
            for (auto &entry : m_insideFrames) {
                if (insideThisFrame.count(entry.first) == 0) { entry.second = 0; }
            }
            for (const auto id : insideThisFrame) {
                const unsigned duration = ++m_insideFrames[id];
                m_longestInside = std::max(m_longestInside, duration);
            }
        }
        if (phase == FramePhase::Drawn) {
            if (!m_captured && !m_actors.empty()) {
                Capture::SaveFrame(frame.window, TestOutput::Path("haven-actor-inside.png"));
                m_captured = true;
            }
            if (m_frames >= 2400) {
                Capture::SaveFrame(frame.window, TestOutput::Path("haven-collision-final.png"));
                std::printf("[haven-collision-check] frames=%u peak-enemies=%zu actors-inside=%zu shots-inside=%zu birth-crossings=%u\n", m_frames, m_peakEnemies, m_actors.size(), m_shots.size(), m_birthCrossings);
                std::printf("[haven-penetration] max-depth=%.3f deepest=%.3f,%.3f longest-inside-ms=%u\n",
                    m_maxDepth, m_deepestX, m_deepestY, m_longestInside * 16);
                if (!m_actors.empty() || m_peakEnemies == 0) { return 1; }
                return 0;
            }
        }
        return -1;
    }
private:
    CMap *m_map = nullptr;
    bool m_artillery = false;
    unsigned m_frames = 0;
    std::size_t m_peakEnemies = 0;
    unsigned m_birthCrossings = 0;
    float m_maxDepth = 0, m_deepestX = 0, m_deepestY = 0;
    unsigned m_longestInside = 0;
    std::map<Collision::ObjectId, unsigned> m_insideFrames;
    bool m_captured = false;
    std::set<Collision::ObjectId> m_actors;
    std::set<Collision::ObjectId> m_shots;
    std::set<Collision::ObjectId> m_seenShots;
};
}

int RunHavenCollisionCheck(const std::string &bigDirectory, bool artillery, bool disableFlock) {
    CGame::Launch launch;
    launch.bigDirectory = bigDirectory;
    launch.packShortName = "pack7";
    launch.mapIndex = 6;
    launch.startWave = 40;
    if (artillery) { launch.startWave = 44; }
    HavenCollisionObserver observer(artillery);
    launch.observer = &observer;
    const bool previous = PerformanceProbe::disableFlock;
    PerformanceProbe::disableFlock = disableFlock;
    std::printf("[haven-study] disable-flock=%d\n", disableFlock);
    const int result = CGame::Run(launch);
    PerformanceProbe::disableFlock = previous;
    return result;
}

int RunHavenMuzzleCheck(const std::string &bigDirectory) {
    CGame::Launch launch;
    launch.bigDirectory = bigDirectory;
    launch.packShortName = "pack7";
    launch.mapIndex = 6;
    HavenMuzzleObserver observer;
    launch.observer = &observer;
    return CGame::Run(launch);
}
