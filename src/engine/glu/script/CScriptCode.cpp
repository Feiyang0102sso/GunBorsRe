/**
 * @file CScriptCode.cpp
 * @brief A block of gluScript bytecode.
 */

#include "engine/glu/script/CScriptCode.h"

#include "engine/glu/script/CScriptCondition.h"
#include "engine/glu/script/CScriptEvent.h"
#include "engine/glu/script/CScriptFunction.h"
#include "engine/glu/script/CScriptResult.h"
#include "engine/glu/script/CScriptReturn.h"
#include "engine/glu/script/CScriptVariable.h"

std::uint8_t ScriptCursor::ReadUInt8() {
    const std::uint8_t value = *at;
    at += 1;
    return value;
}

std::uint16_t ScriptCursor::ReadUInt16() {
    const std::uint16_t low = at[0];
    const std::uint16_t high = at[1];
    at += 2;
    return static_cast<std::uint16_t>(low | (high << 8));
}

CScriptCode::CScriptCode() {}

void CScriptCode::Parse(CArrayInputStream &stream) {
    const std::uint8_t length = stream.ReadUInt8();

    // The length byte is kept in front of the data because Skip reads it back
    // off a cursor that has no other way to know how far to jump.
    m_bytes.resize(static_cast<std::size_t>(length) + 1);
    m_bytes[0] = length;
    for (std::uint8_t i = 0; i < length; ++i) {
        m_bytes[static_cast<std::size_t>(i) + 1] = stream.ReadUInt8();
    }
}

const std::uint8_t *CScriptCode::Begin() const {
    if (m_bytes.empty()) {
        return nullptr;
    }
    return m_bytes.data();
}

bool CScriptCode::IsEmpty() const { return GetStatementCount() == 0; }

std::uint8_t CScriptCode::GetByteLength() const {
    if (m_bytes.empty()) {
        return 0;
    }
    return m_bytes[0];
}

std::uint8_t CScriptCode::GetStatementCount() const {
    // A block of length zero has no statement count byte either.
    if (m_bytes.size() < 2) {
        return 0;
    }
    return m_bytes[1];
}

void CScriptCode::Skip(ScriptCursor &cursor) {
    const std::uint8_t length = cursor.at[0];
    cursor.at += static_cast<std::size_t>(length) + 1;
}

bool CScriptCode::Execute(CScriptInterpreter &interpreter, ScriptCursor &cursor) {
    // Step over the length byte, then take the statement count.
    cursor.at += 1;
    const std::uint8_t statementCount = cursor.ReadUInt8();

    for (std::uint8_t i = 0; i < statementCount; ++i) {
        const std::uint8_t opcode = cursor.ReadUInt8();
        bool interrupted = false;

        switch (opcode) {
            case kScriptOpFunction:
                interrupted = CScriptFunction::Execute(interpreter, cursor);
                break;
            case kScriptOpResult:
                interrupted = CScriptResult::Execute(interpreter, cursor);
                break;
            case kScriptOpVariable:
                interrupted = CScriptVariable::Execute(interpreter, cursor);
                break;
            case kScriptOpEvent:
                // Handlers are inert while running; only Evaluate looks at them.
                CScriptEvent::Skip(cursor);
                continue;
            case kScriptOpCondition:
                interrupted = CScriptCondition::Execute(interpreter, cursor);
                break;
            case kScriptOpReturn:
                interrupted = CScriptReturn::Execute(interpreter, cursor);
                break;
            default:
                // An unknown opcode cannot be skipped -- its length is not
                // known -- so the original simply moves to the next statement,
                // which desynchronises. Kept as it is; nothing in these
                // archives reaches it.
                continue;
        }

        if (interrupted) {
            return true;
        }
    }

    return false;
}

bool CScriptCode::Execute(CScriptInterpreter &interpreter) const {
    const std::uint8_t *start = Begin();
    if (start == nullptr) {
        return false;
    }
    ScriptCursor cursor(start);
    return Execute(interpreter, cursor);
}

bool CScriptCode::Evaluate(CScriptInterpreter &interpreter, ScriptCursor &cursor,
                           std::uint16_t eventId) {
    cursor.at += 1;
    const std::uint8_t statementCount = cursor.ReadUInt8();

    for (std::uint8_t i = 0; i < statementCount; ++i) {
        const std::uint8_t opcode = cursor.ReadUInt8();

        switch (opcode) {
            case kScriptOpFunction:
                CScriptFunction::Skip(cursor);
                break;
            case kScriptOpResult:
                CScriptResult::Skip(cursor);
                break;
            case kScriptOpVariable:
                CScriptVariable::Skip(cursor);
                break;
            case kScriptOpEvent:
                if (CScriptEvent::Evaluate(interpreter, cursor, eventId)) {
                    return true;
                }
                break;
            case kScriptOpCondition:
                CScriptCondition::Skip(cursor);
                break;
            case kScriptOpReturn:
                CScriptReturn::Skip(cursor);
                break;
            default:
                break;
        }
    }

    return false;
}

bool CScriptCode::Evaluate(CScriptInterpreter &interpreter, std::uint16_t eventId) const {
    const std::uint8_t *start = Begin();
    if (start == nullptr) {
        return false;
    }
    ScriptCursor cursor(start);
    return Evaluate(interpreter, cursor, eventId);
}
