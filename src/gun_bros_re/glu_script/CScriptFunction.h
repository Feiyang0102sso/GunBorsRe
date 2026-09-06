/**
 * @file CScriptFunction.h
 * @brief Statement 0 -- call a function, script-level or native.
 *
 * Port of CScriptFunction (src/gluScript/scriptFunction.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:107118 (Skip), :107131 (Execute)
 *
 * Operands:
 *   uint16 functionId
 *   uint8  argumentCount
 *   uint16 arguments[argumentCount]
 *
 * An id of 0xFF or below names one of the script's own functions; anything
 * above goes out through ScriptResolver, its high byte picking the class.
 */

#ifndef GUN_BROS_RE_GLU_SCRIPT_CSCRIPTFUNCTION_H
#define GUN_BROS_RE_GLU_SCRIPT_CSCRIPTFUNCTION_H

#include "glu_script/CScriptCode.h"

class CScriptInterpreter;

/** A call statement. No state; the class exists to group the two halves. */
struct CScriptFunction {
    /** @return false always -- a call never ends the block it is in. */
    static bool Execute(CScriptInterpreter &interpreter, ScriptCursor &cursor);

    static void Skip(ScriptCursor &cursor);
};

#endif  // GUN_BROS_RE_GLU_SCRIPT_CSCRIPTFUNCTION_H
