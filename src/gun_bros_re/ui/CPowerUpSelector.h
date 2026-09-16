#pragma once
#include "gun_bros_re/ui/ZHudState.h"
#include "gun_bros_re/ui/ZHudResources.h"

/** CPowerUpSelector owns purchase choices, match slots and selector animation.
 * Original consumers: powerUpSelector.cpp :183797..187670; ui_movie.bt.
 * The input pad routes input and retains the resource cache's lifetime.
 */
class CPowerUpSelector {
public:
    void InitEntries();
    void Close() { m_selectorBound = false; m_selectorHits.clear(); }
    void Update(unsigned deltaMs);
    void Scroll(const ZInputPadState &state, float amount);
    void ScrollInput(const ZInputPadState &state, float wheel, float dragX);
    ZInputPadAction Pointer(const ZInputPadState &state, float x, float y, bool down, bool previousDown);
    bool FindActionRegion(ZInputPadAction action, ZMovieRegion &region) const;
    bool DrawPowerupCooldown(const ZPowerupEntry &entry, int remaining, const ZMovieRegion &region, float scale = 0.5f);
    explicit CPowerUpSelector(ZHudResources &resources) : m_resources(resources) {}
    void BrowseRemoteShop(unsigned selection);
    bool BackFromSelectorPrompt();
    void ReportSelectorPurchase(ZPurchaseResult result, const ZInputPadState &state);
    const ZStoreEntry *SelectedItem() const;
    bool ConfigureDeathmatch(const std::vector<GameObjectRef> &stores);
    void AdvanceMatchSelection();
    bool DrawMatchGuns(const ZInputPadState &state, const ZMovieRegion &area);
    bool DrawMatchTabs(const ZMovieRegion &area);
    bool DrawMatchGunCard(unsigned index, const ZMovieRegion &area);
    bool DrawMatchGunIcon(const ZStoreEntry &entry, const ZMovieRegion &area);
    bool DrawSelector(const ZInputPadState &state);
    bool DrawSelectorPrompt();
    bool DrawSelectorItem(const ZInputPadState &state, unsigned index, const ZMovieRegion &area);
    bool DrawSelectorButton(const char *table, unsigned index, float x, float y, float alpha,
        ZInputPadAction action, int storeIndex = -1, ZMovieRegion *touch = nullptr);
    std::string SelectorCurrency(const char *name, std::uint64_t amount);
    void ResetSelector(bool startOnGuns = false) {
        m_selectorBound = false;
        m_selectorHits.clear();
        m_selectorStartOnGuns = startOnGuns;
    }
    unsigned MatchSelectionSlot() const { return m_matchSelectedSlot; }
private:
    friend int CheckDeathmatchMenus(CResTOCManager &, ZPackTables &, CProfileManager &);
    friend int RunOriginalHudCheck(const std::string &bigDirectory);
    friend int RunOriginalDialogCheck(const std::string &bigDirectory);
    friend int RunOriginalPowerupSelectorCheck(const std::string &bigDirectory);
    friend int RunOriginalPauseCheck(const std::string &bigDirectory);
    ZHudResources &m_resources;
    std::vector<unsigned> m_matchGunEntries;
    bool m_matchGuns = false;
    bool m_selectorStartOnGuns = false;
    unsigned m_matchSelectedSlot = 0, m_matchSlotTime = 0, m_matchSlotChapter = 1;
    float m_matchGunPosition = 0;
    CMenuPopupPrompt m_selectorPrompt;
    const char *m_selectorPromptTable = nullptr;
    std::string m_selectorPromptBody;
    bool m_selectorPromptFunds = false, m_selectorPromptRequested = false;
    std::vector<std::pair<ZMovieRegion, unsigned>> m_selectorPromptHits;
    struct SelectorHit { ZMovieRegion area; ZInputPadAction action; int storeIndex = -1; };
    std::vector<SelectorHit> m_selectorHits;
    std::vector<unsigned> m_selectorEntries;
    std::vector<unsigned> m_selectorAllEntries;
    unsigned m_selectorTime = 0, m_selectorChoiceTime = 0;
    float m_selectorPosition = 0, m_selectorTarget = 0;
    bool m_selectorBound = false, m_selectorChoice = false;
    float m_selectorPressX = 0, m_selectorPressY = 0;
    bool m_selectorDragged = false, m_selectorPressArmed = false;
    int m_selectedItem = -1;
};
