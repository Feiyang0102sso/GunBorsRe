/**
 * @file CScriptInterpreter.cpp
 * @brief One running instance of a script.
 */

#include "engine/glu/script/CScriptInterpreter.h"

#include "engine/glu/script/CScriptState.h"

#include <cstdio>

CScriptInterpreter::CScriptInterpreter()
    : m_script(nullptr),
      m_host(nullptr),
      m_state(nullptr),
      m_stateId(0),
      m_sequencePosition(0),
      m_started(false),
      m_returnValueRef(0) {
    for (std::uint32_t i = 0; i < kScriptScratchSlots; ++i) {
        m_scratch[i] = 0;
    }
    for (std::uint8_t i = 0; i < kScriptMaxArguments; ++i) {
        m_arguments[i] = 0;
    }
}

void CScriptInterpreter::SetScript(const CScript &script, IScriptObject &host) {
    m_script = &script;
    m_host = &host;
    m_state = nullptr;
    m_stateId = 0;
    m_sequencePosition = 0;
    m_started = false;
    m_returnValueRef = 0;

    // The script itself is read-only and shared; its variables and data blocks
    // are not, so each instance gets its own copy seeded from the template.
    // Reference: :106211 (AllocateStorage), :105985 (InitializeStorage)
    m_variables = script.GetVariableInitialValues();
    m_dataBlocks = script.GetDataBlocks();
}

bool CScriptInterpreter::HasScript() const {
    return m_script != nullptr && m_script->IsPresent();
}

std::int16_t *CScriptInterpreter::GetData(std::uint16_t operand, std::uint32_t slot) {
    if (slot >= kScriptScratchSlots) {
        slot = 0;
    }

    if ((operand & kScriptOperandLiteral) != 0) {
        // Two literal widths. With bit 14 set the word already is the int16 it
        // stands for, sign bit included; without it the value is the low
        // fifteen bits, so a literal can never be negative that way.
        std::int16_t value = 0;
        if ((operand & kScriptOperandWideLiteral) != 0) {
            value = static_cast<std::int16_t>(operand);
        } else {
            value = static_cast<std::int16_t>(operand & 0x7FFF);
        }

        m_scratch[slot] = value;
        return &m_scratch[slot];
    }

    const std::uint8_t classId = static_cast<std::uint8_t>((operand >> 8) & 0xFF);
    const std::uint8_t low = static_cast<std::uint8_t>(operand & 0xFF);

    if (classId != 0) {
        std::int16_t *variable = nullptr;
        if (m_host != nullptr) {
            variable = ScriptResolver::ResolveVariable(m_host, operand);
        }
        if (variable != nullptr) {
            return variable;
        }

        // The host does not implement it. The original hands back a scratch
        // slot too, so the script keeps running and the value goes nowhere.
        return &m_scratch[slot];
    }

    if (low < kScriptFirstArgumentRegister) {
        if (low < m_variables.size()) {
            return &m_variables[low];
        }

        std::printf("[script] variable %u past the script's %u\n", low,
                    static_cast<unsigned>(m_variables.size()));
        return &m_scratch[slot];
    }

    // Argument registers count downwards from 0xFE, so 0xFE is the first.
    const int argumentIndex = 254 - static_cast<int>(low);
    if (argumentIndex >= 0 && argumentIndex < static_cast<int>(kScriptMaxArguments)) {
        return &m_arguments[argumentIndex];
    }

    // 0xFF is the one index that lands before the register block, on the last
    // scratch slot. Kept because the original does the same arithmetic.
    return &m_scratch[kScriptScratchSlots - 1];
}

std::int16_t CScriptInterpreter::GetDataBlockData(std::uint8_t block,
                                                  std::uint16_t index) const {
    if (block >= m_dataBlocks.size()) {
        return 0;
    }
    if (index >= m_dataBlocks[block].size()) {
        return 0;
    }
    return m_dataBlocks[block][index];
}

bool CScriptInterpreter::GetResource(std::uint16_t index, std::uint32_t &packHash,
                                     std::uint32_t &resourceId) const {
    if (m_script == nullptr) {
        return false;
    }

    const std::vector<ScriptResourceRef> &resources = m_script->GetResources();
    if (index >= resources.size()) {
        return false;
    }

    packHash = resources[index].packHash;
    resourceId = resources[index].resourceId;
    return true;
}

void CScriptInterpreter::CallFunction(std::uint16_t functionId,
                                      std::uint8_t argumentCount,
                                      const std::uint16_t *arguments) {
    m_returnValueRef = 0;

    // Arguments are resolved before anything is called, and all through the
    // same scratch slot: each is copied out immediately, so they never have to
    // coexist.
    std::int16_t resolved[kScriptMaxArguments];
    for (std::uint8_t i = 0; i < argumentCount && i < kScriptMaxArguments; ++i) {
        resolved[i] = *GetData(arguments[i], 0);
    }

    if (functionId > kScriptLastLocalFunctionId) {
        std::int16_t result = 0;
        if (m_host != nullptr) {
            result = ScriptResolver::ResolveFunction(m_host, functionId, resolved,
                                                     argumentCount);
        }

        // The result is stored as a literal operand rather than a value, which
        // is why a native function cannot return anything outside the range a
        // fifteen-bit literal covers.
        m_returnValueRef =
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(result) |
                                       kScriptOperandLiteral);
        return;
    }

    // One of the script's own functions: its arguments go into the registers,
    // where operands 0xFE downwards will find them.
    for (std::uint8_t i = 0; i < argumentCount && i < kScriptMaxArguments; ++i) {
        m_arguments[i] = resolved[i];
    }

    if (m_script == nullptr) {
        return;
    }

    const CScriptCode *function =
        m_script->GetFunction(static_cast<std::uint8_t>(functionId));
    if (function == nullptr) {
        std::printf("[script] call to function %u, which this script does not have\n",
                    functionId);
        return;
    }

    function->Execute(*this);
}

bool CScriptInterpreter::CallFunctionDirect(std::uint8_t index) {
    m_returnValueRef = 0;

    if (m_script == nullptr) {
        return false;
    }

    const CScriptCode *function = m_script->GetFunction(index);
    if (function == nullptr) {
        return false;
    }

    return function->Execute(*this);
}

bool CScriptInterpreter::CallExportFunction(std::uint8_t exportId,
                                            std::int16_t argument0,
                                            std::int16_t argument1,
                                            std::int16_t argument2) {
    m_returnValueRef = 0;
    m_arguments[0] = argument0;
    m_arguments[1] = argument1;
    m_arguments[2] = argument2;

    // The current state gets first refusal, which is how a state overrides one
    // handler without redeclaring the rest.
    if (m_state != nullptr) {
        const std::vector<ScriptStateExport> &exports = m_state->GetExports();
        for (std::size_t i = 0; i < exports.size(); ++i) {
            if (exports[i].id == exportId) {
                return exports[i].code.Execute(*this);
            }
        }
    }

    if (m_script == nullptr) {
        return false;
    }

    const std::vector<std::uint8_t> &exportFunctions = m_script->GetExportFunctions();
    if (exportId >= exportFunctions.size()) {
        std::printf("[script] export %u past the script's %u\n", exportId,
                    static_cast<unsigned>(exportFunctions.size()));
        return false;
    }

    const CScriptCode *function = m_script->GetFunction(exportFunctions[exportId]);
    if (function == nullptr) {
        std::printf("[script] export %u names function %u, which does not exist\n",
                    exportId, exportFunctions[exportId]);
        return false;
    }

    return function->Execute(*this);
}

bool CScriptInterpreter::HandleEvent(std::uint8_t classId, std::uint8_t eventIndex) {
    if (m_state == nullptr) {
        return false;
    }

    // The class id is stored one higher than ScriptResolver's, leaving zero
    // free to mean "no class" in an operand's high byte.
    const std::uint16_t eventId =
        static_cast<std::uint16_t>(((classId + 1) << 8) | eventIndex);
    return m_state->Evaluate(*this, eventId);
}

bool CScriptInterpreter::SetState(std::uint8_t stateId) {
    m_stateId = stateId;

    if (m_state != nullptr) {
        m_state->OnExit(*this);
    }

    if (m_script == nullptr) {
        m_state = nullptr;
        return false;
    }

    const CScriptState *next = m_script->GetState(stateId);
    if (next == nullptr) {
        std::printf("[script] state %u past the script's %u\n", stateId,
                    static_cast<unsigned>(m_script->GetStates().size()));
        m_state = nullptr;
        return false;
    }

    m_state = next;
    m_started = true;
    m_sequencePosition = 0;

    if (m_host != nullptr) {
        m_host->OnScriptStateEntered();

        const std::uint8_t *sequence = m_state->GetSequence(*this);
        if (sequence != nullptr) {
            m_host->SetScriptSequenceFrame(sequence[m_sequencePosition]);
        }
    }

    return m_state->Execute(*this);
}

void CScriptInterpreter::Refresh() {
    if (m_host == nullptr || m_state == nullptr) {
        return;
    }

    // Nothing moves until the host says the frame on screen is done with.
    if (!m_host->IsScriptSequenceFrameFinished()) {
        return;
    }

    const std::uint8_t length = m_state->GetSequenceLength(*this);
    if (length > 0 && m_sequencePosition == length - 1) {
        if (m_state->Evaluate(*this, kScriptEventSequenceEnded)) {
            return;
        }
    }

    if (m_state->Evaluate(*this, kScriptEventSequenceAdvanced)) {
        return;
    }

    // Either event may have changed state, so the sequence is read again.
    if (m_state == nullptr) {
        return;
    }

    const std::uint8_t currentLength = m_state->GetSequenceLength(*this);
    if (currentLength == 0) {
        return;
    }

    m_sequencePosition = static_cast<std::uint8_t>((m_sequencePosition + 1) % currentLength);

    const std::uint8_t *sequence = m_state->GetSequence(*this);
    if (sequence != nullptr) {
        m_host->SetScriptSequenceFrame(sequence[m_sequencePosition]);
    }
}
