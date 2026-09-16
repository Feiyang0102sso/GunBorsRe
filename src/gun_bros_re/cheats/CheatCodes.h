#pragma once
#include "gun_bros_re/cheats/CheatConfig.h"
#include "engine/platform/ZWindow.h"
#include <string>
#include <vector>
namespace GameCheats {
/** Input recognition only records commands; menus and combat handle their respective actions. */
bool Consume(std::string &prefix, std::vector<std::string> &commands, char letter, bool repeat);
inline void Bind() { ZWindow::SetCommandMatcher(Consume, InputTimeoutMs); }
}
