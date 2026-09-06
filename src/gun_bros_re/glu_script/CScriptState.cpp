/**
 * @file CScriptState.cpp
 * @brief One state of a script's state machine, and its parent chain.
 */

#include "glu_script/CScriptState.h"

#include "glu_script/CScript.h"
#include "glu_script/CScriptInterpreter.h"

namespace {

/**
 * The state one state inherits from, or null at the top of the chain.
 * Every lookup in this file walks upwards with it.
 */
const CScriptState *GetParentState(const CScriptState &state,
                                   const CScriptInterpreter &interpreter) {
    if (state.GetParent() == kNoParentState) {
        return nullptr;
    }

    const CScript *script = interpreter.GetScript();
    if (script == nullptr) {
        return nullptr;
    }

    return script->GetState(state.GetParent());
}

}  // namespace

CScriptState::CScriptState() : m_parent(kNoParentState) {}

void CScriptState::Parse(CArrayInputStream &stream) {
    m_parent = stream.ReadUInt8();

    const std::uint8_t sequenceLength = stream.ReadUInt8();
    m_sequence.resize(sequenceLength);
    for (std::uint8_t i = 0; i < sequenceLength; ++i) {
        m_sequence[i] = stream.ReadUInt8();
    }

    const std::uint8_t exportCount = stream.ReadUInt8();
    m_exports.resize(exportCount);
    for (std::uint8_t i = 0; i < exportCount; ++i) {
        m_exports[i].id = stream.ReadUInt8();
        m_exports[i].code.Parse(stream);
    }

    m_enterCode.Parse(stream);
    m_exitCode.Parse(stream);
}

std::uint8_t CScriptState::GetSequenceLength(const CScriptInterpreter &interpreter) const {
    const CScriptState *state = this;

    while (state->m_sequence.empty()) {
        state = GetParentState(*state, interpreter);
        if (state == nullptr) {
            return 0;
        }
    }

    return static_cast<std::uint8_t>(state->m_sequence.size());
}

const std::uint8_t *CScriptState::GetSequence(const CScriptInterpreter &interpreter) const {
    const CScriptState *state = this;

    while (state->m_sequence.empty()) {
        state = GetParentState(*state, interpreter);
        if (state == nullptr) {
            return nullptr;
        }
    }

    return state->m_sequence.data();
}

bool CScriptState::Execute(CScriptInterpreter &interpreter) const {
    const CScriptState *state = this;

    while (state->m_enterCode.IsEmpty()) {
        const CScriptState *parent = GetParentState(*state, interpreter);
        if (parent == nullptr) {
            // Nothing to run anywhere up the chain. The original still calls
            // through to the empty block, which comes to the same thing.
            return false;
        }
        state = parent;
    }

    return state->m_enterCode.Execute(interpreter);
}

bool CScriptState::OnExit(CScriptInterpreter &interpreter) const {
    const CScriptState *state = this;

    while (state->m_exitCode.IsEmpty()) {
        const CScriptState *parent = GetParentState(*state, interpreter);
        if (parent == nullptr) {
            return false;
        }
        state = parent;
    }

    return state->m_exitCode.Execute(interpreter);
}

bool CScriptState::Evaluate(CScriptInterpreter &interpreter, std::uint16_t eventId) const {
    const CScriptState *state = this;

    // Unlike the lookups above, this does not stop at the first state with
    // code: an event unhandled here may still be handled by an ancestor.
    while (state != nullptr) {
        if (state->m_enterCode.Evaluate(interpreter, eventId)) {
            return true;
        }
        state = GetParentState(*state, interpreter);
    }

    return false;
}
