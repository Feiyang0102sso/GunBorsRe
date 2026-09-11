#include "gun_bros_re/gameplay/GameScriptObject.h"
/** @file CPickup.h
 * @brief Original pickup template and collection script, independent of drawing.
 */
#ifndef GUN_BROS_RE_CPICKUP_H
#define GUN_BROS_RE_CPICKUP_H
#include "gun_bros_re/data/CGameAssetRef.h"
#include "engine/glu/script/CScriptInterpreter.h"

struct PickupAction {
    enum class Kind { Xplodium, Experience, Health, Sound, StoreItem };
    Kind kind = Kind::Xplodium;
    int amount = 0;
    GameObjectRef resource;
};

class CPickup : public GameScriptObject {
public:
    /** CPickup::Template::Init :99591; section 13, original object type 12. */
    struct Template {
        CGameAssetRef name;
        CGameSpriteGluRef sprite;
        GameObjectRef particleEffect;
        CScript script;
        std::vector<GameObjectRef> items;
        bool Init(CArrayInputStream &stream);
    };
    void Bind(const Template &data);
    /** Export 0 runs exactly once, when either living brother touches it. */
    bool Collect();
    bool IsCollected() const { return m_collected; }
    unsigned GetUnsupportedCount() const { return m_unsupported; }
    std::vector<PickupAction> TakeActions();
    std::int16_t FunctionResolver(std::uint8_t function,
        const std::int16_t *arguments, std::uint8_t argumentCount);
private:
    const Template *m_template = nullptr;
    CScriptInterpreter m_interpreter;
    bool m_collected = false;
    unsigned m_unsupported = 0;
    std::vector<PickupAction> m_actions;
};
#endif
