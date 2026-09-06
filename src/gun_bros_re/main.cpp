/**
 * @file main.cpp
 * @brief Entry point. Picks a milestone harness and runs it.
 *
 * Each milestone in PLAN.md has a harness that produces the result that
 * milestone is signed off against; they live in milestones/ and this only
 * decides which one to run.
 */

#include "milestones/M1Resources.h"
#include "milestones/M2Texture.h"
#include "milestones/M3Map.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

// Defaults for the M2 image: a 512x512 RGB texture in the core pack.
const char *const kDefaultImagePack = "pack0_core";
constexpr std::uint32_t kDefaultImageResourceId = 313;

// Defaults for the M3 map: the first of pack2's nine.
const char *const kDefaultMapPack = "pack2";
constexpr std::uint32_t kDefaultMapIndex = 0;

void PrintUsage() {
    std::printf(
        "usage: gun_bros_re [options]\n"
        "\n"
        "  (no options)              M3: show a level, terrain and scenery\n"
        "  --m1                      M1: verify cross-pack resource addressing\n"
        "  --m2                      M2: show a single PNG from a .big\n"
        "  --dump <pack>             list one pack's resource table\n"
        "  --maps                    list every map, in the viewer's order\n"
        "  --map <pack> <n>          which map M3 should START on; the arrow\n"
        "                            keys reach every other one\n"
        "                            (default: %s %u)\n"
        "  --image <pack> <id>       which PNG M2 should display\n"
        "                            (default: %s %u)\n"
        "  --big <directory>         where the .big files are\n"
        "                            (default: ASSET_ROOT/big)\n"
        "  --screenshot <file.png>   save the first frame and exit\n"
        "  --advance <ms>            run the animations on this far before\n"
        "                            that first frame\n",
        kDefaultMapPack, kDefaultMapIndex, kDefaultImagePack, kDefaultImageResourceId);
}

}  // namespace

int main(int argc, char **argv) {
    std::string bigDirectory = std::string(ASSET_ROOT) + "/big";
    std::string dumpPackName;
    std::string screenshotPath;
    std::uint32_t advanceMs = 0;

    std::string imagePackName = kDefaultImagePack;
    std::uint32_t imageResourceId = kDefaultImageResourceId;

    std::string mapPackName = kDefaultMapPack;
    std::uint32_t mapIndex = kDefaultMapIndex;

    bool runM1 = false;
    bool runM2 = false;
    bool listMaps = false;

    for (int i = 1; i < argc; ++i) {
        const char *argument = argv[i];

        if (std::strcmp(argument, "--m1") == 0) {
            runM1 = true;
        } else if (std::strcmp(argument, "--m2") == 0) {
            runM2 = true;
        } else if (std::strcmp(argument, "--maps") == 0) {
            listMaps = true;
        } else if (std::strcmp(argument, "--dump") == 0 && i + 1 < argc) {
            dumpPackName = argv[++i];
        } else if (std::strcmp(argument, "--map") == 0 && i + 2 < argc) {
            mapPackName = argv[++i];
            mapIndex = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else if (std::strcmp(argument, "--image") == 0 && i + 2 < argc) {
            runM2 = true;
            imagePackName = argv[++i];
            imageResourceId = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else if (std::strcmp(argument, "--big") == 0 && i + 1 < argc) {
            bigDirectory = argv[++i];
        } else if (std::strcmp(argument, "--screenshot") == 0 && i + 1 < argc) {
            screenshotPath = argv[++i];
        } else if (std::strcmp(argument, "--advance") == 0 && i + 1 < argc) {
            advanceMs = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else {
            PrintUsage();
            return 1;
        }
    }

    if (!dumpPackName.empty()) {
        return RunPackDump(bigDirectory, dumpPackName);
    }
    if (listMaps) {
        return RunMapList(bigDirectory);
    }
    if (runM1) {
        return RunM1Resources(bigDirectory);
    }
    if (runM2) {
        return RunM2Texture(bigDirectory, imagePackName, imageResourceId, screenshotPath);
    }
    return RunM3Map(bigDirectory, mapPackName, mapIndex, screenshotPath, advanceMs);
}
