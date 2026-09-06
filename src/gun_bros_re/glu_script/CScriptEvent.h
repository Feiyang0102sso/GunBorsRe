/**
 * @file CScriptEvent.h
 * @brief Statement 3 -- an event handler.
 *
 * Port of CScriptEvent (src/gluScript/scriptEvent.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:107073 (Skip), :107080 (Evaluate)
 *
 * Operands:
 *   uint16 eventId
 *   block
 *
 * There is no Execute. An event handler is inert while a block is running and
 * only comes alive during Evaluate, which is what splits one block between the
 * two passes: statements do their work in Execute, handlers wait for Evaluate.
 *
 * The id has two forms. Normally it is matched whole. With bit 7 of the low
 * byte set it is a mask instead: the high byte still has to be the same class,
 * and the low byte is a set of event bits.
 */

#ifndef GUN_BROS_RE_GLU_SCRIPT_CSCRIPTEVENT_H
#define GUN_BROS_RE_GLU_SCRIPT_CSCRIPTEVENT_H

#include "glu_script/CScriptCode.h"

class CScriptInterpreter;

// Set in the low byte when the id is a mask of events rather than one event.
constexpr std::uint8_t kScriptEventMaskForm = 0x80;

/** An event handler statement. */
struct CScriptEvent {
    /**
     * Run the handler if its id matches.
     * @return whatever the handler's block returned, or false if it did not match.
     */
    static bool Evaluate(CScriptInterpreter &interpreter, ScriptCursor &cursor,
                         std::uint16_t eventId);

    static void Skip(ScriptCursor &cursor);
};

#endif  // GUN_BROS_RE_GLU_SCRIPT_CSCRIPTEVENT_H
