/** @file ZPickupScene.h
 * @brief Pickup lifetime, animated sprites and original collection rewards.
 */
#ifndef GUN_BROS_RE_ZPICKUPSCENE_H
#define GUN_BROS_RE_ZPICKUPSCENE_H
#include "gun_bros_re/data/ZPickupCatalog.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "engine/glu/sprite/CSpriteGlu.h"
#include "engine/glu/sprite/CSpriteIterator.h"
#include "engine/glu/sprite/CSpritePlayer.h"
#include <map>

class CLevel;
class ZWeaponEffects;
class CParticlePool;

struct ZPickupCollection {
    GameObjectRef resource;
    int objectId = 0;
    unsigned peer = 0;
};

class ZPickupScene {
public:
    ZPickupScene(CResTOCManager &toc, ZPackTables &tables, const ZShaderProgram &program,
        CProfileManager *profile = nullptr, std::shared_ptr<CParticlePool> particlePool = nullptr);
    bool Init();
    void SetPeerProfile(CProfileManager *profile) { m_peerProfile = profile; }
    void Reset();
    bool Spawn(const GameObjectRef &ref, float x, float y, int objectId = 0);
    void Update(int deltaMs, CLevel &scene, ZWeaponEffects &effects);
    void UpdateEffects(int deltaMs, ZWeaponEffects &effects);
    void Draw(const float *mvp, float scale);
    std::size_t GetCount() const { return m_instances.size(); }
    bool GetObjectPosition(int objectId, float &x, float &y) const;
    bool FindNearest(float x, float y, float &goalX, float &goalY) const;
    bool GetIndicatorTarget(unsigned serial, float &x, float &y) const;
    unsigned spawned = 0;
    unsigned collected = 0;
    unsigned failures = 0;
    std::vector<ZPickupCollection> collections;
private:
    struct Visual {
        const ZPickupEntry *entry = nullptr;
        std::vector<std::uint16_t> durations;
        std::vector<std::vector<ZSpriteQuad>> frames;
    };
    struct Instance {
        unsigned serial = 0;
        CPickup pickup;
        CSpritePlayer animation;
        Visual *visual = nullptr;
        float x = 0;
        float y = 0;
        int objectId = 0;
        std::uint64_t effectHandle = 0;
        bool effectStarted = false;
    };
    void GrantStoreItem(const GameObjectRef &ref, CProfileManager *profile);
    CResTOCManager &m_toc;
    ZPackTables &m_tables;
    const ZShaderProgram &m_program;
    CProfileManager *m_profile;
    CProfileManager *m_peerProfile = nullptr;
    ZWeaponEffects *m_effects = nullptr;
    std::shared_ptr<CParticlePool> m_particlePool;
    ZQuadBatch m_batch;
    std::vector<ZPickupEntry> m_catalog;
    std::vector<std::unique_ptr<Visual>> m_visuals;
    std::vector<std::unique_ptr<Instance>> m_instances;
    std::map<std::uint32_t, std::unique_ptr<CSpriteGlu>> m_spritePacks;
};
#endif
