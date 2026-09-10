/** @file SurvivalHud.h
 * @brief Original CInputPad artwork bound to the desktop survival state.
 */
#ifndef GUN_BROS_RE_SURVIVALHUD_H
#define GUN_BROS_RE_SURVIVALHUD_H
#include "runtime/MovieRenderer.h"
#include "runtime/OriginalDialogPopup.h"
#include "gun_bros/CInputPadMeter.h"
#include "gun_bros/CMenuPopupPrompt.h"
#include "runtime/StoreCatalog.h"
#include "gun_bros/CLevelIndicator.h"
#include "gun_bros/CProfileManager.h"
#include "runtime/PowerupCatalog.h"
#include "runtime/CombatScene.h"

enum class SurvivalHudAction { None, Pause, Resume, Retry, Exit, Weapon1, Weapon2, UseItem, NextItem, Continue,
    SwapWeapon, OpenShop, CloseShop, SelectItem, BuyItem, EquipLeft, EquipRight, UseNow, CancelItem, UseLeft, Sound, Music, DockedSticks };

struct SurvivalHudState {
    float health = 0, maximumHealth = 1, brotherHealth = 0, brotherMaximumHealth = 1;
    unsigned level = 1, wave = 0, enemies = 0, kills = 0, weaponSlot = 0, itemCount = 0;
    std::uint64_t experience = 0, experienceDelta = 1, xplodium = 0, perfectBonus = 0;
    unsigned transitionTime = 0;
    unsigned stopwatchMs = 0;
    unsigned bossIntroSerial = 0;
    int xplodiumMultiplier = 100;
    float frameMs = 0, playerX = 0, playerY = 0, damageDealt = 0;
    bool horde = false;
    unsigned score = 0, killStreak = 0;
    bool paused = false, dead = false, cleared = false, transitioning = false, withBrother = false;
    bool shopOpen = false, itemChoice = false, soundEnabled = true, musicEnabled = true;
    bool originalUi = false, dockedSticks = true;
    bool swapKeyDown = false;
    bool inputHidden = false;
    PowerupStatus powerupStatus;
    std::uint64_t coins = 0, warbucks = 0;
    float moveX = 0, moveY = 0, aimX = 0, aimY = 0;
    GameObjectRef leftPowerup, rightPowerup;
    unsigned leftCount = 0, rightCount = 0;
    std::vector<PowerupInventoryEntry> inventory;
    std::string weapon, item, buffs, dialog, mission;
    int tutorialStep = -1;
    std::string brotherName;
    float brotherLabelX = 0, brotherLabelY = 0, brotherLabelAlpha = 0;
    GameObjectRef guns[2], powerup;
    std::vector<CLevelIndicator> indicators;
    std::vector<CombatScene::HealthBar> enemyHealthBars;
};

/** The same rectangles drive drawing and pointer input; no gameplay is owned here. */
class SurvivalHud {
public:
    bool ShowDialog(const std::string &text, bool automatic, unsigned arrow) { return m_dialog.Show(m_movies, text, automatic, arrow); }
    void UpdateDialog(unsigned deltaMs) { m_dialog.Update(deltaMs); }
    void ClearDialog(bool immediate) { m_dialog.Clear(immediate); }
    bool IsDialogDone() const { return m_dialog.IsDone(); }
    bool Init(CResTOCManager &toc, PackTables &tables);
    bool Draw(const SurvivalHudState &state);
    /** Original level effect pass, also callable by the permanent death check. */
    bool DrawExperienceTexts(const std::vector<CombatScene::ExperienceText> &texts, bool horde);
    void Advance(int deltaMs);
    void ResetNotices();
    // Original CInputPad interstitial callbacks release LEVEL event 2.
    void BeginOriginalLevel(unsigned wave, bool horde, bool boss);
    void OnOriginalWaveClear(unsigned wave, bool perfect, unsigned rewardPercent, bool boss);
    bool HasInterstitial() const;
    bool TakeInterstitialCompletion();
    unsigned NoticeCount() const { return static_cast<unsigned>(m_notices.size()); }
    unsigned NoticeTime() const { if (m_notices.empty()) { return 0; } return m_notices.front().elapsed; }
    SurvivalHudAction Pointer(const SurvivalHudState &state, float x, float y, bool down);
    bool CapturesPointer(const SurvivalHudState &state, float x, float y) const;
    void Scroll(const SurvivalHudState &state, float amount);
    void ScrollMenuInput(const SurvivalHudState &state, float wheel, float dragX, float dragY);
    void AdvanceMenu(unsigned deltaMs);
    bool BackFromHelp();
    bool BackFromSelectorPrompt();
    void ReportSelectorPurchase(PurchaseResult result, const SurvivalHudState &state);
    const StoreEntry *SelectedItem() const;
    // Research/accessibility queries use the same authored hit regions as input.
    bool FindActionRegion(const SurvivalHudState &state, SurvivalHudAction action, MovieRegion &region) const;
private:
    CDialogPopup m_dialog;
    struct Button;
    friend int RunOriginalHudCheck(const std::string &bigDirectory);
    friend int RunOriginalDialogCheck(const std::string &bigDirectory);
    friend int RunOriginalPowerupSelectorCheck(const std::string &bigDirectory);
    friend int RunOriginalPauseCheck(const std::string &bigDirectory);
    bool DrawOriginalPause(const SurvivalHudState &state);
    bool DrawOriginalControls(const SurvivalHudState &state);
    bool DrawOriginalMeter(const MovieRegion &area, unsigned slot);
    bool DrawOriginalPowerup(const SurvivalHudState &state, unsigned slot, float x, float y, unsigned movie, unsigned time);
    bool DrawOriginalSelector(const SurvivalHudState &state);
    bool DrawSelectorPrompt();
    CMenuPopupPrompt m_selectorPrompt;
    const char *m_selectorPromptTable = nullptr;
    std::string m_selectorPromptBody;
    bool m_selectorPromptFunds = false, m_selectorPromptRequested = false;
    std::vector<std::pair<MovieRegion, unsigned>> m_selectorPromptHits;
    bool DrawSelectorItem(const SurvivalHudState &state, unsigned index, const MovieRegion &area);
    bool DrawSelectorButton(const char *table, unsigned index, float x, float y, float alpha,
        SurvivalHudAction action, int storeIndex = -1, MovieRegion *touch = nullptr);
    std::string SelectorCurrency(const char *name, std::uint64_t amount);
    struct SelectorHit { MovieRegion area; SurvivalHudAction action; int storeIndex = -1; };
    std::vector<SelectorHit> m_selectorHits;
    std::vector<unsigned> m_selectorEntries;
    unsigned m_selectorTime = 0, m_selectorChoiceTime = 0;
    float m_selectorPosition = 0, m_selectorTarget = 0;
    bool m_selectorBound = false, m_selectorChoice = false;
    float m_selectorPressX = 0, m_selectorPressY = 0;
    bool m_selectorDragged = false, m_selectorPressArmed = false;
    std::vector<Button> OriginalControlButtons(const SurvivalHudState &state) const;
    SurvivalHudAction OriginalPausePointer(const SurvivalHudState &state, float x, float y);
    std::string PauseText(const SurvivalHudState &state, unsigned index, unsigned slot);
    void ResetPauseList();
    bool m_pauseBound = false, m_pauseHelp = false;
    unsigned m_pauseTime = 0, m_pauseBodyTime = 0, m_pauseFocus = 0, m_pauseDelta = 0;
    float m_pausePosition = 0, m_pauseTarget = 0, m_pauseBodyPosition = 0;
    std::vector<unsigned> m_pauseItems, m_pauseButtonTimes;
    std::vector<std::pair<MovieRegion, unsigned>> m_pauseHits;
    struct Button { MovieRegion rect; SurvivalHudAction action; const char *label; };
    std::vector<Button> Buttons(const SurvivalHudState &state) const;
    void Centre(const std::string &text, float y, unsigned font = 0, float scale = 1);
    void Icon(unsigned type, const GameObjectRef &object, const MovieRegion &region);
    void ObserveProgress(const SurvivalHudState &state);
    void DrawNotice();
    void QueueOriginalNotice(const char *movie, const std::string &title, const std::string &footer = "", bool releaseLevel = false);
    std::string OriginalNoticeNumber(const char *name, unsigned number);
    int m_selectedItem = -1;
    struct Notice { unsigned movie, elapsed; std::string title, footer; bool releaseLevel = false; };
    std::vector<Notice> m_notices;
    unsigned m_previousLevel = 0;
    bool m_observedProgress = false;
    bool m_interstitialCompleted = false;
    mutable MovieRenderer m_movies;
    CInputPadMeter m_meters[2];
    bool m_metersBound = false;
    unsigned m_controlTime = 0;
    PackTables *m_tables = nullptr;
    std::vector<StoreEntry> m_store;
    std::vector<PowerupEntry> m_powerups;
    std::map<std::uint32_t, std::unique_ptr<MovieRenderer>> m_powerupRenderers;
    std::map<std::uint64_t, std::unique_ptr<CTexture>> m_icons;
    bool m_previousDown = false;
    float m_mouseX = -1, m_mouseY = -1;
};

int RunOriginalPauseCheck(const std::string &bigDirectory);
int RunOriginalHudCheck(const std::string &bigDirectory);
int RunOriginalPowerupSelectorCheck(const std::string &bigDirectory);
int RunSurvivalHudCheck(const std::string &bigDirectory);
#endif
