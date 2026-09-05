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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

// Defaults for the M2 image: a 512x512 RGB texture in the core pack.
const char *const kDefaultImagePack = "pack0_core";
constexpr std::uint32_t kDefaultImageResourceId = 313;

void PrintUsage() {
    std::printf(
        "usage: gun_bros_re [options]\n"
        "\n"
        "  (no options)              M2: show a PNG from a .big in a window\n"
        "  --m1                      M1: verify cross-pack resource addressing\n"
        "  --dump <pack>             list one pack's resource table\n"
        "  --image <pack> <id>       which PNG M2 should display\n"
        "                            (default: %s %u)\n"
        "  --big <directory>         where the .big files are\n"
        "                            (default: ASSET_ROOT/big)\n"
        "  --screenshot <file.png>   save the first frame and exit\n",
        kDefaultImagePack, kDefaultImageResourceId);
}

}  // namespace

int main(int argc, char **argv) {
    std::string bigDirectory = std::string(ASSET_ROOT) + "/big";
    std::string dumpPackName;
    std::string imagePackName = kDefaultImagePack;
    std::uint32_t imageResourceId = kDefaultImageResourceId;
    std::string screenshotPath;
    bool runM1 = false;

    for (int i = 1; i < argc; ++i) {
        const char *argument = argv[i];

        if (std::strcmp(argument, "--m1") == 0) {
            runM1 = true;
        } else if (std::strcmp(argument, "--dump") == 0 && i + 1 < argc) {
            dumpPackName = argv[++i];
        } else if (std::strcmp(argument, "--image") == 0 && i + 2 < argc) {
            imagePackName = argv[++i];
            imageResourceId = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else if (std::strcmp(argument, "--big") == 0 && i + 1 < argc) {
            bigDirectory = argv[++i];
        } else if (std::strcmp(argument, "--screenshot") == 0 && i + 1 < argc) {
            screenshotPath = argv[++i];
        } else {
            PrintUsage();
            return 1;
        }
    }

    if (!dumpPackName.empty()) {
        return RunPackDump(bigDirectory, dumpPackName);
    }
    if (runM1) {
        return RunM1Resources(bigDirectory);
    }
    return RunM2Texture(bigDirectory, imagePackName, imageResourceId, screenshotPath);
}
