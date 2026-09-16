/**
 * @file CScriptCode.h
 * @brief A block of gluScript bytecode.
 *
 * Port of CScriptCode (src/gluScript/scriptCode.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:106762 (Parse), :106783 (Skip),
 *            :106801 (Execute), :106866 (Evaluate)
 *
 * Wire format:
 *   uint8 byteLength     -- of everything after this byte
 *   uint8 statementCount
 *   statements[statementCount]
 *
 * The length prefix is what makes Skip O(1), and skipping is not an
 * optimisation here: it is how a condition steps over the arm it did not take.
 *
 * A statement is an opcode byte and then its operands, each opcode handled by
 * its own class:
 *
 *   0 CScriptFunction   1 CScriptResult   2 CScriptVariable
 *   3 CScriptEvent      4 CScriptCondition 5 CScriptReturn
 *
 * Every uint16 operand is an addressing word rather than a value; see
 * CScriptInterpreter::GetData for the four things one can mean.
 */

#ifndef GUN_BROS_RE_GLU_SCRIPT_CSCRIPTCODE_H
#define GUN_BROS_RE_GLU_SCRIPT_CSCRIPTCODE_H

#include "engine/resources/CArrayInputStream.h"

#include <cstdint>
#include <vector>

class CScriptInterpreter;

// Statement opcodes, as switched on in Execute and Evaluate.
constexpr std::uint8_t kScriptOpFunction = 0;
constexpr std::uint8_t kScriptOpResult = 1;
constexpr std::uint8_t kScriptOpVariable = 2;
constexpr std::uint8_t kScriptOpEvent = 3;
constexpr std::uint8_t kScriptOpCondition = 4;
constexpr std::uint8_t kScriptOpReturn = 5;

/**
 * Where execution is inside a block.
 *
 * The original passes a two-word stack structure holding the block and a
 * moving byte pointer; only the pointer is ever read, so only it is here.
 */
struct ZScriptCursor {
    const std::uint8_t *at;

    explicit ZScriptCursor(const std::uint8_t *start) : at(start) {}

    std::uint8_t ReadUInt8();
    std::uint16_t ReadUInt16();
};

/** One block of bytecode. */
class CScriptCode {
public:
    CScriptCode();

    /** Read a length-prefixed block off the stream. */
    void Parse(CArrayInputStream &stream);

    /** Start of the block, which is where a cursor into it begins. */
    const std::uint8_t *Begin() const;

    /** Whether the block holds no statements, so running it would do nothing. */
    bool IsEmpty() const;

    /** Declared length, for the parse-consumed-everything check. */
    std::uint8_t GetByteLength() const;

    std::uint8_t GetStatementCount() const;

    /**
     * Run every statement.
     *
     * @return true when a statement ended the block early -- a state change or
     *         a return. The caller passes that up so an enclosing condition
     *         stops too.
     */
    static bool Execute(CScriptInterpreter &interpreter, ZScriptCursor &cursor);

    /** Convenience form: run this block from its start. */
    bool Execute(CScriptInterpreter &interpreter) const;

    /**
     * Look for an event handler rather than running anything.
     *
     * Walks the same statements skipping all of them except CScriptEvent,
     * whose id is matched against `eventId`; on a match that event's block is
     * executed. This is why an event handler is inert during Execute -- the
     * two passes split the block between them.
     */
    static bool Evaluate(CScriptInterpreter &interpreter, ZScriptCursor &cursor,
                         std::uint16_t eventId);

    /** Convenience form: evaluate this block from its start. */
    bool Evaluate(CScriptInterpreter &interpreter, std::uint16_t eventId) const;

    /** Step the cursor over a whole block without running it. */
    static void Skip(ZScriptCursor &cursor);

private:
    // The block exactly as stored: byte 0 is the length, byte 1 the statement
    // count, the rest the statements. Kept whole because every cursor walks it
    // in place, and Skip needs that leading length.
    std::vector<std::uint8_t> m_bytes;
};

#endif  // GUN_BROS_RE_GLU_SCRIPT_CSCRIPTCODE_H
