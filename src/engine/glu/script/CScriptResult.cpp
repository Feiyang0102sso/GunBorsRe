/**
 * @file CScriptResult.cpp
 * @brief Statement 1 -- change state.
 */

#include "engine/glu/script/CScriptResult.h"

#include "engine/glu/script/CScriptInterpreter.h"

bool CScriptResult::Execute(CScriptInterpreter &interpreter, ScriptCursor &cursor) {
    const std::uint8_t stateId = cursor.ReadUInt8();
    interpreter.SetState(stateId);
    return true;
}

void CScriptResult::Skip(ScriptCursor &cursor) { cursor.at += 1; }
