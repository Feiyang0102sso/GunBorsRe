#include "ui/GameMenuStudy.h"
#include "TestApplication.h"
#include "checks/ArenaChecks.h"
#include "gun_bros_viewer/scenes/ResourceInfo.h"
#include "research/ResearchDefaults.h"
#include "gun_bros_viewer/scenes/MapPreview.h"
#include "tests/research/SurvivalStudyHost.h"
#include "gun_bros_re/debug/DebugKeys.h"
#include "engine/core/ZPaths.h"
/**
 * @file main.cpp
 * @brief Entry point. Picks a milestone harness and runs it.
 *
 * Each milestone in PLAN.md has a harness that produces the result that
 * milestone is signed off against; they live in milestones/ and this only
 * decides which one to run.
 */

// Keep the Windows compatibility macros out of shared original-state headers.
#define NOMINMAX
#include "TestOutput.h"
#include "tests/research/ResourceSurvey.h"
#include "tests/gameplay/SurvivalPilot.h"
#include "tests/gameplay/SurvivalStudy.h"
#include "gun_bros_re/debug/PerformanceProbe.h"
#include "Checks.h"
#include "ui/MenuChecks.h"
#include "tests/research/TextureStudy.h"
#include "gun_bros_viewer/scenes/MapPreview.h"
#include "gun_bros_viewer/scenes/MeshPreview.h"
#include "gun_bros_viewer/scenes/EnemyPreview.h"
#include "gun_bros_viewer/scenes/ArenaPreview.h"
#include "tests/checks/M5LevelFlow.h"
#include "gun_bros_re/data/ZWeaponCatalog.h"
#include "gun_bros_re/data/ZArmorCatalog.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/ui/host/ZGameFrontEnd.h"
#include "tests/checks/PropCatalog.h"
#include "gun_bros_re/data/ZPowerupCatalog.h"
#include "gun_bros_re/data/ZMissionCatalog.h"
#include "gun_bros_re/data/ZProfileImport.h"
#include "gun_bros_re/data/ZProfileStorage.h"
#include "engine/platform/ZAudioPlayer.h"
#include "gun_bros_re/startup/ZStartupSequence.h"
#include "tests/research/MovieStudy.h"
#include "gun_bros_re/ui/hud/CInputPad.h"
#include "gun_bros_re/host/ZHostSettings.h"
#include "gun_bros_re/data/CDailyBonusTracking.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
int OpenGameMenu(const std::string &bigDirectory, const std::string &screenshotPath = "", unsigned page = 0,
    bool originalProfile = false, const std::string &profilePath = "") {
    return RunGameMenuStudy(bigDirectory, screenshotPath, page, originalProfile, profilePath);
}

using namespace ResearchDefaults;

/**
 * The menu shown when the program is started with no arguments.
 *
 * Every harness is reachable from the command line, but the command line is
 * not much use when the executable was double-clicked or launched from the
 * debugger. Returns the chosen number, or the default when the line is empty.
 */

}  // namespace

int RunTestApplication(int argc, char **argv) {
    GameCheats::Bind();
    // The test driver supplies the absolute case directory before any dispatch.
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--test-output") == 0) {
            if (index + 1 >= argc || !std::filesystem::path(argv[index + 1]).is_absolute()) {
                std::printf("[test-output] expected an absolute directory\n");
                return 1;
            }
            TestOutput::Configure(argv[++index]);
            std::printf("[test-output] directory=%s\n", TestOutput::directory.string().c_str());
        }
    }
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--fixtures") == 0 && index + 1 < argc) {
            TestOutput::fixtureDirectory = Paths::Resolve(std::filesystem::u8path(argv[++index]));
        }
        if (std::strcmp(argv[index], "--reference-gallery") == 0) { TestOutput::referenceGallery = true; }
    }
    bool checkDailyBonus = false;
    bool checkUpgradePopup = false;
    bool checkNativeProfile = false;
    bool checkNativeProfilePlay = false;
    bool checkBank = false;
    bool checkOptions = false;
    bool checkSocial = false;
    bool checkPlanetMenu = false;
    bool checkPlayInteraction = false;
    bool checkMissionMenu = false;
    bool checkHeader = false;
    bool checkRefineryMenu = false;
    bool checkGreeting = false;
    bool checkPlayerSelect = false;
    bool checkPostGame = false;
    bool checkPause = false;
    bool checkOriginalHud = false;
    bool checkPowerupSelector = false;
    bool checkTutorial = false;
    bool checkPerformance = false;
    bool checkLoadingWipe = false;
    bool checkPromotion = false;
    bool checkSceneTransition = false;
    bool checkDualWeapon = false;
    bool checkStoreEquipped = false;
    bool checkCombatFeedback = false;
    bool checkBoss = false;
    bool checkMapOcclusion = false;
    bool checkPlayerDeath = false;
    bool checkLocalLive = false;
    bool checkDialog = false;
    int modeArgumentCount = 0;
    bool researchMenu = false;
    bool inspectBigVersion = false;
    std::string profilePath;
    bool introStudy = false;
    bool checkMedia = false;
    bool checkMovies = false;
    bool checkFontBitmap = false;
    bool checkHud = false;
    bool movieStudy = false;
    bool movieGallery = false;
    unsigned movieOrdinal = 0;
    bool skipIntro = false;
    std::string bigDirectory = (Paths::Root() / Paths::BigDirectory).u8string();
    bool surveyWeapons = false;
    bool checkArmor = false;
    bool checkProgress = false;
    bool checkPickups = false;
    bool checkProps = false;
    bool checkPropCombat = false;
    bool checkActorFeedback = false;
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
    bool checkStoreTemplate = false;
    bool checkUiFeedback = false;
    bool checkPackagePurchase = false;
    bool checkStoreCards = false;
    unsigned menuPage = 0;
    bool movieRegions = false;
    bool checkArmorRendering = false;
    bool checkLevelFlow = false;
    bool playSurvival = false;
    bool checkSurvival = false;
    bool withBrother = false;
    bool checkSpawnPerformance = false;
    bool checkRealtimePerformance = false;
    bool performanceUncachedPaths = false;
    bool checkPathCache = false;
    bool checkFlock = false;
    bool checkFlockPerformance = false;
    unsigned checkWaves = 2;
    unsigned startWave = 0;
    bool explicitMap = false;
    int armorIndex = -1;
    bool checkWeapons = false;
    bool checkWeaponEffects = false;
    bool checkMines = false;
    bool checkAudioTransitions = false;
    bool checkPostGamePresentation = false;
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

        if (std::strcmp(argument, "--fixtures") == 0) { ++i; continue; }
        if (std::strcmp(argument, "--reference-gallery") == 0) { continue; }
        if (std::strcmp(argument, "--test-output") == 0) {
            ++i;
            continue;
        }

        if (std::strcmp(argument, "--help") == 0 || std::strcmp(argument, "-h") == 0) {
            PrintTestUsage();
            return 0;
        }
        if (std::strcmp(argument, "--mute") == 0) {
            ZAudioPlayer::SetMuted(true);
            continue;
        }
        if (std::strcmp(argument, "--profile") == 0 && i + 1 < argc) {
            profilePath = Paths::Resolve(std::filesystem::u8path(argv[++i])).u8string();
            continue;
        }
        ++modeArgumentCount;

        if (std::strcmp(argument, "--movie") == 0 && i + 1 < argc) {
            movieStudy = true;
            movieOrdinal = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
        } 
else if (std::strcmp(argument, "--movie-gallery") == 0) {
            movieStudy = true;
            movieGallery = true;
        }
 else if (std::strcmp(argument, "--movie-regions") == 0) {
            movieRegions = true;
            --modeArgumentCount;
        } 
else if (std::strcmp(argument, "--hud-check") == 0) { checkHud = true; }

else if (std::strcmp(argument, "--movie-check") == 0) {
            checkMovies = true;
        }
else if (std::strcmp(argument, "--fontbitmap") == 0) { checkFontBitmap = true; }
 else if (std::strcmp(argument, "--intro") == 0) {
            introStudy = true;
        } 
else if (std::strcmp(argument, "--media-check") == 0) {
            checkMedia = true;
        }
 else if (std::strcmp(argument, "--skip-intro") == 0) {
            skipIntro = true;
            playGame = true;
        } else if (std::strcmp(argument, "--viewer") == 0) {
            researchMenu = true;
        } else if (std::strcmp(argument, "--game") == 0) {
            playGame = true;
        } 
else if (std::strcmp(argument, "--pickup-check") == 0) {
            checkPickups = true;
        }
 
else if (std::strcmp(argument, "--mission-check") == 0) {
            checkMissions = true;
        }
 
else if (std::strcmp(argument, "--original-save-check") == 0) {
            checkOriginalSaves = true;
        }
 else if (std::strcmp(argument, "--original-profile") == 0) {
            playOriginalProfile = true;
        } 
else if (std::strcmp(argument, "--original-profile-check") == 0) {
            checkOriginalProfile = true;
        }
 
else if (std::strcmp(argument, "--horde") == 0 || std::strcmp(argument, "--horde-check") == 0) {
            playCampaign = true;
            checkCampaign = std::strcmp(argument, "--horde-check") == 0;
            campaignPack = "pack11";
            campaignMission = 0;
            if (i + 1 < argc && argv[i + 1][0] != '-') { campaignMission = std::atoi(argv[++i]); }
        }
 
else if ((std::strcmp(argument, "--campaign") == 0 || std::strcmp(argument, "--campaign-check") == 0) && i + 2 < argc) {
            playCampaign = true;
            checkCampaign = std::strcmp(argument, "--campaign-check") == 0;
            campaignPack = argv[++i];
            campaignMission = std::atoi(argv[++i]);
        }
 
else if (std::strcmp(argument, "--prop-check") == 0) {
            checkProps = true;
        }
else if (std::strcmp(argument, "--prop-combat-check") == 0) {
            checkPropCombat = true;
        }
else if (std::strcmp(argument, "--actor-feedback-check") == 0) {
            checkActorFeedback = true;
        }
 
else if (std::strcmp(argument, "--powerup-check") == 0) {
            checkPowerups = true;
        }
 
else if (std::strcmp(argument, "--powerup-study") == 0) {
            powerupStudy = true;
            playSurvival = true;
        }
 
else if (std::strcmp(argument, "--powerup-play-check") == 0) {
            powerupStudy = true;
            playSurvival = true;
            checkSurvival = true;
        }
 
else if (std::strcmp(argument, "--pickup-render-check") == 0) {
            checkPickupRendering = true;
        }
 
else if (std::strcmp(argument, "--menu-page") == 0 && i + 1 < argc) {
            menuPage = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
        }
 
else if (std::strcmp(argument, "--start-wave") == 0 && i + 1 < argc) {
            const unsigned displayWave = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
            if (displayWave == 0 || displayWave > 500) { return 1; }
            startWave = displayWave - 1;
        }
 
else if (std::strcmp(argument, "--check-waves") == 0 && i + 1 < argc) {
            checkWaves = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
            if (checkWaves == 0 || checkWaves > 500) { return 1; }
        }
 
else if (std::strcmp(argument, "--brother-check") == 0) {
            playSurvival = true;
            checkSurvival = true;
            withBrother = true;
        }
 else if (std::strcmp(argument, "--brother") == 0) {
            withBrother = true;
            playSurvival = true;
        } 
else if (std::strcmp(argument, "--survival-check") == 0) {
            playSurvival = true;
            checkSurvival = true;
        }
 else if (std::strcmp(argument, "--play") == 0) {
            playSurvival = true;
        } 
else if (std::strcmp(argument, "--level-flow-check") == 0) {
            checkLevelFlow = true;
        }
 
else if (std::strcmp(argument, "--game-menu-check") == 0) {
            checkGameMenu = true;
        }
 
else if (std::strcmp(argument, "--bank-check") == 0) {
            checkBank = true;
            ++modeArgumentCount;
        }
 
else if (std::strcmp(argument, "--options-check") == 0) {
            checkOptions = true;
        }
 
else if (std::strcmp(argument, "--social-check") == 0) {
            checkSocial = true;
        }
 
else if (std::strcmp(argument, "--play-interaction-check") == 0) {
            checkPlayInteraction = true;
        }
 
else if (std::strcmp(argument, "--mission-menu-check") == 0) {
            checkMissionMenu = true;
        }
 
else if (std::strcmp(argument, "--header-check") == 0) {
            checkHeader = true;
        }
 
else if (std::strcmp(argument, "--refinery-menu-check") == 0) {
            checkRefineryMenu = true;
        }
 
else if (std::strcmp(argument, "--greeting-check") == 0) {
            checkGreeting = true;
        }
 
else if (std::strcmp(argument, "--player-select-check") == 0) {
            checkPlayerSelect = true;
        }
 
else if (std::strcmp(argument, "--original-hud-check") == 0) {
            checkOriginalHud = true;
        }
 
else if (std::strcmp(argument, "--powerup-selector-check") == 0) {
            checkPowerupSelector = true;
        }
 
else if (std::strcmp(argument, "--pause-check") == 0) {
            checkPause = true;
        }
 
else if (std::strcmp(argument, "--postgame-menu-check") == 0) {
            checkPostGame = true;
        }
 
else if (std::strcmp(argument, "--planet-menu-check") == 0) {
            checkPlanetMenu = true;
            ++modeArgumentCount;
        }
 
else if (std::strcmp(argument, "--native-profile-play-check") == 0) {
            checkNativeProfilePlay = true;
            ++modeArgumentCount;
        }
 
else if (std::strcmp(argument, "--native-profile-check") == 0) {
            checkNativeProfile = true;
        }
 
else if (std::strcmp(argument, "--upgrade-popup-check") == 0) {
            checkUpgradePopup = true;
        }
 
else if (std::strcmp(argument, "--store-card-check") == 0) {
            checkStoreCards = true;
        }
 
else if (std::strcmp(argument, "--store-template-check") == 0) {
            checkStoreTemplate = true;
        }
 
else if (std::strcmp(argument, "--ui-feedback-check") == 0) {
            checkUiFeedback = true;
        }
 
else if (std::strcmp(argument, "--package-purchase-check") == 0) {
            checkPackagePurchase = true;
        }
 
else if (std::strcmp(argument, "--tutorial-check") == 0) {
            checkTutorial = true;
        }
 
else if (std::strcmp(argument, "--daily-bonus-check") == 0) {
            checkDailyBonus = true;
        }
 
else if (std::strcmp(argument, "--loading-wipe-check") == 0) {
            checkLoadingWipe = true;
        }
 
else if (std::strcmp(argument, "--promotion-check") == 0) {
            checkPromotion = true;
        }
 
else if (std::strcmp(argument, "--scene-transition-check") == 0) {
            checkSceneTransition = true;
        }
 
else if (std::strcmp(argument, "--store-equipped-check") == 0) {
            checkStoreEquipped = true;
        }

else if (std::strcmp(argument, "--dual-weapon-check") == 0) {
            checkDualWeapon = true;
        }
 
else if (std::strcmp(argument, "--boss-check") == 0) {
            checkBoss = true;
        }
 
else if (std::strcmp(argument, "--map-occlusion-check") == 0) {
            checkMapOcclusion = true;
        }
 
else if (std::strcmp(argument, "--player-death-check") == 0) {
            checkPlayerDeath = true;
        }
 
else if (std::strcmp(argument, "--combat-feedback-check") == 0) {
            checkCombatFeedback = true;
        }
 
else if (std::strcmp(argument, "--dialog-check") == 0) {
            checkDialog = true;
        }
 
else if (std::strcmp(argument, "--performance-check") == 0) {
            checkPerformance = true;
        }
else if (std::strcmp(argument, "--local-live-check") == 0) { checkLocalLive = true; }
else if (std::strcmp(argument, "--spawn-performance-check") == 0) {
            checkSpawnPerformance = true;
        }
else if (std::strcmp(argument, "--path-cache-check") == 0) {
            checkPathCache = true;
        }
else if (std::strcmp(argument, "--flock-check") == 0) {
            checkFlock = true;
        }
else if (std::strcmp(argument, "--flock-performance-check") == 0) {
            checkFlockPerformance = true;
        }
else if (std::strcmp(argument, "--disable-flock") == 0) {
            PerformanceProbe::disableFlock = true;
        }
else if (std::strcmp(argument, "--spawn-performance-realtime-check") == 0) {
            checkSpawnPerformance = true;
            checkRealtimePerformance = true;
        }
else if (std::strcmp(argument, "--uncached-paths") == 0) {
            performanceUncachedPaths = true;
        }
 
else if (std::strcmp(argument, "--profile-play-check") == 0) {
            checkProfilePlay = true;
        }
 
else if (std::strcmp(argument, "--progress-check") == 0) {
            checkProgress = true;
        }
 
else if (std::strcmp(argument, "--armor-check") == 0) {
            checkArmor = true;
        }
 
else if (std::strcmp(argument, "--armor-render-check") == 0) {
            checkArmorRendering = true;
        }
 else if (std::strcmp(argument, "--armor") == 0) {
            armorIndex = 0;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                armorIndex = static_cast<int>(std::strtoul(argv[++i], nullptr, 10));
            }
        } else if (std::strcmp(argument, "--m1") == 0) {
            runM1 = true;
        } else if (std::strcmp(argument, "--weapons") == 0) {
            surveyWeapons = true;
        } 
else if (std::strcmp(argument, "--weapon-check") == 0) {
            checkWeapons = true;
        }
 
else if (std::strcmp(argument, "--postgame-presentation-check") == 0) {
            checkPostGamePresentation = true;
        }
 
else if (std::strcmp(argument, "--audio-transitions-check") == 0) {
            checkAudioTransitions = true;
        }
 
else if (std::strcmp(argument, "--weapon-effects-check") == 0) {
            checkWeaponEffects = true;
        }
else if (std::strcmp(argument, "--mine-check") == 0) {
            checkMines = true;
        }
 
else if (std::strcmp(argument, "--arena-check") == 0) {
            arena = true;
            checkArena = true;
        }
 else if (std::strcmp(argument, "--arena") == 0) {
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
            bigDirectory = Paths::Resolve(std::filesystem::u8path(argv[++i])).u8string();
        } else if (std::strcmp(argument, "--big-version") == 0) {
            inspectBigVersion = true;
        } 
else if (std::strcmp(argument, "--screenshot") == 0 && i + 1 < argc) {
            screenshotPath = argv[++i];
        }
 else if (std::strcmp(argument, "--advance") == 0 && i + 1 < argc) {
            advanceMs = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else {
            PrintTestUsage();
            return 1;
        }
    }

    auto configPath = Paths::Root() / ResearchDefaults::Filename;
    if (!TestOutput::directory.empty()) { configPath = TestOutput::directory / ResearchDefaults::Filename; }
    if (!GameHostSettings().Load(configPath)) { return 1; }
    // The original dial is 0..10 and a voice plays at dial x 0.1;
    // CAudioPlayer::SetEffectsGain documents the chain.
    ZAudioPlayer::SetEffectsGain(GameHostSettings().effectsVolume * 0.1f);

    // Nothing on the command line means nobody typed one: ask instead.
    if (ZAudioPlayer::IsMuted()) {
        std::printf("[audio] muted: playback streams disabled\n");
    }
    // Retail startup now enters the game; the historical menu above is explicit.
    if (modeArgumentCount == 0 || researchMenu) { PrintTestUsage(); return 0; }
    ZBigVersion bigVersion = ZBigVersion::Unknown;
    if (inspectBigVersion || (!introStudy && !checkMedia)) {
        if (!DetectViewerBigVersion(bigDirectory, bigVersion, inspectBigVersion)) { return 1; }
    }
    if (inspectBigVersion) { return 0; }
    if (checkMedia) { return RunMediaCheck(); }
    if (checkLoadingWipe) { return RunLoadingWipeCheck(bigDirectory); }
    if (checkPromotion) { return RunPromotionCheck(bigDirectory); }
    if (checkSceneTransition) { return RunSceneTransitionCheck(bigDirectory); }
    if (checkStoreEquipped) { return RunStoreEquippedCheck(bigDirectory); }
    if (checkDualWeapon) { return RunDualWeaponCheck(bigDirectory); }
    if (checkBoss) { return RunBossCheck(bigDirectory); }
    if (checkMapOcclusion) { return RunMapOcclusionCheck(bigDirectory); }
    if (checkPlayerDeath) { return RunPlayerDeathCheck(bigDirectory); }
    if (checkLocalLive) { return RunLocalLiveCheck(bigDirectory); }
    if (checkCombatFeedback) { return RunViewerSurvival(bigDirectory, "pack7", 6, 0, -1, "", 0, false, false, false, 2, 0, nullptr, false, false, nullptr, false, nullptr, true); }
    if (checkDialog) { return RunOriginalDialogCheck(bigDirectory); }
    if (checkHud) { return RunSurvivalHudCheck(bigDirectory); }
    if (checkMovies) { return RunMovieCheck(bigDirectory); }
    if (checkFontBitmap) { return RunFontBitmapCheck(bigDirectory); }
    if (movieStudy) { return RunMovieStudy(bigDirectory, movieOrdinal, screenshotPath, advanceMs, movieGallery, movieRegions); }
    if (introStudy) { return RunStartupSequence(screenshotPath, advanceMs); }
    if ((playGame || playOriginalProfile) && screenshotPath.empty() && !skipIntro) {
        const int result = RunStartupSequence();
        if (result == 2) { return 0; }
        if (result != 0) { return result; }
    }

    if (checkProgress) { return RunProgressCheck(bigDirectory); }
    if (checkPickups) { return RunPickupCheck(bigDirectory); }
    if (checkProps) { return RunPropCheck(bigDirectory); }
    if (checkPropCombat) { return RunPropCombatCheck(bigDirectory); }
    if (checkActorFeedback) { return RunActorFeedbackCheck(bigDirectory); }
    if (checkPowerups) { return RunPowerupCheck(bigDirectory); }
    if (checkMissions) { return RunMissionCheck(bigDirectory); }
    if (checkOriginalSaves) { return RunOriginalProfileCheck(bigDirectory); }
    if (playOriginalProfile) { return OpenGameMenu(bigDirectory, screenshotPath, menuPage, true, profilePath); }
    if (checkOriginalProfile) { return RunOriginalProfilePlayCheck(bigDirectory); }
    if (playCampaign) { return RunMissionPlay(bigDirectory, campaignPack, campaignMission, gunIndex, armorIndex, screenshotPath, advanceMs, firePreview, checkCampaign); }
    if (checkPickupRendering) { return RunPickupRenderCheck(bigDirectory); }
    if (checkProfilePlay) { return RunProfilePlayCheck(bigDirectory); }
    if (playGame) { return OpenGameMenu(bigDirectory, screenshotPath, menuPage, false, profilePath); }
    if (checkGameMenu) { return RunGameMenuCheck(bigDirectory); }
    if (checkUpgradePopup) { return RunUpgradePopupCheck(bigDirectory); }
    if (checkNativeProfile) { return RunNativeProfileCheck(bigDirectory); }
    if (checkNativeProfilePlay) { return RunNativeProfilePlayCheck(bigDirectory); }
    if (checkBank) { return RunStoreTemplateCheck(bigDirectory, false, true); }
    if (checkOptions) { return RunOptionsCheck(bigDirectory); }
    if (checkSocial) { return RunSocialOfflineCheck(bigDirectory); }
    if (checkPlanetMenu) { return RunPlanetMenuCheck(bigDirectory); }
    if (checkPlayInteraction) { return RunPlayInteractionCheck(bigDirectory); }
    if (checkMissionMenu) { return RunMissionMenuCheck(bigDirectory); }
    if (checkHeader) { return RunNavigationBarCheck(bigDirectory); }
    if (checkRefineryMenu) { return RunRefineryMenuCheck(bigDirectory); }
    if (checkGreeting) { return RunGreetingCheck(bigDirectory); }
    if (checkPlayerSelect) { return RunPlayerSelectCheck(bigDirectory); }
    if (checkOriginalHud) { return RunOriginalHudCheck(bigDirectory); }
    if (checkPowerupSelector) { return RunOriginalPowerupSelectorCheck(bigDirectory); }
    if (checkPause) { return RunOriginalPauseCheck(bigDirectory); }
    if (checkPostGame) { return RunPostGameMenuCheck(bigDirectory); }
    if (checkStoreCards) { return RunStoreTemplateCheck(bigDirectory, true); }
    if (checkStoreTemplate) { return RunStoreTemplateCheck(bigDirectory); }
    if (checkUiFeedback) { return RunStoreTemplateCheck(bigDirectory, false, false, true); }
    if (checkPackagePurchase) { return RunPackagePurchaseCheck(bigDirectory); }
    if (checkDailyBonus) { return RunDailyBonusCheck(bigDirectory); }
    if (checkTutorial) { return RunTutorialPlayCheck(bigDirectory); }
    PerformanceProbe::uncachedPaths = performanceUncachedPaths;
    if (checkSpawnPerformance) { return RunSpawnPerformanceCheck(bigDirectory, checkRealtimePerformance, performanceUncachedPaths); }
    if (checkPathCache) { return RunPathCacheCheck(); }
    if (checkFlock) { return RunFlockCheck(bigDirectory); }
    if (checkFlockPerformance) { return RunFlockPerformanceCheck(bigDirectory); }
    if (checkPerformance) {
        if (!explicitMap) { mapPackName = "pack2"; mapIndex = 7; }
        return RunViewerSurvival(bigDirectory, mapPackName, mapIndex, gunIndex, armorIndex, "", 0, false, false, false, 2, startWave, nullptr, true, false, nullptr, true);
    }
    if (checkArmor) {
        return RunArmorCheck(bigDirectory);
    }
    if (playSurvival) {
        if (!explicitMap) { mapPackName = "pack2"; mapIndex = 7; }
        return RunViewerSurvival(bigDirectory, mapPackName, mapIndex, gunIndex, armorIndex, screenshotPath, advanceMs, firePreview, showCollisions, checkSurvival, checkWaves, startWave, nullptr, withBrother, powerupStudy);
    }
    if (checkLevelFlow) {
        return RunLevelFlowCheck(bigDirectory);
    }
    if (checkArmorRendering) {
        return RunArmorRenderCheck(bigDirectory);
    }
    if (arena) {
        if (checkArena) { return RunArenaCheck(bigDirectory, enemyIndex, gunIndex); }
        return RunArena(bigDirectory, enemyIndex, gunIndex, screenshotPath, advanceMs, firePreview, showCollisions, armorIndex);
    }
    if (armorIndex >= 0) {
        return RunPlayerEquipmentPreview(bigDirectory, gunIndex, meshSpinDegrees, screenshotPath, advanceMs, firePreview, armorIndex);
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
        return RunEnemyPreview(bigDirectory, enemyIndex, meshSpinDegrees,
                           screenshotPath, advanceMs, bodyMoveIndex, stepStates,
                           stateIndex);
    }
    if (surveyWeapons) { return RunWeaponSurvey(bigDirectory); }
    if (checkPostGamePresentation) { return RunPostGamePresentationCheck(bigDirectory); }
    if (checkAudioTransitions) { return RunAudioTransitionsCheck(bigDirectory); }
    if (checkWeaponEffects) { return RunWeaponEffectsCheck(bigDirectory); }
    if (checkMines) { return RunMineCheck(bigDirectory); }
    if (checkWeapons) { return RunWeaponCheck(bigDirectory); }
    if (runM37) {
        return RunPlayerEquipmentPreview(bigDirectory, gunIndex, meshSpinDegrees,
                               screenshotPath, advanceMs, firePreview);
    }
    if (runM35) {
        return RunMeshPreview(bigDirectory, meshIndex, meshSpinDegrees,
                          meshFrameIndex, screenshotPath, advanceMs);
    }
    if (runM1) {
        return RunResourceSurvey(bigDirectory);
    }
    if (runM2) {
        return RunTextureStudy(bigDirectory, imagePackName, imageResourceId, screenshotPath);
    }
    ZMapViewMode mapViewMode = ZMapViewMode::Preview;
    if (runGameView) {
        mapViewMode = ZMapViewMode::GameView;
    }
    return RunMapPreview(bigDirectory, mapPackName, mapIndex, screenshotPath,
                    advanceMs, showSpawns, showCollisions, mapViewMode, gunIndex, firePreview);
}
