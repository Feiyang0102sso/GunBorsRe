#include "gun_bros_viewer/ViewerMenu.h"
#include <charconv>
#include <cstdio>
#include <cstring>

void PrintUsage() {
    std::printf(
        "GunBrosViewer [mode] [options]\n"
        "  No mode: open the six-view menu\n"
        "  --map [pack index]     Map viewer\n"
        "  --mesh [index]         Raw mesh, animation and texture\n"
        "  --enemy [index]        Assembled enemy, script state animations\n"
        "  --weapon [index]       Player weapon and firing presentation\n"
        "  --armor [index]        Player armor presentation\n"
        "  --arena [index]        Single-enemy combat laboratory\n"
        "  --big <directory>      Original BIG resource directory\n"
        "  --config <file>        Viewer settings (default: GunBrosViewer.cfg)\n"
        "  --gun <index>          Equipment for armor preview or arena\n"
        "  --equipment <index>    Armor for arena\n"
        "  --state <index>        Enemy animation state\n"
        "  --frame <index>        Initial raw mesh frame\n"
        "  --fire                 Preview firing\n"
        "  --collisions           Show collision geometry\n"
        "  --spawns               Show map spawn points\n"
        "  --mute                 Disable sound playback\n"
#if GB_ENABLE_CAPTURE
        "  --screenshot <path>    Capture one frame (Debug)\n"
        "  --advance <ms>         Advance before capture (Debug)\n"
#endif
        "  --help                 Show this help\n");
}

int PromptForViewer() {
    for (;;) {
        std::printf("\n=== GunBrosViewer ===\n"
            "  1  Map viewer\n"
            "  2  Mesh viewer\n"
            "  3  Enemy viewer\n"
            "  4  Player weapon viewer\n"
            "  5  Player armor viewer\n"
            "  6  Arena\n"
            "  0  Exit\nSelect: ");
        std::fflush(stdout);
        char line[64];
        if (std::fgets(line, sizeof(line), stdin) == nullptr) { return 0; }
        const char *end = line + std::strcspn(line, "\r\n");
        int choice = -1;
        const auto parsed = std::from_chars(line, end, choice);
        if (parsed.ec == std::errc() && parsed.ptr == end && choice >= 0 && choice <= 6) {
            return choice;
        }
        std::printf("[viewer] select 0..6\n");
    }
}
