/** @file PickupScene.h
 * @brief Pickup lifetime, animated sprites and original collection rewards.
 */
#ifndef GUN_BROS_RE_PICKUPSCENE_H
#define GUN_BROS_RE_PICKUPSCENE_H
#include "runtime/PickupCatalog.h"
#include "gun_bros/CProfileManager.h"
#include "sprite_glu/CSpriteGlu.h"
#include "sprite_glu/CSpriteIterator.h"
#include "sprite_glu/CSpritePlayer.h"
#include <map>

class CombatScene;
class WeaponEffects;

struct PickupCollection {
    GameObjectRef resource;
    int objectId = 0;
};

class PickupScene {
public:
    PickupScene(CResTOCManager &toc, PackTables &tables, const CShaderProgram &program,
        CProfileManager *profile = nullptr);
    bool Init();
    void Reset();
    bool Spawn(const GameObjectRef &ref, float x, float y, int objectId = 0);
    void Update(int deltaMs, CombatScene &scene, WeaponEffects &effects);
    void UpdateEffects(int deltaMs, WeaponEffects &effects);
    void Draw(const float *mvp, float scale);
    std::size_t GetCount() const { return m_instances.size(); }
    unsigned spawned = 0;
    unsigned collected = 0;
    unsigned failures = 0;
    std::vector<PickupCollection> collections;
private:
    struct Visual {
        const PickupEntry *entry = nullptr;
        std::vector<std::uint16_t> durations;
        std::vector<std::vector<SpriteQuad>> frames;
    };
    struct Instance {
        CPickup pickup;
        CSpritePlayer animation;
        Visual *visual = nullptr;
        float x = 0;
        float y = 0;
        int objectId = 0;
        std::uint64_t effectHandle = 0;
        bool effectStarted = false;
    };
    void GrantStoreItem(const GameObjectRef &ref);
    CResTOCManager &m_toc;
    PackTables &m_tables;
    const CShaderProgram &m_program;
    CProfileManager *m_profile;
    WeaponEffects *m_effects = nullptr;
    CQuadBatch m_batch;
    std::vector<PickupEntry> m_catalog;
    std::vector<std::unique_ptr<Visual>> m_visuals;
    std::vector<std::unique_ptr<Instance>> m_instances;
    std::map<std::uint32_t, std::unique_ptr<CSpriteGlu>> m_spritePacks;
};
#endif
