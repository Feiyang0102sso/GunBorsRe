/** @file PowerupScene.cpp
 * @brief Separate availability, request and the actual grenade inventory commit.
 */
#include "runtime/PowerupScene.h"
#include "engine/CStringToKey.h"
#include <cmath>
#include <cstdio>

PowerupScene::PowerupScene(CResTOCManager &toc, PackTables &tables, PlayerModel &player,
    PlayerVitals &vitals, CombatScene &scene, WeaponEffects &effects, CProfileManager &profile)
    : m_toc(toc), m_tables(tables), m_player(player), m_vitals(vitals), m_scene(scene),
      m_effects(effects), m_profile(profile) {}

bool PowerupScene::Init() { return LoadPowerupCatalog(m_toc, m_tables, m_catalog); }

bool PowerupScene::IsSupported(const PowerupEntry &entry) const {
    // Expose only completed hosts. Legacy Tantrum and movie/auto-fire/turret
    // templates stay available in the full research catalogue.
    // Auto-fire and turret now have their original targeting/spawn hosts;
    // movie-driven items and legacy Tantrum still remain archived.
    return IsPlayablePowerup(entry.resource);
}

const PowerupEntry *PowerupScene::GetSelected() const {
    if (m_selected >= m_catalog.size()) { return nullptr; }
    return &m_catalog[m_selected];
}

unsigned PowerupScene::GetCount() const {
    const PowerupEntry *entry = GetSelected();
    if (entry == nullptr) { return 0; }
    return m_profile.GetPowerupCount(entry->resource);
}

bool PowerupScene::Select(unsigned index) {
    if (index >= m_catalog.size() || !IsSupported(m_catalog[index])) { return false; }
    m_selected = index;
    return true;
}

void PowerupScene::Cycle() {
    for (unsigned step = 1; step <= m_catalog.size(); ++step) {
        const unsigned index = (m_selected + step) % m_catalog.size();
        if (IsSupported(m_catalog[index]) && m_profile.GetPowerupCount(m_catalog[index].resource) > 0) {
            Select(index);
            return;
        }
    }
}

bool PowerupScene::Use() {
    const PowerupEntry *entry = GetSelected();
    if (entry == nullptr || !IsSupported(*entry) || GetCount() == 0 || m_vitals.dead || !m_player.weapon) { return false; }
    // CBrother::UsePowerup :138000 guards this exact item before querying its
    // script. Native 29 exists, but the current turret's CanUse export is true.
    const bool turret = entry->resource.packHash == CStringToKey("pack5") && entry->resource.localIndex == 19;
    if (turret && m_player.weapon->brother.IsTurretActive()) { return false; }
    PowerupStatus status;
    status.healthPercent = static_cast<int>(std::lround(m_vitals.health * 100 / m_vitals.maximum));
    status.shield = m_player.weapon->brother.IsShield();
    status.autoFire = m_player.weapon->brother.IsAutoFire();
    status.turret = m_player.weapon->brother.IsTurretActive();
    for (unsigned type = 0; type < 3; ++type) { status.frenzyTypes[type] = m_player.weapon->brother.IsFrenzyType(type); }
    CPowerup query;
    query.Bind(entry->data, status);
    if (!query.Query(0) || !query.Query(1)) { return false; }
    const bool decrement = query.Query(3);
    CPowerup powerup;
    powerup.Bind(entry->data, status);
    powerup.Equip();
    powerup.Use();
    bool requested = false;
    for (const PowerupAction &action : powerup.TakeActions()) {
        if (action.function == 24) {
            // Keep the equipped reference stable while an animation is pending.
            if (!m_equipped.IsNull()) { return false; }
            m_equipped = entry->resource;
            m_player.weapon->brother.SetGrenade(0, action.resource, GetCount());
        } else if (action.function == 25) {
            requested = m_player.weapon->brother.OnThrowGrenade(0);
            if (!requested) { m_equipped = {}; }
        } else if (action.function == 10) {
            m_scene.AddHealth(action.arguments[0]);
            requested = true;
        } else if (action.function == 16) {
            m_player.weapon->brother.StartShield(action.resource, action.arguments[1] * 1000 / 256);
            requested = true;
        } else if (action.function == 22) {
            m_player.weapon->brother.StartAutoFire(action.resource, action.arguments[1]);
            requested = true;
        } else if (action.function == 27) {
            m_player.weapon->brother.StartFrenzyType(action.resource, action.arguments[1] * 1000 / 256,
                action.arguments[2] / 256.0f, action.arguments[3]);
            requested = true;
        } else if (action.function == 9) {
            GunCue cue;
            cue.kind = GunCue::Kind::Sound;
            cue.resource = action.resource;
            m_effects.Emit(cue, m_scene.playerX, m_scene.playerY, 0, 0, kPlayerCombatId);
        } else {
            ++failures;
            std::printf("[powerup] unhandled action=%u\n", action.function);
        }
    }
    if (requested && turret) { m_player.weapon->brother.SetTurretIsActive(true); }
    if (requested && decrement) {
        if (!m_profile.ConsumePowerup(entry->resource)) { ++failures; return false; }
        ++consumed;
    }
    return requested;
}

void PowerupScene::Update(int deltaMs) {
    if (!m_player.weapon) { return; }
    const unsigned thrown = m_player.weapon->brother.TakeThrownGrenades(0);
    if (thrown > 0) {
        if (!m_profile.ConsumePowerup(m_equipped, thrown)) { ++failures; }
        consumed += thrown;
        std::printf("[powerup] thrown=%u remaining=%u\n", thrown, m_profile.GetPowerupCount(m_equipped));
        m_equipped = {};
    }
    // A cancelled throw must not reserve inventory forever. Death and weapon
    // replacement destroy pending character animation; inventory stays intact.
    if (m_vitals.dead || !m_player.weapon->brother.HasGrenadeRequest(0)) {
        if (!m_equipped.IsNull() && m_equipped.packHash == CStringToKey("pack5") && m_equipped.localIndex == 19) {
            m_player.weapon->brother.SetTurretIsActive(false);
        }
        m_equipped = {};
    }
}
