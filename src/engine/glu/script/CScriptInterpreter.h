/**
 * @file CScriptInterpreter.h
 * @brief One running instance of a script.
 *
 * Port of CScriptInterpreter (src/gluScript/scriptInterpreter.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:107176 (constructor), :107258
 *            (HandleEvent), :107271 (Refresh), :107323 (SetState), :107359
 *            (CallExportFunction), :107424 (GetData), :107496 (CallFunction),
 *            :107566 (SetScript)
 *
 * The interpreter holds everything that varies between two objects running the
 * same script: which state is current, where the animation sequence has got
 * to, the script's variables and data blocks, and the scratch space operands
 * are read through.
 *
 * Nothing here knows about the game. Native calls and class variables go out
 * through ScriptResolver.
 *
 * A script is driven from outside, never on its own:
 *   - CallExportFunction runs a named entry point (CLevel calls export 0 right
 *     after the map is bound, which is OnLevelStart);
 *   - HandleEvent offers an event to the current state;
 *   - Refresh advances the animation sequence, for hosts that have one.
 */

#ifndef GUN_BROS_RE_GLU_SCRIPT_CSCRIPTINTERPRETER_H
#define GUN_BROS_RE_GLU_SCRIPT_CSCRIPTINTERPRETER_H

#include "engine/glu/script/CScript.h"
#include "engine/glu/script/CScriptCode.h"
#include "engine/glu/script/CScriptResolver.h"

#include <cstdint>
#include <vector>

// Operand bits. Bit 15 marks a literal; among literals bit 14 says whether the
// word is already the int16 it stands for or just its low fifteen bits.
constexpr std::uint16_t kScriptOperandLiteral = 0x8000;
constexpr std::uint16_t kScriptOperandWideLiteral = 0x4000;

// Below this, an operand's low byte indexes the script's own variables; from
// here up it indexes the argument registers instead.
constexpr std::uint8_t kScriptFirstArgumentRegister = 0xFA;

// Function ids up to this are the script's own functions; above it the id
// names a native one, its high byte picking the class.
constexpr std::uint16_t kScriptLastLocalFunctionId = 0xFF;

// What CallExportFunction passes for an argument the caller did not supply.
constexpr std::int16_t kScriptNoArgument = 0x7FFF;

// Most arguments any call carries. The original decodes into an eight-entry
// stack array and copies that into the register block.
constexpr std::uint8_t kScriptMaxArguments = 8;

// Scratch slots. GetData writes a decoded literal into one of these and
// returns a pointer to it, so every operand can be handed back as an int16*
// no matter what kind it was. Three, because a Variable statement can be
// reading a destination, a source, and a data block entry at once.
constexpr std::uint32_t kScriptScratchSlots = 3;

/**
 * The two events the interpreter raises on itself as a sequence advances.
 *
 * Same encoding HandleEvent builds -- ((classId + 1) << 8) | index -- with
 * class 2, the engine's own animation class. Ended fires only while the
 * sequence sits on its last frame; Advanced fires on every step.
 */
constexpr std::uint16_t kScriptEventSequenceAdvanced = 0x300;
constexpr std::uint16_t kScriptEventSequenceEnded = 0x301;

class CScriptState;

/** A script, bound to a host, mid-run. */
class CScriptInterpreter {
public:
    CScriptInterpreter();

    /**
     * Bind a script and the object running it.
     *
     * Takes a copy of the script's variables and data blocks, which is what
     * makes two objects sharing a template independent of each other. The
     * script and host must outlive the interpreter.
     */
    void SetScript(const CScript &script, IScriptObject &host);

    bool HasScript() const;

    /** Whether a state has been entered yet. */
    bool IsStarted() const { return m_started; }

    std::uint8_t GetStateId() const { return m_stateId; }

    /**
     * Run an entry point by export id.
     *
     * The current state gets first refusal -- a state may override any export
     * -- and only when it has no handler does this fall back to the script's
     * export table. Reference: :107359
     */
    bool CallExportFunction(std::uint8_t exportId,
                            std::int16_t argument0 = kScriptNoArgument,
                            std::int16_t argument1 = kScriptNoArgument,
                            std::int16_t argument2 = kScriptNoArgument);

    /**
     * Offer an event to the current state and its ancestors.
     * Reference: :107258
     */
    bool HandleEvent(std::uint8_t classId, std::uint8_t eventIndex);

    /**
     * Make a state current: run the old state's exit code, then the new
     * state's enter code. Reference: :107323
     */
    bool SetState(std::uint8_t stateId);

    /**
     * Step the animation sequence on, raising the two sequence events.
     * Does nothing until the host reports the current frame finished.
     * Reference: :107271
     */
    void Refresh();

    /** Run one of the script's own functions by index. Reference: :107352 */
    bool CallFunctionDirect(std::uint8_t index);

    /**
     * Call a function, script-level or native.
     *
     * Arguments arrive as operands and are resolved to values here. An id past
     * kScriptLastLocalFunctionId goes out to the host, and its result is kept
     * as the return value the next Variable statement may read.
     * Reference: :107496
     */
    void CallFunction(std::uint16_t functionId, std::uint8_t argumentCount,
                      const std::uint16_t *arguments);

    /**
     * Turn one operand into the int16 it names.
     *
     * Four forms, and this is why reading scroll speeds without an interpreter
     * only ever worked for literals:
     *   - bit 15 set: a literal, decoded into scratch slot `slot`;
     *   - high byte non-zero: a class variable, resolved through the host;
     *   - low byte below 0xFA: one of the script's own variables;
     *   - low byte 0xFA and up: an argument register.
     *
     * @param slot Which scratch slot a literal may use, 0 to 2. Callers reading
     *             two operands at once must pass different slots.
     * @return never null -- an unresolvable class variable falls back to a
     *         scratch slot, matching the original.
     * Reference: :107424
     */
    std::int16_t *GetData(std::uint16_t operand, std::uint32_t slot);

    /** One entry of one data block. Reference: :107187 */
    std::int16_t GetDataBlockData(std::uint8_t block, std::uint16_t index) const;

    /**
     * Resolve a script's resource index to the pack and id it names.
     *
     * The original returns a pack index by asking CResTOCManager to convert
     * the hash (:107222); this hands back the hash, so the interpreter needs
     * nothing from the resource layer.
     *
     * @return false when the index is out of range.
     */
    bool GetResource(std::uint16_t index, std::uint32_t &packHash,
                     std::uint32_t &resourceId) const;

    /** The operand naming the last call's result. Used by Variable statements. */
    std::uint16_t GetReturnValueRef() const { return m_returnValueRef; }
    void SetReturnValueRef(std::uint16_t operand) { m_returnValueRef = operand; }

    const CScript *GetScript() const { return m_script; }
    IScriptObject *GetHost() const { return m_host; }

private:
    const CScript *m_script;
    IScriptObject *m_host;

    // The current state, and its id. Null until the first SetState.
    const CScriptState *m_state;
    std::uint8_t m_stateId;

    // How far into the current state's animation sequence.
    std::uint8_t m_sequencePosition;

    // Set once a state has been entered.
    bool m_started;

    // Operand naming where the last call left its result.
    std::uint16_t m_returnValueRef;

    // Decoded literals live here so operands can all be returned as pointers.
    std::int16_t m_scratch[kScriptScratchSlots];

    // Arguments of the call in progress. Only the first five are addressable
    // as operands -- low byte 0xFE down to 0xFA -- though a call may carry
    // more, which is how the original behaves too.
    std::int16_t m_arguments[kScriptMaxArguments];

    // The script's mutable half, copied per instance by SetScript.
    std::vector<std::int16_t> m_variables;
    std::vector<std::vector<std::int16_t>> m_dataBlocks;
};

#endif  // GUN_BROS_RE_GLU_SCRIPT_CSCRIPTINTERPRETER_H
