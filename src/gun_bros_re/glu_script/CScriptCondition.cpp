/**
 * @file CScriptCondition.cpp
 * @brief Statement 4 -- an if / else-if chain.
 */

#include "glu_script/CScriptCondition.h"

#include "glu_script/CScriptInterpreter.h"

namespace {

/**
 * Apply one comparison.
 *
 * Equality and the bit tests read their operands as unsigned; the four
 * orderings read them as signed. That split is in the original and matters: a
 * script comparing against 0xFFFF wants one, against -1 the other.
 */
bool Compare(std::uint8_t comparison, std::int16_t left, std::int16_t right) {
    const std::uint16_t unsignedLeft = static_cast<std::uint16_t>(left);
    const std::uint16_t unsignedRight = static_cast<std::uint16_t>(right);

    // Bit indices come out of script data, so they can name a bit that does
    // not exist. ARM shifts by a register take the low eight bits of the count
    // and yield zero past 31, which is what the guard reproduces.
    std::uint32_t bitMask = 0;
    const std::uint32_t shift = static_cast<std::uint32_t>(unsignedRight) & 0xFFu;
    if (shift < 32) {
        bitMask = 1u << shift;
    }

    switch (comparison) {
        case kScriptCmpEqual:
            return unsignedLeft == unsignedRight;
        case kScriptCmpNotEqual:
            return unsignedLeft != unsignedRight;
        case kScriptCmpGreater:
            return left > right;
        case kScriptCmpGreaterOrEqual:
            return left >= right;
        case kScriptCmpLess:
            return left < right;
        case kScriptCmpLessOrEqual:
            return left <= right;
        case kScriptCmpAlways:
            return true;
        case kScriptCmpBitSet:
            return (static_cast<std::uint32_t>(left) & bitMask) != 0;
        case kScriptCmpBitClear:
            return (static_cast<std::uint32_t>(left) & bitMask) == 0;
        case kScriptCmpAnyBitsShared:
            return (unsignedLeft & unsignedRight) != 0;
        case kScriptCmpNoBitsShared:
            return (unsignedLeft & unsignedRight) == 0;
        default:
            // An unknown comparison skips the arm rather than stopping the
            // script.
            return false;
    }
}

}  // namespace

bool CScriptCondition::Execute(CScriptInterpreter &interpreter, ScriptCursor &cursor) {
    for (;;) {
        const std::uint16_t leftOperand = cursor.ReadUInt16();
        const std::uint16_t rightOperand = cursor.ReadUInt16();
        const std::uint8_t comparison = cursor.ReadUInt8();

        // Two different scratch slots, because both operands may be literals
        // and they have to survive each other.
        const std::int16_t left = *interpreter.GetData(leftOperand, 0);
        const std::int16_t right = *interpreter.GetData(rightOperand, 1);

        if (!Compare(comparison, left, right)) {
            CScriptCode::Skip(cursor);
            const std::uint8_t continued = cursor.ReadUInt8();
            if (continued != kScriptConditionContinued) {
                return false;
            }
            continue;
        }

        const bool interrupted = CScriptCode::Execute(interpreter, cursor);
        if (interrupted) {
            return true;
        }

        // Arm taken and finished: step over whatever arms remain.
        std::uint8_t continued = cursor.ReadUInt8();
        while (continued == kScriptConditionContinued) {
            cursor.at += kScriptConditionHeaderBytes;
            CScriptCode::Skip(cursor);
            continued = cursor.ReadUInt8();
        }
        return false;
    }
}

void CScriptCondition::Skip(ScriptCursor &cursor) {
    std::uint8_t continued = 0;
    do {
        cursor.at += kScriptConditionHeaderBytes;
        CScriptCode::Skip(cursor);
        continued = cursor.ReadUInt8();
    } while (continued == kScriptConditionContinued);
}
