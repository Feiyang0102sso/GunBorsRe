#pragma once
#include "gun_bros_re/ui/ZHudState.h"
#include "gun_bros_re/ui/CPowerUpSelector.h"
/** The same rectangles drive drawing and pointer input; no gameplay is owned here. */
class CInputPad {
public:
    void BrowseRemoteShop(unsigned selection) { m_selector.BrowseRemoteShop(selection); }
    bool BackFromSelectorPrompt() { return m_selector.BackFromSelectorPrompt(); }
    void ReportSelectorPurchase(ZPurchaseResult result, const ZInputPadState &state) { m_selector.ReportSelectorPurchase(result, state); }
    const ZStoreEntry *SelectedItem() const { return m_selector.SelectedItem(); }
    bool ConfigureDeathmatch(const std::vector<GameObjectRef> &stores) { return m_selector.ConfigureDeathmatch(stores); }
    void ResetSelector(bool startOnGuns = false) { m_selector.ResetSelector(startOnGuns); }
    unsigned MatchSelectionSlot() const { return m_selector.MatchSelectionSlot(); }
    void AdvanceMatchSelection() { m_selector.AdvanceMatchSelection(); }
    bool ShowDialog(const std::string &text, bool automatic, unsigned arrow) { return m_dialog.Show(m_resources.m_movies, text, automatic, arrow); }
    void UpdateDialog(unsigned deltaMs) { m_dialog.Update(deltaMs); }
    void ClearDialog(bool immediate) { m_dialog.Clear(immediate); }
    bool IsDialogDone() const { return m_dialog.IsDone(); }
    bool Init(CResTOCManager &toc, ZPackTables &tables);
    void SetChallenges(const CChallengeManager *challenges) { m_challenges = challenges; }
    bool HasChallenges() const;
    bool IsChallengeHeld() const { return m_challengeHeld; }
    unsigned ChallengeRowsDrawn() const { return m_challengeRows; }
    bool DrawChallengeOverlay(float x, float y, unsigned elapsed, float alpha = 1);
    bool Draw(const ZInputPadState &state);
    bool DrawTutorialDebugNotice(std::uint64_t ticks);
    /** Original level effect pass, also callable by the permanent death check. */
    bool DrawExperienceTexts(const std::vector<ZCombatWorld::ExperienceText> &texts, bool horde);
    void Advance(int deltaMs);
    void ResetNotices();
    // Original CInputPad interstitial callbacks release LEVEL event 2.
    void BeginLevel(unsigned wave, bool horde, bool boss);
    void BeginDeathmatch(unsigned killLimit);
    bool TakeDeathmatchIntroCompletion();
    void OnDeathmatchPowerup(const std::string &name);
    void AdvanceDeathmatchWrapUp(unsigned deltaMs);
    bool IsDeathmatchWrapUpComplete() const;
    void OnWaveClear(unsigned wave, bool perfect, unsigned rewardPercent, bool boss);
    bool HasInterstitial() const;
    bool TakeInterstitialCompletion();
    void BeginLiveWave(const ZMultiplayerStatistics &player, const ZMultiplayerStatistics &peer);
    unsigned LiveWaveRemaining() const { return m_liveWaveRemaining; }
    void SetLiveBrotherIndex(unsigned index) { m_liveBrotherIndex = index; }
    void SetLivePeerIndex(unsigned index) { m_livePeerIndex = index; }
    void SetLivePeerName(const std::string &name) { m_livePeerName = name; }
    std::string DefaultBrotherName(unsigned index) const {
        if (index == 0) { return m_resources.m_movies.NamedString("IDS_FRIEND_DEFAULT_BRO1"); }
        return m_resources.m_movies.NamedString("IDS_FRIEND_DEFAULT_BRO2");
    }
    unsigned NoticeCount() const { return static_cast<unsigned>(m_notices.size()); }
    unsigned NoticeTime() const { if (m_notices.empty()) { return 0; } return m_notices.front().elapsed; }
    ZInputPadAction Pointer(const ZInputPadState &state, float x, float y, bool down);
    bool CapturesPointer(const ZInputPadState &state, float x, float y) const;
    void Scroll(const ZInputPadState &state, float amount);
    void ScrollMenuInput(const ZInputPadState &state, float wheel, float dragX, float dragY);
    void AdvanceMenu(unsigned deltaMs);
    bool BackFromHelp();
    // Explicit Show also resets a selector replaced while it is still visible.

    // Research/accessibility queries use the same authored hit regions as input.
    bool FindActionRegion(const ZInputPadState &state, ZInputPadAction action, ZMovieRegion &region) const;
private:
    ZHudResources m_resources;
    CPowerUpSelector m_selector{m_resources};
    bool m_matchIntro = false, m_matchIntroCompleted = false;
    bool m_matchWrapUp = false;
    unsigned m_matchWrapUpTime = 0, m_matchWrapUpDuration = 0;
    void AddMatchMessage(const std::string &text);
    struct MatchMessage { std::string text; unsigned remainingMs = 5000; };
    std::vector<MatchMessage> m_matchMessages;
    unsigned m_previousMatchScore[2]{};
    std::string m_livePeerName = "LOCAL BOT";
    const CChallengeManager *m_challenges = nullptr;
    bool m_challengeHeld = false;
    unsigned m_challengeTime = 0, m_challengeRows = 0;
    CDialogPopup m_dialog;
    struct Button;
    friend int CheckDeathmatchMenus(CResTOCManager &, ZPackTables &, CProfileManager &);
    friend int RunOriginalHudCheck(const std::string &bigDirectory);
    friend int RunOriginalDialogCheck(const std::string &bigDirectory);
    friend int RunOriginalPowerupSelectorCheck(const std::string &bigDirectory);
    friend int RunOriginalPauseCheck(const std::string &bigDirectory);
    bool DrawPause(const ZInputPadState &state);
    bool DrawControls(const ZInputPadState &state);
    bool DrawMeter(const ZMovieRegion &area, unsigned slot);
    bool DrawPowerup(const ZInputPadState &state, unsigned slot, float x, float y, unsigned movie, unsigned time);
    std::vector<Button> ControlButtons(const ZInputPadState &state) const;
    ZInputPadAction PausePointer(const ZInputPadState &state, float x, float y);
    std::string PauseText(const ZInputPadState &state, unsigned index, unsigned slot);
    void ResetPauseList();
    bool m_pauseBound = false, m_pauseHelp = false;
    unsigned m_pauseTime = 0, m_pauseBodyTime = 0, m_pauseFocus = 0, m_pauseDelta = 0;
    float m_pausePosition = 0, m_pauseTarget = 0, m_pauseBodyPosition = 0;
    std::vector<unsigned> m_pauseItems, m_pauseButtonTimes;
    std::vector<std::pair<ZMovieRegion, unsigned>> m_pauseHits;
    struct Button { ZMovieRegion rect; ZInputPadAction action; const char *label; };
    std::vector<Button> Buttons(const ZInputPadState &state) const;
    void Centre(const std::string &text, float y, unsigned font = 0, float scale = 1);
    void Icon(unsigned type, const GameObjectRef &object, const ZMovieRegion &region) { m_resources.Icon(type, object, region); }
    void ObserveProgress(const ZInputPadState &state);
    void DrawNotice();
    bool DrawLiveWave(unsigned time);
    ZMultiplayerStatistics m_liveStats[2];
    unsigned m_liveWaveRemaining = 0;
    unsigned m_liveBrotherIndex = 0;
    unsigned m_livePeerIndex = 1;
    void QueueNotice(const char *movie, const std::string &title, const std::string &footer = "", bool releaseLevel = false);
    std::string NoticeNumber(const char *name, unsigned number);
    struct Notice { unsigned movie, elapsed; std::string title, footer; bool releaseLevel = false; };
    std::vector<Notice> m_notices;
    unsigned m_previousLevel = 0;
    bool m_observedProgress = false;
    bool m_interstitialCompleted = false;
    CInputPadMeter m_meters[2];
    bool m_metersBound = false;
    unsigned m_controlTime = 0;
    bool m_previousDown = false;
    float m_mouseX = -1, m_mouseY = -1;
};
