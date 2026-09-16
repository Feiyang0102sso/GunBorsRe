/** @file ZHudState.h
 * @brief Original CInputPad artwork bound to the desktop survival state.
 */
#ifndef GUN_BROS_RE_ZHUDSTATE_H
#define GUN_BROS_RE_ZHUDSTATE_H
#include "engine/glu/movie/ZMovieRenderer.h"
#include "gun_bros_re/ui/CDialogPopup.h"
#include "gun_bros_re/gameplay/CInputPadMeter.h"
#include "gun_bros_re/ui/CMenuPopupPrompt.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/gameplay/CLevelIndicator.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/data/ZPowerupCatalog.h"
#include "gun_bros_re/gameplay/CLevel.h"

enum class ZInputPadAction { None, Pause, Resume, Retry, Exit, Weapon1, Weapon2, UseItem, NextItem, Continue,
    BroOps, SwapWeapon, OpenShop, CloseShop, SelectItem, BuyItem, EquipLeft, EquipRight, UseNow, CancelItem, UseLeft, Sound, Music, DockedSticks,
    ShowPowerups, ShowGuns, SelectMatchGun, MatchSlot1, MatchSlot2 };

struct ZInputPadState {
    float health = 0, maximumHealth = 1, brotherHealth = 0, brotherMaximumHealth = 1;
    unsigned level = 1, wave = 0, enemies = 0, kills = 0, weaponSlot = 0, itemCount = 0;
    std::uint64_t experience = 0, experienceDelta = 1, xplodium = 0, perfectBonus = 0;
    unsigned transitionTime = 0;
    unsigned stopwatchMs = 0;
    unsigned bossIntroSerial = 0;
    int xplodiumMultiplier = 100;
    float playerX = 0, playerY = 0, damageDealt = 0;
    // Read-only diagnostics, populated by the active session rather than UI estimates.
    std::string debugMap;
    int levelState = 0;
    std::size_t projectiles = 0, particles = 0;
    unsigned damageHits = 0, perfectWaves = 0, clearedWaves = 0;
    bool showCollisions = false, lastWavePerfect = false;
    bool horde = false;
    bool localLive = false;
    bool deathmatch = false;
    unsigned matchScore[2]{}, matchLimit = 0, respawnMs = 0;
    std::map<unsigned, int> powerupCooldowns;
    float reviveProgress = 0;
    ZMovieRegion reviveBar;
    float revivePaddingX = 1, revivePaddingY = 1;
    unsigned score = 0, killStreak = 0;
    bool paused = false, dead = false, cleared = false, transitioning = false, withBrother = false;
    bool shopOpen = false, itemChoice = false, soundEnabled = true, musicEnabled = true;
    bool remoteShop = false;
    bool afterDeathShop = false;
    unsigned shopRemainingMs = 0;
    bool dockedSticks = true;
    bool swapKeyDown = false;
    bool inputHidden = false;
    ZPowerupStatus powerupStatus;
    std::uint64_t coins = 0, warbucks = 0;
    float moveX = 0, moveY = 0, aimX = 0, aimY = 0;
    GameObjectRef leftPowerup, rightPowerup;
    unsigned leftCount = 0, rightCount = 0;
    std::vector<ZPowerupInventoryEntry> inventory;
    std::string weapon, item, buffs, dialog, mission;
    int tutorialStep = -1;
    std::string brotherName;
    float brotherLabelX = 0, brotherLabelY = 0, brotherLabelAlpha = 0;
    GameObjectRef guns[2], powerup;
    std::vector<CLevelIndicator> indicators;
    std::vector<CLevel::HealthBar> enemyHealthBars;
};

#endif
