#include "gun_bros_re/gameplay/map/CLevelProps.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace MapDetail;

void CLevel::Props::StartLayer(int layer) {
    // Original OnStart spawns this layer's props. Changing the update
    // layer does not remove objects already added to CLevel's pools.
    unsigned spawned = 0;
    // CLayerObject::OnStart :126250 visits the authored object indices.
    // Rendering sorts m_map.GetResources().props spatially, so maintain a separate registration order.
    std::vector<CProp *> layerProps;
    for (CProp &prop : m_map.GetResources().props) {
        if (prop.objectLayer == static_cast<unsigned>(layer)) { layerProps.push_back(&prop); }
    }
    std::sort(layerProps.begin(), layerProps.end(), [](const CProp *first, const CProp *second) {
        return first->objectId < second->objectId;
    });
    for (CProp *instance : layerProps) {
        CProp &prop = *instance;
        if (prop.objectLayer != static_cast<unsigned>(layer) || prop.active) { continue; }
        bool manual = false;
        for (unsigned index = 0; index < m_map.GetObjectLayerCount(); ++index) {
            const auto &objects = m_map.GetObjectLayer(index);
            if (objects.GetLayerIndex() == prop.objectLayer) {
                manual = m_level.IsManualSpawnTag(objects.GetObjects()[prop.objectId].spawnTag);
                break;
            }
        }
        // CLayerObject::OnStart skips instances whose auto-spawn bit was
        // cleared by SetSpawnMode. Native SpawnInstance activates them later.
        if (manual) { continue; }
        prop.active = true;
        m_activeProps.push_back(&prop);
        ++spawned;
        prop.SetLevelContext(&m_level);
        prop.BindResources();
    }
    m_map.BuildCollisionScene();
    std::printf("[prop] start layer=%d spawned=%u\n", layer, spawned);
}

bool CLevel::Props::Spawn(int layer, int objectId) {
    for (CProp &prop : m_map.GetResources().props) {
        if (prop.objectLayer != static_cast<unsigned>(layer) || prop.objectId != objectId) { continue; }
        if (prop.active) { return true; }
        prop.active = true;
        m_activeProps.push_back(&prop);
        prop.SetLevelContext(&m_level);
        prop.BindResources();
        m_map.BuildCollisionScene();
        return true;
    }
    return false;
}

void CLevel::Props::SendMessage(int objectId, int message) {
    for (CProp &prop : m_map.GetResources().props) {
        if (!prop.active || prop.objectId != objectId || !prop.HasScript()) { continue; }
        const unsigned previous = prop.GetStateId();
        prop.HandleMessage(message);
        if (previous != prop.GetStateId()) {
            std::printf("[prop] id=%d message=%d state=%u->%u\n", objectId, message, previous, prop.GetStateId());
        }
        return;
    }
}

bool CLevel::Props::GetIndicatorTarget(unsigned key, float &x, float &y) const {
    if (key == 0 || key > m_activeProps.size()) { return false; }
    const auto &prop = *m_activeProps[key - 1];
    if (!prop.active || (prop.HasScript() && prop.IsRemoved())) { return false; }
    prop.GetBoundsCenter(x, y);
    return true;
}

void CLevel::Props::Update(int deltaMs) {
    const CLayerCollision *bodyLayer = m_map.GetCurrentCollisionLayer();
    const CLayerCollision *bulletLayer = m_map.GetCurrentBulletCollisionLayer();
    bool changed = bodyLayer != m_bodyLayer || bulletLayer != m_bulletLayer;
    m_bodyLayer = bodyLayer;
    m_bulletLayer = bulletLayer;
    for (CProp &prop : m_map.GetResources().props) {
        if (!prop.active) { continue; }
        // CLevel::TransformObjectElapseMS :114280 scales every PROP,
        // including its timer. Brothers and human bullets are exempt.
        const int propDeltaMs = std::max(1, int(std::lround(deltaMs * m_level.GetObjectTimeScale())));
        prop.Update(propDeltaMs, PlayerInside(prop));
        for (const CProp::Action &action : prop.TakeActions()) { ApplyAction(prop, action); }
        if (prop.CollisionChanged()) { changed = true; prop.ClearCollisionChanged(); }
    }
    if (changed) { m_map.BuildCollisionScene(); }
}

ZCombatTrace CLevel::Props::Trace(const ZCombatHit &hit, float x, float y, float dx, float dy,
    float radius, const std::vector<ZCombatId> &skip) {
    ZCombatTrace nearest;
    for (const CProp &prop : m_map.GetResources().props) {
        // CLayerCollision::TestCollisionSegment :125350 tests prop edges
        // without CanCollide's health gate. CBullet::CheckCollisionWithLevel
        // :61954 then sends Damage, including to zero-health script walls.
        if (!prop.active || !prop.HasScript() || prop.IsRemoved() || hit.ownerType != 0) { continue; }
        const ZCombatId id = kPropIdBase + prop.objectId;
        if (std::find(skip.begin(), skip.end(), id) != skip.end()) { continue; }
        const auto &shape = prop.GetCollision(true);
        const auto &vertices = shape.GetVertices();
        for (const ZCollisionEdge &edge : shape.GetEdges()) {
            if (!edge.enabled) { continue; }
            const ZCollisionPoint &first = vertices[edge.firstVertex], &second = vertices[edge.secondVertex];
            const float fraction = CombatGeometry::EdgeFraction(x - prop.x, y - prop.y, dx, dy, first, second, radius);
            if (fraction < nearest.fraction) {
                nearest = {id, fraction, -1, edge.group, first.y - second.y, second.x - first.x};
            }
        }
    }
    return nearest;
}

ZHitResult CLevel::Props::ApplyHit(ZCombatId target, const ZCombatHit &hit) {
    for (CProp &prop : m_map.GetResources().props) {
        if (!prop.active || kPropIdBase + prop.objectId != target || !prop.HasScript() || prop.IsRemoved()) { continue; }
        prop.lastDamager = hit.owner;
        prop.Damage(hit.damage, hit.flags);
        // CProp::Damage -> Flow -> native 7 (:124595) dispatches the blast
        // synchronously. Finish its nested damage before the outer grenade
        // visits the enemy; delaying until Update lets that grenade remove
        // armor first and incorrectly exposes it to the barrel's blast.
        for (const CProp::Action &action : prop.TakeActions()) { ApplyAction(prop, action); }
        ++m_hitCount;
        return ZHitResult::Hit;
    }
    return ZHitResult::Ignored;
}

void CLevel::Props::Splash(const ZCombatHit &hit, float radius) {
    // CProp::CanCollide accepts human/AI gun ownership, not enemy shots.
    // :123423 also accepts PROP sources (type 2), but rejects a brother
    // source (type 0). A player-owned barrel blast has no bullet object;
    // its allegiance alone must not turn it into a human bullet (type 5).
    if (hit.projectile != 0) {
        if (hit.ownerType != 0) { return; }
    } else if ((hit.owner & kPropIdBase) == 0 || hit.owner == kBrotherCombatId) { return; }
    for (CProp &prop : m_map.GetResources().props) {
        if (!prop.active || !prop.HasScript() || prop.IsRemoved() || prop.GetHealth() <= 0) { continue; }
        if (hit.owner == kPropIdBase + prop.objectId) { continue; }
        const auto &vertices = prop.GetEntryCollision().GetVertices();
        if (vertices.empty()) { continue; }
        float left = vertices[0].x, right = left, top = vertices[0].y, bottom = top;
        for (const ZCollisionPoint &point : vertices) {
            left = std::min(left, point.x); right = std::max(right, point.x);
            top = std::min(top, point.y); bottom = std::max(bottom, point.y);
        }
        const float x = prop.x + (left + right) * 0.5f;
        const float y = prop.y + (top + bottom) * 0.5f;
        const float extent = std::max(right - left, bottom - top) * 0.5f;
        if (std::hypot(hit.x - x, hit.y - y) > radius + extent) { continue; }
        m_scene.ApplyHit(kPropIdBase + prop.objectId, hit);
    }
}

bool CLevel::Props::PlayerInside(const CProp &prop) const {
    if (m_scene.IsMatchSpawnPending(0)) { return false; }
    const auto &vertices = prop.GetEntryCollision().GetVertices();
    if (vertices.empty()) { return false; }
    bool inside = false;
    std::size_t previous = vertices.size() - 1;
    const float x = m_scene.GetPlayer().x - prop.x;
    const float y = m_scene.GetPlayer().y - prop.y;
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const auto &first = vertices[index];
        const auto &second = vertices[previous];
        if ((first.y > y) != (second.y > y)) {
            const float crossing = first.x + (second.x - first.x) * (y - first.y) / (second.y - first.y);
            if (x < crossing) { inside = !inside; }
        }
        previous = index;
    }
    return inside;
}

void CLevel::Props::ApplyAction(CProp &prop, const CProp::Action &action) {
    if (action.kind == CProp::Action::Kind::Entered || action.kind == CProp::Action::Kind::Destroyed) {
        m_level.OnPropEvent(prop.objectId, prop.resources->resource, action.kind == CProp::Action::Kind::Entered);
        return;
    }
    if (action.kind == CProp::Action::Kind::Splash) {
        if (action.playersOnly) {
            float x = 0, y = 0;
            prop.GetBoundsCenter(x, y);
            m_scene.SplashBrothers(x, y, static_cast<float>(action.radius), static_cast<float>(action.damage),
                static_cast<float>(action.force), action.forceMs);
            return;
        }
        ZCombatHit hit;
        hit.x = prop.x;
        hit.y = prop.y;
        hit.damage = static_cast<float>(action.damage);
        hit.propExplosion = true;
        hit.applyArmorAttack = false;
        // CProp::FunctionResolver case 7 :124571 resolves self / last
        // damager / local player. Preserve that object identity for CanCollide.
        hit.owner = kPropIdBase + prop.objectId;
        if (action.damageOwner == 2) { hit.owner = kPlayerCombatId; }
        if (action.damageOwner == 1 && prop.lastDamager != 0) { hit.owner = prop.lastDamager; }
        // Self-owned environmental explosions can hurt both sides; the
        // original knockback native explicitly visits only the brothers.
        // Correction: that direct call belongs solely to native 10 above.
        // Native 7 must retain the actual source type for CanCollide.
        if (m_scene.Find(hit.owner) != nullptr) { hit.ownerType = 1; }
        m_scene.Splash(hit, static_cast<float>(action.radius), 360, 0, 0);
        return;
    }
    ZGunCue cue;
    cue.kind = ZGunCue::Kind::Effect;
    cue.resource = action.resource;
    cue.effectGroup = action.group;
    float x = prop.x, y = prop.y;
    ZCombatId owner = 0;
    if (action.kind == CProp::Action::Kind::Sound) { cue.kind = ZGunCue::Kind::Sound; }
    if (action.kind == CProp::Action::Kind::Portal || action.kind == CProp::Action::Kind::AttachedEffect) {
        x = m_scene.GetPlayer().x; y = m_scene.GetPlayer().y;
    }
    if (action.kind == CProp::Action::Kind::AttachedEffect || action.kind == CProp::Action::Kind::StopEffect) {
        // CProp native 16/17 uses CMap's CParticleSystem (:124680).
        // AddEffect starts a one-shot even when native 16 adds an anchor.
        cue.loopParticles = false;
        cue.anchorToActor = true;
        owner = kPlayerCombatId;
        cue.kind = ZGunCue::Kind::Trail;
        if (action.kind == CProp::Action::Kind::StopEffect) { cue.kind = ZGunCue::Kind::StopTrail; }
    }
    m_scene.Emit(cue, x, y, 0, 0, owner, prop.objectId + 1000, -1, -1);
    if (action.kind == CProp::Action::Kind::Portal) { m_level.HandleEvent(3); }
}
