/**
 * @file CScriptReturn.h
 * @brief Statement 5 -- set the value the caller reads back.
 *
 * Port of CScriptReturn (src/gluScript/scriptReturn.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:107706 (Skip), :107713 (Execute)
 *
 * Operands:
 *   uint16 value
 *
 * What gets stored is the operand, not what it resolves to, so a function can
 * return a variable and have the caller read it live. Ends the block.
 */

#ifndef GUN_BROS_RE_GLU_SCRIPT_CSCRIPTRETURN_H
#define GUN_BROS_RE_GLU_SCRIPT_CSCRIPTRETURN_H

#include "engine/glu/script/CScriptCode.h"

class CScriptInterpreter;

/** A return statement. */
struct CScriptReturn {
    /** @return true always. */
    static bool Execute(CScriptInterpreter &interpreter, ScriptCursor &cursor);

    static void Skip(ScriptCursor &cursor);
};

#endif  // GUN_BROS_RE_GLU_SCRIPT_CSCRIPTRETURN_H
