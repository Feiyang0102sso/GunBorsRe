/**
 * @file CScriptResolver.cpp
 * @brief Where a script's native calls and class variables go.
 */

#include "glu_script/CScriptResolver.h"

#include "gun_bros/CEnemy.h"
#include "gun_bros/CLevel.h"

#include <cstdio>

namespace ScriptResolver {

std::int16_t ResolveFunction(IScriptObject *host, std::uint16_t functionId,
                             const std::int16_t *arguments,
                             std::uint8_t argumentCount) {
    const std::uint8_t classId = static_cast<std::uint8_t>((functionId >> 8) & 0xFF);
    const std::uint8_t function = static_cast<std::uint8_t>(functionId & 0xFF);

    if (classId == kScriptClassLevel) {
        // The cast is the original's, made explicit: the host is whatever the
        // id says it is. Every script running today belongs to a CLevel.
        return static_cast<CLevel *>(host)->FunctionResolver(function, arguments,
                                                             argumentCount);
    }
    if (classId == kScriptClassEnemy) {
        return static_cast<CEnemy *>(host)->FunctionResolver(function, arguments,
                                                             argumentCount);
    }

    std::printf("[script] class %u function %u, %u args:", classId, function,
                argumentCount);
    for (std::uint8_t i = 0; i < argumentCount; ++i) {
        std::printf(" %d", arguments[i]);
    }
    std::printf(" -- class not implemented\n");
    return 0;
}

std::int16_t *ResolveVariable(IScriptObject *host, std::uint16_t variableId) {
    const std::uint8_t classId = static_cast<std::uint8_t>((variableId >> 8) & 0xFF);
    const std::uint8_t variable = static_cast<std::uint8_t>(variableId & 0xFF);

    if (classId == kScriptClassLevel) {
        return static_cast<CLevel *>(host)->VariableResolver(variable);
    }
    if (classId == kScriptClassEnemy) {
        return static_cast<CEnemy *>(host)->VariableResolver(variable);
    }

    // Null is a legitimate answer here, not an error: the original returns it
    // for every class it does not recognise, and the interpreter carries on.
    return nullptr;
}

}  // namespace ScriptResolver
