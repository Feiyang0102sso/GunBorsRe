#include "engine/core/CRandGen.h"
#include <memory>
#pragma once
#include "engine/glu/script/ScriptResolver.h"
class CLevel;

// Class ids -- the high byte of every function and variable id.
constexpr std::uint8_t kScriptClassGame = 4;
constexpr std::uint8_t kScriptClassLevel = 5;
constexpr std::uint8_t kScriptClassBrother = 6;
constexpr std::uint8_t kScriptClassEnemy = 7;
constexpr std::uint8_t kScriptClassGun = 8;
constexpr std::uint8_t kScriptClassBullet = 9;
constexpr std::uint8_t kScriptClassPickup = 10;
constexpr std::uint8_t kScriptClassProp = 11;
constexpr std::uint8_t kScriptClassSpawner = 12;
constexpr std::uint8_t kScriptClassArmor = 13;
constexpr std::uint8_t kScriptClassMission = 14;
constexpr std::uint8_t kScriptClassPowerup = 15;

/**
 * The object a script runs on.
 *
 * Beyond the two resolvers, the interpreter needs three things from its host,
 * and the original takes them straight off the host's own vtable -- SetState
 * (:107323) calls the first two, Refresh (:107271) the third. They drive an
 * animation sequence, so only hosts that animate implement them; CLevel does
 * not.
 *
 * **The class declaring that vtable has not been recovered**, so these are
 * named for what the interpreter does with them rather than after a symbol.
 */
class ZGameScriptObject : public ZScriptObject {
public:
    virtual ~ZGameScriptObject() {}
    std::int16_t ResolveNativeFunction(std::uint16_t id, const std::int16_t *arguments, std::uint8_t count) override;
    std::int16_t *ResolveNativeVariable(std::uint16_t id) override;
    void SetLevelContext(CLevel *level) { m_levelContext = level; }
    CLevel *GetLevelContext() const { return m_levelContext; }
    void SetCooperative(bool enabled) { m_cooperative = enabled; }
    bool IsCooperative() const;
    void SetDeathmatch(bool enabled) { m_deathmatch = enabled; }
    bool IsDeathmatch() const;
    /** Per-host deterministic random stream; independent spawns get own seeds. */
    // Correction: attached actors consume one shared level stream, as CGame does.
    // The local seed is only used by standalone resource/viewer script hosts.
    void SetRandomSeed(std::uint32_t seed);
    CRandGen &GetRandom();
    std::int16_t RandomInteger(std::int16_t minimum, std::int16_t maximum) {
        return GetRandom().Integer(minimum, maximum);
    }
    /** CGame::VariableResolver :74387; normal play sets tutorial mode to -1. */
    std::int16_t *ResolveGameVariable(std::uint8_t variable);

    /** A state has just become current. */
    virtual void OnScriptStateEntered() {}

    /** Show this frame of the current state's sequence. */
    virtual void SetScriptSequenceFrame(std::uint8_t frame) { (void)frame; }

    /** Whether the frame on screen is finished, so the sequence may advance. */
    virtual bool IsScriptSequenceFrameFinished() { return false; }
private:
    friend class CGame;
    std::shared_ptr<CRandGen> m_random;
    std::uint32_t m_randomSeed = 1;
    bool m_cooperative = false;
    bool m_deathmatch = false;
    std::int16_t m_gameVariable = 0;
    CLevel *m_levelContext = nullptr;
};
