#include "gun_bros_re/host/ZGameScriptObject.h"
/** @file CPickup.h
 * @brief Original pickup template and collection script, independent of drawing.
 */
#ifndef GUN_BROS_RE_CPICKUP_H
#define GUN_BROS_RE_CPICKUP_H
#include "gun_bros_re/data/CGameAssetRef.h"
#include "engine/glu/script/CScriptInterpreter.h"
#include "engine/glu/sprite/CSpritePlayer.h"
#include <memory>

class CParticleEffect;
class CParticleSystem;
class ZQuadBatch;
class ZPackTables;

struct ZPickupAction {
    enum class Kind { Xplodium, Experience, Health, Sound, StoreItem };
    Kind kind = Kind::Xplodium;
    int amount = 0;
    GameObjectRef resource;
};

class CPickup : public ZGameScriptObject {
public:
    /** CPickup::Template::Init :99591; section 13, original object type 12. */
    struct Template {
        CGameAssetRef name;
        CGameSpriteGluRef sprite;
        GameObjectRef particleEffect;
        CScript script;
        std::vector<GameObjectRef> items;
        bool Init(CArrayInputStream &stream);
        /** Read one BIG section-13 resource and verify its complete wire payload. */
        bool Load(ZPackTables &tables, const GameObjectRef &resource);
    };
    ~CPickup();
    void Bind(const Template &data);
    /** Runtime binding uses the same template plus stable desktop Sprite data. */
    bool Bind(const Template &data, const GameObjectRef &resource, CSpriteGlu &glu,
        std::shared_ptr<CParticleSystem> particles, const CParticleEffect *effect);
    void Spawn(float x, float y, int objectId, unsigned serial);
    void Update(int deltaMs);
    void OnRemove();
    void Draw(ZQuadBatch &batch, float scale) const;
    float GetX() const { return m_x; }
    float GetY() const { return m_y; }
    int GetID() const { return m_objectId; }
    unsigned GetSerial() const { return m_serial; }
    const GameObjectRef &GetResource() const { return m_resource; }
    /** Export 0 runs exactly once, when either living brother touches it. */
    bool Collect();
    bool IsCollected() const { return m_collected; }
    unsigned GetUnsupportedCount() const { return m_unsupported; }
    std::vector<ZPickupAction> TakeActions();
    std::int16_t FunctionResolver(std::uint8_t function,
        const std::int16_t *arguments, std::uint8_t argumentCount);
private:
    const Template *m_template = nullptr;
    GameObjectRef m_resource;
    CSpritePlayer m_animation;
    std::shared_ptr<CParticleSystem> m_particles;
    const CParticleEffect *m_effect = nullptr;
    std::uint64_t m_effectHandle = 0;
    float m_x = 0, m_y = 0;
    int m_objectId = 0;
    unsigned m_serial = 0; // Desktop indicator identity, not the original pool UID.
    CScriptInterpreter m_interpreter;
    bool m_collected = false;
    unsigned m_unsupported = 0;
    std::vector<ZPickupAction> m_actions;
};
#endif
