#include "gun_bros_viewer/Config.h"
using namespace ViewerConfig;
#include "gun_bros_viewer/ViewerMenu.h"
#include <cstdio>
#include <cstdlib>

void PrintUsage() {
    std::printf(
        "usage: GunBrosViewer [options]\n"
        "\n"
        "  (no options)              open the resource viewer menu\n"
        "  --viewer                open the permanent milestone menu\n"
        "  --intro                   play original Glu video (space/click to skip)\n"
        "  --skip-intro              enter menus without the startup video\n"
#if GB_ENABLE_TESTS
        "  --media-check             decode all original music and intro frames\n"
#endif
#if GB_ENABLE_TESTS
        "  --movie-check             original Glu timelines and Powerup border pixels\n"
#endif
        "  --movie <0..147>           inspect original UI; arrows browse, C regions\n"
#if GB_ENABLE_TESTS
        "  --movie-gallery           render every original core UI movie\n"
#endif
        "  --movie-regions           outline the user regions that define the original layout\n"
#if GB_ENABLE_TESTS
        "  --test-output <absolute>  case artifacts (default: tests/out/manual)\n"
#endif
        "  --help, -h                show available options and exit\n"
        "  --mute                    disable sound playback for every mode\n"
        "  --game                    planets, equipment, shop and local profile\n"
#if GB_ENABLE_TESTS
        "  --menu-page <0..29>       menu screenshot page (25: first brother selection)\n"
#endif
#if GB_ENABLE_TESTS
        "  --hud-check              original combat HUD, overlays and input check\n"
#endif
        "  --profile <directory>    use separate original DataStore files (.dat: legacy study)\n"
#if GB_ENABLE_TESTS
        "  --progress-check          verify progression, purchases and refinery\n"
#endif
#if GB_ENABLE_TESTS
        "  --profile-play-check      play, save, reload and resume test account\n"
#endif
        "  --play                    retail survival; default pack2 map7\n"
#if GB_ENABLE_TESTS
        "  --survival-check          real-map projectile/wave integration check\n"
#endif
#if GB_ENABLE_TESTS
        "  --brother-check           AI-only firing, death and wave revival check\n"
#endif
        "  --brother                 add the original AI follow/aim policy to --play\n"
#if GB_ENABLE_TESTS
        "  --game-menu-check         original navigation, help, social, promotions and transitions\n"
#endif
#if GB_ENABLE_TESTS
        "  --store-card-check        verify GUNS/ARMOR/POWER UPS cards and capture stages\n"
#endif
#if GB_ENABLE_TESTS
        "  --upgrade-popup-check     verify original mastery playback and purchase\n"
#endif
#if GB_ENABLE_TESTS
        "  --native-profile-check    verify original DataStore files and new profile\n"
#endif
#if GB_ENABLE_TESTS
        "  --native-profile-play-check original equipment, waves and native save reload\n"
#endif
#if GB_ENABLE_TESTS
        "  --bank-check        original bank cards, prompt and native save reload\n"
#endif
#if GB_ENABLE_TESTS
        "  --options-check     original list, preferences and native save reload\n"
#endif
#if GB_ENABLE_TESTS
        "  --social-check      original offline social UI; no invented native rewards\n"
#endif
#if GB_ENABLE_TESTS
        "  --planet-menu-check original map, mode overlay and four retail selections\n"
#endif
#if GB_ENABLE_TESTS
        "  --mission-menu-check original mission cards, requirements and wave selection\n"
#endif
#if GB_ENABLE_TESTS
        "  --play-interaction-check  verify selection, mode effects, back and scrolling\n"
#endif
#if GB_ENABLE_TESTS
        "  --header-check      original navigation, meters, entrance and input\n"
#endif
#if GB_ENABLE_TESTS
        "  --refinery-menu-check original refinery layout, transfers and native saving\n"
#endif
#if GB_ENABLE_TESTS
        "  --greeting-check original greeting animation and exit-time daily rewards\n"
#endif
#if GB_ENABLE_TESTS
        "  --player-select-check original brother selection and native save\n"
#endif
#if GB_ENABLE_TESTS
        "  --original-hud-check original regions, meters and control sprites\n"
#endif
#if GB_ENABLE_TESTS
        "  --powerup-selector-check original in-game selector and native purchase\n"
#endif
#if GB_ENABLE_TESTS
        "  --pause-check original pause list, help and native preferences\n"
#endif
#if GB_ENABLE_TESTS
        "  --postgame-menu-check original wrapup with actual native survival results\n"
#endif
#if GB_ENABLE_TESTS
        "  --store-template-check    verify BIG store filter and shared item cards\n"
#endif
#if GB_ENABLE_TESTS
        "  --ui-feedback-check       verify splash, package purchase and store badges\n"
#endif
#if GB_ENABLE_TESTS
        "  --package-purchase-check  verify package delivery, equipment and restart\n"
#endif
#if GB_ENABLE_TESTS
        "  --daily-bonus-check       original rewards, calendar cycle and save checks\n"
#endif
#if GB_ENABLE_TESTS
        "  --tutorial-check          original move, fire, swap and grenade tutorial\n"
#endif
#if GB_ENABLE_TESTS
        "  --loading-wipe-check original CG/STR pairs and two-region menu wipe\n"
#endif
#if GB_ENABLE_TESTS
        "  --promotion-check original Invite / Free Warbucks modal flow\n"
#endif
#if GB_ENABLE_TESTS
        "  --scene-transition-check  verify logo/menu/game share a window and GL context\n"
#endif
#if GB_ENABLE_TESTS
        "  --dialog-check            original BIG dialog playback and completion\n"
#endif
#if GB_ENABLE_TESTS
        "  --dual-weapon-check       both equipped stamps, distinct slots, save reload\n"
#endif
#if GB_ENABLE_TESTS
        "  --combat-feedback-check   spire, authored health bars, hits and silent audio burst checks\n"
#endif
#if GB_ENABLE_TESTS
        "  --boss-check              four retail Boss cameras, grenade armor and wave return\n"
#endif
#if GB_ENABLE_TESTS
        "  --map-occlusion-check     real obstacle/player front and back pixels\n"
#endif
#if GB_ENABLE_TESTS
        "  --player-death-check      original slow motion, death completion and stsuicide\n"
#endif
#if GB_ENABLE_TESTS
        "  --performance-check       record 1200 real gameplay frames to CSV\n"
#endif
#if GB_ENABLE_TESTS
        "  --pickup-check            pickup templates and collection scripts\n"
#endif
#if GB_ENABLE_TESTS
        "  --pickup-render-check     all pickup sprites and animation frames\n"
#endif
#if GB_ENABLE_TESTS
        "  --prop-check              complete prop templates and native callbacks\n"
#endif
#if GB_ENABLE_TESTS
        "  --powerup-check           consumable templates, queries and actions\n"
#endif
#if GB_ENABLE_TESTS
        "  --powerup-study           playable item lab, isolated stock\n"
#endif
#if GB_ENABLE_TESTS
        "  --powerup-play-check      inventory, XP float/fade and Airstrike frame check\n"
#endif
#if GB_ENABLE_TESTS
        "  --mission-check           archive mission, objective and level references\n"
#endif
#if GB_ENABLE_TESTS
        "  --original-save-check     read original data stores without modifying them\n"
#endif
        "  --original-profile       alias for the default original DataStore account\n"
#if GB_ENABLE_TESTS
        "  --original-profile-check validate import, original equipment and play/reload\n"
#endif
        "  --horde [0-9]            play BOKOR with original Horde script\n"
#if GB_ENABLE_TESTS
        "  --horde-check [0-9]      actual combat through one Horde\n"
#endif
        "  --campaign <pack> <n>     play original unfinished campaign mission\n"
#if GB_ENABLE_TESTS
        "  --campaign-check <pack> <n>  movement/combat smoke test in archived mission\n"
#endif
#if GB_ENABLE_TESTS
        "  --check-waves <n>         number of waves to play in survival check\n"
#endif
#if GB_ENABLE_TESTS
        "  --start-wave <1..500>     wave study / saved-progress starting point\n"
#endif
#if GB_ENABLE_TESTS
        "  --armor-check             verify every armor template, script and asset\n"
#endif
#if GB_ENABLE_TESTS
        "  --armor-render-check      render all armor with three weapon poses\n"
#endif
#if GB_ENABLE_TESTS
        "  --level-flow-check        simulate original level spawn/death events\n"
#endif
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
#if GB_ENABLE_TESTS
        "  --weapon-check            verify all weapon models and input transitions\n"
#endif
#if GB_ENABLE_TESTS
        "  --postgame-presentation-check  verify icon sparkles and instant tabs\n"
#endif
#if GB_ENABLE_TESTS
        "  --audio-transitions-check  store move sounds and menu/battle music handoffs\n"
#endif
#if GB_ENABLE_TESTS
        "  --weapon-effects-check    laser continuity, ray collision and projectile visuals\n"
#endif
        "  --arena [n]               combat arena for enemy template n\n"
#if GB_ENABLE_TESTS
        "  --arena-check             verify enemy catalogue and combat contracts\n"
#endif
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
        "                            (default: EXE-directory/big)\n"
#if GB_ENABLE_TESTS
        "  --screenshot <file.png>   save the first frame and exit\n"
#endif
        "  --advance <ms>            run the animations on this far before\n"
        "                            that first frame\n",
        kDefaultMapPack, kDefaultMapIndex, kDefaultImagePack, kDefaultImageResourceId);
}

int PromptForHarness() {
    std::printf(
        "\n=== GunBrosViewer ===\n"
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
#if GB_ENABLE_TESTS
        " 17  armor check      -- every armor template, script and asset\n"
#endif
        " 18  armor viewer     -- attachments, body textures and weapon poses\n"
#if GB_ENABLE_TESTS
        " 19  armor rendering check -- all armor with three weapon poses\n"
#endif
#if GB_ENABLE_TESTS
        " 20  level flow check -- original scripts, timers and spawn rules\n"
#endif
        " 21  Survival         -- original map, enemies and wave scripts\n"
#if GB_ENABLE_TESTS
        " 22  survival check   -- map combat, equipment, death and restart\n"
#endif
#if GB_ENABLE_TESTS
        " 23  wave study       -- choose a starting wave; original scripts\n"
#endif
#if GB_ENABLE_TESTS
        " 24  progress check   -- experience, health, store prices and references\n"
#endif
#if GB_ENABLE_TESTS
        " 25  profile play check -- play, save, reload and resume actual combat\n"
#endif
#if GB_ENABLE_TESTS
        " 27  game menu check  -- refine, buy, equip, select planet and play\n"
#endif
#if GB_ENABLE_TESTS
        " 28  AI brother check -- independent shooting, death and wave revival\n"
#endif
#if GB_ENABLE_TESTS
        " 29  pickup check     -- all templates and collection scripts\n"
#endif
#if GB_ENABLE_TESTS
        " 30  pickup render check -- all original pickup sprites\n"
#endif
#if GB_ENABLE_TESTS
        " 31  prop check       -- full templates, moves and script callbacks\n"
#endif
#if GB_ENABLE_TESTS
        " 32  powerup check    -- consumable templates and native actions\n"
#endif
#if GB_ENABLE_TESTS
        " 33  powerup lab      -- G use, F select; isolated item inventory\n"
#endif
#if GB_ENABLE_TESTS
        " 34  mission archive check -- original mission/objective records\n"
#endif
        " 35  campaign archive -- choose an unfinished original mission\n"
#if GB_ENABLE_TESTS
        " 36  original save check -- read-only native profile archive\n"
#endif
#if GB_ENABLE_TESTS
        " 37  original save lab -- play imported perfect save independently\n"
#endif
#if GB_ENABLE_TESTS
        " 38  original profile play check -- import, equipment and reload\n"
#endif
        " 39  startup movie    -- original Glu M4V and WAV\n"
#if GB_ENABLE_TESTS
        " 40  media check      -- full video and seven MP3 decode\n"
#endif
        " 41  original UI viewer -- CMovie timeline; arrows browse, C regions\n"
#if GB_ENABLE_TESTS
        " 42  original UI check -- all packs and font resource records\n"
#endif
#if GB_ENABLE_TESTS
        " 43  original UI gallery -- render every core movie under test output/ui-movies\n"
#endif
#if GB_ENABLE_TESTS
        " 44  combat HUD check -- active, pause, death and completion\n"
#endif
        " 45  BOKOR Horde -- original map and ten starting difficulties\n"
#if GB_ENABLE_TESTS
        " 46  BOKOR combat check -- real enemies and wave advancement\n"
#endif
#if GB_ENABLE_TESTS
        " 47  compact planet selection -- preserved pre-fidelity study\n"
#endif
#if GB_ENABLE_TESTS
        " 48  daily bonus -- original five-day reward and persistence check\n"
#endif
#if GB_ENABLE_TESTS
        " 49  first tutorial -- original scripts and isolated save check\n"
#endif
#if GB_ENABLE_TESTS
        " 50  performance -- 1200 rendered gameplay frames and CPU timing\n"
#endif
#if GB_ENABLE_TESTS
        " 51  store template -- filter animation, input and screenshots\n"
#endif
#if GB_ENABLE_TESTS
        " 52  store cards -- guns, armor, powerups and resource binding\n"
#endif
#if GB_ENABLE_TESTS
        " 53  upgrade popup -- original chapters, stars and purchase check\n"
#endif
#if GB_ENABLE_TESTS
        " 54  native profile -- original storage, new archive and roundtrip\n"
#endif
#if GB_ENABLE_TESTS
        " 55  native profile play -- original equipment, waves and save reload\n"
#endif
#if GB_ENABLE_TESTS
        " 56  bank -- original cards, currency prompt and native save reload\n"
#endif
#if GB_ENABLE_TESTS
        " 57  options -- original list, preferences and native save reload\n"
#endif
#if GB_ENABLE_TESTS
        " 58  social -- original offline UI and native reward boundary\n"
#endif
#if GB_ENABLE_TESTS
        " 59  planets -- original timeline, mode overlay and retail selection\n"
#endif
#if GB_ENABLE_TESTS
        " 60  missions -- original cards, requirements and wave selection\n"
#endif
#if GB_ENABLE_TESTS
        " 61  header -- original navigation bar, meters and input\n"
#endif
#if GB_ENABLE_TESTS
        " 62  refinery -- original meter layout, transfers and native saving\n"
#endif
#if GB_ENABLE_TESTS
        " 63  greeting -- original animation and native daily rewards\n"
#endif
#if GB_ENABLE_TESTS
        " 64  player select -- original portraits, chapters and native save\n"
#endif
#if GB_ENABLE_TESTS
        " 67  original HUD -- original regions, meters and control sprites\n"
#endif
#if GB_ENABLE_TESTS
        " 69  scene transitions -- logo, menu, game, menu on one window\n"
#endif
#if GB_ENABLE_TESTS
        " 70  dialog -- original BIG popup, portrait and automatic completion\n"
#endif
#if GB_ENABLE_TESTS
        " 71  dual weapons -- equipped stamps, duplicate guard and native save\n"
#endif
#if GB_ENABLE_TESTS
        " 73  promotions -- original Invite and Free Warbucks popups\n"
#endif
#if GB_ENABLE_TESTS
        " 74  loading and wipe -- original CG, STR and menu sweep\n"
#endif
#if GB_ENABLE_TESTS
        " 72  combat feedback -- spire, authored health bars, hits and audio bursts\n"
#endif
#if GB_ENABLE_TESTS
        " 78  Boss restoration -- four maps, camera and grenade armor\n"
#endif
#if GB_ENABLE_TESTS
        " 79  Map occlusion -- real obstacle/player front and back pixels\n"
#endif
#if GB_ENABLE_TESTS
        " 80  Player death -- original slow motion, completion and stsuicide\n"
#endif
#if GB_ENABLE_TESTS
        " 77  postgame presentation -- authored icon animations and instant tabs\n"
#endif
#if GB_ENABLE_TESTS
        " 76  audio transitions -- store swap and continuous scene music\n"
#endif
#if GB_ENABLE_TESTS
        " 75  weapon effects -- laser, Kraken and authored projectile visuals\n"
#endif
#if GB_ENABLE_TESTS
        " 68  powerup selector -- original layout, input and native purchase\n"
#endif
#if GB_ENABLE_TESTS
        " 66  pause -- original pause list, help and native preferences\n"
#endif
#if GB_ENABLE_TESTS
        " 65  postgame -- original result cards, casualties and native progress\n"
#endif
        "\n"
        "choice [1]: ");
    std::fflush(stdout);

    char line[64];
    if (std::fgets(line, sizeof(line), stdin) == nullptr) {
        return 1;
    }

    const int choice = std::atoi(line);
    if (choice < 1 || choice > 80) {
        return 1;
    }
    return choice;
}

