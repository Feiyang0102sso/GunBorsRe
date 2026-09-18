#pragma once
#include "gun_bros_re/ui/ZHudState.h"
#include "gun_bros_re/ui/ZHudResources.h"
#include "gun_bros_re/gameplay/powerup/CPowerup.h"

class CInputPad;

/** CPowerUpSelector owns purchase choices, match slots and selector animation.
 * Original consumers: powerUpSelector.cpp :183797..187670; ui_movie.bt.
 * The input pad routes input and retains the resource cache's lifetime.
 */
class CPowerUpSelector {
public:
    CPowerUpSelector();
    CPowerUpSelector(CResTOCManager &toc, ZPackTables &tables, CBrother &player,
        ZPlayerVitals &vitals, CLevel &level, CProfileManager &profile,
        ZCombatId owner = kPlayerCombatId);
    /** Bind the selector's actor and inventory; UI and equipment share one catalog. */
    void BindPowerups(CResTOCManager &toc, ZPackTables &tables, CBrother &player,
        ZPlayerVitals &vitals, CLevel &level, CProfileManager &profile,
        ZCombatId owner = kPlayerCombatId);
    bool InitPowerups();
    void SetDeathmatch(CMPMatch *match) { m_match = match; }
    bool Select(unsigned index);
    bool SelectResource(const GameObjectRef &resource);
    /** Resolve saved ordinals, or original export 4 for unselected slots. */
    GameObjectRef GetEquipped(unsigned slot);
    bool Equip(unsigned slot, const GameObjectRef &resource);
    void Cycle();
    bool UseSelected(bool fromSelector = false);
    bool HasAfterDeathPowerup() const;
    bool UseAfterDeathPowerup();
    CPowerup &GetPowerup() { return *m_powerup; }
    const CPowerup &GetPowerup() const { return *m_powerup; }
    const ZPowerupEntry *GetSelected() const;
    unsigned GetCount() const;
    unsigned GetCount(unsigned localIndex) const;
    const std::map<unsigned, int> &Cooldowns() const { return m_cooldowns; }
    unsigned consumed = 0, failures = 0;
    void InitEntries();
    void Close() { if (!m_presentingPowerup) { m_selectorBound = false; } m_selectorHits.clear(); }
    bool BeginPowerupPresentation(bool fromSelector);
    void EndPowerupPresentation();
    void FinishPowerupPresentation();
    bool HideOnlyItems();
    bool HideSelector();
    bool Hide();
    bool RestoreInputPad();
    void UpdatePowerupPresentation(unsigned deltaMs);
    bool DrawPowerupPresentation();
    bool HasPowerupFrame() const { return m_presentingPowerup && m_powerupFrameVisible; }
    bool IsPowerupFrameClosing() const { return HasPowerupFrame() && (m_presentationState == 6 || m_presentationState == 7); }
    void Update(unsigned deltaMs);
    void Scroll(const ZInputPadState &state, float amount);
    void ScrollInput(const ZInputPadState &state, float wheel, float dragX);
    ZInputPadAction Pointer(const ZInputPadState &state, float x, float y, bool down, bool previousDown);
    bool FindActionRegion(ZInputPadAction action, ZMovieRegion &region) const;
    bool DrawPowerupCooldown(const ZPowerupEntry &entry, int remaining, const ZMovieRegion &region, float scale = 0.5f);
    explicit CPowerUpSelector(ZHudResources &resources, CInputPad *inputPad = nullptr) : m_resources(resources), m_inputPad(inputPad) {}
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
        // Closing the host shop must not rebind an in-flight item animation.
        if (m_presentingPowerup) { m_selectorHits.clear(); return; }
        m_selectorBound = false;
        m_selectorHits.clear();
        m_selectorStartOnGuns = startOnGuns;
    }
    unsigned MatchSelectionSlot() const { return m_matchSelectedSlot; }
private:
    friend class CBrother;
    friend class CLevel;
    friend class ZLocalPVPBot;
    friend class ZLocalCoopBot;
    bool IsSupported(const ZPowerupEntry &entry) const;
    const CStoreItem *FindStoreItem(const ZPowerupEntry &entry) const;
    bool ModeAllows(const ZPowerupEntry &entry) const;
    // Peer/research selectors load UI resources only for requested presentation.
    std::unique_ptr<ZHudResources> m_ownedResources;
    std::unique_ptr<CPowerup> m_powerup = std::make_unique<CPowerup>();
    CBrother *m_player = nullptr;
    ZPlayerVitals *m_vitals = nullptr;
    CLevel *m_level = nullptr;
    CProfileManager *m_profile = nullptr;
    CMPMatch *m_match = nullptr;
    ZCombatId m_owner = kPlayerCombatId;
    unsigned m_selected = 13;
    std::map<unsigned, int> m_cooldowns;
    std::vector<std::string> m_useMessages;
    friend int CheckDeathmatchMenus(CResTOCManager &, ZPackTables &, CProfileManager &);
    friend int RunOriginalHudCheck(const std::string &bigDirectory);
    friend int RunOriginalDialogCheck(const std::string &bigDirectory);
    friend int RunOriginalPowerupSelectorCheck(const std::string &bigDirectory);
    friend int RunOriginalPauseCheck(const std::string &bigDirectory);
    ZHudResources &m_resources;
    CInputPad *m_inputPad = nullptr;
    CMovie::Playback m_powerupMenu, m_powerupItems;
    ZInputPadState m_presentationSnapshot;
    int m_presentationState = 0;
    bool m_presentingPowerup = false, m_powerupFrameVisible = false;
    bool m_powerupItemsVisible = false, m_presentationHasContent = false;
    bool m_presentationResourcesReady = false;
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
