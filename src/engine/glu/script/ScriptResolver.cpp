#include "engine/glu/script/ScriptResolver.h"
namespace ScriptResolver {
std::int16_t ResolveFunction(ZScriptObject *host, std::uint16_t id, const std::int16_t *arguments, std::uint8_t count) {
    return host->ResolveNativeFunction(id, arguments, count);
}
std::int16_t *ResolveVariable(ZScriptObject *host, std::uint16_t id) {
    return host->ResolveNativeVariable(id);
}
}
