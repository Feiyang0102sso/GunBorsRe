#include "research/ResearchDefaults.h"
using namespace ResearchDefaults;
#include "TestApplication.h"
#include <cstdio>
#include <cstdlib>

void PrintTestUsage() {
    std::printf(
        "usage: GunBrosTests [options]\n"
        "\n"
        "  --intro                   play original Glu video (space/click to skip)\n"
        "  --skip-intro              enter menus without the startup video\n"
#if GB_ENABLE_TESTS
        "  --media-check             decode all original music and intro frames\n"
#endif
#if GB_ENABLE_TESTS
        "  --movie-check             original Glu timelines and Powerup border pixels\n"
        "  --fontbitmap              render all BIG fonts and export their original PNG atlases\n"
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
        "  --mine-check              mine recycling, boundary explosions and authored lifetime\n"
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
        "  --big <directory>         where the .big files are (XGA or plain TOC)\n"
        "  --big-version             inspect auto-detected BigVersion 1/2/3\n"
        "                            (default: EXE-directory/big)\n"
#if GB_ENABLE_TESTS
        "  --screenshot <file.png>   save the first frame and exit\n"
#endif
        "  --advance <ms>            run the animations on this far before\n"
        "                            that first frame\n",
        kDefaultMapPack, kDefaultMapIndex, kDefaultImagePack, kDefaultImageResourceId);
}

