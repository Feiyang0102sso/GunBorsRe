#pragma once
#include <cstdint>
#include <string>

/** Run archive combat checks through the viewer's normal scene setup. */
int RunArenaCheck(const std::string &bigDirectory, std::uint32_t enemyIndex, std::uint32_t weaponIndex);
