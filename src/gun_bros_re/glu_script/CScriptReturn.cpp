/**
 * @file CScriptReturn.cpp
 * @brief Statement 5 -- set the value the caller reads back.
 */

#include "glu_script/CScriptReturn.h"

#include "glu_script/CScriptInterpreter.h"

bool CScriptReturn::Execute(CScriptInterpreter &interpreter, ScriptCursor &cursor) {
    const std::uint16_t operand = cursor.ReadUInt16();
    interpreter.SetReturnValueRef(operand);
    return true;
}

void CScriptReturn::Skip(ScriptCursor &cursor) { cursor.at += 2; }
