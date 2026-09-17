#include "gun_bros_re/gameplay/ZMapWorldInternal.h"

namespace MapDetail {

    void ZMapPropWorld::StartLayer(int layer) {
        // Original OnStart spawns this layer's props. Changing the update
        // layer does not remove objects already added to CLevel's pools.
        unsigned spawned = 0;
        // CLayerObject::OnStart :126250 visits the authored object indices.
        // Rendering sorts m_map.props spatially, so maintain a separate registration order.
        std::vector<ZPlacedProp *> layerProps;
        for (ZPlacedProp &prop : m_map.props) {
            if (prop.objectLayer == static_cast<unsigned>(layer)) { layerProps.push_back(&prop); }
        }
        std::sort(layerProps.begin(), layerProps.end(), [](const ZPlacedProp *first, const ZPlacedProp *second) {
            return first->objectId < second->objectId;
        });
        for (ZPlacedProp *instance : layerProps) {
            ZPlacedProp &prop = *instance;
            if (prop.objectLayer != static_cast<unsigned>(layer) || prop.active) { continue; }
            bool manual = false;
            for (unsigned index = 0; index < m_map.map.GetObjectLayerCount(); ++index) {
                const auto &objects = m_map.map.GetObjectLayer(index);
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
            if (prop.runtime == nullptr) { continue; }
            prop.runtime->SetLevelContext(&m_level);
            prop.runtime->Bind(prop.sprite->data, &prop.sprite->durations);
            SyncPlayers(prop);
        }
        BuildCollisionScene(m_map);
        std::printf("[prop] start layer=%d spawned=%u\n", layer, spawned);
    }

    bool ZMapPropWorld::Spawn(int layer, int objectId) {
        for (ZPlacedProp &prop : m_map.props) {
            if (prop.objectLayer != static_cast<unsigned>(layer) || prop.objectId != objectId) { continue; }
            if (prop.active) { return true; }
            prop.active = true;
            m_activeProps.push_back(&prop);
            if (prop.runtime != nullptr) {
                prop.runtime->SetLevelContext(&m_level);
                prop.runtime->Bind(prop.sprite->data, &prop.sprite->durations);
                SyncPlayers(prop);
            }
            BuildCollisionScene(m_map);
            return true;
        }
        return false;
    }

    void ZMapPropWorld::SendMessage(int objectId, int message) {
        for (ZPlacedProp &prop : m_map.props) {
            if (!prop.active || prop.objectId != objectId || prop.runtime == nullptr) { continue; }
            const unsigned previous = prop.runtime->GetStateId();
            prop.runtime->HandleMessage(message);
            SyncPlayers(prop);
            if (previous != prop.runtime->GetStateId()) {
                std::printf("[prop] id=%d message=%d state=%u->%u\n", objectId, message, previous, prop.runtime->GetStateId());
            }
            return;
        }
    }

    static void GetPropBoundsCenter(const ZPlacedProp &prop, float &x, float &y) {
        // CProp::GetBounds :123561 unions the active animation bounds of its
        // three SpritePlayers. GetOrientation :191317 tracks the rectangle center.
        const ZPropSlot *slots[] = {BackgroundSlotFor(prop), MainSlotFor(prop), ForegroundSlotFor(prop)};
        float left = 0, top = 0, right = 0, bottom = 0;
        bool hasBounds = false;
        for (const auto *slot : slots) {
            if (!slot) { continue; }
            for (const auto &frame : slot->quadsByStep) {
                for (const auto &quad : frame) {
                    if (!hasBounds) {
                        left = right = quad.offsetX;
                        top = bottom = quad.offsetY;
                        hasBounds = true;
                    }
                    left = std::min(left, float(quad.offsetX));
                    top = std::min(top, float(quad.offsetY));
                    right = std::max(right, float(quad.offsetX + quad.Width()));
                    bottom = std::max(bottom, float(quad.offsetY + quad.Height()));
                }
            }
        }
        x = prop.x + left + int(right - left) / 2;
        y = prop.y + top + int(bottom - top) / 2;
    }

    bool ZMapPropWorld::GetIndicatorTarget(unsigned key, float &x, float &y) const {
        if (key == 0 || key > m_activeProps.size()) { return false; }
        const auto &prop = *m_activeProps[key - 1];
        if (!prop.active || (prop.runtime && prop.runtime->IsRemoved())) { return false; }
        GetPropBoundsCenter(prop, x, y);
        return true;
    }

    void ZMapPropWorld::Update(int deltaMs) {
        const CLayerCollision *bodyLayer = m_map.map.GetCurrentCollisionLayer();
        const CLayerCollision *bulletLayer = m_map.map.GetCurrentBulletCollisionLayer();
        bool changed = bodyLayer != m_bodyLayer || bulletLayer != m_bulletLayer;
        m_bodyLayer = bodyLayer;
        m_bulletLayer = bulletLayer;
        for (ZPlacedProp &prop : m_map.props) {
            if (!prop.active || prop.runtime == nullptr) { continue; }
            // CLevel::TransformObjectElapseMS :114280 scales every PROP,
            // including its timer. Brothers and human bullets are exempt.
            const int propDeltaMs = std::max(1, int(std::lround(deltaMs * m_level.GetObjectTimeScale())));
            prop.runtime->Update(propDeltaMs, PlayerInside(prop));
            SyncPlayers(prop);
            for (const ZPropAction &action : prop.runtime->TakeActions()) { ApplyAction(prop, action); }
            if (prop.runtime->CollisionChanged()) { changed = true; prop.runtime->ClearCollisionChanged(); }
        }
        if (changed) { BuildCollisionScene(m_map); }
    }

    ZCombatTrace ZMapPropWorld::Trace(const ZCombatHit &hit, float x, float y, float dx, float dy,
        float radius, const std::vector<ZCombatId> &skip) {
        ZCombatTrace nearest;
        for (const ZPlacedProp &prop : m_map.props) {
            // CLayerCollision::TestCollisionSegment :125350 tests prop edges
            // without CanCollide's health gate. CBullet::CheckCollisionWithLevel
            // :61954 then sends Damage, including to zero-health script walls.
            if (!prop.active || prop.runtime == nullptr || prop.runtime->IsRemoved() || hit.ownerType != 0) { continue; }
            const ZCombatId id = kPropIdBase + prop.objectId;
            if (std::find(skip.begin(), skip.end(), id) != skip.end()) { continue; }
            const auto &shape = prop.runtime->GetCollision(true);
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

    ZHitResult ZMapPropWorld::ApplyHit(ZCombatId target, const ZCombatHit &hit) {
        for (ZPlacedProp &prop : m_map.props) {
            if (!prop.active || kPropIdBase + prop.objectId != target || prop.runtime == nullptr || prop.runtime->IsRemoved()) { continue; }
            prop.lastDamager = hit.owner;
            prop.runtime->Damage(hit.damage, hit.flags);
            // CProp::Damage -> Flow -> native 7 (:124595) dispatches the blast
            // synchronously. Finish its nested damage before the outer grenade
            // visits the enemy; delaying until Update lets that grenade remove
            // armor first and incorrectly exposes it to the barrel's blast.
            for (const ZPropAction &action : prop.runtime->TakeActions()) { ApplyAction(prop, action); }
            SyncPlayers(prop);
            ++m_hitCount;
            return ZHitResult::Hit;
        }
        return ZHitResult::Ignored;
    }

    void ZMapPropWorld::Splash(const ZCombatHit &hit, float radius) {
        // CProp::CanCollide accepts human/AI gun ownership, not enemy shots.
        // :123423 also accepts PROP sources (type 2), but rejects a brother
        // source (type 0). A player-owned barrel blast has no bullet object;
        // its allegiance alone must not turn it into a human bullet (type 5).
        if (hit.projectile != 0) {
            if (hit.ownerType != 0) { return; }
        } else if ((hit.owner & kPropIdBase) == 0 || hit.owner == kBrotherCombatId) { return; }
        for (ZPlacedProp &prop : m_map.props) {
            if (!prop.active || prop.runtime == nullptr || prop.runtime->IsRemoved() || prop.runtime->GetHealth() <= 0) { continue; }
            if (hit.owner == kPropIdBase + prop.objectId) { continue; }
            const auto &vertices = prop.runtime->GetEntryCollision().GetVertices();
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

    bool ZMapPropWorld::PlayerInside(const ZPlacedProp &prop) const {
        if (m_scene.IsMatchSpawnPending(0)) { return false; }
        const auto &vertices = prop.runtime->GetEntryCollision().GetVertices();
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

    void ZMapPropWorld::ApplyAction(ZPlacedProp &prop, const ZPropAction &action) {
        if (action.kind == ZPropAction::Kind::Entered || action.kind == ZPropAction::Kind::Destroyed) {
            m_level.OnPropEvent(prop.objectId, prop.sprite->resource, action.kind == ZPropAction::Kind::Entered);
            return;
        }
        if (action.kind == ZPropAction::Kind::Splash) {
            if (action.playersOnly) {
                float x = 0, y = 0;
                GetPropBoundsCenter(prop, x, y);
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
        float x = prop.x, y = prop.y;
        ZCombatId owner = 0;
        if (action.kind == ZPropAction::Kind::Sound) { cue.kind = ZGunCue::Kind::Sound; }
        if (action.kind == ZPropAction::Kind::Portal || action.kind == ZPropAction::Kind::AttachedEffect) {
            x = m_scene.GetPlayer().x; y = m_scene.GetPlayer().y;
        }
        if (action.kind == ZPropAction::Kind::AttachedEffect || action.kind == ZPropAction::Kind::StopEffect) {
            // CProp native 16/17 uses CMap's CParticleSystem (:124680).
            cue.particlePool = m_map.particleSystemPool;
            // AddEffect starts a one-shot even when native 16 adds an anchor.
            cue.loopParticles = false;
            owner = kPlayerCombatId;
            cue.kind = ZGunCue::Kind::Trail;
            if (action.kind == ZPropAction::Kind::StopEffect) { cue.kind = ZGunCue::Kind::StopTrail; }
        }
        m_effects.Emit(cue, x, y, 0, 0, owner, prop.objectId + 1000);
        if (action.kind == ZPropAction::Kind::Portal) { m_level.HandleEvent(3); }
    }
}
