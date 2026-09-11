/**
 * @file CScriptVariable.cpp
 * @brief Statement 2 -- assign to or update a variable.
 */

#include "engine/glu/script/CScriptVariable.h"

#include "engine/glu/script/CScriptFunction.h"
#include "engine/glu/script/CScriptInterpreter.h"

#include <cstdio>

namespace {

/**
 * Bit mask for a set-bit or clear-bit operation.
 *
 * The bit index comes out of script data, so it can name a bit that does not
 * exist. ARM shifts by a register take the low eight bits of the count and
 * yield zero past 31, which is what this reproduces.
 */
std::uint32_t BitMaskFor(std::int16_t bitIndex) {
    const std::uint32_t shift = static_cast<std::uint32_t>(bitIndex) & 0xFFu;
    if (shift >= 32) {
        return 0;
    }
    return 1u << shift;
}

}  // namespace

bool CScriptVariable::Execute(CScriptInterpreter &interpreter, ScriptCursor &cursor) {
    const std::uint16_t destinationOperand = cursor.ReadUInt16();
    const std::uint8_t operation = cursor.ReadUInt8();

    if (operation == kScriptVarFromFunction) {
        // The source is a whole call statement, inline. It leaves its result
        // behind as an operand, which is then read like any other.
        CScriptFunction::Execute(interpreter, cursor);
        std::int16_t *destination = interpreter.GetData(destinationOperand, 0);
        *destination = *interpreter.GetData(interpreter.GetReturnValueRef(), 1);
        return false;
    }

    const std::uint16_t sourceOperand = cursor.ReadUInt16();
    std::int16_t *destination = interpreter.GetData(destinationOperand, 0);
    std::int16_t value = *interpreter.GetData(sourceOperand, 1);

    std::uint8_t effectiveOperation = operation;
    if ((operation & kScriptVarDataBlockFlag) != 0) {
        // The source was an index into a data block, and what it finds there
        // is itself an operand.
        const std::uint8_t block = static_cast<std::uint8_t>((operation >> 4) & 7);
        const std::int16_t entry =
            interpreter.GetDataBlockData(block, static_cast<std::uint16_t>(value));
        effectiveOperation = static_cast<std::uint8_t>(operation & 0x0F);
        value = *interpreter.GetData(static_cast<std::uint16_t>(entry), 2);
    }

    switch (effectiveOperation) {
        case kScriptVarAdd:
            *destination = static_cast<std::int16_t>(*destination + value);
            break;
        case kScriptVarSubtract:
            *destination = static_cast<std::int16_t>(*destination - value);
            break;
        case kScriptVarIncrement:
            *destination = static_cast<std::int16_t>(*destination + 1);
            break;
        case kScriptVarDecrement:
            *destination = static_cast<std::int16_t>(*destination - 1);
            break;
        case kScriptVarMultiply:
            *destination = static_cast<std::int16_t>(*destination * value);
            break;
        case kScriptVarDivide:
            // The original divides straight through. Script data deciding to
            // divide by zero would take the process down, so it is the one
            // case worth checking.
            if (value == 0) {
                std::printf("[script] variable divide by zero, left unchanged\n");
                break;
            }
            *destination = static_cast<std::int16_t>(*destination / value);
            break;
        case kScriptVarAssign:
            *destination = value;
            break;
        case kScriptVarSetBit:
            *destination = static_cast<std::int16_t>(
                static_cast<std::uint32_t>(static_cast<std::uint16_t>(*destination)) |
                BitMaskFor(value));
            break;
        case kScriptVarClearBit:
            *destination = static_cast<std::int16_t>(
                static_cast<std::uint32_t>(static_cast<std::uint16_t>(*destination)) &
                ~BitMaskFor(value));
            break;
        default:
            break;
    }

    return false;
}

void CScriptVariable::Skip(ScriptCursor &cursor) {
    const std::uint8_t *start = cursor.at;
    const std::uint8_t operation = start[2];

    if (operation == kScriptVarFromFunction) {
        cursor.at = start + 3;
        CScriptFunction::Skip(cursor);
        return;
    }

    cursor.at = start + 5;
}
