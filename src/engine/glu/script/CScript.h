/**
 * @file CScript.h
 * @brief A compiled gluScript program, as stored inside an object template.
 *
 * Port of CScript (src/gluScript/script.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:106317 (Load)
 *
 * A script is not a resource of its own. It sits inline in the template that
 * owns it -- CLevel::Template::Init (:114771) reads its map reference and then
 * hands the same stream straight to CScript::Load -- so the whole thing is
 * seven consecutive tables, each with a uint8 count in front:
 *
 *   uint8 present            -- zero means no script, and the rest is absent
 *   uint8 reserved[6]
 *   uint8 n, uint8 exportFunctions[n]   export id -> index into functions
 *   uint8 n, uint8 secondary[n]         read, never used by the engine
 *   uint8 n, { uint32 packHash, uint8, uint32 id } resources[n]
 *   uint8 n, { uint8 m, int16 values[m] } dataBlocks[n]
 *   uint8 n, int16 variableInitialValues[n]
 *   uint8 n, CScriptState states[n]
 *   uint8 n, CScriptCode functions[n]
 *
 * The script is immutable once loaded and can back several running instances;
 * the mutable half -- variables and data blocks -- is copied into each
 * CScriptInterpreter by SetScript (:107566, via AllocateStorage and
 * InitializeStorage).
 */

#ifndef GUN_BROS_RE_GLU_SCRIPT_CSCRIPT_H
#define GUN_BROS_RE_GLU_SCRIPT_CSCRIPT_H

#include "engine/resources/CArrayInputStream.h"
#include "engine/glu/script/CScriptCode.h"
#include "engine/glu/script/CScriptState.h"

#include <cstdint>
#include <vector>

// Bytes after the present flag that Load reads and discards.
constexpr std::uint32_t kScriptReservedBytes = 6;

/**
 * One entry of a script's resource table.
 *
 * Scripts never name resources directly. They carry an index into this table,
 * and CScriptInterpreter::GetResource (:107222) turns the entry into a pack
 * and an id -- the same indirection game object references use, so a script
 * can point into another pack.
 */
struct ZScriptResourceRef {
    std::uint32_t packHash;
    std::uint8_t sectionOrType;  // read by Load; no reader found in the engine
    std::uint32_t resourceId;

    ZScriptResourceRef() : packHash(0), sectionOrType(255), resourceId(255) {}
};

/** A loaded script program. */
class CScript {
public:
    CScript();

    /**
     * Read the whole program off the template's stream.
     *
     * Always consumes exactly what the script occupies, including when there
     * is none: the present flag is read either way, so the fields following
     * the script in the template stay aligned.
     */
    void Load(CArrayInputStream &stream);

    /** Whether the template carried a script at all. */
    bool IsPresent() const { return m_present; }

    /** Maps an export id to a function index. Used when no state overrides it. */
    const std::vector<std::uint8_t> &GetExportFunctions() const {
        return m_exportFunctions;
    }

    const std::vector<std::uint8_t> &GetSecondaryTable() const {
        return m_secondaryTable;
    }

    const std::vector<ZScriptResourceRef> &GetResources() const { return m_resources; }

    const std::vector<std::vector<std::int16_t>> &GetDataBlocks() const {
        return m_dataBlocks;
    }

    const std::vector<std::int16_t> &GetVariableInitialValues() const {
        return m_variableInitialValues;
    }

    const std::vector<CScriptState> &GetStates() const { return m_states; }

    const std::vector<CScriptCode> &GetFunctions() const { return m_functions; }

    /** One state by id, or null when the id is out of range. */
    const CScriptState *GetState(std::uint8_t stateId) const;

    /** One function by index, or null when the index is out of range. */
    const CScriptCode *GetFunction(std::uint8_t index) const;

private:
    bool m_present;
    std::vector<std::uint8_t> m_exportFunctions;
    std::vector<std::uint8_t> m_secondaryTable;
    std::vector<ZScriptResourceRef> m_resources;
    std::vector<std::vector<std::int16_t>> m_dataBlocks;
    std::vector<std::int16_t> m_variableInitialValues;
    std::vector<CScriptState> m_states;
    std::vector<CScriptCode> m_functions;
};

#endif  // GUN_BROS_RE_GLU_SCRIPT_CSCRIPT_H
