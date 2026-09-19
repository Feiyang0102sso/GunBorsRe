/** Original CGame::VariableResolver :74387; mode and random queries are game policy. */
#include "gun_bros_re/gameplay/game/CGame.h"

std::int16_t *CGame::VariableResolver(ZGameScriptObject &host, std::uint8_t variable) {
    switch (variable) {
    case 0: host.m_gameVariable = host.GetRandom().GetRandRange(0, 1000) >= 500; break;
    case 1: host.m_gameVariable = host.RandomInteger(0, 3); break;
    case 2: host.m_gameVariable = host.RandomInteger(0, 100); break;
    case 3: host.m_gameVariable = host.RandomInteger(0, 1000); break;
    // Single-player, not co-op.
    // The original zero above is now conditional: local Live is GameType 2.
    case 4: host.m_gameVariable = host.IsCooperative(); break;
    // Previously fixed to zero; original CGame::VariableResolver tests GameType 3.
    case 5: host.m_gameVariable = host.IsDeathmatch(); break;
    case 6: host.m_gameVariable = -1; break; // CGunBros menu :94113.
    default: return nullptr;
    }
    return &host.m_gameVariable;
}
