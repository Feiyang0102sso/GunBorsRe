/**
 * @file CScript.cpp
 * @brief Loading a compiled gluScript program.
 */

#include "glu_script/CScript.h"

CScript::CScript() : m_present(false) {}

void CScript::Load(CArrayInputStream &stream) {
    m_present = stream.ReadUInt8() != 0;
    if (!m_present) {
        return;
    }

    for (std::uint32_t i = 0; i < kScriptReservedBytes; ++i) {
        stream.ReadUInt8();
    }

    // --- export id -> function index ---
    const std::uint8_t exportCount = stream.ReadUInt8();
    m_exportFunctions.resize(exportCount);
    for (std::uint8_t i = 0; i < exportCount; ++i) {
        m_exportFunctions[i] = stream.ReadUInt8();
    }

    // --- a second byte table, which nothing in the engine reads back ---
    const std::uint8_t secondaryCount = stream.ReadUInt8();
    m_secondaryTable.resize(secondaryCount);
    for (std::uint8_t i = 0; i < secondaryCount; ++i) {
        m_secondaryTable[i] = stream.ReadUInt8();
    }

    // --- resources the script may name by index ---
    const std::uint8_t resourceCount = stream.ReadUInt8();
    m_resources.resize(resourceCount);
    for (std::uint8_t i = 0; i < resourceCount; ++i) {
        m_resources[i].packHash = stream.ReadUInt32();
        m_resources[i].sectionOrType = stream.ReadUInt8();
        m_resources[i].resourceId = stream.ReadUInt32();
    }

    // --- data blocks: constant arrays a variable statement can index ---
    const std::uint8_t blockCount = stream.ReadUInt8();
    m_dataBlocks.resize(blockCount);
    for (std::uint8_t i = 0; i < blockCount; ++i) {
        const std::uint8_t valueCount = stream.ReadUInt8();
        m_dataBlocks[i].resize(valueCount);
        for (std::uint8_t v = 0; v < valueCount; ++v) {
            m_dataBlocks[i][v] = stream.ReadInt16();
        }
    }

    // --- starting values of the script's own variables ---
    const std::uint8_t variableCount = stream.ReadUInt8();
    m_variableInitialValues.resize(variableCount);
    for (std::uint8_t i = 0; i < variableCount; ++i) {
        m_variableInitialValues[i] = stream.ReadInt16();
    }

    // --- states ---
    const std::uint8_t stateCount = stream.ReadUInt8();
    m_states.resize(stateCount);
    for (std::uint8_t i = 0; i < stateCount; ++i) {
        m_states[i].Parse(stream);
    }

    // --- the script's own functions ---
    const std::uint8_t functionCount = stream.ReadUInt8();
    m_functions.resize(functionCount);
    for (std::uint8_t i = 0; i < functionCount; ++i) {
        m_functions[i].Parse(stream);
    }
}

const CScriptState *CScript::GetState(std::uint8_t stateId) const {
    if (stateId >= m_states.size()) {
        return nullptr;
    }
    return &m_states[stateId];
}

const CScriptCode *CScript::GetFunction(std::uint8_t index) const {
    if (index >= m_functions.size()) {
        return nullptr;
    }
    return &m_functions[index];
}
