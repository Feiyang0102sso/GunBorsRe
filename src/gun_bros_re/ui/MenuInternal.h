#include "engine/core/Paths.h"
#pragma once
#include "gun_bros_re/cheats/CheatActions.h"
#include "gun_bros_re/debug/DebugMaps.h"
/** @file GameFrontEnd.cpp
 * @brief Connect the rebuilt offline account to actual gameplay.
 */
#define NOMINMAX
#include "gun_bros_re/ui/GameFrontEnd.h"
#include "gun_bros_re/data/OriginalProfile.h"
#include "gun_bros_re/data/MissionCatalog.h"
#include "gun_bros_re/data/PlanetCatalog.h"
#include "gun_bros_re/data/StoreCatalog.h"
#include "gun_bros_re/data/WeaponCatalog.h"
#include "gun_bros_re/data/ArmorCatalog.h"
#include "gun_bros_re/data/PowerupCatalog.h"
#include "gun_bros_re/gameplay/PlayerModel.h"
#include "engine/glu/movie/MovieRenderer.h"
#include "gun_bros_re/ui/LoadingScreen.h"
#include "gun_bros_re/ui/OriginalMenuData.h"
#include "gun_bros_re/ui/OriginalTextLayout.h"
#include "gun_bros_re/ui/OriginalPromotionPopup.h"
#include "gun_bros_re/ui/MenuWipe.h"
#include "gun_bros_re/gameplay/SurvivalGameContext.h"
#include "gun_bros_re/HostSettings.h"
#include "gun_bros_re/LocalOnlineServices.h"
#include "gun_bros_re/data/CDailyBonusTracking.h"
#include "gun_bros_re/data/CChallengeManager.h"
#include "gun_bros_re/ui/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/CMenuMesh.h"
#include "gun_bros_re/ui/CMenuPopupPrompt.h"
#include "gun_bros_re/data/Planet.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/gameplay/CBGM.h"
#include "gun_bros_re/gameplay/WeaponEffects.h"
#include "gun_bros_re/gameplay/CParticleEffect.h"
#include "gun_bros_re/gameplay/MapScene.h"
#include "gun_bros_re/StartupSequence.h"
#include "gun_bros_re/gameplay/EnemyModel.h"
#include "engine/graphics/CQuadBatch.h"
#include "engine/core/CMatrix4d.h"
#include "engine/glu/sprite/CSpriteGlu.h"
#include "engine/glu/sprite/CSpriteIterator.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <cctype>
#include <cmath>

// Internal menu collaboration interfaces; GameFrontEnd.h remains the production public entry.
namespace MenuDetail {

constexpr const char *kPlanetPacks[] = {"pack2", "pack7", "pack9", "pack12", "pack11", "pack1"};
constexpr unsigned kPlanetMaps[] = {7, 6, 0, 0, 0};

constexpr unsigned kArmorSlots[] = {0, 0, 2, 1, 0};
constexpr float kMenuWidth = 1024;
// The store's character column starts where the black content area does; the
// original character reaches up past the category tabs on that side.

constexpr float kMenuHeight = 768;

std::int64_t CurrentSeconds();

bool SameObject(const GameObjectRef &first, const GameObjectRef &second);

/** Windows pointer adapter for CMenuMovieControl's continuous playback.
 * Geometry and base speed come from the Movie. Integrate native damping at a
 * fixed 60 Hz reference so desktop frame rate does not change flick distance.
 */
struct MenuScrollMotion {
    float velocity = 0;
    std::uint64_t lastTick = 0, lastMotionTick = 0;
    bool captured = false;

    void Update(float &position, std::uint64_t clock, float delta, float wheel, bool held,
        bool pressedInside, bool enabled, float maximum, float stride, unsigned duration) {
        const float seconds = std::min(0.05f, static_cast<float>(clock - lastTick) / 1000);
        lastTick = clock;
        if (!enabled || maximum <= 0) { velocity = 0; captured = false; return; }
        const float baseSpeed = stride * 1000 / std::max(1u, duration);
        if (pressedInside) { captured = true; velocity = 0; }
        if (captured && held) {
            position -= delta;
            if (delta != 0 && seconds > 0) {
                velocity = std::clamp(-delta / seconds, -5 * baseSpeed, 5 * baseSpeed);
                lastMotionTick = clock;
            } else if (clock - lastMotionTick > 80) { velocity = 0; }
        } else {
            captured = false;
            // CalculateBaseVelocity :141749 sets 250000 / chapter-ms;
            // UpdatePlaybackSpeed :141086 subtracts this * dt^2 / 2.
            const float deceleration = baseSpeed * (250000.0f / std::max(1u, duration)) / 120;
            const float speed = std::abs(velocity);
            const float travelTime = std::min(seconds, speed / deceleration);
            const float distance = speed * travelTime - deceleration * travelTime * travelTime / 2;
            if (velocity < 0) { position -= distance; }
            else { position += distance; }
            velocity = std::copysign(std::max(0.0f, speed - deceleration * seconds), velocity);
        }
        if (wheel != 0) { position -= wheel * stride; velocity = 0; }
        position = std::clamp(position, 0.0f, maximum);
        if ((position == 0 && velocity < 0) || (position == maximum && velocity > 0)) { velocity = 0; }
    }
};

// Each page owns its binding state and playback cursor.
struct StarMapMenuState {
    int startingWave = -1;
    float starPanX = 0, starPanY = 0;
    bool starBound = false, starReverse = false, starLocked = false, starEntering = false;
    unsigned starTime = 0, starReticleTime = 0, starFlagTime = 0, starFadeTime = 0;
    int starSelectedSlot = -1, starTargetTime = -1;
    float starSpeed = 0;
    float starSelectorX = 0, starSelectorY = 0, starFlagX = 0, starFlagY = 0;
    std::uint64_t starLastTick = 0;

};
struct StoreMenuState {
    unsigned shopCategory = 0;
    unsigned shopGunSlot = 0;
    // CMenuMovieButton states: showing 0, hiding 1, idle 2, selected 4, hidden 8.
    unsigned shopSwapPhase = 8, shopSwapTime = 0;
    std::uint64_t shopSwapLastTick = 0;
    bool shopSwapKeyRequested = false;
    float shopScroll = 0;
    MenuScrollMotion shopMotion;
    std::uint64_t shopDetailStart = 0;
    std::uint64_t shopDetailLastTick = 0;
    unsigned shopDetailTime = 0;
    bool shopDetailClosing = false;
    float shopFocusAmount = 0;
    // Selecting a card never previews; only the PREVIEW button does.
    bool shopPreview = false;
    unsigned shopFilter = 0;
    unsigned shopExclusionFilter = 0;
    bool shopFilterOpen = false;
    // Each menu owns its playback cursor; cached CMovie resources stay immutable.
    unsigned shopFilterTime = 0;
    std::uint64_t shopFilterLastTick = 0;
    bool shopFilterBound = false;
    bool shopDetailOpen = false;

};
struct MissionsMenuState {
    MenuScrollMotion missionMotion, waveMotion;
    float missionPosition = 0, wavePosition = 0;
    float missionScroll = 0;
    bool missionBound = false, missionClosing = false;
    unsigned missionTime = 0, missionListTime = 0, missionCardTime = 0, missionFocusTime = 0;
    unsigned missionFirst = 0, missionWaveTime = 0, missionWaveButtonTime = 0;
    bool missionListMoving = false, missionListReverse = false, missionWaveMoving = false, missionWaveReverse = false;
    int missionFocused = -1;
    float missionFocusX = 0, missionFocusY = 0;
    std::uint64_t missionLastTick = 0;
    unsigned revolution = 0, wavePage = 0, missionTab = 0;

};
struct ModeMenuState {
    bool modeSelected = false;
    bool modeBound = false;
    unsigned modeTime = 0, modePhase = 0, modeSpriteTime = 0;
    std::uint64_t modeLastTick = 0;

};
struct PostGameMenuState {
    bool postGameMusic = false;
    bool postGameBound = false, postGameUpgradePending = false, postGameClosing = false;
    unsigned postGameTime = 0, postGameItemTime = 0, postGameCloseTime = 0;
    unsigned postGameIconTime = 0;
    float postGameGalleryPosition = 0, postGameGalleryVelocity = 0;
    unsigned postGameDelta = 0;
    std::uint64_t postGameLastTick = 0;

};
struct RefineryMenuState {
    unsigned refineryTab = 0, casualtyPage = 0;
    bool refineryBound = false;
    bool refineryExitPending = false;
    unsigned refineryTime = 0, refineryElapsed = 0;
    std::uint64_t refineryLastTick = 0;
    std::array<unsigned, kRefinementSlotCount> refineryStatusTime{}, refineryFillTime{};
    std::array<unsigned, kRefinementSlotCount> refineryStatusChapter{};
    int refineryTransfer = -1;
    unsigned refineryTransferTime = 0, refineryTransferSprite = 0;
    std::uint64_t refineryTransferAmount = 0;
    float refineryTransferX = 0, refineryTransferY = 0, refineryTargetX = 0, refineryTargetY = 0;

};
struct GreetingMenuState {
    bool greetingBound = false, greetingExitRequested = false, greetingClosing = false;
    unsigned greetingTime = 0, greetingElapsed = 0, greetingTarget = 0;
    std::uint64_t greetingLastTick = 0;

};
struct SelectionMenuState {
    bool playerSelectBound = false, playerSelectReady = false;
    int playerSelection = -1;
    unsigned playerSelectTime = 0, playerSelectChapter = 1;
    std::uint64_t playerSelectLastTick = 0;

};
struct SettingsMenuState {
    float optionsScroll = 0;
    unsigned optionsFocus = 0;
    unsigned optionsReturnFocus = 0;
    float optionsReturnScroll = 0;
    bool optionsBound = false;
    unsigned optionsOpening = 0;
    unsigned optionsBodyTime = 0;
    unsigned optionsScrollbarTime = 0;
    float optionsTarget = 0;
    float optionsBodyScroll = 0;
    std::uint64_t optionsLastTick = 0;
    std::vector<unsigned> optionsButtonTimes;

};
struct SocialMenuState {
    CChallengeManager challenges;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armors;
    CProfileManager defaultBrother;
    CGameAssetRef avatar;
    std::string brotherName;
    bool contentBound = false;
    unsigned contentPage = 0;
    unsigned selectedChallenge = 0;
    float scrollPosition = 0;
    MenuScrollMotion scrollMotion;
    unsigned renderedEntries = 0; // Actual local content cards drawn this frame.
    bool onlinePage = false;
    unsigned socialTab = 0;
    bool socialBound = false;
    unsigned socialTime = 0;
    std::uint64_t socialLastTick = 0;

};

struct MenuState {
    LocalOnlineServices online;
    bool matchingPrompt = false;
    bool currencySimulated = false;
    std::string currencyOfferProduct;
    bool resumeAfterDebugTutorial = false; // Returning from a no-save replay must not trigger a menu checkpoint.
    DebugMapSelection debugMap;

    unsigned page = 0;
    unsigned planet = 0;
    unsigned slot = 0;
    unsigned itemPage = 0;
    int selectedItem = -1;
    std::string message;
    unsigned detail = 0;
    unsigned hordeStart = 0;
    unsigned currencyTab = 0, currencyPage = 0;
    int currencyItem = -1;
    CMenuMesh playerMesh;
    std::uint64_t playerMeshLastTick = 0;
    // Last popup update tick; the resource chapters own its playback cursor.
    std::uint64_t masteryOpened = 0;
    CMenuUpgradePopup masteryPopup;
    unsigned gameMode = 0;
    GameObjectRef selectedMission;
    bool currencyPending = false;
    CMenuPopupPrompt storePopup;
    std::uint64_t storePopupLastTick = 0;
    unsigned storePromptSpriteTime = 0;
    std::string challengeRewardTitle, challengeRewardBody;
    const char *storePromptTable = "MDS_IAP_PLEASE_WAIT";
    unsigned storePromptIndex = 0;
    bool storePromptRequested = false;
    bool storePromptSideVisual = true;
    bool storePromptDismiss = false;
    const char *storePromptButtons = nullptr;
    unsigned failedCurrency = 0, failedPrice = 0, failedMissing = 0;
    int currencyOffer = -1;
    std::uint64_t currencyReadyAt = 0;
    SurvivalResult result;
    GameObjectRef masteryWeapon;
    bool refinementRequired = false;
    OriginalPromotionPopup promotion;
    std::uint64_t promotionTick = 0;
    std::vector<unsigned> history;

    StarMapMenuState starMap;
    StoreMenuState store;
    MissionsMenuState missions;
    ModeMenuState mode;
    PostGameMenuState postGame;
    RefineryMenuState refinery;
    GreetingMenuState greeting;
    SelectionMenuState selection;
    SettingsMenuState settings;
    SocialMenuState social;

    void ShowStorePrompt(const char *table, bool sideVisual, bool dismissible, unsigned index = 0) {
        challengeRewardTitle.clear();
        challengeRewardBody.clear();
        storePromptTable = table;
        storePromptIndex = index;
        storePromptSideVisual = sideVisual;
        storePromptDismiss = dismissible;
        storePromptButtons = nullptr;
        storePromptRequested = true;
        storePopup = CMenuPopupPrompt();
    }

    void BeginOfflineIAP(int item, std::uint64_t clock, const std::string &product = {}) {
        if (currencyPending) { return; }
        online.SetConnected(GameHostSettings().isConnected);
        currencySimulated = online.IsConnected();
        if (currencySimulated && !online.BeginPurchase(product, clock)) {
            ShowStorePrompt("MDS_STORE_PROMPT_UNAVAILABLE", false, true);
            return;
        }
        currencyItem = item;
        currencyPending = true;
        currencyReadyAt = clock + 4000; // User-authorized offline wait, UI_sample/ui.md.
        ShowStorePrompt("MDS_IAP_PLEASE_WAIT", true, false);
    }

    // Every nested page remembers its caller; trunk navigation starts a new path.
    void Navigate(unsigned target, bool root = false) {
        if (root) { history.clear(); }
        if (target == 17) {
            store.shopCategory = 3;
            store.shopScroll = 0;
            store.shopFilter = 0;
            store.shopDetailOpen = false;
        }
        if (target == page) { return; }
        // A hidden control must not resume an old fling on another page.
        store.shopMotion = MenuScrollMotion{};
        missions.missionMotion = MenuScrollMotion{};
        missions.waveMotion = MenuScrollMotion{};
        mode.modeLastTick = 0;
        if (page == 24 && greeting.greetingBound) {
            greeting.greetingTarget = target;
            greeting.greetingExitRequested = true;
            return;
        }
        if (target == 26) { masteryPopup = CMenuUpgradePopup(); }
        if (target == 6) { settings.optionsBound = false; }
        if (target == 8) {
            settings.optionsReturnFocus = settings.optionsFocus;
            settings.optionsReturnScroll = settings.optionsScroll;
            settings.optionsBound = false;
            settings.optionsFocus = 0;
            settings.optionsScroll = settings.optionsTarget = settings.optionsBodyScroll = 0;
        }
        if (target == 3) { refinery.refineryBound = false; }
        if (target == 24) { greeting.greetingBound = false; }
        if (target == 25 || target == 29) { selection.playerSelectBound = false; }
        if (target == 27 && page != 26 && page != 28) { postGame.postGameBound = false; }
        if (target == 0) { starMap.starBound = false; }
        if (target == 21) { missions.missionBound = false; }
        social.socialBound = false;
        if (!root && page != 14) { history.push_back(page); }
        page = target;
    }
    void Back() {
        store.shopMotion = MenuScrollMotion{};
        missions.missionMotion = MenuScrollMotion{};
        missions.waveMotion = MenuScrollMotion{};
        mode.modeLastTick = 0;
        if (page == 24 && greeting.greetingBound) { Navigate(0, true); return; }
        if (history.empty()) { page = 0; return; }
        const bool fromHelp = page == 8;
        page = history.back();
        if (page == 6) {
            settings.optionsBound = false;
            if (fromHelp) { settings.optionsFocus = settings.optionsReturnFocus; settings.optionsScroll = settings.optionsReturnScroll; }
        }
        social.socialBound = false;
        history.pop_back();
    }
};

struct MenuTransitionTrace {
    bool active = false;
    unsigned time = 0, starts = 0;
#if GB_ENABLE_TESTS
    struct Frame {
        unsigned page, headerTime, wipeTime;
        bool navigationReady, refineryExitPending, wipeActive;
    };
    std::vector<Frame> frames;
#endif
};

struct MenuTestClick { float x; float y; unsigned advanceMs = 0; unsigned renderDelayMs = 0; };

/** All GL owners are destroyed before the menu window's context. */
class GameMenu {
public:
    explicit GameMenu(CWindow *sharedWindow = nullptr) : window(sharedWindow ? *sharedWindow : ownedWindow) {}
    /** Integration harness input; it still goes through rendered button hit tests. */
#if GB_ENABLE_TESTS
    void SetTestClick(const MenuTestClick &click) { mouseX = click.x; mouseY = click.y; clicked = true; }
#endif
    /** Temporarily route this frame's click exclusively to a modal panel. */
    bool ExchangeClick(bool enabled) { const bool previous = clicked; clicked = enabled; return previous; }
    std::pair<float, float> Cursor() const { return {mouseX, mouseY}; }
    bool Open(CResTOCManager &toc, PackTables &tables, const CProfileManager *profile = nullptr, bool startup = false, CBGM *music = nullptr);

    void Begin(unsigned page = 0);

    void Text(float x, float y, const std::string &text, float size = 2,
        float r = 0.88f, float g = 0.93f, float b = 0.95f) {
        unsigned font = 1;
        float scale = size * 7 / 18.0f;
        if (r > 0.9f && g < 0.8f) { font = 5; scale = size * 7 / 27.0f; }
        movies.Text(text, x, y, font, scale);
    }

    bool Hit(float x, float y, float width, float height) {
        if (inputEnabled && clicked && mouseX >= x && mouseX < x + width && mouseY >= y && mouseY < y + height) {
            clicked = false;
            return true;
        }
        return false;
    }

    void CenterText(const std::string &text, float center, float y, unsigned font = 0, float scale = 1) {
        movies.Text(text, center - movies.TextWidth(text, font, scale) * 0.5f, y, font, scale);
    }

    void Clip(float x, float y, float width, float height) {
        int screenWidth = 0, screenHeight = 0;
        window.GetDrawableSize(screenWidth, screenHeight);
        glEnable(GL_SCISSOR_TEST);
        glScissor(static_cast<int>(x * screenWidth / 1024), static_cast<int>((768 - y - height) * screenHeight / 768),
            static_cast<int>(width * screenWidth / 1024), static_cast<int>(height * screenHeight / 768));
    }

    void EndClip() { glDisable(GL_SCISSOR_TEST); }
    void UpdateMeshRotation(CMenuMesh &mesh, unsigned deltaMs, const MovieRegion &region, bool enabled) const {
        mesh.UpdateRotation(deltaMs, window.IsLeftMouseDown(), mouseX, mouseY,
            region.x, region.y, region.width, region.height, enabled);
    }

    bool MouseIn(float x, float y, float width, float height) const {
        return inputEnabled && mouseX >= x && mouseX < x + width && mouseY >= y && mouseY < y + height;
    }

    bool TitleImage();

    int Header(const CProfileManager &profile, const CPlayerProgress &progress, unsigned currentPage);
    bool IsNavigationReady() const { return navigationReady; }
#if GB_ENABLE_TESTS
    unsigned HeaderTime() const { return originalHeaderTime; }
#endif

    // Historical explicit .dat research UI; native profiles use Header below.

    bool Icon(CResTOCManager &toc, PackTables &tables, const StoreEntry &entry, float x, float y, float width,
        float height, float alpha = 1, bool originalSize = false, bool fitHeight = false);

    bool DrawEquippedPlayer(CResTOCManager &toc, PackTables &tables, const CProfileManager &profile,
        const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armors, unsigned slot,
        const GameObjectTypeRef *previewItem = nullptr, const MovieRegion *storePanel = nullptr, float spin = 0);

    /** CEnemy::SpawnForUI assembles the original result-card model. */
    bool DrawCasualty(PackTables &tables, CResTOCManager &toc, const EnemyCasualty &casualty, float x,
        const MovieRegion *originalRegion = nullptr);

    CWindow ownedWindow;
    CWindow &window;
    // Menu time; the integration harness advances it instead of real ticks.
    std::uint64_t clock = 0;
    // Under the harness the real pointer must not scroll anything, or a stray
    // drag over the window moves a list out from under a scripted click.
#if GB_ENABLE_TESTS
    bool scripted = false;
#endif
    // The plate movies own the press burst; remember the last press so the
    // following frames can play it where the button was.
    unsigned pressMovie = 0;
    float pressX = 0, pressY = 0, pressWidth = 0, pressHeight = 0;
    std::uint64_t pressStart = 0;

    void NotePress(unsigned movie, float x, float y, float width, float height) {
        pressMovie = movie;
        pressX = x;
        pressY = y;
        pressWidth = width;
        pressHeight = height;
        pressStart = clock;
    }

    /** Chapter 1 of a button movie is its release burst; it runs 300 ms. */
    void DrawPress() {
        if (pressMovie == 0 || clock < pressStart) { return; }
        const std::uint64_t elapsed = clock - pressStart;
        const auto *movie = movies.GetMovie(pressMovie);
        unsigned start = 0, end = 0;
        if (!movie || !movie->GetChapterRange(1, start, end) || elapsed > end - start) { return; }
        movies.DrawFitted(pressMovie, start + static_cast<unsigned>(elapsed), pressX, pressY, pressWidth, pressHeight, 1);
    }
    MovieRenderer movies;
    unsigned storeRestTime = 0;
    std::vector<std::string> names, descriptions;
    std::vector<PlanetEntry> planetEntries;

    int PlanetForMapSlot(unsigned slot) const {
        for (unsigned index = 0; index < planetEntries.size(); ++index) {
            if (planetEntries[index].data.mapSlot == slot) { return static_cast<int>(index); }
        }
        return -1;
    }

    /** PlanetImageCallback :188957 retains original sprite geometry and origin. */
    void DrawPlanetOriginal(unsigned index, const MovieRegion &region);

    /** Original PlanetCallback uses native sprite pixels and a centered hit box. */
    MovieRegion DrawPlanetThumb(unsigned index, const MovieRegion &region, float fade = 1);

    void PlanetFlagLines(float centerX, float centerY, const MovieRegion &area);
    float dragX = 0, dragY = 0;
    bool pointerHeld = false, pointerPressed = false;
    bool PrepareModeEffects();
    bool StartModeSelectionEffect() {
        if (!PrepareModeEffects()) { return false; }
        modeEffects[0]->Clear();
        return modeEffects[0]->StartPersistentEffect(modeEffectRefs[0], 0, 0) != 0;
    }
    bool AdvanceModeEffects(unsigned elapsed) {
        if (!PrepareModeEffects()) { return false; }
        for (auto &effect : modeEffects) { effect->AdvanceAmbientEffects(elapsed); }
        return true;
    }
    void DrawModeEffects(const MovieRegion &label);
    std::size_t ModeParticleCount() const {
        if (!modeEffects[0]) { return 0; }
        return modeEffects[0]->GetParticleCount();
    }
    /** CMenuPostGameOption::Bind/Update/Draw :249755..249964 owns one
     * particle player per card, behind the centered icon. */
    bool AdvancePostGameEffect(unsigned index, unsigned elapsed);
    void ResetPostGameEffects() {
        for (auto &effect : postGameEffects) { effect.reset(); }
    }
    std::size_t PostGameParticleCount(unsigned index) const {
        if (!postGameEffects[index]) { return 0; }
        return postGameEffects[index]->GetParticleCount();
    }
    void DrawPostGameEffect(unsigned index, const MovieRegion &icon) {
        float transform[16];
        std::copy(movies.CurrentProjection(), movies.CurrentProjection() + 16, transform);
        Matrix4dTranslate(transform, icon.x + icon.width / 2, icon.y + icon.height / 2);
        postGameEffects[index]->Draw(transform);
    }
    /** Native refinery transfer players keep living particles after arrival. */
    bool StartRefineryEffect(unsigned slot, unsigned icon, float x, float y);
    void AdvanceRefineryEffects(unsigned elapsed);
    void MoveRefineryEffect(unsigned slot, float x, float y);
    void StopRefineryEffect(unsigned slot);
    void DrawRefineryEffects();
    void ResetRefineryEffects() {
        for (auto &effect : refineryEffects) { effect = {}; }
    }
    std::size_t RefineryParticleCount(unsigned slot) const {
        if (!refineryEffects[slot].player) { return 0; }
        return refineryEffects[slot].player->GetParticleCount();
    }
    void Scroll(MenuScrollMotion &motion, float &position, const MovieRegion &viewport,
        bool enabled, float maximum, float stride, unsigned duration);
    bool animateNavigation = true;
    bool inputEnabled = true;
    bool verifyPlayerProjection = false;
    std::size_t PreviewSoundCount() const { return previewSoundCount; }
    void EnableSilentPreviewAudio() { previewAudio.EnableSilentValidation(); }
    AudioPlaybackState PreviewAudioState() const { return previewAudio.GetPlaybackState(); }
    PlayerModel *GetPlayerPreview() const { return equippedPreview.get(); }
    unsigned GetPlayerPreviewSlot() const { return previewGunSlot; }
    bool TakePlayerPreviewSlotChange() {
        const bool changed = previewSlotChanged;
        previewSlotChanged = false;
        return changed;
    }
    /** CMenuMeshPlayer::Update observes configuration only after native 3. */
    void AdvancePlayerPreview(int deltaMs);
private:
    void PlayPreviewSounds(const std::vector<MoveSoundRef> &sounds);
    CAudioPlayer previewAudio;
    std::size_t previewSoundCount = 0;
    CResTOCManager *resourceToc = nullptr;
    PackTables *resourceTables = nullptr;
    CShaderProgram textProgram;
    CShaderProgram imageProgram;
    std::array<std::unique_ptr<WeaponEffects>, 2> modeEffects;
    std::array<std::unique_ptr<WeaponEffects>, 7> postGameEffects;
    struct RefineryEffect {
        std::unique_ptr<WeaponEffects> player;
        std::uint64_t handle = 0;
        float x = 0, y = 0;
    };
    std::array<RefineryEffect, kRefinementSlotCount> refineryEffects;
    std::array<GameObjectRef, 2> modeEffectRefs;
    CMarkerBatch markers;
    CQuadBatch images;
    CTexture titleImage;
    float projection[16]{};
    float mouseX = 0, mouseY = 0;
    bool clicked = false, previousDown = false;
    float dragDistance = 0;
    std::vector<std::vector<SpriteQuad>> planetQuads, planetThumbs;
    std::map<int, std::unique_ptr<CSpriteGlu>> spritePacks;
    std::map<std::uint64_t, std::unique_ptr<CTexture>> icons;
    std::unique_ptr<PlayerModel> equippedPreview;
    CPlayerConfiguration previewConfiguration;
    unsigned previewGunSlot = 0;
    unsigned previewPrimarySlot = 0;
    bool previewSwapPending = false;
    bool previewSlotChanged = false;
    std::uint64_t previewTicks = 0;
    std::uint64_t navigationStart = 0;
    bool navigationVisible = false;
    bool navigationReady = false;
    bool originalHeaderBound = false;
    unsigned originalHeaderTime = 0, originalHeaderButtonTime = 0;
    std::uint64_t originalHeaderTick = 0;
    struct EnemyPreview {
        EnemyTemplateData data;
        EnemyModel model;
        std::string name;
        std::uint64_t lastTick = 0;
    };
    std::map<std::uint64_t, std::unique_ptr<EnemyPreview>> enemyPreviews;
};
bool MatchesEquipmentSlot(const StoreEntry &entry, unsigned slot,
    const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armors);

GameObjectRef &Equipped(CProfileManager &profile, unsigned slot);

/** Shared compact/expanded store status, distinct from the preview slot. */
bool IsStoreObjectEquipped(CProfileManager &profile, unsigned slot, const GameObjectRef &object);

// GLU_MOVIE_STORE_MENU carries the whole screen skeleton. Its four user
// regions are bound, in this order, by CMenuStore::Init (:180199) to the
// content list, the category row, the player mesh and the gun swap button.
constexpr unsigned kStoreContentRegion = 0;
constexpr unsigned kStoreCategoryRegion = 1;
constexpr unsigned kStorePlayerRegion = 2;
constexpr unsigned kStoreGunSwapRegion = 3;
// CMenuStore::CategoryCallback places each category button four pixels after
// the previous one, starting at the left edge of the category region.
constexpr float kCategoryGap = 4;
// GLU_MOVIE_STORE_SCROLL holds five card columns. Slot 1 is the leftmost one
// on screen and every further slot is one column to the right; the slots that
// reach past the player already carry the original half transparency.
constexpr unsigned kFirstColumnRegion = 1;

// GLU_MOVIE_SHOP_BOX regions. Chapter 0 is the folded 254x164 card; chapter 2
// expands the same card to 500x328 and reveals the stats, the upgrade meter,
// the description and the action row.
constexpr unsigned kCardBodyRegion = 0;
// Region 1 is the hex plate the OWNED/EQUIPPED stamps lie across; the icon
// itself fits region 5, which is what the original card art measures.
constexpr unsigned kCardStampRegion = 1;
constexpr unsigned kCardCategoryRegion = 2;
constexpr unsigned kCardBadgeRegion = 3;
constexpr unsigned kCardRightRegion = 4;
constexpr unsigned kCardIconRegion = 5;
constexpr unsigned kCardNameRegion = 6;
constexpr unsigned kCardPriceRegion = 7;
constexpr unsigned kCardUpgradeRegion = 8;
constexpr unsigned kCardDescriptionRegion = 9;
constexpr unsigned kCardActionRegion = 10;
constexpr unsigned kCardStatsRegion = 11;
constexpr unsigned kCardFoldedTime = 0;
// CMenuStoreOption::Update :181486 advances SHOP_BOX by 4 * elapsed MS.
constexpr unsigned kCardPlaybackRate = 4;
// GLU_MOVIE_SORT_BAR: CMenuStore::Init :180331 binds region 1 to
// SortButtonCallback and region 2 to SortLabelCallback; region 0 is the touch area.
constexpr unsigned kSortButtonRegion = 0;
constexpr unsigned kSortPanelRegion = 1;
constexpr unsigned kSortLabelRegion = 2;
// CMenuStore::SortButtonCallback stacks the options at 1.5 button heights.
constexpr float kSortRowSpacing = 1.5f;
// Owned and equipped markers, from the sprite character the store loads.
constexpr unsigned kOwnedStamp = 17;
constexpr unsigned kEquippedStamp = 18;
// The two promotional cards in the first column: the invite/loot panel and the
// free Warbucks money pile, both from the same sprite character.
constexpr unsigned kInviteCard = 52;
constexpr unsigned kFreeWarbucksCard = 36;
// Bronze, silver and gold mastery badges for the folded card's corner region.
// The actual folded badge binding is archetype 26, animations 24..26 (:150159).
// CGun::Template::GetMasteryLevel tops out here; a mastered gun cannot upgrade.
constexpr unsigned kMaxMasteryLevel = 3;
// Currency icons come from the sprite character CMenuSystem::Load pulls with
// the menu itself: 23:1 is the coin stack, 23:7 the Warbuck bundle.
constexpr unsigned kCurrencyCharacter = 23;
constexpr unsigned kCoinIcon = 1;
constexpr unsigned kWarbuckIcon = 7;
// MDS_BUTTON_STORE_ITEMS rows: buy, equip and upgrade.
constexpr unsigned kBuyButtonEntry = 0;
constexpr unsigned kEquipButtonEntry = 3;
constexpr unsigned kUpgradeButtonEntry = 4;
// Action dimensions now come from each MDS button movie, region 1.
// Filter bits. Categories keep the low bits so a gun category maps directly.
constexpr unsigned kOwnedFilterBit = 1u << 18; // CStoreAggregator native filter criterion.
// GLU_MOVIE_WEAPON_UPGRADE_MASTERY is positioned by its origin and has no
// fitting region, so its authored extent is used to centre it.
// Historical popup dimensions are no longer used by the store child movie.

/** One card face placed on screen, with the belt's own fade applied. */
struct StoreCardFace {
    float x = 0;
    float y = 0;
    float alpha = 1;
    unsigned time = kCardFoldedTime;
};

/** A layout region must exist; a miss means the movie or chapter is wrong. */
bool RequireRegion(GameMenu &view, unsigned movie, unsigned index, unsigned time, MovieRegion &region, const char *what);

/** SHOP_BOX regions resolved for a card whose own origin sits at (x, y). */
bool CardRegion(GameMenu &view, unsigned card, unsigned index, const StoreCardFace &face, MovieRegion &region);

// Every menu button prints its label at the same size; the original never
// squeezes one to fit a narrower plate, it picks a wider plate instead.

/** Centre one original label inside a plate. */
void PlateLabel(GameMenu &view, const std::string &label, float x, float y, float width, float height);

/** Draw one MDS_BUTTON_STORE_ITEMS plate. Its width is the width of the button
 * movie that row names, right aligned on `right`: BUY and EQUIP take the small
 * plate, UPGRADE the large one, which is why UPGRADE reaches further left. */
bool StoreItemButton(GameMenu &view, unsigned entryIndex, float right, float y, float height, bool enabled, float alpha = 1);

/** Right aligned price: the original prints the currency sprite and the number,
 * never the currency's name. */
// CreateItemCostString :157573 resolves IDS_SHOP_COMMON/RARE (Omega/delta
// glyph plus %i in this BIG). The currency icons are glyphs in font 0; drawing
// an unrelated sprite at 1.3 times the row height duplicated their layout.
std::string StoreCostText(GameMenu &view, const CStoreItem &item);

void DrawCardPrice(GameMenu &view, const CStoreItem &item, const MovieRegion &row, float alpha);

/** A bundle is owned once every object it hands over is. Only the records with
 * the single-purchase flag are treated this way. */
bool OwnsBundle(const CProfileManager &profile, const CStoreItem &item);

const WeaponEntry *FindWeaponEntry(const std::vector<WeaponEntry> &weapons, const GameObjectRef &ref);

/** Category caption under the icon, from the weapon or armour catalogue. */
// CreateItemCategoryString :157413 indexes IDS_SHOP_SORT3 + STORE.category.
std::string StoreItemKind(GameMenu &view, const StoreEntry &item);

/** The three cell upgrade meter. CMenuStore::Load pulls sprite character 26
 * for exactly this movie, so the store shares the upgrade popup's artwork. */
// The former popup-meter implementation was replaced by the store child movie below.

/** Store display strings are templates: `^fN` marks a font switch and `#KEY` a
 * value slot. The `^fN` code indexes a font table this rebuild has not resolved
 * yet; against the iOS card, labels use the blue menu font and values the blue
 * digit font. Pieces wrap inside their own card region and each line is centred
 * there, which is what stacks POWER above its number. */
// Correction to the historical approximation above: the table is now resolved.
// CMenuStoreOptionGroup::InitOption :233898 -> SetFont :181499 ->
// SetupTextBox :181194 maps ^f0..4 to menu fonts 1,2,4,3,0. CTextBox::paint
// :104153 centers when byte 0 is set; byte 1, not byte 0, means right alignment.
// Use real newlines, font metrics and whitespace; never invent a row after #KEY.

std::string SubstituteStoreStats(const std::string &text,
    const std::vector<std::pair<std::string, std::string>> &values);

/** CTextBox intersects the animated region with its parent's scissor and restores
 * it after painting (:104019). Keep the belt's clip when painting folded cards. */

void DrawStoreTemplate(GameMenu &view, const std::string &text, const MovieRegion &area,
    const std::vector<std::pair<std::string, std::string>> &values, bool centered = false, float layoutWidth = 0);

/** CMenuDataProvider::CreateContentMovie :149246 and CMenuStoreOption::Bind
 * :181883 bind the store-specific eight-region movie, not the upgrade popup.
 * Child time is derived from CGun mastery XP and that movie's chapter lengths. */
bool StoreMasteryTarget(const CMovie &movie, const CGun::Template &weapon, unsigned experience, unsigned &target);

bool DrawMasteryMeter(GameMenu &view, const WeaponEntry &weapon, unsigned experience,
    const MovieRegion &area, unsigned elapsed);

/** Powerups use their own child movie; row locations and visibility live in BIG.
 * Bind :182003, GameTypeCallback :180652, GameTypeCompatibilityCallback :180688. */
bool DrawPowerupCompatibility(GameMenu &view, const CStoreItem &item, const MovieRegion &area, unsigned elapsed);

/** The current mastery tier's values for the card templates. */
std::vector<std::pair<std::string, std::string>> StoreStatValues(const CStoreItem &item, std::size_t mastery);

/** The four category tabs. Their widths come from the button movie each
 * MDS_BUTTON_STORE_CATEGORIES row names, not from measured screenshots. */
void DrawStoreCategories(GameMenu &view, const MovieRegion &bar, MenuState &state, bool interactive);

/** CMenuStore::InitSortButtons binds every row in the selected MDS table. */
unsigned StoreFilterRows(unsigned category, const char *&table);

/** Non-looping SORT_BAR playback, CMenuStore::Bind :180092 and
 * HandleTouchInput :179259. Chapter 0 holds the initial closed pose; clicks
 * play chapter 1 forward or backward without restarting the current frame.
 * Bounds come from BIG ui_movie.bt / MovieChapter, never copied timestamps.
 * CMovie::Update :109097 advances milliseconds and clamps at chapter bounds. */
bool AdvanceStoreFilter(GameMenu &view, MenuState &state, const CMovie &movie);

/** The original store draws two cards per column (ItemCallback :178878) on a
 * horizontal belt and expands the focused card in place. Item identity and
 * purchases still come directly from the BIG catalog. */
/** CornerCallback :180757 and CreateContentSprite :149994 display quantity
 * in a 0:87/88 badge, with the original numeric font; no handwritten OWN label. */
void DrawStoreQuantity(GameMenu &view, const CProfileManager &profile, const GameObjectTypeRef &ref,
    const MovieRegion &area, float alpha = 1);

/** Focus/UnFocus :181356/:181402 reverse chapter 1; Update :181486 uses 4x.
 * Keep a closing card modal until its last frame, so a click cannot buy the card
 * underneath it. Bounds are read each time from CMovie, including resource edits. */
bool AdvanceStoreCard(GameMenu &view, MenuState &state, const CMovie &movie);

/** GetLastFailPurchaseInfo :156610; ARM 0xD25A8/0xD25F8 confirms the total
 * price and missing balance arguments omitted by the decompiler. */
bool StoreFailureText(GameMenu &view, const MenuState &state, std::string &body);

void ShowStoreFundsPrompt(MenuState &state, const std::vector<StoreEntry> &store,
    const CProfileManager &profile, unsigned currency, unsigned price, bool inGame = true);

bool DrawOriginalMovieButton(GameMenu &view, const OriginalMenuEntry &entry, const MovieRegion &area,
    const std::string &label, unsigned font, bool interactive, bool &pressed,
    unsigned chapter = 0, unsigned elapsed = 0, unsigned timeOverride = UINT32_MAX, bool stateArtwork = false);

/** CMenuStore::GunSwapCallback :178863; button size comes from Movie region 1. */
bool StoreGunSwapOrigin(GameMenu &view, const MovieRegion &parent, MovieRegion &origin);

/** Original button chapters own appearance, press completion and category hide. */
bool DrawStoreGunSwap(GameMenu &view, MenuState &state, const MovieRegion &parent, bool interactive);

bool CompleteOfflineIAP(std::uint64_t clock, MenuState &state, CProfileManager &profile,
    const std::vector<StoreEntry> &store, const std::filesystem::path &savePath);

/** IAP is a standard modal prompt, layout mode 1 (visual left), no buttons.
 * CMenuSystem::ShowPopup :96455 selects fonts 0/0/1/5 and GLU_MOVIE_POPUP.
 * BindContent :207403 derives its target size from fonts and sprite bounds. */
bool DrawStorePrompt(GameMenu &view, MenuState &state);

/** Currency entries have no object references and no cost string. The original
 * LevelCallback :180839 therefore places BUY/CONVERT in the bottom right.
 * Focus :181402 requires a cost string, so these cards do not expand. */
bool DrawCurrencyCard(GameMenu &view, CResTOCManager &toc, PackTables &tables,
    const StoreEntry &item, unsigned index, unsigned movie, const StoreCardFace &face,
    bool enabled, MenuState &state, CProfileManager &profile, const std::filesystem::path &savePath);

/** CStoreAggregator::EquipItem :156082 and SetGun/SetArmor :171658.
 * Granting inventory and equipping it are separate original menu actions.
 */
bool EquipStoreItem(CProfileManager &profile, const CStoreItem &item, const std::vector<ArmorEntry> &armors);

bool DrawStore(GameMenu &view, CResTOCManager &toc, PackTables &tables, CProfileManager &profile,
    unsigned level, const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::vector<ArmorEntry> &armors, MenuState &state, const std::filesystem::path &savePath);

/** The original mode medallions are MDS_BUTTON_MP_TOGGLE, archetype 8. */

/** CMenuMovieMultiplayerOverlay :250020..250880, region callbacks 0..5,
 * font 0 and MDS_BUTTON_MP_TOGGLE. No locally fabricated online mode. */
bool DrawOriginalModeOverlay(GameMenu &view, MenuState &state);

/** Paint dynamic planets at their original Movie layer, and retain native bounds. */

/** Original UpdatePosition :161040 advances along the dominant axis. */
void MoveStarPoint(float &x, float &y, float targetX, float targetY, unsigned elapsed, float speed);

/** CMenuMission :161015..163482, MENU_MISSION_ROOT VA 0x402d38.
 * The map is a bounded Movie timeline, not host coordinates or depth factors. */
bool DrawOriginalStarMap(GameMenu &view, MenuState &state, const CProfileManager &profile);

/** CMissionWaveStatus collection 1003; the four live retail projections may
 * contain progress that has not reached the next disk checkpoint yet. */
unsigned NativeMissionProgress(const CProfileManager &profile, const GameObjectRef &level);

bool OriginalMissionLocked(const CProfileManager &profile, const Mission &mission, const PlanetMissionInfo &info);

void DrawMissionText(GameMenu &view, const MovieRegion &region, const std::string &text, unsigned font, bool centered = false);

/** A page transition is the authored chapter 1; chapter 2 is the next page's
 * matching pose. The wheel is a Windows adapter for one native page gesture.
 * Historical note above described the former one-page adapter. Continuous
 * control now maps arbitrary positions onto those same authored poses.
 */
/** Map continuous scroll onto the original repeating Movie chapter. */
bool ScrollMissionMovie(GameMenu &view, MenuScrollMotion &motion, float &position,
    const CMovie &movie, const MovieRegion &viewport, float stride, unsigned maximum,
    bool enabled, unsigned &page, unsigned &time, bool &moving);

/** CMenuMissionOption::WaveSelectCallback :189920, invoked in Movie65 layers. */

/** MENU_MISSION_DETAIL: main48/list49/box50; original callbacks and references. */
bool DrawOriginalMissionInfo(GameMenu &view, MenuState &state, const CProfileManager &profile, bool &launched);

/** Horizontal revolution/horde cards use the same two-dimensional sprites as iOS. */

/** Returns true only after an unlocked wave/horde is explicitly launched. */

const WeaponEntry *FindMasteryWeapon(const std::vector<WeaponEntry> &weapons, const GameObjectRef &ref);

const StoreEntry *FindWeaponStore(const std::vector<StoreEntry> &store, const GameObjectRef &ref);

void BeginPostGame(MenuState &state, const SurvivalGameContext &context, const std::vector<WeaponEntry> &weapons);

// GLU_MOVIE_UPGRADE_POPUP regions, in the order the movie declares them:
// portrait, close, meter, CURRENT and NEXT headers, the two stat columns, the
// weapon icon plate, the title bar, the buy tab and the weapon name strip.
constexpr unsigned kUpgradePortraitRegion = 0;
constexpr unsigned kUpgradeCloseRegion = 1;
constexpr unsigned kUpgradeMeterRegion = 2;
constexpr unsigned kUpgradeCurrentHeaderRegion = 3;
constexpr unsigned kUpgradeNextHeaderRegion = 4;
constexpr unsigned kUpgradeCurrentColumnRegion = 5;
constexpr unsigned kUpgradeNextColumnRegion = 6;
constexpr unsigned kUpgradeIconRegion = 7;
constexpr unsigned kUpgradeTitleRegion = 8;
constexpr unsigned kUpgradeBuyRegion = 9;
constexpr unsigned kUpgradeNameRegion = 11;
// The player headshots the menus print next to a title.
constexpr unsigned kBrotherPortrait = 161;
// The old fixed fill interval is replaced by CMenuUpgradePopup's original 1x playback.

/** The upgrade popup is reached from the store as well as from the results,
 * so closing it returns to whichever page pushed it. */
void CloseMastery(MenuState &state);

/** How far into GLU_MOVIE_WEAPON_UPGRADE_MASTERY the meter stands for this
 * much experience. The movie's chapters are the three cells. */
unsigned MasteryMeterTime(GameMenu &view, const WeaponEntry &weapon, unsigned experience);

/** Bind CMenuMovieButton's original region 1 graphic/label and region 0 hit box. */
bool DrawUpgradeButton(GameMenu &view, unsigned index, const MovieRegion &area,
    const std::string &label, unsigned font, bool interactive, bool &pressed);

bool DrawOriginalMovieButton(GameMenu &view, const OriginalMenuEntry &entry, const MovieRegion &area,
    const std::string &label, unsigned font, bool interactive, bool &pressed, unsigned chapter, unsigned elapsed, unsigned timeOverride, bool stateArtwork);

/** Which navigation branch a host page sits in, named by that branch's own page.
 *
 * CMenuSystem::SetBranch :96614 leaves through its first test when the branch
 * asked for is the one already shown, and PushMenu/SetMenu :96666/:96700 route
 * every in-branch menu through that same early exit -- only the other path
 * restarts the WIPE movie with CMovie::SetTime(..., 0). So the sweep belongs to
 * navigation between branches. Menus inside one branch never play it: the store
 * category buttons carry action 64, which DoAction :93478 hands to the store
 * menu's own handler :95106 without going near SetBranch, and a planet click
 * pushes the REV list into the branch it is already in.
 *
 * The groups below are the ones the header already lights up as one option.
 */
unsigned MenuBranchPage(unsigned page);

/** Original callbacks use each MovieRegion and bitmap font without fitting. */
void UpgradeCenteredText(GameMenu &view, const MovieRegion &region, const std::string &text, unsigned font);

/** CMenuUpgradePopup::DrawBodyText :392576: only changed stats, then CRIT.
 * CURRENT is the absolute STORE value; NEXT is the relative percentage change. */
void DrawUpgradeStats(GameMenu &view, const MovieRegion &area, const CStoreItem &item, unsigned level, bool next);

/** The original popup advances its own movie and stars through six states.
 * All geometry, fonts, item values, chapter times and art are read from BIG. */
bool DrawMastery(GameMenu &view, MenuState &state, CProfileManager &profile, CResTOCManager &toc,
    PackTables &tables, const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::filesystem::path &savePath, CPlayerProgress *headerProgress = nullptr);

/** Resource printf substitution for the original CGame result strings. */
std::string PostGameFormat(GameMenu &view, const char *name, const std::vector<std::string> &values);

/** CMenuPostGame::OverviewCallback :164593; single-player default bro
 * provider93 count=3, last data index=7+Mission.type-4. */

/** MENU_POST_GAME_WRAPUP VA0x403350, CMenuPostGame :164559..166204.
 * Native menu/provider logic below; layouts, fonts and artwork stay in BIG. */
bool DrawOriginalPostGame(GameMenu &view, MenuState &state, CResTOCManager &toc, PackTables &tables,
    const CProfileManager &profile);

/** Standard intervals occupy 6..11; premium intervals occupy 0..5. */

/** Original menu provider 69; strings are BIG resources except the native
 * GetTimeIntervalString printf patterns at ARM VA 0x3c4980/0x3c49c4. */
std::string RefineryNumber(GameMenu &view, const char *name, std::uint64_t value);

bool SetRefineryStatus(GameMenu &view, MenuState &state, unsigned slot, unsigned chapter);

/** CMenuGameResources::Init/Bind :172652/172835, MENU_GAME_RESOURCES
 * VA0x402d50. Original provider SLOT_PHASE_OFFSETS={0,6}, COUNT={6,6}.
 * Resource dimensions, durations, text, sprite geometry and prices stay BIG. */

bool DrawRefinery(GameMenu &view, MenuState &state, CProfileManager &profile,
    const CRefinementManager::Template &data, const std::filesystem::path &savePath, std::int64_t now);
/** CMenuGameResources::DrawOverlay :173407 runs above the navigation bar. */
bool DrawRefineryOverlay(GameMenu &view, const MenuState &state);

/** CMenuFriends::Bind :197028 and CMenuChallenges::Bind :236612 select
 * chapter 1 while profile validity is false. Region 0 owns button 165/0,
 * region 1 owns centered font-0 text. No host flag can validate an NGS user.
 * ui_movie.bt and MENU_CHALLENGES VA 0x402eb0 identify the original Movie. */
bool DrawOriginalSocialOffline(GameMenu &view, MenuState &state, bool hasCredentials);
bool DrawOriginalSocialMenu(GameMenu &view, MenuState &state, const CProfileManager &profile, bool hasCredentials);
bool BindOriginalSocialContent(GameMenu &view, MenuState &state, const CProfileManager &profile);
bool DrawOriginalSocialContent(GameMenu &view, MenuState &state, const CProfileManager &profile, const MovieRegion &region);
bool DrawOriginalSocialModel(GameMenu &view, MenuState &state, const CProfileManager &profile);
bool BeginLocalMatch(MenuState &state);
void UpdateLocalConnection(MenuState &state);

/** CMenuDataProvider::CreateContentString :151507 resolves the action only
 * when the corresponding original MDS string slot is null. */
std::string OptionsText(GameMenu &view, const CProfileManager &profile, unsigned index, unsigned slot, const char *table = "MDS_OPTIONS");

/** MENU_OPTIONS at original VA 0x402e50 selects LIST_MENU, list offset 2,
 * bounds 1/1, LIST_MENU_BUTTON, LIST_MENU_TEXT; all geometry stays in BIG.
 * CMenuList :140175..140657, CMenuListOption :144029..144317, ui_movie.bt. */
bool DrawOptions(GameMenu &view, MenuState &state, CProfileManager &profile, bool &saveChanged);

/** CMenuPlayerSelect :194878..195343; original Movie70 owns both portraits,
 * highlights, chapter timings, title and touch rectangles. */
bool DrawOriginalPlayerSelect(GameMenu &view, MenuState &state, CProfileManager &profile,
    const std::filesystem::path &savePath, bool &launchTutorial);

/** CMenuGreeting callbacks :207876..208207. Local daily rewards are the
 * user-authorized clock adapter; social entry follows the service availability. */

bool DrawOriginalGreeting(GameMenu &view, MenuState &state, CResTOCManager &toc, PackTables &tables,
    CProfileManager &profile, const CDailyBonusTracking &daily, const std::vector<StoreEntry> &store,
    CPlayerProgress &progress, const std::filesystem::path &savePath, std::int64_t seconds);

/** User-authorized cht advances the native elapsed-day accumulator. The saved
 * launch timestamp stays on the real clock, so restarting cannot underflow it. */

/** Returns selected planet, -1 for quit, -2 after capture, -3 on failure. */
int ShowGameMenu(CResTOCManager &toc, PackTables &tables, CProfileManager &profile,
    const CPlayerProgress::Template &progressData, const CRefinementManager::Template &refinement,
    const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::vector<ArmorEntry> &armors, MenuState &state, const std::filesystem::path &savePath,
    const std::string &capturePath, const std::vector<MenuTestClick> *testClicks = nullptr, bool originalProfile = false, CWindow *sharedWindow = nullptr, bool testTransitions = false, MenuTransitionTrace *transitionTrace = nullptr, CBGM *sharedMusic = nullptr);

}

