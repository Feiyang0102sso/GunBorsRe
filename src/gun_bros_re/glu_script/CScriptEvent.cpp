/**
 * @file CScriptEvent.cpp
 * @brief Statement 3 -- an event handler.
 */

#include "glu_script/CScriptEvent.h"

#include "glu_script/CScriptInterpreter.h"

bool CScriptEvent::Evaluate(CScriptInterpreter &interpreter, ScriptCursor &cursor,
                            std::uint16_t eventId) {
    const std::uint8_t low = cursor.ReadUInt8();
    const std::uint8_t high = cursor.ReadUInt8();
    const std::uint16_t descriptor = static_cast<std::uint16_t>(low | (high << 8));

    bool matched = false;
    if ((low & kScriptEventMaskForm) != 0) {
        const bool sameClass =
            ((static_cast<std::uint16_t>(high << 8) ^ eventId) & 0xFF00) == 0;

        // The original masks the result with 0x7F, so only bits 0 to 6 of the
        // descriptor can ever match; a wider index matches nothing.
        const std::uint8_t bitIndex = static_cast<std::uint8_t>(eventId & 0x7F);
        bool bitSet = false;
        if (bitIndex < 7) {
            bitSet = (descriptor & (1u << bitIndex) & 0x7Fu) != 0;
        }

        matched = sameClass && bitSet;
    } else {
        matched = (descriptor == eventId);
    }

    if (matched) {
        return CScriptCode::Execute(interpreter, cursor);
    }

    CScriptCode::Skip(cursor);
    return false;
}

void CScriptEvent::Skip(ScriptCursor &cursor) {
    cursor.at += 2;
    CScriptCode::Skip(cursor);
}
