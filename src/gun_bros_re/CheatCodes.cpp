#include "gun_bros_re/CheatCodes.h"
#if GB_ENABLE_CHEATS
#include <cstring>
bool GameCheats::Consume(std::string &prefix, std::vector<std::string> &commands, char letter, bool repeat) {
    // Repeated suffix letters must not leak into E/Q/C gameplay keys.
    if (repeat) { return prefix.size() >= 2 && prefix.compare(0, 2, "st") == 0; }
    // Desktop Boss shortcut is six letters. A lone S still reaches
    // movement; only an established ST prefix consumes its suffix.
    // The same prefix also accepts the desktop suicide shortcut.
    if (prefix.size() >= 2 && prefix.compare(0, 2, "st") == 0) {
        prefix += letter;
        if (prefix == Boss || prefix == Suicide) {
            commands.push_back(prefix);
            prefix.clear();
            return true;
        }
        if (std::string(Boss).compare(0, prefix.size(), prefix) == 0 ||
            std::string(Suicide).compare(0, prefix.size(), prefix) == 0) { return true; }
        prefix.clear();
    }
    if (prefix == "s" && letter == 't') { prefix = "st"; return true; }
    if (prefix == "ch") {
        if (std::strchr("mtdchiw", letter) != nullptr) {
            commands.push_back(prefix + letter);
            prefix.clear();
            return true; // A completed cheat must not also trigger a weapon hotkey.
        }
        prefix.clear();
    }
    if (prefix == "c" && letter == 'h') { prefix = "ch"; return true; }
    if (letter == 'c') { prefix = "c"; return true; }
    prefix.clear();
    if (letter == 's') { prefix = "s"; }

    return false;
}
#endif
