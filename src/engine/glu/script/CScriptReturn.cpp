/**
 * @file CScriptReturn.cpp
 * @brief Statement 5 -- set the value the caller reads back.
 */

#include "engine/glu/script/CScriptReturn.h"

#include "engine/glu/script/CScriptInterpreter.h"

bool CScriptReturn::Execute(CScriptInterpreter &interpreter, ZScriptCursor &cursor) {
    const std::uint16_t operand = cursor.ReadUInt16();
    interpreter.SetReturnValueRef(operand);
    return true;
}

void CScriptReturn::Skip(ZScriptCursor &cursor) { cursor.at += 2; }
