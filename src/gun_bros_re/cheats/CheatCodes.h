#pragma once
#if GB_ENABLE_CHEATS
#include "gun_bros_re/cheats/CheatConfig.h"
#include "engine/platform/CWindow.h"
#include <string>
#include <vector>
namespace GameCheats {
/** Input recognition only records commands; menus and combat handle their respective actions. */
bool Consume(std::string &prefix, std::vector<std::string> &commands, char letter, bool repeat);
inline void Bind() { CWindow::SetCommandMatcher(Consume, InputTimeoutMs); }
}
#endif

