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

/** The engine calls only the host interface; GameScriptObject implements game enums and native dispatch. */
class IScriptObject {
public:
    virtual ~IScriptObject() = default;
    virtual std::int16_t ResolveNativeFunction(std::uint16_t id, const std::int16_t *arguments, std::uint8_t count) = 0;
    virtual std::int16_t *ResolveNativeVariable(std::uint16_t id) = 0;
    virtual void OnScriptStateEntered() {}
    virtual void SetScriptSequenceFrame(std::uint8_t frame) { (void)frame; }
    virtual bool IsScriptSequenceFrameFinished() { return false; }
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

