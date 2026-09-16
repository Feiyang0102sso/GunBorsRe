/**
 * @file CScriptFunction.cpp
 * @brief Statement 0 -- call a function, script-level or native.
 */

#include "engine/glu/script/CScriptFunction.h"

#include "engine/glu/script/CScriptInterpreter.h"

#include <cstdio>

bool CScriptFunction::Execute(CScriptInterpreter &interpreter, ZScriptCursor &cursor) {
    const std::uint16_t functionId = cursor.ReadUInt16();
    const std::uint8_t declaredArgumentCount = cursor.ReadUInt8();

    std::uint16_t arguments[kScriptMaxArguments];
    std::uint8_t keptArgumentCount = 0;

    for (std::uint8_t i = 0; i < declaredArgumentCount; ++i) {
        const std::uint16_t operand = cursor.ReadUInt16();
        // Arguments past the register block are read anyway so the cursor
        // stays in step; the original writes them off the end of its stack
        // array instead, which is worth not reproducing.
        if (i < kScriptMaxArguments) {
            arguments[i] = operand;
            keptArgumentCount += 1;
        }
    }

    if (declaredArgumentCount > kScriptMaxArguments) {
        std::printf("[script] call %u declares %u arguments; only %u fit\n", functionId,
                    declaredArgumentCount, kScriptMaxArguments);
    }

    interpreter.CallFunction(functionId, keptArgumentCount, arguments);
    return false;
}

void CScriptFunction::Skip(ZScriptCursor &cursor) {
    const std::uint8_t *start = cursor.at;
    const std::uint8_t argumentCount = start[2];
    cursor.at = start + 3 + 2 * static_cast<std::size_t>(argumentCount);
}
