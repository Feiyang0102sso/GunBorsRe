#include "gun_bros_re/cheats/CheatCodes.h"
#include <cstring>

bool GameCheats::Consume(std::string &prefix, std::vector<std::string> &commands,
    char letter, bool repeat) {
    // Repeated suffix letters must not leak into E/Q/C gameplay keys.
    if (repeat) { return prefix.size() >= 2; }
    prefix += letter;
    bool exact = false;
    bool longer = false;
    for (const char *code : Commands) {
        if (std::strncmp(code, prefix.c_str(), prefix.size()) != 0) { continue; }
        if (std::strlen(code) == prefix.size()) { exact = true; }
        else { longer = true; }
    }
    if (exact && !longer) {
        commands.push_back(prefix);
        prefix.clear();
        return true; // A completed cheat must not also trigger a weapon hotkey.
    }
    if (longer) {
        // Desktop Boss shortcut is six letters. A lone S still reaches
        // movement; only an established ST prefix consumes its suffix.
        // The same prefix also accepts the desktop suicide shortcut.
        // STBROW requests the brother's normal weapon swap animation.
        // Renamed to BROW; every BRO command is reserved for the test peer.
        return prefix.size() >= 2 || letter == 'c' || letter == 'b';
    }
    prefix.clear();
    for (const char *code : Commands) {
        if (code[0] == letter) { prefix += letter; break; }
    }
    return letter == 'c';
}
