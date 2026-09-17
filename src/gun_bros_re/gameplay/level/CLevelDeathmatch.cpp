/** CLevel deathmatch lifecycle adapted to two local input providers.
 * InitDeathMatch :114880, RespawnPlayerForDeathMatch :114940, SetAuxGun :136986.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/CMPMatch.h"
#include "gun_bros_re/gameplay/ZPickupScene.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
#include "gun_bros_re/data/ZWeaponCatalog.h"
#include <cmath>
#include <limits>

namespace {
// CLayerPathLink::FindFarthestNode :166874 compares unlocked node distances.
const ILayerPath::Node *FarthestSpawn(const ILayerPath &path, float x, float y) {
    const ILayerPath::Node *result = nullptr;
    float farthest = -1;
    for (const auto &node : path.GetNodes()) {
        if (node.locked) { continue; }
        const float distance = std::hypot(node.x - x, node.y - y);
        if (distance > farthest) { farthest = distance; result = &node; }
    }
    return result;
}
}

bool CLevel::IsMatchSpawnPending(unsigned peer) const {
    if (!IsDeathmatch()) { return false; }
    if (peer == 0) { return !m_playerModel->weapon->brother.HasSpawned(); }
    return !m_brotherModel->weapon->brother.HasSpawned();
}

void CLevel::SetDeathmatch(CMPMatch *match, const std::vector<ZWeaponEntry> *weapons, ZPickupScene *pickups) {
    m_match = match; m_matchWeapons = weapons; m_matchPickups = pickups;
}
GameObjectRef CLevel::ActiveMatchGun(unsigned peer) const {
    if (peer == 0) { return m_playerModel->gunResource; }
    return m_brotherModel->gunResource;
}
bool CLevel::SelectMatchGun(unsigned peer, unsigned slot, const GameObjectRef &gun) {
    if (m_match == nullptr || peer > 1 || slot > 1) { return false; }
    const unsigned other = 1 - slot;
    if (m_gunConfigurations[peer][other].packHash == gun.packHash && m_gunConfigurations[peer][other].localIndex == gun.localIndex) {
        m_gunConfigurations[peer][other] = m_gunConfigurations[peer][slot];
    }
    m_gunConfigurations[peer][slot] = gun;
    m_auxiliaryMs[peer] = 0;
    if (IsMatchSpawnPending(peer) || m_match->GetLife(peer).dead) { return true; }
    return EquipMatchGun(peer, m_gunConfigurations[peer][m_matchSlots[peer]]);
}
bool CLevel::EquipMatchGun(unsigned peer, const GameObjectRef &ref, bool resetActor) {
    ZPlayerModel *model = m_playerModel;
    if (peer == 1) { model = m_brotherModel; }
    for (const auto &weapon : *m_matchWeapons) {
        if (weapon.packHash != ref.packHash || weapon.ordinal != ref.localIndex) { continue; }
        model->gunResource = ref;
        const std::uint64_t key = (static_cast<std::uint64_t>(ref.packHash) << 8) | ref.localIndex;
        model->masteryExperience = model->masteryByWeapon[key];
        if (resetActor) {
            if (!EquipPlayerWeapon(*m_tables, model->weapon->playerScript, weapon.data, weapon.name, *model) ||
                !CreatePlayerBuffers(*model, *m_program)) { return false; }
        } else {
            auto &bank = model->matchWeapons[key];
            if (bank == nullptr) {
                if (!PreparePlayerUIWeapon(*m_tables, weapon.data, weapon.name, *model) ||
                    !CreatePlayerBuffers(*model, *m_program)) { return false; }
                bank = std::move(model->uiOtherWeapon);
            }
            std::vector<const CMesh *> meshes;
            for (const auto &part : bank->configs) { meshes.push_back(&part->mesh); }
            bank->gun.SetDeathmatch(true);
            bank->gun.SetLevelContext(GetScriptLevel());
            bank->gun.SetMasteryExperience(model->masteryExperience);
            model->weapon->brother.SetUIGun(bank->gun, meshes);
            model->uiActiveWeapon = bank.get();
        }
        model->weapon->brother.SetLevelContext(GetScriptLevel());
        model->weapon->gun.SetLevelContext(GetScriptLevel());
        model->gunSlot = m_matchSlots[peer];
        if (peer == 1) { m_brotherWeaponSlot = m_matchSlots[peer]; }
        return true;
    }
    std::printf("[deathmatch] unresolved gun=%08x:%u\n", ref.packHash, ref.localIndex);
    return false;
}
bool CLevel::StartDeathmatch() {
    if (m_match == nullptr || m_brother == nullptr || m_map == nullptr) { return false; }
    m_match->Restart();
    m_auxiliaryMs[0] = 0; m_auxiliaryMs[1] = 0;
    m_matchSlots[0] = 0; m_matchSlots[1] = 0;
    m_matchShopping[0] = false; m_matchShopping[1] = false;
    m_matchSwap[0] = false; m_matchSwap[1] = false;
    m_matchStreaks[0] = 0; m_matchStreaks[1] = 0; m_matchDeaths.clear();
    m_vitals->maximum = static_cast<float>(m_match->Data().health);
    m_brother->vitals.maximum = m_vitals->maximum;
    m_vitals->Reset(); m_brother->vitals.Reset();
    // Keep the MAP placement and loading camera while equipment is pending.
    // Resolve the spawn endpoints when a participant confirms entry.
    m_matchMapSpawn = {m_actor.x, m_actor.y};
    m_playerModel->weapon->brother.WaitForSpawn();
    m_brotherModel->weapon->brother.WaitForSpawn();
    std::printf("[deathmatch] waiting for initial equipment\n");
    return true;
}
bool CLevel::RespawnDeathmatch(unsigned peer, bool initial, bool resumeFromShop) {
    if (m_match == nullptr || peer > 1 || m_match->GetResult() != CMPMatch::Result::Playing) { return false; }
    if (initial && !IsMatchSpawnPending(peer)) { return false; }
    if (initial) {
        // OnStart :120748 first finds a node farthest from the MAP player placement,
        // then its opposite endpoint. These saved positions are not random rolls.
        // Defer this preparation to entry confirmation without changing the rule.
        const ILayerPath *path = m_map->GetPathLayer(GetRespawnPathLayer());
        if (path == nullptr || path->GetNodes().empty()) { return false; }
        const auto *first = FarthestSpawn(*path, m_matchMapSpawn.x, m_matchMapSpawn.y);
        if (first == nullptr) { return false; }
        const auto *second = FarthestSpawn(*path, first->x, first->y);
        if (second == nullptr) { return false; }
        m_matchInitialSpawns[0] = {first->x, first->y};
        m_matchInitialSpawns[1] = {second->x, second->y};
        m_matchInitialAngles[0] = std::atan2(second->y - first->y, second->x - first->x) * 180 / 3.14159265f + 90;
        m_matchInitialAngles[1] = m_matchInitialAngles[0] - 180;
    }
    if (!initial) {
        const ZPlayerVitals *vitals = m_vitals;
        if (peer == 1) { vitals = &m_brother->vitals; }
        if (!vitals->dead || !vitals->deathAnimationComplete) { return false; }
    }
    float x = m_matchInitialSpawns[peer].x, y = m_matchInitialSpawns[peer].y, angle = m_matchInitialAngles[peer];
    float opponentX = m_brother->x, opponentY = m_brother->y;
    if (peer == 1) { opponentX = m_actor.x; opponentY = m_actor.y; }
    if (!initial && !IsMatchSpawnPending(1 - peer) && !m_match->GetLife(1 - peer).dead) {
        // Both entrants use the DM spawn layer. The map PLAYER placement can
        // belong to a sealed survival/tutorial room outside the active arena.
        // RespawnPlayerForDeathMatch :114940 uses the farthest node only when
        // this actor has spawned before and the opponent is still alive.
        ILayerPath *path = m_map->GetPathLayer(GetRespawnPathLayer());
        if (path == nullptr || path->GetNodes().empty()) { return false; }
        const auto *node = FarthestSpawn(*path, opponentX, opponentY);
        if (node == nullptr) { return false; }
        x = node->x; y = node->y;
        angle = std::atan2(opponentY - y, opponentX - x) * 180 / 3.14159265f + 90;
    }
    if (!initial && !m_match->Respawn(peer, resumeFromShop)) { return false; }
    m_auxiliaryMs[peer] = 0;
    m_matchSwap[peer] = false;
    ZPlayerModel *model = m_playerModel;
    if (peer == 0) {
        m_vitals->Reset(); m_actor.forceMs = 0;
        m_actor.x = x; m_actor.y = y; m_actor.facing = angle;
        m_actor.previousX = x; m_actor.previousY = y;
    } else { m_brother->Reset(x, y, angle); model = m_brotherModel; }
    model->powerups = {};
    if (!EquipMatchGun(peer, m_gunConfigurations[peer][m_matchSlots[peer]], true)) { return false; }
    if (!model->weapon->brother.Respawn()) { return false; }
    // RespawnPlayerForDeathMatch :115122 restores the local camera after Spawn.
    // A remote/Bot entry must never move the waiting player's camera.
    if (peer == 0) { m_map->GetCamera().SetCameraMode(0); }
    std::printf("[deathmatch] spawn peer=%u life=%u position=%.1f,%.1f\n", peer, m_match->GetLife(peer).serial, x, y);
    return true;
}
void CLevel::RecordMatchDeath(unsigned peer, int killer) {
    if (m_match == nullptr || !m_match->Kill(peer, killer)) { return; }
    m_matchDeaths.push_back(peer);
    ++m_multiplayer[peer].total.deaths;
    m_matchStreaks[peer] = 0;
    killer = 1 - peer;
    auto &statistics = m_multiplayer[killer].total;
    ++statistics.kills;
    ++m_matchStreaks[killer];
    statistics.bestStreak = std::max(statistics.bestStreak, m_matchStreaks[killer]);
    // Original OnPlayerKilled :118645 uses the LEVEL global XP multiplier.
    const unsigned experience = static_cast<unsigned>(std::max(0.0f, GetEnemyMultiplier(-1, 3)));
    if (killer == 0) {
        AddExperience(experience);
        const float armorRatio = PlayerArmorMultiplier(*m_brotherModel, 0) /
            PlayerArmorMultiplier(*m_playerModel, 0);
        const unsigned points = std::max(1u,
            static_cast<unsigned>(experience * m_matchStreaks[0] * armorRatio));
        AddMatchScore(points, m_matchStreaks[0]);
    }
    else { AddPeerExperience(experience); }
}
bool CLevel::AdvanceDeathmatchEnding(int deltaMs) {
    // PLAYER export 2 emits the burst before state 7's move reaches native 1.
    // Continue both dead actors, including simultaneous final kills, without AI or combat.
    m_effects->BeginAudioFrame();
    ZPlayerModel *models[] = {m_playerModel, m_brotherModel};
    ZPlayerVitals *vitals[] = {m_vitals, &m_brother->vitals};
    const ZCombatId actors[] = {kPlayerCombatId, kBrotherCombatId};
    bool complete = true;
    for (unsigned peer = 0; peer < 2; ++peer) {
        if (!vitals[peer]->dead) { continue; }
        auto &model = *models[peer];
        AdvancePlayer(model, deltaMs);
        float x = m_actor.x, y = m_actor.y, direction = m_actor.facing;
        if (peer == 1) { x = m_brother->x; y = m_brother->y; direction = m_brother->facing; }
        for (const auto &cue : model.weapon->brother.TakeCues()) {
            if (cue.kind == ZGunCue::Kind::Grenade || cue.kind == ZGunCue::Kind::Splash) { continue; }
            m_effects->Emit(cue, x, y, 0, direction - 90, actors[peer], cue.hand, -1, -1);
        }
        if (!vitals[peer]->deathAnimationComplete) { complete = false; }
    }
    m_effects->AdvanceAmbientEffects(deltaMs);
    for (unsigned peer = 0; peer < 2; ++peer) {
        if (vitals[peer]->dead && m_effects->HasActorBurst(actors[peer])) { complete = false; }
    }
    return complete;
}

void CLevel::UpdateDeathmatch(unsigned deltaMs) {
    if (m_match == nullptr) { return; }
    ZPlayerVitals *vitals[] = {m_vitals, &m_brother->vitals};
    for (unsigned peer = 0; peer < 2; ++peer) {
        if (vitals[peer]->dead && !m_match->GetLife(peer).dead) { RecordMatchDeath(peer, -1); }
    }
    for (unsigned peer : m_matchDeaths) {
        float x = m_actor.x, y = m_actor.y;
        if (peer == 1) { x = m_brother->x; y = m_brother->y; }
        OnDeathmatchKill(x, y);
    }
    m_matchDeaths.clear();
    m_match->Update(deltaMs);
    if (m_match->GetResult() != CMPMatch::Result::Playing) { return; }
    for (unsigned peer = 0; peer < 2; ++peer) {
        if (m_match->GetLife(peer).dead) {
            if (m_match->GetLife(peer).respawnMs == 0 && vitals[peer]->deathAnimationComplete && !RespawnDeathmatch(peer)) {
                m_objects.RecordInvalidSpawn();
            }
            continue;
        }
        if (m_auxiliaryMs[peer] == 0) { continue; }
        if (deltaMs >= m_auxiliaryMs[peer]) {
            m_auxiliaryMs[peer] = 0;
            if (!EquipMatchGun(peer, m_gunConfigurations[peer][m_matchSlots[peer]])) {
                m_objects.RecordInvalidSpawn();
            }
        } else { m_auxiliaryMs[peer] -= deltaMs; }
    }
}
bool CLevel::CollectMatchWeapon(unsigned peer, unsigned index) {
    if (m_match == nullptr || IsMatchSpawnPending(peer) || index >= m_match->Data().pickups.size() || m_match->GetLife(peer).dead) { return false; }
    if (!EquipMatchGun(peer, m_match->Data().pickups[index])) { return false; }
    m_auxiliaryMs[peer] = static_cast<unsigned>(m_match->Data().pickupRules[index].seconds) * 1000;
    std::printf("[deathmatch] auxiliary peer=%u item=%u duration=%u\n", peer, index, m_auxiliaryMs[peer]);
    return true;
}
bool CLevel::RequestMatchWeaponSwap(unsigned peer) {
    if (m_match == nullptr || IsMatchSpawnPending(peer) || m_match->GetLife(peer).dead || m_matchSwap[peer]) { return false; }
    ZPlayerModel *model = m_playerModel;
    if (peer == 1) { model = m_brotherModel; }
    if (model->weapon->brother.HasGrenadeRequest(0)) { return false; }
    if (!model->weapon->brother.OnSwapGun()) { return false; }
    m_matchSwap[peer] = true;
    return true;
}
bool CLevel::FinishMatchWeaponSwap(unsigned peer) {
    m_matchSwap[peer] = false;
    m_auxiliaryMs[peer] = 0;
    m_matchSlots[peer] = 1 - m_matchSlots[peer];
    return EquipMatchGun(peer, m_gunConfigurations[peer][m_matchSlots[peer]]);
}
bool CLevel::CanHitBrother(const ZCombatHit &hit, ZCombatId target) const {
    if (m_match != nullptr) {
        if (m_match->GetResult() != CMPMatch::Result::Playing) { return false; }
        if (target == kPlayerCombatId && IsMatchSpawnPending(0)) { return false; }
        if (target == kBrotherCombatId && IsMatchSpawnPending(1)) { return false; }
        const ZCombatId owner = ParticipantOwner(hit.owner);
        // CBrother::CanCollide :135310 accepts an enemy actor source, but
        // rejects direct PROP / BROTHER sources even in DM. A bullet is distinct.
        if (hit.propExplosion) { return hit.ownerType == 1 && hit.owner != target; }
        return owner != target && (hit.ownerType == 1 || owner == kPlayerCombatId || owner == kBrotherCombatId);
    }
    return hit.ownerType == 1 && hit.owner != target;
}

ZCombatId CLevel::ParticipantOwner(ZCombatId owner) const {
    // In-flight shots retain allegiance after the turret actor is removed.
    const ZCombatId summoner = m_objects.GetSummoner(owner);
    if (summoner != 0) { return summoner; }
    return owner;
}
bool CLevel::HasLineOfFire(float x, float y, float targetX, float targetY) const {
    if (m_weaponCollision == nullptr) { return true; }
    const auto &geometry = m_weaponCollision->walls;
    for (const auto &edge : geometry.GetEdges()) {
        if (!edge.enabled) { continue; }
        if (CombatGeometry::EdgeFraction(x, y, targetX - x, targetY - y,
            geometry.GetVertices()[edge.firstVertex], geometry.GetVertices()[edge.secondVertex], 0) <= 1) { return false; }
    }
    return true;
}
bool CLevel::FindMatchDestination(float x, float y, bool cover, float targetX, float targetY, float &goalX, float &goalY, unsigned choice) const {
    if (m_map == nullptr) { return false; }
    const ILayerPath *path = m_map->GetPathLayer(m_pathLayer);
    if (path == nullptr) { return false; }
    const int from = path->FindNode(x, y);
    float best = std::numeric_limits<float>::max();
    bool found = false;
    std::vector<const ILayerPath::Node *> patrolNodes;
    // Destination selection only needs reachability. Running FindNext for
    // every candidate repeated a full shortest-path search hundreds of times.
    const auto &nodes = path->GetNodes();
    std::vector<bool> reachable(nodes.size(), false);
    std::vector<unsigned> pending;
    if (from >= 0) { reachable[from] = true; pending.push_back(static_cast<unsigned>(from)); }
    for (unsigned cursor = 0; cursor < pending.size(); ++cursor) {
        for (unsigned neighbour : nodes[pending[cursor]].neighbours) {
            if (reachable[neighbour] || nodes[neighbour].locked) { continue; }
            reachable[neighbour] = true;
            pending.push_back(neighbour);
        }
    }
    for (unsigned index = 0; index < path->GetNodes().size(); ++index) {
        const auto &node = path->GetNodes()[index];
        const float distance = std::hypot(node.x - x, node.y - y);
        if (node.locked || distance < 40 || !CanBrotherWalk(node.x, node.y, node.x, node.y)) { continue; }
        if (from >= 0 && !reachable[index]) { continue; }
        if (cover && HasLineOfFire(targetX, targetY, node.x, node.y)) { continue; }
        if (!cover) { patrolNodes.push_back(&node); continue; }
        if (distance < best) { best = distance; goalX = node.x; goalY = node.y; found = true; }
    }
    if (!cover && !patrolNodes.empty()) {
        const auto &node = *patrolNodes[choice % patrolNodes.size()];
        goalX = node.x; goalY = node.y; return true;
    }
    return found;
}
bool CLevel::FindMatchSupply(float x, float y, float &goalX, float &goalY) const {
    if (m_matchPickups == nullptr) { return false; }
    return m_matchPickups->FindNearest(x, y, goalX, goalY);
}
