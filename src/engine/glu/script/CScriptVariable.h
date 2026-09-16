/**
 * @file CScriptVariable.h
 * @brief Statement 2 -- assign to or update a variable.
 *
 * Port of CScriptVariable (src/gluScript/scriptVariable.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:107948 (Skip), :107984 (Execute)
 *
 * Operands:
 *   uint16 destination
 *   uint8  operation
 *   uint16 source        -- unless the operation is kScriptVarFromFunction, in
 *                           which case a whole Function statement follows
 */

#ifndef GUN_BROS_RE_GLU_SCRIPT_CSCRIPTVARIABLE_H
#define GUN_BROS_RE_GLU_SCRIPT_CSCRIPTVARIABLE_H

#include "engine/glu/script/CScriptCode.h"

class CScriptInterpreter;

// The operation byte between the two operands.
constexpr std::uint8_t kScriptVarAdd = 0;
constexpr std::uint8_t kScriptVarSubtract = 1;
constexpr std::uint8_t kScriptVarIncrement = 2;
constexpr std::uint8_t kScriptVarDecrement = 3;
constexpr std::uint8_t kScriptVarMultiply = 4;
constexpr std::uint8_t kScriptVarDivide = 5;
constexpr std::uint8_t kScriptVarAssign = 6;
constexpr std::uint8_t kScriptVarSetBit = 7;
constexpr std::uint8_t kScriptVarClearBit = 8;

// Not an operation but a form: the right-hand side is a function's result, so
// a whole Function statement takes the place of the source operand.
constexpr std::uint8_t kScriptVarFromFunction = 10;

// Set in the operation byte when the source is an index into a data block
// rather than a value. Bits 4 to 6 then hold which block.
constexpr std::uint8_t kScriptVarDataBlockFlag = 0x80;

/** A variable statement. */
struct CScriptVariable {
    /** @return false always -- an assignment never ends the block. */
    static bool Execute(CScriptInterpreter &interpreter, ZScriptCursor &cursor);

    static void Skip(ZScriptCursor &cursor);
};

#endif  // GUN_BROS_RE_GLU_SCRIPT_CSCRIPTVARIABLE_H
