/** @file ZPowerupScene.h
 * @brief Desktop consumable host; original scripts choose actual actions.
 */
#ifndef GUN_BROS_RE_ZPOWERUPSCENE_H
#define GUN_BROS_RE_ZPOWERUPSCENE_H
#include "gun_bros_re/data/ZPowerupCatalog.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/gameplay/ZCombatWorld.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/gameplay/ZPowerupMoviePlayer.h"

class ZPowerupScene {
public:
    ZPowerupScene(CResTOCManager &toc, ZPackTables &tables, ZPlayerModel &player,
        ZPlayerVitals &vitals, ZCombatWorld &scene, ZWeaponEffects &effects, CProfileManager &profile, ZCombatId owner = kPlayerCombatId);
    bool Init();
    void SetDeathmatch(CMPMatch *match) { m_match = match; }
    bool UseMatchConsumable(bool grenade);
    bool Select(unsigned index);
    bool SelectResource(const GameObjectRef &resource);
    /** Resolve saved ordinals, or original export 4 for unselected slots. */
    GameObjectRef GetEquipped(unsigned slot);
    bool Equip(unsigned slot, const GameObjectRef &resource);
    void Cycle();
    bool Use(bool fromSelector = false);
    bool UseAny(bool grantTestCharge = false);
    /** Host bot policy only; ordinary player and cheat input still use Use. */
    bool CanBotUseSelected() const;
    static constexpr float BotGrenadeRadius = 250.0f;
    bool HasAfterDeathPowerup() const;
    bool UseAfterDeathPowerup();
    void Update(int deltaMs);
    void Reset();
    bool DrawMovies();
    bool IsMovieActive() const { return m_moviePlayer.IsActive(); }
    const ZPowerupMoviePlayer &GetMoviePlayer() const { return m_moviePlayer; }
    const ZPowerupEntry *GetSelected() const;
    unsigned GetCount() const;
    unsigned GetCount(unsigned localIndex) const;
    const std::map<unsigned, int> &Cooldowns() const { return m_cooldowns; }
    std::vector<std::string> TakeUseMessages() {
        std::vector<std::string> messages;
        messages.swap(m_useMessages);
        return messages;
    }
    unsigned consumed = 0;
    unsigned failures = 0;
private:
    struct BotUseRules { bool airstrike = false; bool grenade = false; };
    std::vector<BotUseRules> m_botUseRules;
    bool IsSupported(const ZPowerupEntry &entry) const;
    const CStoreItem *FindStoreItem(const ZPowerupEntry &entry) const;
    bool ModeAllows(const ZPowerupEntry &entry) const;
    bool MatchAllows(const ZPowerupEntry &entry) const;
    bool HasUnlimitedMatchInventory() const;
    void CommitMatchUse(const GameObjectRef &resource);
    CMPMatch *m_match = nullptr;
    std::map<unsigned, int> m_cooldowns;
    std::vector<std::string> m_useMessages;
    CResTOCManager &m_toc;
    ZPackTables &m_tables;
    ZPlayerModel &m_player;
    ZPlayerVitals &m_vitals;
    ZCombatWorld &m_scene;
    ZWeaponEffects &m_effects;
    CProfileManager &m_profile;
    ZPowerupMoviePlayer m_moviePlayer;
    ZCombatId m_owner;
    unsigned m_choice = 0;
    std::vector<ZPowerupEntry> m_catalog;
    std::vector<ZStoreEntry> m_store;
    GameObjectRef m_equipped;
    unsigned m_selected = 13;
};
#endif
