#pragma once
#include "engine/glu/script/CScriptResolver.h"
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
class GameScriptObject : public IScriptObject {
public:
    virtual ~GameScriptObject() {}
    std::int16_t ResolveNativeFunction(std::uint16_t id, const std::int16_t *arguments, std::uint8_t count) override;
    std::int16_t *ResolveNativeVariable(std::uint16_t id) override;
    void SetLevelContext(CLevel *level) { m_levelContext = level; }
    CLevel *GetLevelContext() const { return m_levelContext; }
    /** Per-host deterministic random stream; independent spawns get own seeds. */
    void SetRandomSeed(std::uint32_t seed) { m_randomState = seed; }
    std::int16_t RandomInteger(std::int16_t minimum, std::int16_t maximum) {
        int first = minimum;
        int last = maximum;
        if (first > last) { first = maximum; last = minimum; }
        m_randomState = m_randomState * 1664525u + 1013904223u;
        return static_cast<std::int16_t>(first + (m_randomState >> 8) % (last - first + 1));
    }

    /** CGame::VariableResolver :74387; normal play sets tutorial mode to -1. */
    std::int16_t *ResolveGameVariable(std::uint8_t variable) {
        switch (variable) {
        case 0: m_gameVariable = RandomInteger(0, 1); break;
        case 1: m_gameVariable = RandomInteger(0, 3); break;
        case 2: m_gameVariable = RandomInteger(0, 100); break;
        case 3: m_gameVariable = RandomInteger(0, 1000); break;
        case 4: m_gameVariable = 0; break; // Single-player, not co-op.
        case 5: m_gameVariable = 0; break; // Not deathmatch.
        case 6: m_gameVariable = -1; break; // CGunBros menu :94113.
        default: return nullptr;
        }
        return &m_gameVariable;
    }

    /** A state has just become current. */
    virtual void OnScriptStateEntered() {}

    /** Show this frame of the current state's sequence. */
    virtual void SetScriptSequenceFrame(std::uint8_t frame) { (void)frame; }

    /** Whether the frame on screen is finished, so the sequence may advance. */
    virtual bool IsScriptSequenceFrameFinished() { return false; }
private:
    std::uint32_t m_randomState = 1;
    std::int16_t m_gameVariable = 0;
    CLevel *m_levelContext = nullptr;
};

