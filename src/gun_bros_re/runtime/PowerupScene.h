/** @file PowerupScene.h
 * @brief Desktop consumable host; original scripts choose actual actions.
 */
#ifndef GUN_BROS_RE_POWERUPSCENE_H
#define GUN_BROS_RE_POWERUPSCENE_H
#include "runtime/PowerupCatalog.h"
#include "runtime/CombatScene.h"
#include "gun_bros/CProfileManager.h"

class PowerupScene {
public:
    PowerupScene(CResTOCManager &toc, PackTables &tables, PlayerModel &player,
        PlayerVitals &vitals, CombatScene &scene, WeaponEffects &effects, CProfileManager &profile);
    bool Init();
    bool Select(unsigned index);
    void Cycle();
    bool Use();
    void Update(int deltaMs);
    const PowerupEntry *GetSelected() const;
    unsigned GetCount() const;
    unsigned consumed = 0;
    unsigned failures = 0;
private:
    bool IsSupported(const PowerupEntry &entry) const;
    CResTOCManager &m_toc;
    PackTables &m_tables;
    PlayerModel &m_player;
    PlayerVitals &m_vitals;
    CombatScene &m_scene;
    WeaponEffects &m_effects;
    CProfileManager &m_profile;
    std::vector<PowerupEntry> m_catalog;
    GameObjectRef m_equipped;
    unsigned m_selected = 13;
};
#endif
