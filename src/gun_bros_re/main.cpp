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
#include "milestones/Arena.h"
#include "milestones/M5LevelFlow.h"
#include "runtime/WeaponCatalog.h"
#include "runtime/ArmorCatalog.h"
#include "runtime/StoreCatalog.h"
#include "runtime/GameFrontEnd.h"
#include "runtime/PickupCatalog.h"
#include "runtime/PropCatalog.h"
#include "runtime/PowerupCatalog.h"
#include "runtime/MissionCatalog.h"
#include "runtime/OriginalProfile.h"
#include "engine/CAudioPlayer.h"
#include "runtime/StartupSequence.h"
#include "runtime/MovieStudy.h"
#include "runtime/SurvivalHud.h"

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
        "  (no options)              start Gun Bros\n"
        "  --research                open the permanent milestone menu\n"
        "  --intro                   play original Glu video (space/click to skip)\n"
        "  --skip-intro              enter menus without the startup video\n"
        "  --media-check             decode all original music and intro frames\n"
        "  --movie-check             parse original Glu UI timelines\n"
        "  --movie <0..147>           inspect original UI; arrows browse, C regions\n"
        "  --movie-gallery           render every original core UI movie\n"
        "  --help, -h                show available options and exit\n"
        "  --mute                    disable sound playback for every mode\n"
        "  --game                    planets, equipment, shop and local profile\n"
        "  --menu-page <0..19>       menu screenshot page\n"
        "  --hud-check              original combat HUD, overlays and input check\n"
        "  --profile <file>         use a separate writable rebuilt profile\n"
        "  --progress-check          verify progression, purchases and refinery\n"
        "  --profile-play-check      play, save, reload and resume test account\n"
        "  --play                    retail survival; default pack2 map7\n"
        "  --survival-check          real-map projectile/wave integration check\n"
        "  --brother-check           AI-only firing, death and wave revival check\n"
        "  --brother                 add the original AI follow/aim policy to --play\n"
        "  --game-menu-check         refine, buy, equip and play an isolated profile\n"
        "  --pickup-check            pickup templates and collection scripts\n"
        "  --pickup-render-check     all pickup sprites and animation frames\n"
        "  --prop-check              complete prop templates and native callbacks\n"
        "  --powerup-check           consumable templates, queries and actions\n"
        "  --powerup-study           playable item lab, isolated stock\n"
        "  --powerup-play-check      inventory, throw, buffs and visual check\n"
        "  --mission-check           archive mission, objective and level references\n"
        "  --original-save-check     read original data stores without modifying them\n"
        "  --original-profile       play an independent copy of the original perfect save\n"
        "  --original-profile-check validate import, original equipment and play/reload\n"
        "  --horde [0-9]            play BOKOR with original Horde script\n"
        "  --horde-check [0-9]      actual combat through one Horde\n"
        "  --campaign <pack> <n>     play original unfinished campaign mission\n"
        "  --campaign-check <pack> <n>  movement/combat smoke test in archived mission\n"
        "  --check-waves <n>         number of waves to play in survival check\n"
        "  --start-wave <1..500>     wave study / saved-progress starting point\n"
        "  --armor-check             verify every armor template, script and asset\n"
        "  --armor-render-check      render all armor with three weapon poses\n"
        "  --level-flow-check        simulate original level spawn/death events\n"
        "  --armor [n]               armor viewer; combine with --arena to fight\n"
        "  --m1                      M1: verify cross-pack resource addressing\n"
        "  --m2                      M2: show a single PNG from a .big\n"
        "  --dump <pack>             list one pack's resource table\n"
        "  --maps                    list every map, in the viewer's order\n"
        "  --levels                  list which levels scroll a tile layer\n"
        "  --meshes                  parse every 3D model and list what is in it\n"
        "  --movesets                follow every model to the atlas it wears\n"
        "  --mesh <n>                M3.5: show model <n> of the catalogue\n"
        "  --character [n]           M3.7: the player, his legs, and gun <n>\n"
        "  --player-weapon [n]       weapon preview: 1-7 category, N/M weapon\n"
        "  --weapons                 list weapon templates and holding overrides\n"
        "  --weapon-check            verify all weapon models and input transitions\n"
        "  --arena [n]               combat arena for enemy template n\n"
        "  --arena-check             verify enemy catalogue and combat contracts\n"
        "  --weapon <n>              initial weapon in --gameview / --arena\n"
        "  --fire                    hold fire during preview / screenshot\n"
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
        "  --spawns                  M3: start with the spawn overlay on\n"
        "  --collisions              M4: start with collision edges visible\n"
        "  --gameview                open the map as a playable fixed view\n"
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
        " 26  Play Gun Bros    -- planets, equipment, shop and saved progress\n"
        "  1  Preview          -- whole-map canvas; pan and zoom freely\n"
        "  2  GameView         -- playable game camera; fixed view\n"
        "  3  model viewer     -- one 3D model at a time, on a turntable\n"
        "  4  character viewer -- a player assembled out of his parts\n"
        "  5  enemy viewer     -- an enemy assembled by its own script\n"
        "  6  enemy animations -- the idle/attack/death its states play\n"
        "  7  texture viewer   -- one PNG out of a .big\n"
        "\n"
        "  8  list every map\n"
        "  9  list every model\n"
        " 10  list every model with the atlas it wears\n"
        " 11  list what each enemy script assembles\n"
        " 12  list what each enemy script animates\n"
        " 13  list every level script\n"
        " 14  resource addressing self-check\n"
        " 15  player weapon    -- all weapon categories, holding poses and firing\n"
        " 16  Arena            -- enemies, damage and player health\n"
        " 17  armor check      -- every armor template, script and asset\n"
        " 18  armor viewer     -- attachments, body textures and weapon poses\n"
        " 19  armor rendering check -- all armor with three weapon poses\n"
        " 20  level flow check -- original scripts, timers and spawn rules\n"
        " 21  Survival         -- original map, enemies and wave scripts\n"
        " 22  survival check   -- map combat, equipment, death and restart\n"
        " 23  wave study       -- choose a starting wave; original scripts\n"
        " 24  progress check   -- experience, health, store prices and references\n"
        " 25  profile play check -- play, save, reload and resume actual combat\n"
        " 27  game menu check  -- refine, buy, equip, select planet and play\n"
        " 28  AI brother check -- independent shooting, death and wave revival\n"
        " 29  pickup check     -- all templates and collection scripts\n"
        " 30  pickup render check -- all original pickup sprites\n"
        " 31  prop check       -- full templates, moves and script callbacks\n"
        " 32  powerup check    -- consumable templates and native actions\n"
        " 33  powerup lab      -- G use, F select; isolated item inventory\n"
        " 34  mission archive check -- original mission/objective records\n"
        " 35  campaign archive -- choose an unfinished original mission\n"
        " 36  original save check -- read-only native profile archive\n"
        " 37  original save lab -- play imported perfect save independently\n"
        " 38  original profile play check -- import, equipment and reload\n"
        " 39  startup movie    -- original Glu M4V and WAV\n"
        " 40  media check      -- full video and seven MP3 decode\n"
        " 41  original UI viewer -- CMovie timeline; arrows browse, C regions\n"
        " 42  original UI check -- all packs and font resource records\n"
        " 43  original UI gallery -- render every core movie to out/ui-movies\n"
        " 44  combat HUD check -- active, pause, death and completion\n"
        " 45  BOKOR Horde -- original map and ten starting difficulties\n"
        " 46  BOKOR combat check -- real enemies and wave advancement\n"
        "\n"
        "choice [26]: ");
    std::fflush(stdout);

    char line[64];
    if (std::fgets(line, sizeof(line), stdin) == nullptr) {
        return 26;
    }

    const int choice = std::atoi(line);
    if (choice < 1 || choice > 46) {
        return 26;
    }
    return choice;
}

}  // namespace

int main(int argc, char **argv) {
    int modeArgumentCount = 0;
    bool researchMenu = false;
    std::string profilePath;
    bool introStudy = false;
    bool checkMedia = false;
    bool checkMovies = false;
    bool checkHud = false;
    bool movieStudy = false;
    bool movieGallery = false;
    unsigned movieOrdinal = 0;
    bool skipIntro = false;
    std::string bigDirectory = std::string(ASSET_ROOT) + "/big";
    bool surveyWeapons = false;
    bool checkArmor = false;
    bool checkProgress = false;
    bool checkPickups = false;
    bool checkProps = false;
    bool checkPowerups = false;
    bool powerupStudy = false;
    bool checkPickupRendering = false;
    bool checkMissions = false;
    bool checkOriginalSaves = false;
    bool playOriginalProfile = false;
    bool checkOriginalProfile = false;
    bool playCampaign = false;
    bool checkCampaign = false;
    std::string campaignPack;
    int campaignMission = -1;
    bool checkProfilePlay = false;
    bool playGame = false;
    bool checkGameMenu = false;
    unsigned menuPage = 0;
    bool checkArmorRendering = false;
    bool checkLevelFlow = false;
    bool playSurvival = false;
    bool checkSurvival = false;
    bool withBrother = false;
    unsigned checkWaves = 2;
    unsigned startWave = 0;
    bool explicitMap = false;
    int armorIndex = -1;
    bool checkWeapons = false;
    bool arena = false;
    bool checkArena = false;
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
    bool firePreview = false;
    bool runM38 = false;
    bool surveyEnemies = false;
    bool surveyEnemyAnimations = false;
    bool showSpawns = false;
    bool showCollisions = false;
    bool runGameView = false;
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

        if (std::strcmp(argument, "--help") == 0 || std::strcmp(argument, "-h") == 0) {
            PrintUsage();
            return 0;
        }
        if (std::strcmp(argument, "--mute") == 0) {
            CAudioPlayer::SetMuted(true);
            continue;
        }
        if (std::strcmp(argument, "--profile") == 0 && i + 1 < argc) {
            profilePath = argv[++i];
            continue;
        }
        ++modeArgumentCount;

        if (std::strcmp(argument, "--movie") == 0 && i + 1 < argc) {
            movieStudy = true;
            movieOrdinal = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argument, "--movie-gallery") == 0) {
            movieStudy = true;
            movieGallery = true;
        } else if (std::strcmp(argument, "--hud-check") == 0) { checkHud = true; }
        else if (std::strcmp(argument, "--movie-check") == 0) {
            checkMovies = true;
        } else if (std::strcmp(argument, "--intro") == 0) {
            introStudy = true;
        } else if (std::strcmp(argument, "--media-check") == 0) {
            checkMedia = true;
        } else if (std::strcmp(argument, "--skip-intro") == 0) {
            skipIntro = true;
            playGame = true;
        } else if (std::strcmp(argument, "--research") == 0) {
            researchMenu = true;
        } else if (std::strcmp(argument, "--game") == 0) {
            playGame = true;
        } else if (std::strcmp(argument, "--pickup-check") == 0) {
            checkPickups = true;
        } else if (std::strcmp(argument, "--mission-check") == 0) {
            checkMissions = true;
        } else if (std::strcmp(argument, "--original-save-check") == 0) {
            checkOriginalSaves = true;
        } else if (std::strcmp(argument, "--original-profile") == 0) {
            playOriginalProfile = true;
        } else if (std::strcmp(argument, "--original-profile-check") == 0) {
            checkOriginalProfile = true;
        } else if (std::strcmp(argument, "--horde") == 0 || std::strcmp(argument, "--horde-check") == 0) {
            playCampaign = true;
            checkCampaign = std::strcmp(argument, "--horde-check") == 0;
            campaignPack = "pack11";
            campaignMission = 0;
            if (i + 1 < argc && argv[i + 1][0] != '-') { campaignMission = std::atoi(argv[++i]); }
        } else if ((std::strcmp(argument, "--campaign") == 0 || std::strcmp(argument, "--campaign-check") == 0) && i + 2 < argc) {
            playCampaign = true;
            checkCampaign = std::strcmp(argument, "--campaign-check") == 0;
            campaignPack = argv[++i];
            campaignMission = std::atoi(argv[++i]);
        } else if (std::strcmp(argument, "--prop-check") == 0) {
            checkProps = true;
        } else if (std::strcmp(argument, "--powerup-check") == 0) {
            checkPowerups = true;
        } else if (std::strcmp(argument, "--powerup-study") == 0) {
            powerupStudy = true;
            playSurvival = true;
        } else if (std::strcmp(argument, "--powerup-play-check") == 0) {
            powerupStudy = true;
            playSurvival = true;
            checkSurvival = true;
        } else if (std::strcmp(argument, "--pickup-render-check") == 0) {
            checkPickupRendering = true;
        } else if (std::strcmp(argument, "--menu-page") == 0 && i + 1 < argc) {
            menuPage = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argument, "--start-wave") == 0 && i + 1 < argc) {
            const unsigned displayWave = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
            if (displayWave == 0 || displayWave > 500) { return 1; }
            startWave = displayWave - 1;
        } else if (std::strcmp(argument, "--check-waves") == 0 && i + 1 < argc) {
            checkWaves = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
            if (checkWaves == 0 || checkWaves > 500) { return 1; }
        } else if (std::strcmp(argument, "--brother-check") == 0) {
            playSurvival = true;
            checkSurvival = true;
            withBrother = true;
        } else if (std::strcmp(argument, "--brother") == 0) {
            withBrother = true;
            playSurvival = true;
        } else if (std::strcmp(argument, "--survival-check") == 0) {
            playSurvival = true;
            checkSurvival = true;
        } else if (std::strcmp(argument, "--play") == 0) {
            playSurvival = true;
        } else if (std::strcmp(argument, "--level-flow-check") == 0) {
            checkLevelFlow = true;
        } else if (std::strcmp(argument, "--game-menu-check") == 0) {
            checkGameMenu = true;
        } else if (std::strcmp(argument, "--profile-play-check") == 0) {
            checkProfilePlay = true;
        } else if (std::strcmp(argument, "--progress-check") == 0) {
            checkProgress = true;
        } else if (std::strcmp(argument, "--armor-check") == 0) {
            checkArmor = true;
        } else if (std::strcmp(argument, "--armor-render-check") == 0) {
            checkArmorRendering = true;
        } else if (std::strcmp(argument, "--armor") == 0) {
            armorIndex = 0;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                armorIndex = static_cast<int>(std::strtoul(argv[++i], nullptr, 10));
            }
        } else if (std::strcmp(argument, "--m1") == 0) {
            runM1 = true;
        } else if (std::strcmp(argument, "--weapons") == 0) {
            surveyWeapons = true;
        } else if (std::strcmp(argument, "--weapon-check") == 0) {
            checkWeapons = true;
        } else if (std::strcmp(argument, "--arena-check") == 0) {
            arena = true;
            checkArena = true;
        } else if (std::strcmp(argument, "--arena") == 0) {
            arena = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                enemyIndex = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
            }
        } else if (std::strcmp(argument, "--weapon") == 0 && i + 1 < argc) {
            gunIndex = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
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
        } else if (std::strcmp(argument, "--spawns") == 0) {
            showSpawns = true;
        } else if (std::strcmp(argument, "--collisions") == 0) {
            showCollisions = true;
        } else if (std::strcmp(argument, "--gameview") == 0) {
            runGameView = true;
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
        } else if (std::strcmp(argument, "--fire") == 0) {
            firePreview = true;
        } else if (std::strcmp(argument, "--character") == 0 ||
                   std::strcmp(argument, "--player-weapon") == 0) {
            // The gun index is optional: there is only one player, so the
            // number after it is the only thing left to choose.
            runM37 = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                gunIndex = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
            }
        } else if (std::strcmp(argument, "--dump") == 0 && i + 1 < argc) {
            dumpPackName = argv[++i];
        } else if (std::strcmp(argument, "--map") == 0 && i + 2 < argc) {
            explicitMap = true;
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
    if (CAudioPlayer::IsMuted()) {
        std::printf("[audio] muted: playback streams disabled\n");
    }
    // Retail startup now enters the game; the historical menu above is explicit.
    if (modeArgumentCount == 0) { playGame = true; }
    if (checkMedia) { return RunMediaCheck(); }
    if (checkHud) { return RunSurvivalHudCheck(bigDirectory); }
    if (checkMovies) { return RunMovieCheck(bigDirectory); }
    if (movieStudy) { return RunMovieStudy(bigDirectory, movieOrdinal, screenshotPath, advanceMs, movieGallery); }
    if (introStudy) { return RunStartupSequence(screenshotPath, advanceMs); }
    if ((playGame || playOriginalProfile) && screenshotPath.empty() && !skipIntro) {
        const int result = RunStartupSequence();
        if (result == 2) { return 0; }
        if (result != 0) { return result; }
    }
    if (researchMenu) {
        const int choice = PromptForHarness();
        if (choice == 1) {
            return RunM3Map(bigDirectory, mapPackName, mapIndex,
                            screenshotPath, advanceMs, showSpawns,
                            showCollisions, MapViewMode::Preview);
        }
        if (choice == 2) {
            return RunM3Map(bigDirectory, mapPackName, mapIndex,
                            screenshotPath, advanceMs, showSpawns,
                            showCollisions, MapViewMode::GameView);
        }
        if (choice == 3) {
            return RunM35Mesh(bigDirectory, 0, 0.0f, 0, screenshotPath, advanceMs);
        }
        if (choice == 4) {
            return RunM37Character(bigDirectory, 0, 0.0f, screenshotPath, advanceMs);
        }
        if (choice == 5) {
            return RunM38Enemy(bigDirectory, 0, 0.0f, screenshotPath, advanceMs,
                               -1, false, -1);
        }
        if (choice == 6) {
            return RunM38Enemy(bigDirectory, 0, 0.0f, screenshotPath, advanceMs,
                               -1, true, -1);
        }
        if (choice == 7) {
            return RunM2Texture(bigDirectory, imagePackName, imageResourceId,
                                screenshotPath);
        }
        if (choice == 8) {
            return RunMapList(bigDirectory);
        }
        if (choice == 9) {
            return RunMeshSurvey(bigDirectory);
        }
        if (choice == 10) {
            return RunMoveSetSurvey(bigDirectory);
        }
        if (choice == 11) {
            return RunEnemySurvey(bigDirectory);
        }
        if (choice == 12) {
            return RunEnemyAnimationSurvey(bigDirectory);
        }
        if (choice == 13) {
            return RunLevelSurvey(bigDirectory);
        }
        if (choice == 14) {
            return RunM1Resources(bigDirectory);
        }
        if (choice == 15) {
            return RunM37Character(bigDirectory, 0, 0.0f, screenshotPath, advanceMs);
        }
        if (choice == 16) {
            return RunArena(bigDirectory, 0, 0, screenshotPath, advanceMs, false, false, showCollisions);
        }
        if (choice == 17) {
            return RunArmorCheck(bigDirectory);
        }
        if (choice == 18) {
            return RunM37Character(bigDirectory, 0, 0.0f, screenshotPath, advanceMs, false, 0);
        }
        if (choice == 19) {
            return RunArmorRenderCheck(bigDirectory);
        }
        if (choice == 20) {
            return RunLevelFlowCheck(bigDirectory);
        }
        if (choice == 21) {
            return RunSurvival(bigDirectory, "pack2", 7, gunIndex, armorIndex, screenshotPath, advanceMs, firePreview, showCollisions);
        }
        if (choice == 22) {
            return RunSurvival(bigDirectory, "pack2", 7, gunIndex, armorIndex, screenshotPath, advanceMs, firePreview, showCollisions, true);
        }
        if (choice == 24) { return RunProgressCheck(bigDirectory); }
        if (choice == 25) { return RunProfilePlayCheck(bigDirectory); }
        if (choice == 26) { return RunGameFrontEnd(bigDirectory, screenshotPath, menuPage, false, profilePath); }
        if (choice == 27) { return RunGameMenuCheck(bigDirectory); }
        if (choice == 29) { return RunPickupCheck(bigDirectory); }
        if (choice == 30) { return RunPickupRenderCheck(bigDirectory); }
        if (choice == 31) { return RunPropCheck(bigDirectory); }
        if (choice == 32) { return RunPowerupCheck(bigDirectory); }
        if (choice == 34) { return RunMissionCheck(bigDirectory); }
        if (choice == 35) { return RunMissionPlay(bigDirectory, "", -1, gunIndex, armorIndex, screenshotPath, advanceMs, firePreview); }
        if (choice == 36) { return RunOriginalProfileCheck(bigDirectory); }
        if (choice == 37) { return RunGameFrontEnd(bigDirectory, screenshotPath, menuPage, true, profilePath); }
        if (choice == 38) { return RunOriginalProfilePlayCheck(bigDirectory); }
        if (choice == 39) { return RunStartupSequence(screenshotPath, advanceMs); }
        if (choice == 40) { return RunMediaCheck(); }
        if (choice == 41) { return RunMovieStudy(bigDirectory, movieOrdinal, screenshotPath, advanceMs); }
        if (choice == 42) { return RunMovieCheck(bigDirectory); }
        if (choice == 43) { return RunMovieStudy(bigDirectory, 0, "", 0, true); }
        if (choice == 45) { return RunGameFrontEnd(bigDirectory, screenshotPath, 16); }
        if (choice == 46) { return RunMissionPlay(bigDirectory, "pack11", 0, 80, -1, "", 0, false, true); }
        if (choice == 44) { return RunSurvivalHudCheck(bigDirectory); }
        if (choice == 33) { return RunSurvival(bigDirectory, "pack2", 7, gunIndex, armorIndex, screenshotPath,
            advanceMs, firePreview, showCollisions, false, 2, 0, nullptr, false, true); }
        if (choice == 28) {
            return RunSurvival(bigDirectory, "pack2", 7, 65, armorIndex, screenshotPath, 0,
                false, showCollisions, true, 2, 0, nullptr, true);
        }
        if (choice == 23) {
            std::printf("Starting wave 1..500 [21]: ");
            std::fflush(stdout);
            char line[64];
            unsigned wave = 21;
            if (std::fgets(line, sizeof(line), stdin) != nullptr && std::atoi(line) > 0) {
                wave = static_cast<unsigned>(std::atoi(line));
            }
            if (wave > 500) { return 1; }
            return RunSurvival(bigDirectory, "pack2", 7, gunIndex, armorIndex, screenshotPath, advanceMs, firePreview, showCollisions, false, 2, wave - 1);
        }
    }

    if (checkProgress) { return RunProgressCheck(bigDirectory); }
    if (checkPickups) { return RunPickupCheck(bigDirectory); }
    if (checkProps) { return RunPropCheck(bigDirectory); }
    if (checkPowerups) { return RunPowerupCheck(bigDirectory); }
    if (checkMissions) { return RunMissionCheck(bigDirectory); }
    if (checkOriginalSaves) { return RunOriginalProfileCheck(bigDirectory); }
    if (playOriginalProfile) { return RunGameFrontEnd(bigDirectory, screenshotPath, menuPage, true, profilePath); }
    if (checkOriginalProfile) { return RunOriginalProfilePlayCheck(bigDirectory); }
    if (playCampaign) { return RunMissionPlay(bigDirectory, campaignPack, campaignMission, gunIndex, armorIndex, screenshotPath, advanceMs, firePreview, checkCampaign); }
    if (checkPickupRendering) { return RunPickupRenderCheck(bigDirectory); }
    if (checkProfilePlay) { return RunProfilePlayCheck(bigDirectory); }
    if (playGame) { return RunGameFrontEnd(bigDirectory, screenshotPath, menuPage, false, profilePath); }
    if (checkGameMenu) { return RunGameMenuCheck(bigDirectory); }
    if (checkArmor) {
        return RunArmorCheck(bigDirectory);
    }
    if (playSurvival) {
        if (!explicitMap) { mapPackName = "pack2"; mapIndex = 7; }
        return RunSurvival(bigDirectory, mapPackName, mapIndex, gunIndex, armorIndex, screenshotPath, advanceMs, firePreview, showCollisions, checkSurvival, checkWaves, startWave, nullptr, withBrother, powerupStudy);
    }
    if (checkLevelFlow) {
        return RunLevelFlowCheck(bigDirectory);
    }
    if (checkArmorRendering) {
        return RunArmorRenderCheck(bigDirectory);
    }
    if (arena) { return RunArena(bigDirectory, enemyIndex, gunIndex, screenshotPath, advanceMs, firePreview, checkArena, showCollisions, armorIndex); }
    if (armorIndex >= 0) {
        return RunM37Character(bigDirectory, gunIndex, meshSpinDegrees, screenshotPath, advanceMs, firePreview, armorIndex);
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
    if (surveyWeapons) { return RunWeaponSurvey(bigDirectory); }
    if (checkWeapons) { return RunWeaponCheck(bigDirectory); }
    if (runM37) {
        return RunM37Character(bigDirectory, gunIndex, meshSpinDegrees,
                               screenshotPath, advanceMs, firePreview);
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
    MapViewMode mapViewMode = MapViewMode::Preview;
    if (runGameView) {
        mapViewMode = MapViewMode::GameView;
    }
    return RunM3Map(bigDirectory, mapPackName, mapIndex, screenshotPath,
                    advanceMs, showSpawns, showCollisions, mapViewMode, gunIndex, firePreview);
}
