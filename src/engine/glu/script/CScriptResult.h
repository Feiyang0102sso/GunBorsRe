/**
 * @file CScriptResult.h
 * @brief Statement 1 -- change state.
 *
 * Port of CScriptResult (src/gluScript/scriptResult.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:107686 (Skip), :107693 (Execute)
 *
 * Operands:
 *   uint8 stateId
 *
 * Always ends the block it is in: the statements after it belong to a state
 * that is no longer current.
 */

#ifndef GUN_BROS_RE_GLU_SCRIPT_CSCRIPTRESULT_H
#define GUN_BROS_RE_GLU_SCRIPT_CSCRIPTRESULT_H

#include "engine/glu/script/CScriptCode.h"

class CScriptInterpreter;

/** A state change statement. */
struct CScriptResult {
    /** @return true always. */
    static bool Execute(CScriptInterpreter &interpreter, ScriptCursor &cursor);

    static void Skip(ScriptCursor &cursor);
};

#endif  // GUN_BROS_RE_GLU_SCRIPT_CSCRIPTRESULT_H
