/**
 * @file CScriptState.h
 * @brief One state of a script's state machine.
 *
 * Port of CScriptState (src/gluScript/scriptState.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:107728 (Parse), :107830
 *            (GetSequenceLength), :107846 (GetSequence), :107886 (Evaluate),
 *            :107912 (OnExit), :107942 (Execute)
 *
 * Wire format:
 *   uint8  parent            -- 255 for none
 *   uint8  n, uint8 frames[n] -- the animation sequence this state plays
 *   uint8  m, then m times: uint8 exportId, CScriptCode
 *   CScriptCode enterCode
 *   CScriptCode exitCode
 *
 * **States inherit.** Every lookup here -- sequence, export, enter block, exit
 * block -- walks up the parent chain when this state has nothing of its own.
 * So a state can override one handler and take everything else from its
 * parent, and a chain of states can share one animation.
 */

#ifndef GUN_BROS_RE_GLU_SCRIPT_CSCRIPTSTATE_H
#define GUN_BROS_RE_GLU_SCRIPT_CSCRIPTSTATE_H

#include "engine/resources/CArrayInputStream.h"
#include "engine/glu/script/CScriptCode.h"

#include <cstdint>
#include <vector>

class CScriptInterpreter;

// Parent id meaning "this state is a root".
constexpr std::uint8_t kNoParentState = 255;

/** One of a state's export handlers -- its id and the code behind it. */
struct ZScriptStateExport {
    std::uint8_t id;
    CScriptCode code;
};

/** A state: an animation sequence plus the code that runs while it is current. */
class CScriptState {
public:
    CScriptState();

    void Parse(CArrayInputStream &stream);

    std::uint8_t GetParent() const { return m_parent; }

    /** This state's own sequence, empty when it inherits one. */
    const std::vector<std::uint8_t> &GetOwnSequence() const { return m_sequence; }

    const std::vector<ZScriptStateExport> &GetExports() const { return m_exports; }
    /** Raw state entry bytecode retained for the permanent script research harness. */
    const CScriptCode &GetEnterCode() const { return m_enterCode; }

    /**
     * How many frames the current sequence has, following the parent chain.
     * @return 0 when no state in the chain declares one.
     */
    std::uint8_t GetSequenceLength(const CScriptInterpreter &interpreter) const;

    /**
     * The sequence itself, following the same chain.
     * @return null when no state in the chain declares one.
     */
    const std::uint8_t *GetSequence(const CScriptInterpreter &interpreter) const;

    /** Run the code that belongs to entering this state. */
    bool Execute(CScriptInterpreter &interpreter) const;

    /** Run the code that belongs to leaving it. */
    bool OnExit(CScriptInterpreter &interpreter) const;

    /**
     * Offer an event to this state and then to its ancestors.
     * @return true once some state handled it.
     */
    bool Evaluate(CScriptInterpreter &interpreter, std::uint16_t eventId) const;

private:
    std::uint8_t m_parent;
    std::vector<std::uint8_t> m_sequence;
    std::vector<ZScriptStateExport> m_exports;
    CScriptCode m_enterCode;
    CScriptCode m_exitCode;
};

#endif  // GUN_BROS_RE_GLU_SCRIPT_CSCRIPTSTATE_H
