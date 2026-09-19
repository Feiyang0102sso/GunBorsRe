/** Desktop bridge for the interpreter's unrecovered game-object vtable. */
#include "gun_bros_re/host/ZGameScriptObject.h"
#include "gun_bros_re/gameplay/script/ScriptResolver.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/game/CGame.h"
bool ZGameScriptObject::IsDeathmatch() const {
    if (m_deathmatch) { return true; }
    return m_levelContext != nullptr && m_levelContext != this && m_levelContext->IsDeathmatch();
}

std::int16_t ZGameScriptObject::ResolveNativeFunction(std::uint16_t id, const std::int16_t *arguments, std::uint8_t count) {
    return ScriptResolver::ResolveFunction(this, id, arguments, count);
}
std::int16_t *ZGameScriptObject::ResolveNativeVariable(std::uint16_t id) {
    return ScriptResolver::ResolveVariable(this, id);
}

bool ZGameScriptObject::IsCooperative() const {
    if (m_cooperative) { return true; }
    return m_levelContext != nullptr && m_levelContext != this && m_levelContext->IsCooperative();
}
void ZGameScriptObject::SetRandomSeed(std::uint32_t seed) {
    // Attached actors use the level stream; keep their fallback seed without
    // allocating a second, unused 624-word generator for every spawned actor.
    m_randomSeed = seed;
    if (m_random) { m_random->Seed(seed); }
}
CRandGen &ZGameScriptObject::GetRandom() {
    if (m_levelContext != nullptr && m_levelContext != this) { return m_levelContext->GetRandom(); }
    if (!m_random) { m_random = std::make_shared<CRandGen>(m_randomSeed); }
    return *m_random;
}
std::int16_t *ZGameScriptObject::ResolveGameVariable(std::uint8_t variable) {
    return CGame::VariableResolver(*this, variable);
}
