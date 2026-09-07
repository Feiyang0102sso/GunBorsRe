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
#include "milestones/M35Mesh.h"
#include "milestones/M38Enemy.h"

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
        "  --levels                  list which levels scroll a tile layer\n"
        "  --meshes                  parse every 3D model and list what is in it\n"
        "  --movesets                follow every model to the atlas it wears\n"
        "  --mesh <n>                M3.5: show model <n> of the catalogue\n"
        "  --character [n]           M3.7: the player, his legs, and gun <n>\n"
        "  --enemy <n>               M3.8: enemy <n>, assembled by its script\n"
        "  --enemies                 run every enemy script, list its parts\n"
        "  --enemyanim <n>           M3.8: enemy <n>, M/N walking the script's\n"
        "                            states -- its real idle/attack/death\n"
        "  --enemyanims              list every enemy's states and the moves\n"
        "                            each one chains\n"
        "  --move <n>                M3.8: hold body move <n> instead of the\n"
        "                            one the script chose, and loop it\n"
        "  --state <n>               M3.8: enter state <n> and let its whole\n"
        "                            sequence play\n"
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

/**
 * The menu shown when the program is started with no arguments.
 *
 * Every harness is reachable from the command line, but the command line is
 * not much use when the executable was double-clicked or launched from the
 * debugger. Returns the chosen number, or the default when the line is empty.
 */
int PromptForHarness() {
    std::printf(
        "\n=== gun_bros_re ===\n"
        "\n"
        "  1  map viewer       -- terrain, scenery, animation, scrolling\n"
        "  2  model viewer     -- one 3D model at a time, on a turntable\n"
        "  3  character viewer -- a player assembled out of his parts\n"
        "  4  enemy viewer     -- an enemy assembled by its own script\n"
        "  5  enemy animations -- the idle/attack/death its states play\n"
        "  6  texture viewer   -- one PNG out of a .big\n"
        "\n"
        "  7  list every map\n"
        "  8  list every model\n"
        "  9  list every model with the atlas it wears\n"
        " 10  list what each enemy script assembles\n"
        " 11  list what each enemy script animates\n"
        " 12  list every level script\n"
        " 13  resource addressing self-check\n"
        "\n"
        "choice [1]: ");
    std::fflush(stdout);

    char line[64];
    if (std::fgets(line, sizeof(line), stdin) == nullptr) {
        return 1;
    }

    const int choice = std::atoi(line);
    if (choice < 1 || choice > 13) {
        return 1;
    }
    return choice;
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
    bool surveyLevels = false;
    bool surveyMeshes = false;
    bool surveyMoveSets = false;
    bool runM35 = false;
    bool runM37 = false;
    bool runM38 = false;
    bool surveyEnemies = false;
    bool surveyEnemyAnimations = false;
    bool stepStates = false;
    std::uint32_t enemyIndex = 0;
    std::int32_t bodyMoveIndex = -1;
    std::int32_t stateIndex = -1;
    std::uint32_t gunIndex = 0;
    std::uint32_t meshIndex = 0;
    float meshSpinDegrees = 0.0f;
    std::uint32_t meshFrameIndex = 0;

    for (int i = 1; i < argc; ++i) {
        const char *argument = argv[i];

        if (std::strcmp(argument, "--m1") == 0) {
            runM1 = true;
        } else if (std::strcmp(argument, "--m2") == 0) {
            runM2 = true;
        } else if (std::strcmp(argument, "--maps") == 0) {
            listMaps = true;
        } else if (std::strcmp(argument, "--levels") == 0) {
            surveyLevels = true;
        } else if (std::strcmp(argument, "--meshes") == 0) {
            surveyMeshes = true;
        } else if (std::strcmp(argument, "--move") == 0 && i + 1 < argc) {
            bodyMoveIndex = static_cast<std::int32_t>(std::strtol(argv[++i], nullptr, 0));
        } else if (std::strcmp(argument, "--state") == 0 && i + 1 < argc) {
            stateIndex = static_cast<std::int32_t>(std::strtol(argv[++i], nullptr, 0));
        } else if (std::strcmp(argument, "--enemies") == 0) {
            surveyEnemies = true;
        } else if (std::strcmp(argument, "--enemyanims") == 0) {
            surveyEnemyAnimations = true;
        } else if (std::strcmp(argument, "--enemyanim") == 0 && i + 1 < argc) {
            runM38 = true;
            stepStates = true;
            enemyIndex = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else if (std::strcmp(argument, "--enemy") == 0 && i + 1 < argc) {
            runM38 = true;
            enemyIndex = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else if (std::strcmp(argument, "--movesets") == 0) {
            surveyMoveSets = true;
        } else if (std::strcmp(argument, "--frame") == 0 && i + 1 < argc) {
            meshFrameIndex = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else if (std::strcmp(argument, "--spin") == 0 && i + 1 < argc) {
            meshSpinDegrees = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(argument, "--mesh") == 0 && i + 1 < argc) {
            runM35 = true;
            meshIndex = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else if (std::strcmp(argument, "--character") == 0) {
            // The gun index is optional: there is only one player, so the
            // number after it is the only thing left to choose.
            runM37 = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                gunIndex = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
            }
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

    // Nothing on the command line means nobody typed one: ask instead.
    if (argc == 1) {
        const int choice = PromptForHarness();
        if (choice == 2) {
            return RunM35Mesh(bigDirectory, 0, 0.0f, 0, screenshotPath, advanceMs);
        }
        if (choice == 3) {
            return RunM37Character(bigDirectory, 0, 0.0f, screenshotPath, advanceMs);
        }
        if (choice == 4) {
            return RunM38Enemy(bigDirectory, 0, 0.0f, screenshotPath, advanceMs,
                               -1, false, -1);
        }
        if (choice == 5) {
            return RunM38Enemy(bigDirectory, 0, 0.0f, screenshotPath, advanceMs,
                               -1, true, -1);
        }
        if (choice == 6) {
            return RunM2Texture(bigDirectory, imagePackName, imageResourceId,
                                screenshotPath);
        }
        if (choice == 7) {
            return RunMapList(bigDirectory);
        }
        if (choice == 8) {
            return RunMeshSurvey(bigDirectory);
        }
        if (choice == 9) {
            return RunMoveSetSurvey(bigDirectory);
        }
        if (choice == 10) {
            return RunEnemySurvey(bigDirectory);
        }
        if (choice == 11) {
            return RunEnemyAnimationSurvey(bigDirectory);
        }
        if (choice == 12) {
            return RunLevelSurvey(bigDirectory);
        }
        if (choice == 13) {
            return RunM1Resources(bigDirectory);
        }
        return RunM3Map(bigDirectory, mapPackName, mapIndex, screenshotPath,
                        advanceMs);
    }

    if (!dumpPackName.empty()) {
        return RunPackDump(bigDirectory, dumpPackName);
    }
    if (listMaps) {
        return RunMapList(bigDirectory);
    }
    if (surveyLevels) {
        return RunLevelSurvey(bigDirectory);
    }
    if (surveyMeshes) {
        return RunMeshSurvey(bigDirectory);
    }
    if (surveyMoveSets) {
        return RunMoveSetSurvey(bigDirectory);
    }
    if (surveyEnemies) {
        return RunEnemySurvey(bigDirectory);
    }
    if (surveyEnemyAnimations) {
        return RunEnemyAnimationSurvey(bigDirectory);
    }
    if (runM38) {
        return RunM38Enemy(bigDirectory, enemyIndex, meshSpinDegrees,
                           screenshotPath, advanceMs, bodyMoveIndex, stepStates,
                           stateIndex);
    }
    if (runM37) {
        return RunM37Character(bigDirectory, gunIndex, meshSpinDegrees,
                               screenshotPath, advanceMs);
    }
    if (runM35) {
        return RunM35Mesh(bigDirectory, meshIndex, meshSpinDegrees,
                          meshFrameIndex, screenshotPath, advanceMs);
    }
    if (runM1) {
        return RunM1Resources(bigDirectory);
    }
    if (runM2) {
        return RunM2Texture(bigDirectory, imagePackName, imageResourceId, screenshotPath);
    }
    return RunM3Map(bigDirectory, mapPackName, mapIndex, screenshotPath, advanceMs);
}
