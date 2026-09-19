#pragma once
/** Game natives layered over the engine's interpreter-host interface. */
#include <cstdint>
class ZGameScriptObject;
namespace ScriptResolver {
std::int16_t ResolveFunction(ZGameScriptObject *host, std::uint16_t functionId,
    const std::int16_t *arguments, std::uint8_t argumentCount);
std::int16_t *ResolveVariable(ZGameScriptObject *host, std::uint16_t variableId);
}
