/**
 * @file CScriptResult.cpp
 * @brief Statement 1 -- change state.
 */

#include "glu_script/CScriptResult.h"

#include "glu_script/CScriptInterpreter.h"

bool CScriptResult::Execute(CScriptInterpreter &interpreter, ScriptCursor &cursor) {
    const std::uint8_t stateId = cursor.ReadUInt8();
    interpreter.SetState(stateId);
    return true;
}

void CScriptResult::Skip(ScriptCursor &cursor) { cursor.at += 1; }
