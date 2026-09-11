/**
 * @file CScriptCondition.h
 * @brief Statement 4 -- an if / else-if chain.
 *
 * Port of CScriptCondition (src/gluScript/scriptCondition.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:106933 (Skip), :106956 (Execute)
 *
 * One arm is:
 *   uint16 left
 *   uint16 right
 *   uint8  comparison
 *   block
 *   uint8  continued      -- 1 means another arm follows
 *
 * An else is an arm whose comparison is kScriptCmpAlways. Arms that are not
 * taken have their block stepped over with CScriptCode::Skip, which is what
 * the block's leading length byte exists for.
 */

#ifndef GUN_BROS_RE_GLU_SCRIPT_CSCRIPTCONDITION_H
#define GUN_BROS_RE_GLU_SCRIPT_CSCRIPTCONDITION_H

#include "engine/glu/script/CScriptCode.h"

class CScriptInterpreter;

// The comparison byte.
constexpr std::uint8_t kScriptCmpEqual = 0;
constexpr std::uint8_t kScriptCmpNotEqual = 1;
constexpr std::uint8_t kScriptCmpGreater = 2;
constexpr std::uint8_t kScriptCmpGreaterOrEqual = 3;
constexpr std::uint8_t kScriptCmpLess = 4;
constexpr std::uint8_t kScriptCmpLessOrEqual = 5;
constexpr std::uint8_t kScriptCmpAlways = 6;  // the else arm
constexpr std::uint8_t kScriptCmpBitSet = 7;
constexpr std::uint8_t kScriptCmpBitClear = 8;
constexpr std::uint8_t kScriptCmpAnyBitsShared = 9;
constexpr std::uint8_t kScriptCmpNoBitsShared = 10;

// Follows an arm's block: 1 means another arm, anything else ends the chain.
constexpr std::uint8_t kScriptConditionContinued = 1;

// Bytes an arm spends before its block: two operands and the comparison.
constexpr std::size_t kScriptConditionHeaderBytes = 5;

/** A condition statement. */
struct CScriptCondition {
    /**
     * Take the first arm whose comparison holds.
     * @return true when the arm's block ended the enclosing block.
     */
    static bool Execute(CScriptInterpreter &interpreter, ScriptCursor &cursor);

    static void Skip(ScriptCursor &cursor);
};

#endif  // GUN_BROS_RE_GLU_SCRIPT_CSCRIPTCONDITION_H
