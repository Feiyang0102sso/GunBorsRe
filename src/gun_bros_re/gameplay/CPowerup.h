#include "gun_bros_re/gameplay/ZGameScriptObject.h"
/** @file CPowerup.h
 * @brief Original consumable template (CPowerup::Template::Init :187947).
 */
#ifndef GUN_BROS_RE_CPOWERUP_H
#define GUN_BROS_RE_CPOWERUP_H
#include "gun_bros_re/data/CGameAssetRef.h"
#include "engine/glu/script/CScriptInterpreter.h"
#include <array>

struct ZPowerupAction {
    std::uint8_t function = 0;
    std::array<std::int16_t, 8> arguments{};
    std::uint8_t count = 0;
    GameObjectRef resource;
};

/** Native queries read a snapshot; gameplay applies emitted actions itself. */
struct ZPowerupStatus {
    int healthPercent = 50;
    bool shield = false;
    bool frenzy = false;
    bool autoFire = false;
    bool turret = false;
    std::array<bool, 4> frenzyTypes{};
};

class CPowerup : public ZGameScriptObject {
public:
    struct Template {
        CGameAssetRef name;
        CGameSpriteGluRef sprite;
        std::uint8_t field28 = 0;
        std::uint8_t field29 = 0;
        CScript script;
        std::uint8_t field30 = 0;
        std::uint8_t field112 = 0;
        GameObjectRef effect;
        std::uint8_t field124 = 0;
        bool Init(CArrayInputStream &stream);
    };
    void Bind(const Template &data, const ZPowerupStatus &status = {});
    /** Original exports: 0 equipable, 1 usable, 2 selector, 3 decrement, 4 default. */
    bool Query(std::uint8_t exportId, int argument = kScriptNoArgument);
    void Equip();
    void Use(bool fromSelector = false);
    void Update(int deltaMs);
    void HandleEvent(std::uint8_t event);
    void OnScriptStateEntered() override { m_timerMs = 0; }
    bool IsDone() const { return m_done; }
    unsigned GetStateId() const { return m_interpreter.GetStateId(); }
    unsigned GetUnsupportedCount() const { return m_unsupported; }
    std::vector<ZPowerupAction> TakeActions();
    std::int16_t FunctionResolver(std::uint8_t function, const std::int16_t *arguments, std::uint8_t count);
private:
    const Template *m_template = nullptr;
    CScriptInterpreter m_interpreter;
    ZPowerupStatus m_status;
    std::vector<ZPowerupAction> m_actions;
    int m_timerMs = 0;
    bool m_done = false;
    unsigned m_unsupported = 0;
};
#endif
