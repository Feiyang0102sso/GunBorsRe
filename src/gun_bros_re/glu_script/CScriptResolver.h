/**
 * @file CScriptResolver.h
 * @brief Where a script's native calls and class variables go.
 *
 * Port of ScriptResolver (src/gluScript/scriptResolver.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:107580 (ResolveVariable),
 *            :107633 (ResolveFunction)
 *
 * Both are one switch on the id's high byte, handing the host pointer to the
 * class that byte names:
 *
 *   4 CGame   5 CLevel  6 CBrother  7 CEnemy   8 CGun      9 CBullet
 *   A CPickup B CProp   C Spawner   D CArmor   E Mission   F CPowerup
 *
 * The host pointer is the same whichever class the id picks -- the original
 * passes a void pointer and each class casts it -- so a script can only call
 * functions of a class its own host actually is. Every host in these archives
 * is a CLevel.
 *
 * **Cases 5 and 7 are implemented.** CLevel arrived with M3.4; CEnemy arrived
 * with M3.8, which needed it because an enemy's own script is what assembles
 * its model out of parts. The other ten classes are M4 and M5; a call to one
 * is logged with its id and arguments and returns zero, which is how the list
 * of what to build next gets collected.
 *
 * The comment above about every host being a CLevel no longer holds: a CEnemy
 * hosts its own script, and calls a class-7 id.
 */

#ifndef GUN_BROS_RE_GLU_SCRIPT_CSCRIPTRESOLVER_H
#define GUN_BROS_RE_GLU_SCRIPT_CSCRIPTRESOLVER_H

#include <cstdint>

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
class IScriptObject {
public:
    virtual ~IScriptObject() {}
    /** Per-host deterministic random stream; independent spawns get own seeds. */
    void SetRandomSeed(std::uint32_t seed) { m_randomState = seed; }
    std::int16_t RandomInteger(std::int16_t minimum, std::int16_t maximum) {
        int first = minimum;
        int last = maximum;
        if (first > last) { first = maximum; last = minimum; }
        m_randomState = m_randomState * 1664525u + 1013904223u;
        return static_cast<std::int16_t>(first + (m_randomState >> 8) % (last - first + 1));
    }

    /** A state has just become current. */
    virtual void OnScriptStateEntered() {}

    /** Show this frame of the current state's sequence. */
    virtual void SetScriptSequenceFrame(std::uint8_t frame) { (void)frame; }

    /** Whether the frame on screen is finished, so the sequence may advance. */
    virtual bool IsScriptSequenceFrameFinished() { return false; }
private:
    std::uint32_t m_randomState = 1;
};

namespace ScriptResolver {

/**
 * Run one native function on the host.
 *
 * @param functionId    High byte the class, low byte the function's ordinal
 *                      within it.
 * @param arguments     Already resolved to values.
 * @return the function's result, or zero for the many that return nothing and
 *         for every class not implemented yet.
 */
std::int16_t ResolveFunction(IScriptObject *host, std::uint16_t functionId,
                             const std::int16_t *arguments,
                             std::uint8_t argumentCount);

/**
 * Where one class variable lives, so a script can read and write it.
 *
 * @return a pointer into the host's own state, or null when that class or
 *         variable is not implemented. The interpreter then reads and writes a
 *         scratch slot instead, exactly as the original does.
 */
std::int16_t *ResolveVariable(IScriptObject *host, std::uint16_t variableId);

}  // namespace ScriptResolver

#endif  // GUN_BROS_RE_GLU_SCRIPT_CSCRIPTRESOLVER_H
