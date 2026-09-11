#include "engine/glu/script/CScriptResolver.h"
namespace ScriptResolver {
std::int16_t ResolveFunction(IScriptObject *host, std::uint16_t id, const std::int16_t *arguments, std::uint8_t count) {
    return host->ResolveNativeFunction(id, arguments, count);
}
std::int16_t *ResolveVariable(IScriptObject *host, std::uint16_t id) {
    return host->ResolveNativeVariable(id);
}
}
