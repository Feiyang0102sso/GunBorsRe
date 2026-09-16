/** @file ZPowerupScene.cpp
 * @brief Separate availability, request and the actual grenade inventory commit.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/ZPowerupScene.h"
#include "gun_bros_re/gameplay/CMPMatch.h"
#include "gun_bros_re/gameplay/CBullet.h"
#include "engine/core/CStringToKey.h"
#include <cmath>
#include <cstdio>

ZPowerupScene::ZPowerupScene(CResTOCManager &toc, ZPackTables &tables, ZPlayerModel &player,
    ZPlayerVitals &vitals, ZCombatWorld &scene, ZWeaponEffects &effects, CProfileManager &profile, ZCombatId owner)
    : m_toc(toc), m_tables(tables), m_player(player), m_vitals(vitals), m_scene(scene),
      m_effects(effects), m_profile(profile), m_moviePlayer(toc, tables, scene), m_owner(owner) { m_moviePlayer.SetOwner(owner); }

bool ZPowerupScene::Init() {
    if (!LoadPowerupCatalog(m_toc, m_tables, m_catalog) || !LoadStoreCatalog(m_toc, m_tables, m_store)) { return false; }
    m_botUseRules.clear();
    for (const auto &entry : m_catalog) {
        if (IsPlayablePowerup(entry.resource) && FindStoreItem(entry) == nullptr) {
            std::printf("[powerup] missing mode rules resource=%08x:%u\n", entry.resource.packHash, entry.resource.localIndex);
            return false;
        }
        BotUseRules rules;
        // POWERUP script resources: 253 denotes a Movie dependency. Retail
        // offensive Movies are air strikes; afterDeath is a separate action.
        // powerup_template.bt; CPowerup::FunctionResolver native 1 :188267.
        for (const auto &ref : entry.data.script.GetResources()) {
            if (ref.sectionOrType == 253 && entry.data.field112 == 0) { rules.airstrike = true; }
            if (ref.sectionOrType != static_cast<unsigned>(ZGameSection::Bullet) - 1) { continue; }
            std::vector<std::uint8_t> bytes;
            if (!m_tables.ReadSectionResource(ref.packHash, ZGameSection::Bullet, ref.resourceId, bytes)) { return false; }
            CArrayInputStream input(bytes);
            CBullet::Template bullet;
            if (!bullet.Init(input) || input.Available() != 0) {
                std::printf("[powerup] invalid bot projectile resource=%08x:%u\n", ref.packHash, ref.resourceId);
                return false;
            }
            // CBullet::IsGrenade :60370 uses flag bit 4, not the powerup ID.
            if ((bullet.GetFlags() & 16) != 0) { rules.grenade = true; }
        }
        m_botUseRules.push_back(rules);
    }
    return true;
}

const CStoreItem *ZPowerupScene::FindStoreItem(const ZPowerupEntry &entry) const {
    // Follow the same dedicated STORE -> POWERUP reference as the selector;
    // bundle contents do not define an individual powerup's mode restrictions.
    for (const auto &store : m_store) {
        const auto &item = store.data;
        if (item.type < 10 || item.type > 13 || item.objects.empty()) { continue; }
        const auto &reference = item.objects.front();
        if (reference.type == 17 && reference.object.packHash == entry.resource.packHash &&
            reference.object.localIndex == entry.resource.localIndex) { return &item; }
    }
    return nullptr;
}

bool ZPowerupScene::ModeAllows(const ZPowerupEntry &entry) const {
    const auto *store = FindStoreItem(entry);
    if (store == nullptr) { return false; }
    unsigned gameType = 0;
    if (m_scene.IsLocalLive()) { gameType = 1; }
    if (m_match != nullptr || m_scene.IsDeathmatch()) { gameType = 2; }
    return !store->IsExcludedFromGameType(gameType);
}

bool ZPowerupScene::MatchAllows(const ZPowerupEntry &entry) const {
    if (m_match == nullptr || m_owner != kBrotherCombatId) { return true; }
    // Hard removes the host's item/life budget, never retail STORE mode rules.
    if (m_match->HasHardBot()) { return m_match->CanUse(1, false) && entry.data.field112 == 0; }
    if (entry.resource.packHash != CStringToKey("pack5")) { return false; }
    const unsigned id = entry.resource.localIndex;
    // Resource identities, not replacement effect data. Effects remain Flow-driven.
    if (id != 13 && id != 1 && id != 8 && id != 9) { return false; }
    return m_match->CanUse(1, id == 13);
}

bool ZPowerupScene::HasUnlimitedMatchInventory() const {
    return m_match != nullptr && m_match->HasUnlimitedBotPowerups() && m_owner == kBrotherCombatId;
}

void ZPowerupScene::CommitMatchUse(const GameObjectRef &resource) {
    if (m_match == nullptr) { return; }
    if (m_owner == kBrotherCombatId && resource.packHash == CStringToKey("pack5")) {
        // Only Easy's two budget categories belong in these counters.
        if (resource.localIndex == 13) { m_match->CommitUse(1, true); }
        else if (resource.localIndex == 1 || resource.localIndex == 8 || resource.localIndex == 9) { m_match->CommitUse(1, false); }
    }
    for (const auto &entry : m_catalog) {
        if (entry.resource.packHash == resource.packHash && entry.resource.localIndex == resource.localIndex) {
            m_cooldowns[resource.localIndex] = entry.data.field124 * 1000;
            if (m_owner == kPlayerCombatId) { m_useMessages.push_back(entry.name); }
            break;
        }
    }
}

bool ZPowerupScene::UseMatchConsumable(bool grenade) {
    // Prefer the largest available health pack; the script rejects full health.
    for (auto entry = m_catalog.rbegin(); entry != m_catalog.rend(); ++entry) {
        const unsigned id = entry->resource.localIndex;
        if (entry->resource.packHash != CStringToKey("pack5")) { continue; }
        if (grenade && id != 13) { continue; }
        if (!grenade && id != 1 && id != 8 && id != 9) { continue; }
        if (SelectResource(entry->resource) && Use(!grenade)) { return true; }
    }
    return false;
}

GameObjectRef ZPowerupScene::GetEquipped(unsigned slot) {
    if (slot >= 2) { return {}; }
    const auto ordinal = m_profile.configuration.powerups[slot];
    bool excluded = false;
    // Bind :187378 searches the selector catalog by its stored ordinal.
    for (const auto &entry : m_catalog) {
        if (entry.resource.localIndex != ordinal) { continue; }
        if (IsSupported(entry)) { return entry.resource; }
        // Bind :187421 replaces a mode-excluded saved choice via export 4.
        excluded = !ModeAllows(entry);
        break;
    }
    if (ordinal != 255 && !excluded) {
        std::printf("[powerup] unresolved equipped slot=%u ordinal=%u\n", slot, ordinal);
        return {};
    }
    for (const auto &entry : m_catalog) {
        CPowerup query;
        query.SetLevelContext(m_scene.GetLevel());
        query.Bind(entry.data);
        if (IsSupported(entry) && query.Query(4, slot)) {
            m_profile.configuration.powerups[slot] = entry.resource.localIndex;
            if (excluded) {
                std::printf("[powerup] mode excluded slot=%u ordinal=%u default=%u\n", slot, ordinal, entry.resource.localIndex);
            }
            return entry.resource;
        }
    }
    return {};
}

bool ZPowerupScene::Equip(unsigned slot, const GameObjectRef &resource) {
    if (slot >= 2) { return false; }
    for (const auto &entry : m_catalog) {
        if (entry.resource.packHash != resource.packHash || entry.resource.localIndex != resource.localIndex) { continue; }
        CPowerup query;
        query.SetLevelContext(m_scene.GetLevel());
        query.Bind(entry.data);
        if (!IsSupported(entry) || !query.Query(0)) { return false; }
        m_profile.configuration.powerups[slot] = resource.localIndex;
        std::printf("[powerup] equipped slot=%u resource=%08x:%u\n", slot, resource.packHash, resource.localIndex);
        return true;
    }
    return false;
}

bool ZPowerupScene::IsSupported(const ZPowerupEntry &entry) const {
    if (!ModeAllows(entry)) { return false; }
    if (!MatchAllows(entry)) { return false; }
    // Expose only completed hosts. Legacy Tantrum and movie/auto-fire/turret
    // templates stay available in the full research catalogue.
    // Auto-fire and turret now have their original targeting/spawn hosts;
    // movie-driven items and legacy Tantrum still remain archived.
    // Movie-driven air strikes now have real timeline/callback hosts below.
    // Legacy Tantrum now follows its original timer/effect and expiry callbacks.
    return IsPlayablePowerup(entry.resource);
}

const ZPowerupEntry *ZPowerupScene::GetSelected() const {
    if (m_selected >= m_catalog.size()) { return nullptr; }
    return &m_catalog[m_selected];
}

unsigned ZPowerupScene::GetCount() const {
    const ZPowerupEntry *entry = GetSelected();
    if (entry == nullptr) { return 0; }
    // A virtual charge avoids granting or persisting fake account inventory.
    if (HasUnlimitedMatchInventory() && IsSupported(*entry)) { return 1; }
    return m_profile.GetPowerupCount(entry->resource);
}

unsigned ZPowerupScene::GetCount(unsigned localIndex) const {
    for (const ZPowerupEntry &entry : m_catalog) {
        if (entry.resource.localIndex != localIndex) { continue; }
        if (HasUnlimitedMatchInventory() && IsSupported(entry)) { return 1; }
        return m_profile.GetPowerupCount(entry.resource);
    }
    return 0;
}

bool ZPowerupScene::Select(unsigned index) {
    if (index >= m_catalog.size() || !IsSupported(m_catalog[index])) { return false; }
    m_selected = index;
    return true;
}

bool ZPowerupScene::SelectResource(const GameObjectRef &resource) {
    for (unsigned index = 0; index < m_catalog.size(); ++index) {
        const GameObjectRef &candidate = m_catalog[index].resource;
        if (candidate.packHash == resource.packHash && candidate.localIndex == resource.localIndex) { return Select(index); }
    }
    return false;
}

void ZPowerupScene::Cycle() {
    for (unsigned step = 1; step <= m_catalog.size(); ++step) {
        const unsigned index = (m_selected + step) % m_catalog.size();
        if (IsSupported(m_catalog[index]) && m_profile.GetPowerupCount(m_catalog[index].resource) > 0) {
            Select(index);
            return;
        }
    }
}

bool ZPowerupScene::Use(bool fromSelector) {
    if (m_match != nullptr && m_match->GetResult() != CMPMatch::Result::Playing) { return false; }
    if (m_moviePlayer.IsActive()) { return false; }
    const ZPowerupEntry *entry = GetSelected();
    if (entry == nullptr || !IsSupported(*entry) || GetCount() == 0 || !m_player.weapon) { return false; }
    if (!m_player.weapon->brother.HasSpawned()) { return false; }
    if (m_match != nullptr && m_cooldowns[entry->resource.localIndex] > 0) { return false; }
    if (m_vitals.dead != (entry->data.field112 != 0)) { return false; }
    // CBrother::UsePowerup :138000 guards this exact item before querying its
    // script. Native 29 exists, but the current turret's CanUse export is true.
    const bool turret = entry->resource.packHash == CStringToKey("pack5") && entry->resource.localIndex == 19;
    if (turret && m_player.weapon->brother.IsTurretActive()) { return false; }
    ZPowerupStatus status;
    status.healthPercent = static_cast<int>(std::lround(m_vitals.health * 100 / m_vitals.maximum));
    status.shield = m_player.weapon->brother.IsShield();
    status.frenzy = m_player.weapon->brother.IsFrenzy();
    status.autoFire = m_player.weapon->brother.IsAutoFire();
    status.turret = m_player.weapon->brother.IsTurretActive();
    for (unsigned type = 0; type < 3; ++type) { status.frenzyTypes[type] = m_player.weapon->brother.IsFrenzyType(type); }
    CPowerup query;
    query.SetLevelContext(m_scene.GetLevel());
    query.Bind(entry->data, status);
    if (!query.Query(1)) { return false; }
    if (fromSelector && !query.Query(2)) { return false; }
    if (!fromSelector && !query.Query(0)) { return false; }
    const bool decrement = query.Query(3);
    const unsigned itemIndex = entry->resource.localIndex;
    if (entry->data.field112 != 0 || itemIndex == 0 || itemIndex == 10 || itemIndex == 11) {
        if (!m_moviePlayer.Start(*entry, fromSelector)) { ++failures; return false; }
        if (decrement) {
            if (!HasUnlimitedMatchInventory() && !m_profile.ConsumePowerup(entry->resource)) { m_moviePlayer.Reset(); ++failures; return false; }
            ++consumed;
            CommitMatchUse(entry->resource);
            if (m_owner == kPlayerCombatId) { m_scene.RecordChallengePowerup(entry->resource); }
        }
        return true;
    }
    CPowerup powerup;
    powerup.SetLevelContext(m_scene.GetLevel());
    powerup.Bind(entry->data, status);
    powerup.Equip();
    powerup.Use();
    bool requested = false;
    for (const ZPowerupAction &action : powerup.TakeActions()) {
        if (action.function == 24) {
            // Keep the equipped reference stable while an animation is pending.
            if (!m_equipped.IsNull()) { return false; }
            m_equipped = entry->resource;
            m_player.weapon->brother.SetGrenade(0, action.resource, GetCount());
        } else if (action.function == 25) {
            requested = m_player.weapon->brother.OnThrowGrenade(0);
            if (!requested) { m_equipped = {}; }
        } else if (action.function == 10) {
            if (!m_vitals.dead) { m_vitals.health = std::min(m_vitals.maximum, m_vitals.health + action.arguments[0]); }
            requested = true;
        } else if (action.function == 16) {
            m_player.weapon->brother.StartShield(action.resource, action.arguments[1] * 1000 / 256);
            requested = true;
        } else if (action.function == 22) {
            m_player.weapon->brother.StartAutoFire(action.resource, action.arguments[1]);
            requested = true;
        } else if (action.function == 17) {
            m_player.weapon->brother.StartFrenzy(action.resource, action.arguments[1] * 1000 / 256,
                action.arguments[2] / 256.0f, action.arguments[3] / 256.0f, action.arguments[4] / 256.0f);
            requested = true;
        } else if (action.function == 27) {
            m_player.weapon->brother.StartFrenzyType(action.resource, action.arguments[1] * 1000 / 256,
                action.arguments[2] / 256.0f, action.arguments[3]);
            requested = true;
        } else if (action.function == 9) {
            ZGunCue cue;
            cue.kind = ZGunCue::Kind::Sound;
            cue.resource = action.resource;
            float x = 0, y = 0;
            m_scene.ActorPosition(m_owner, x, y);
            m_effects.Emit(cue, x, y, 0, 0, m_owner);
        } else {
            ++failures;
            std::printf("[powerup] unhandled action=%u\n", action.function);
        }
    }
    if (requested && turret) { m_player.weapon->brother.SetTurretIsActive(true); }
    if (requested && decrement) {
        if (!HasUnlimitedMatchInventory() && !m_profile.ConsumePowerup(entry->resource)) { ++failures; return false; }
        ++consumed;
        CommitMatchUse(entry->resource);
        if (m_owner == kPlayerCombatId) { m_scene.RecordChallengePowerup(entry->resource); }
    }
    return requested;
}

void ZPowerupScene::Update(int deltaMs) {
    for (auto &cooldown : m_cooldowns) { cooldown.second = std::max(0, cooldown.second - deltaMs); }
    m_moviePlayer.Update(deltaMs);
    if (!m_player.weapon) { return; }
    const unsigned thrown = m_player.weapon->brother.TakeThrownGrenades(0);
    if (thrown > 0) {
        if (!HasUnlimitedMatchInventory() && !m_profile.ConsumePowerup(m_equipped, thrown)) { ++failures; }
        consumed += thrown;
        CommitMatchUse(m_equipped);
        for (unsigned index = 0; index < thrown; ++index) { if (m_owner == kPlayerCombatId) { m_scene.RecordChallengePowerup(m_equipped); } }
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

void ZPowerupScene::Reset() {
    m_cooldowns.clear();
    m_useMessages.clear();
    m_equipped = {};
    m_moviePlayer.Reset();
}

bool ZPowerupScene::DrawMovies() { return m_moviePlayer.Draw(); }

bool ZPowerupScene::HasAfterDeathPowerup() const {
    if (m_match != nullptr) { return false; }
    for (const auto &entry : m_catalog) {
        if (entry.data.field112 != 0 && IsSupported(entry) && m_profile.GetPowerupCount(entry.resource) > 0) { return true; }
    }
    return false;
}

bool ZPowerupScene::UseAfterDeathPowerup() {
    if (m_match != nullptr) { return false; }
    for (unsigned index = 0; index < m_catalog.size(); ++index) {
        if (m_catalog[index].data.field112 != 0 && Select(index) && Use(true)) { return true; }
    }
    return false;
}

bool ZPowerupScene::UseAny(bool grantTestCharge) {
    if (m_catalog.empty()) { return false; }
    // Input policy only: every attempt still enters the original CanUse/Use.
    m_choice = m_choice * 1664525u + 1013904223u;
    for (unsigned offset = 0; offset < m_catalog.size(); ++offset) {
        const unsigned index = (m_choice % m_catalog.size() + offset) % m_catalog.size();
        if (!Select(index)) { continue; }
        if (!grantTestCharge && m_scene.IsLocalLive() && !CanBotUseSelected()) { continue; }
        const auto &ref = m_catalog[index].resource;
        bool granted = false;
        if (grantTestCharge && m_owner == kBrotherCombatId && m_scene.HasLocalBot() && GetCount() == 0) {
            m_profile.AddPowerup(ref, 1); granted = true;
        }
        if (Use()) { return true; }
        // Health and other selector-only actions still pass their original exports.
        if (HasUnlimitedMatchInventory() && Use(true)) { return true; }
        if (granted) { m_profile.ConsumePowerup(ref); }
    }
    return false;
}

bool ZPowerupScene::CanBotUseSelected() const {
    if (m_selected >= m_botUseRules.size() || m_vitals.dead || m_scene.IsRescuePending()) { return false; }
    const auto &rules = m_botUseRules[m_selected];
    if (!rules.airstrike && !rules.grenade) { return true; }
    unsigned alive = 0, nearby = 0;
    float x = 0, y = 0;
    m_scene.ActorPosition(m_owner, x, y);
    for (const auto &actor : m_scene.enemies) {
        const auto &enemy = actor->model.enemy.combat;
        if (enemy.dead || enemy.removed || !enemy.enabled || enemy.health <= 0) { continue; }
        ++alive;
        const float dx = enemy.x - x, dy = enemy.y - y;
        if (dx * dx + dy * dy <= BotGrenadeRadius * BotGrenadeRadius) { ++nearby; }
    }
    if (rules.airstrike && alive <= 10) { return false; }
    if (rules.grenade && nearby < 2) { return false; }
    return true;
}
