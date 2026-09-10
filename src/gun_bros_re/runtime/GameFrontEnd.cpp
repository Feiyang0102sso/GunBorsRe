/** @file GameFrontEnd.cpp
 * @brief Connect the rebuilt offline account to actual gameplay.
 */
#define NOMINMAX
#include "runtime/GameFrontEnd.h"
#include "runtime/OriginalProfile.h"
#include "runtime/MissionCatalog.h"
#include "runtime/PlanetCatalog.h"
#include "runtime/StoreCatalog.h"
#include "runtime/WeaponCatalog.h"
#include "runtime/ArmorCatalog.h"
#include "runtime/PowerupCatalog.h"
#include "runtime/PlayerModel.h"
#include "runtime/HudText.h"
#include "runtime/MovieRenderer.h"
#include "runtime/LoadingScreen.h"
#include "runtime/OriginalMenuData.h"
#include "runtime/OriginalTextLayout.h"
#include "runtime/OriginalPromotionPopup.h"
#include "runtime/MenuWipe.h"
#include "runtime/SurvivalGameContext.h"
#include "runtime/HostSettings.h"
#include "gun_bros/CDailyBonusTracking.h"
#include "gun_bros/CMenuUpgradePopup.h"
#include "gun_bros/CMenuMesh.h"
#include "gun_bros/CMenuPopupPrompt.h"
#include "gun_bros/Planet.h"
#include "gun_bros/CLevel.h"
#include "gun_bros/CBGM.h"
#include "gun_bros/WeaponEffects.h"
#include "gun_bros/CParticleEffect.h"
#include "runtime/MapScene.h"
#include "runtime/StartupSequence.h"
#include "runtime/EnemyModel.h"
#include "engine/CQuadBatch.h"
#include "engine/CMatrix4d.h"
#include "sprite_glu/CSpriteGlu.h"
#include "sprite_glu/CSpriteIterator.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <cctype>
#include <cmath>

namespace {
constexpr const char *kPlanetPacks[] = {"pack2", "pack7", "pack9", "pack12", "pack11", "pack1"};
constexpr unsigned kPlanetMaps[] = {7, 6, 0, 0, 0};

constexpr unsigned kArmorSlots[] = {0, 0, 2, 1, 0};
constexpr float kMenuWidth = 1024;
// The store's character column starts where the black content area does; the
// original character reaches up past the category tabs on that side.

constexpr float kMenuHeight = 768;

std::int64_t CurrentSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

bool SameObject(const GameObjectRef &first, const GameObjectRef &second) {
    return first.packHash == second.packHash && first.localIndex == second.localIndex;
}

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

struct MenuState {
    unsigned page = 0;
    unsigned planet = 0;
    unsigned slot = 0;
    unsigned itemPage = 0;
    int selectedItem = -1;
    std::string message;
    int startingWave = -1;
    unsigned detail = 0;
    unsigned hordeStart = 0;
    unsigned currencyTab = 0, currencyPage = 0;
    int currencyItem = -1;
    unsigned shopCategory = 0;
    unsigned shopGunSlot = 0;
    // CMenuMovieButton states: showing 0, hiding 1, idle 2, selected 4, hidden 8.
    unsigned shopSwapPhase = 8, shopSwapTime = 0;
    std::uint64_t shopSwapLastTick = 0;
    bool shopSwapKeyRequested = false;
    float shopScroll = 0;
    MenuScrollMotion shopMotion, missionMotion, waveMotion;
    float missionPosition = 0, wavePosition = 0;
    std::uint64_t shopDetailStart = 0;
    std::uint64_t shopDetailLastTick = 0;
    unsigned shopDetailTime = 0;
    bool shopDetailClosing = false;
    float shopFocusAmount = 0;
    // Selecting a card never previews; only the PREVIEW button does.
    bool shopPreview = false;
    CMenuMesh playerMesh;
    std::uint64_t playerMeshLastTick = 0;
    // Last popup update tick; the resource chapters own its playback cursor.
    std::uint64_t masteryOpened = 0;
    CMenuUpgradePopup masteryPopup;
    unsigned shopFilter = 0;
    unsigned shopExclusionFilter = 0;
    bool shopFilterOpen = false;
    // Each menu owns its playback cursor; cached CMovie resources stay immutable.
    unsigned shopFilterTime = 0;
    std::uint64_t shopFilterLastTick = 0;
    bool shopFilterBound = false;
    bool shopDetailOpen = false;
    unsigned gameMode = 0;
    bool modeSelected = false;
    bool modeBound = false;
    unsigned modeTime = 0, modePhase = 0, modeSpriteTime = 0;
    std::uint64_t modeLastTick = 0;
    float starPanX = 0, starPanY = 0;
    bool starBound = false, starReverse = false, starLocked = false, starEntering = false;
    unsigned starTime = 0, starReticleTime = 0, starFlagTime = 0, starFadeTime = 0;
    int starSelectedSlot = -1, starTargetTime = -1;
    float starSpeed = 0;
    float starSelectorX = 0, starSelectorY = 0, starFlagX = 0, starFlagY = 0;
    std::uint64_t starLastTick = 0;
    float missionScroll = 0;
    bool missionBound = false, missionClosing = false;
    unsigned missionTime = 0, missionListTime = 0, missionCardTime = 0, missionFocusTime = 0;
    unsigned missionFirst = 0, missionWaveTime = 0, missionWaveButtonTime = 0;
    bool missionListMoving = false, missionListReverse = false, missionWaveMoving = false, missionWaveReverse = false;
    int missionFocused = -1;
    float missionFocusX = 0, missionFocusY = 0;
    GameObjectRef selectedMission;
    std::uint64_t missionLastTick = 0;
    unsigned revolution = 0, wavePage = 0, missionTab = 0;
    bool currencyPending = false;
    CMenuPopupPrompt storePopup;
    std::uint64_t storePopupLastTick = 0;
    unsigned storePromptSpriteTime = 0;
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
    bool postGameMusic = false;
    GameObjectRef masteryWeapon;
    bool refinementRequired = false;
    unsigned refineryTab = 0, casualtyPage = 0;
    bool refineryBound = false;
    unsigned refineryTime = 0, refineryElapsed = 0;
    std::uint64_t refineryLastTick = 0;
    std::array<unsigned, kRefinementSlotCount> refineryStatusTime{}, refineryFillTime{};
    std::array<unsigned, kRefinementSlotCount> refineryStatusChapter{};
    int refineryTransfer = -1;
    unsigned refineryTransferTime = 0, refineryTransferSprite = 0;
    std::uint64_t refineryTransferAmount = 0;
    float refineryTransferX = 0, refineryTransferY = 0, refineryTargetX = 0, refineryTargetY = 0;
    bool greetingBound = false, greetingExitRequested = false, greetingClosing = false;
    unsigned greetingTime = 0, greetingElapsed = 0, greetingTarget = 0;
    std::uint64_t greetingLastTick = 0;
    bool playerSelectBound = false, playerSelectReady = false;
    bool postGameBound = false, postGameUpgradePending = false, postGameClosing = false;
    unsigned postGameTime = 0, postGameItemTime = 0, postGameCloseTime = 0;
    float postGameGalleryPosition = 0, postGameGalleryVelocity = 0;
    unsigned postGameDelta = 0;
    std::uint64_t postGameLastTick = 0;
    int playerSelection = -1;
    unsigned playerSelectTime = 0, playerSelectChapter = 1;
    std::uint64_t playerSelectLastTick = 0;
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
    unsigned socialTab = 0;
    bool socialBound = false;
    unsigned socialTime = 0;
    std::uint64_t socialLastTick = 0;
    OriginalPromotionPopup promotion;
    std::uint64_t promotionTick = 0;
    std::vector<unsigned> history;

    void ShowStorePrompt(const char *table, bool sideVisual, bool dismissible, unsigned index = 0) {
        storePromptTable = table;
        storePromptIndex = index;
        storePromptSideVisual = sideVisual;
        storePromptDismiss = dismissible;
        storePromptButtons = nullptr;
        storePromptRequested = true;
        storePopup = CMenuPopupPrompt();
    }

    void BeginOfflineIAP(int item, std::uint64_t clock) {
        currencyItem = item;
        currencyPending = true;
        currencyReadyAt = clock + 4000; // User-authorized offline wait, UI_sample/ui.md.
        ShowStorePrompt("MDS_IAP_PLEASE_WAIT", true, false);
    }

    // Every nested page remembers its caller; trunk navigation starts a new path.
    void Navigate(unsigned target, bool root = false) {
        if (root) { history.clear(); }
        if (target == 17) {
            shopCategory = 3;
            shopScroll = 0;
            shopFilter = 0;
            shopDetailOpen = false;
        }
        if (target == page) { return; }
        // A hidden control must not resume an old fling on another page.
        shopMotion = MenuScrollMotion{};
        missionMotion = MenuScrollMotion{};
        waveMotion = MenuScrollMotion{};
        modeLastTick = 0;
        if (page == 24 && greetingBound) {
            greetingTarget = target;
            greetingExitRequested = true;
            return;
        }
        if (target == 26) { masteryPopup = CMenuUpgradePopup(); }
        if (target == 6) { optionsBound = false; }
        if (target == 8) {
            optionsReturnFocus = optionsFocus;
            optionsReturnScroll = optionsScroll;
            optionsBound = false;
            optionsFocus = 0;
            optionsScroll = optionsTarget = optionsBodyScroll = 0;
        }
        if (target == 3) { refineryBound = false; }
        if (target == 24) { greetingBound = false; }
        if (target == 25 || target == 29) { playerSelectBound = false; }
        if (target == 27 && page != 26 && page != 28) { postGameBound = false; }
        if (target == 0) { starBound = false; }
        if (target == 21) { missionBound = false; }
        socialBound = false;
        if (!root && page != 14) { history.push_back(page); }
        page = target;
    }
    void Back() {
        shopMotion = MenuScrollMotion{};
        missionMotion = MenuScrollMotion{};
        waveMotion = MenuScrollMotion{};
        modeLastTick = 0;
        if (page == 24 && greetingBound) { Navigate(0, true); return; }
        if (history.empty()) { page = 0; return; }
        const bool fromHelp = page == 8;
        page = history.back();
        if (page == 6) {
            optionsBound = false;
            if (fromHelp) { optionsFocus = optionsReturnFocus; optionsScroll = optionsReturnScroll; }
        }
        socialBound = false;
        history.pop_back();
    }
};

struct MenuTransitionTrace { bool active = false; unsigned time = 0, starts = 0; };

struct MenuTestClick { float x; float y; unsigned advanceMs = 0; unsigned renderDelayMs = 0; };

/** All GL owners are destroyed before the menu window's context. */
class GameMenu {
public:
    explicit GameMenu(CWindow *sharedWindow = nullptr) : window(sharedWindow ? *sharedWindow : ownedWindow) {}
    /** Integration harness input; it still goes through rendered button hit tests. */
    void SetTestClick(const MenuTestClick &click) { mouseX = click.x; mouseY = click.y; clicked = true; }
    /** Temporarily route this frame's click exclusively to a modal panel. */
    bool ExchangeClick(bool enabled) { const bool previous = clicked; clicked = enabled; return previous; }
    std::pair<float, float> Cursor() const { return {mouseX, mouseY}; }
    bool Open(CResTOCManager &toc, PackTables &tables, const CProfileManager *profile = nullptr, bool startup = false, CBGM *music = nullptr) {
        resourceToc = &toc;
        resourceTables = &tables;
        if (!window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return false; }
        window.SetEscapeCloses(false);
        window.EnableCheats(true);
        const char *directory = ASSET_ROOT "/src/gun_bros_re/shaders";
        if (!textProgram.Load(directory, "ogles_vs_mvp_constcolor", "ogles_ps_constcolor") ||
            !imageProgram.Load(directory, "ogles_vs_mvp_tex0", "ogles_ps_tex0") ||
            !markers.Create(textProgram) || !images.Create(imageProgram)) { return false; }
        Matrix4dOrthoTopLeft(kMenuWidth, kMenuHeight, 100, projection);
        CResPackTOC *core = toc.GetPack(toc.GetCorePackIndex());
        if (!movies.Init(*core, *core)) { return false; }
        const auto *belt = movies.GetMovie(movies.Ordinal("GLU_MOVIE_STORE_SCROLL"));
        unsigned beltEnd = 0;
        if (!belt || !belt->GetChapterRange(1, storeRestTime, beltEnd)) { return false; }
        LoadingScreen loading(window, movies, tables, profile, false, startup, music);
        if (!loading.IsValid()) { return false; }
        for (unsigned index = 0; index < 7; ++index) {
            const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_TRUNK", index);
            if (entry == nullptr) { return false; }
            std::printf("[navigation] index=%u label=%s sprite=%u\n", index,
                movies.NamedString(entry->strings[0]).c_str(), entry->sprites[0]);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (!LoadPlanetCatalog(toc, tables, planetEntries)) { return false; }
        names.resize(planetEntries.size());
        descriptions.resize(planetEntries.size());
        planetQuads.resize(planetEntries.size());
        planetThumbs.resize(planetEntries.size());
        for (unsigned index = 0; index < planetEntries.size(); ++index) {
            const Planet &planet = planetEntries[index].data;
            names[index] = ReadGameString(toc, planet.name);
            descriptions[index] = ReadGameString(toc, planet.description);
            // Planet::CreateLargeImage :170099 uses the map sprite's pack.
            const int spritePack = toc.GetPackIndexFromHash(planet.thumbnail.packHash);
            if (spritePacks.count(spritePack) == 0) {
                auto glu = std::make_unique<CSpriteGlu>();
                if (!glu->Init(*toc.GetPack(spritePack))) { return false; }
                spritePacks[spritePack] = std::move(glu);
            }
            CSpriteGlu &glu = *spritePacks[spritePack];
            const CSpriteGluArchetype *large = glu.GetArchetype(planet.largeImage.archetype);
            const CSpriteGluArchetype *thumb = glu.GetArchetype(planet.thumbnail.archetype);
            if (large == nullptr || thumb == nullptr) { return false; }
            CSpriteIterator largeIterator(glu, *large), thumbIterator(glu, *thumb);
            if (!largeIterator.Expand(planet.largeImage.animation, 0, planetQuads[index]) ||
                !thumbIterator.Expand(planet.thumbnail.animation, 0, planetThumbs[index])) { return false; }
            std::printf("[planet-menu] host=%u slot=%u resource=%u:%u missions=%zu name=%s\n",
                index, planet.mapSlot, planetEntries[index].resource.packHash, planetEntries[index].resource.localIndex,
                planet.missions.size(), names[index].c_str());
        }
        return loading.IsValid() && !loading.Cancelled();
    }

    void Begin(unsigned page = 0) {
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        glViewport(0, 0, width, height);
        glClearColor(0.063f, 0.114f, 0.176f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        if (!window.GetMousePosition(mouseX, mouseY)) { mouseX = -100; mouseY = -100; }
        mouseX *= kMenuWidth / width;
        mouseY *= kMenuHeight / height;
        const bool down = window.IsLeftMouseDown();
        int pixelsX = 0, pixelsY = 0;
        window.TakeDragDelta(pixelsX, pixelsY);
        dragX = pixelsX * kMenuWidth / width;
        dragY = pixelsY * kMenuHeight / height;
        if (down && !previousDown) { dragDistance = 0; }
        pointerPressed = down && !previousDown;
        pointerHeld = down;
        dragDistance += std::abs(dragX) + std::abs(dragY);
        // CMenuMission handles selection on release; dragging must never enter a planet.
        clicked = !down && previousDown && dragDistance < 9;
        previousDown = down;
        if (scripted) {
            pointerPressed = false;
            pointerHeld = false;
            dragX = 0;
            dragY = 0;
            dragDistance = 0;
            window.TakeWheelDelta();
        }
        if (page != 0 && page != 22) { movies.Draw(47, 1600); }
        if (page == 3) { movies.Draw(36, 1600); }
        if (page == 2 || page == 17 || page == 18) { movies.Rectangle(0, 132, 1024, 627, 0, 0, 0); }
    }

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

    bool TitleImage() {
        if (!titleImage.IsValid()) {
            if (!LoadStartupSplash(titleImage)) { return false; }
        }
        images.Begin();
        const SourceRect source{0, 0, static_cast<std::uint16_t>(titleImage.GetWidth()), static_cast<std::uint16_t>(titleImage.GetHeight())};
        images.AddQuad(titleImage, 0, 0, 1024, 768, source, false, false, BlendMode::Alpha);
        images.Upload();
        images.Draw(imageProgram, projection);
        return true;
    }

    int Header(const CProfileManager &profile, const CPlayerProgress &progress, unsigned currentPage);

    // Historical explicit .dat research UI; native profiles use Header below.

    bool Icon(CResTOCManager &toc, PackTables &tables, const StoreEntry &entry, float x, float y, float width,
        float height, float alpha = 1, bool originalSize = false, bool fitHeight = false) {
        const CGameAssetRef &ref = entry.data.assets[1];
        if (ref.assetId < 0 || ref.IsNull()) { return false; }
        const std::uint64_t key = (static_cast<std::uint64_t>(ref.packHash) << 32) | static_cast<unsigned>(ref.assetId);
        if (icons.count(key) == 0) {
            std::vector<std::uint8_t> payload;
            PNGImage decoded;
            auto texture = std::make_unique<CTexture>();
            if (!tables.ReadSectionResource(ref.packHash, GameSection::Png, ref.assetId, payload) ||
                !PNGDecode(payload, decoded) || !texture->Create(decoded)) { return false; }
            icons[key] = std::move(texture);
        }
        const CTexture &texture = *icons[key];
        float scale = std::min(width / texture.GetWidth(), height / texture.GetHeight());
        // CMenuStoreOption::ThumbCallback :181036 preserves the PNG dimensions.
        // A thumbnail wider than region 5 starts at its left edge.
        if (originalSize) { scale = 1; }
        // CMenuGreeting::BonusIconCallback :208122 scales by region HEIGHT,
        // including its 16.16 truncation, instead of fitting both dimensions.
        if (fitHeight) { scale = std::floor(height * 65536 / texture.GetHeight()) / 65536; }
        const float drawnWidth = texture.GetWidth() * scale;
        const float drawnHeight = texture.GetHeight() * scale;
        float drawnX = x + (width - drawnWidth) * 0.5f;
        if (originalSize && drawnWidth > width) { drawnX = x; }
        const SourceRect source{0, 0, static_cast<std::uint16_t>(texture.GetWidth()), static_cast<std::uint16_t>(texture.GetHeight())};
        images.Begin();
        images.AddTransformedQuad(texture, drawnX, y + (height - drawnHeight) * 0.5f,
            drawnWidth, drawnHeight, source, false, false, BlendMode::Alpha, 0, 0, 1, 1, 0, alpha);
        images.Upload();
        images.Draw(imageProgram, movies.CurrentProjection());
        return true;
    }

    bool DrawEquippedPlayer(CResTOCManager &toc, PackTables &tables, const CProfileManager &profile,
        const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armors, unsigned slot,
        const GameObjectTypeRef *previewItem = nullptr, const MovieRegion *storePanel = nullptr, float spin = 0) {
        unsigned gunSlot = profile.activeWeaponSlot;
        if (slot < 2) { gunSlot = slot; }
        // Preview substitutes only the model configuration. Ownership, currency
        // and the saved loadout remain owned by the explicit purchase action.
        CPlayerConfiguration configuration = profile.configuration;
        if (previewItem != nullptr) {
            if (previewItem->type == 6) { configuration.guns[gunSlot] = previewItem->object; }
            if (previewItem->type == 2 && slot >= 2 && slot <= 4) { configuration.armor[kArmorSlots[slot]] = previewItem->object; }
        }
        // Switching weapon slot is the original's swap, not just a rebuild.
        bool changed = equippedPreview == nullptr;
        if (equippedPreview != nullptr && equippedPreview->brotherIndex != profile.playerBrother) { changed = true; }
        for (unsigned index = 0; index < 2; ++index) {
            if (!SameObject(previewConfiguration.guns[index], configuration.guns[index])) { changed = true; }
        }
        for (unsigned index = 0; index < kArmorSlotCount; ++index) {
            if (!SameObject(previewConfiguration.armor[index], configuration.armor[index])) { changed = true; }
        }
        if (changed) {
            PlayerTemplateData playerTemplate;
            if (!FindPlayerTemplate(toc, tables, playerTemplate)) { return false; }
            auto candidate = std::make_unique<PlayerModel>();
            candidate->brotherIndex = profile.playerBrother;
            if (!BuildPlayerBody(tables, playerTemplate.moveSet, *candidate)) { return false; }
            const WeaponEntry *gun = nullptr;
            for (const WeaponEntry &entry : weapons) {
                const auto &ref = configuration.guns[gunSlot];
                if (entry.packHash == ref.packHash && entry.ordinal == ref.localIndex) { gun = &entry; break; }
            }
            if (gun == nullptr || !EquipPlayerWeapon(tables, playerTemplate.script, gun->data, gun->owner, *candidate)) { return false; }
            const GameObjectRef &otherRef = configuration.guns[1 - gunSlot];
            if (!otherRef.IsNull()) {
                const WeaponEntry *otherGun = nullptr;
                for (const WeaponEntry &entry : weapons) {
                    if (entry.packHash == otherRef.packHash && entry.ordinal == otherRef.localIndex) { otherGun = &entry; break; }
                }
                if (otherGun == nullptr || !PreparePlayerUIWeapon(tables, otherGun->data, otherGun->owner, *candidate)) { return false; }
            }
            if (!candidate->weapon->brother.SpawnForUI()) { return false; }
            const auto &uiTorso = candidate->weapon->brother.GetTorso();
            std::printf("[player-ui] gun=%s state=%d weapon-torso=%d move=%d config=%d time=%d range=%d..%d override9=%d\n",
                gun->owner.c_str(), candidate->weapon->brother.GetStateId(), candidate->weapon->brother.TorsoUsesWeapon(),
                uiTorso.GetMoveIndex(), uiTorso.GetMeshConfigIndex(), uiTorso.GetAnimation().GetTimeMs(),
                uiTorso.GetAnimation().GetRangeStartMs(), uiTorso.GetAnimation().GetRangeStartMs() + uiTorso.GetAnimation().GetRangeDurationMs(),
                candidate->weapon->gun.GetOverrides()[9]);
            for (unsigned index = 0; index < kArmorSlotCount; ++index) {
                const auto &ref = configuration.armor[index];
                if (ref.IsNull()) { continue; }
                for (const ArmorEntry &entry : armors) {
                    if (entry.packHash == ref.packHash && entry.ordinal == ref.localIndex) {
                        if (!EquipPlayerArmor(tables, entry.data, imageProgram, *candidate)) { return false; }
                        break;
                    }
                }
            }
            if (!CreatePlayerBuffers(*candidate, imageProgram)) { return false; }
            equippedPreview = std::move(candidate);
            previewConfiguration = configuration;
            previewGunSlot = gunSlot;
            previewPrimarySlot = gunSlot;
            previewSwapPending = false;
            previewSlotChanged = false;
            previewTicks = clock;
            // CPlayer::OnSwapGun :101048 hands input event 5 to the player
            // script, which owns the swap animation.
        }
        if (previewGunSlot != gunSlot && !previewSwapPending && equippedPreview->uiOtherWeapon) {
            previewSwapPending = equippedPreview->weapon->brother.OnSwapGun();
        }
        if (storePanel == nullptr) { return false; }
        const std::uint64_t now = clock;
        unsigned previewDelta = 0;
        if (now >= previewTicks) { previewDelta = static_cast<unsigned>(std::min<std::uint64_t>(now - previewTicks, 100)); }
        AdvancePlayerPreview(previewDelta);
        PosePlayer(*equippedPreview);
        previewTicks = now;
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        float model[16];
        if (storePanel != nullptr) {
            // CMenuStore::Bind :180082 supplies region 2 as mesh bounds.
            // CMenuMeshPlayer::Draw :169591 calls DrawUI in the full page;
            // that region is not a viewport or a scissor rectangle.
            if (!BuildPlayerUIMatrix(*equippedPreview, storePanel->x + static_cast<int>(storePanel->width) / 2,
                storePanel->y, storePanel->height, spin, kMenuWidth, kMenuHeight, model)) { return false; }
            glViewport(0, 0, width, height);
            glDisable(GL_SCISSOR_TEST);
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
        }
        if (verifyPlayerProjection && storePanel != nullptr) {
            // Independent oracle from CBrother::DrawUI :136337-136357.
            // Compare the actual GL viewport + MVP, not just a helper's return.
            const auto &brother = equippedPreview->weapon->brother;
            const MeshBounds &torso = brother.GetTorso().GetAnimation().GetMesh()->GetBounds();
            const float expectedScale = storePanel->height / std::abs(torso.maxZ - torso.minZ);
            const float expectedX = storePanel->x + static_cast<int>(storePanel->width) / 2;
            const float expectedY = static_cast<float>(static_cast<int>(storePanel->y - torso.centerZ * expectedScale +
                storePanel->height + storePanel->height * 0.5f));
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);
            const float actualX = (viewport[0] + (model[3] + 1) * viewport[2] / 2) * kMenuWidth / width;
            const float actualY = (height - viewport[1] - (model[7] + 1) * viewport[3] / 2) * kMenuHeight / height;
            const float actualScale = std::abs(model[0]) * viewport[2] * kMenuWidth / (2 * width);
            const bool matches = std::abs(actualX - expectedX) < 0.01f && std::abs(actualY - expectedY) < 0.01f &&
                std::abs(actualScale - expectedScale) < 0.01f && glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE;
            std::printf("[store-player-check] origin=(%.3f,%.3f) expected=(%.3f,%.3f) scale=%.3f expected=%.3f clip=%u match=%u\n",
                actualX, actualY, expectedX, expectedY, actualScale, expectedScale, glIsEnabled(GL_SCISSOR_TEST), matches);
            if (!matches) { return false; }
        }
        DrawPlayer(*equippedPreview, imageProgram, model);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, width, height);
        return true;
    }

    /** CEnemy::SpawnForUI assembles the original result-card model. */
    bool DrawCasualty(PackTables &tables, CResTOCManager &toc, const EnemyCasualty &casualty, float x,
        const MovieRegion *originalRegion = nullptr) {
        const std::uint64_t key = (static_cast<std::uint64_t>(casualty.resource.packHash) << 8) | casualty.resource.localIndex;
        if (enemyPreviews.count(key) == 0) {
            auto preview = std::make_unique<EnemyPreview>();
            if (!ReadEnemyTemplate(tables, casualty.resource.packHash, casualty.resource.localIndex, casualty.name, preview->data) ||
                !LoadEnemyModel(tables, preview->data, true, &imageProgram, EnemySpawnMode::Menu, preview->model)) { return false; }
            // The first ENEMY asset is its localized name, before its script.
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(casualty.resource.packHash, GameSection::Enemy, casualty.resource.localIndex, bytes)) { return false; }
            CArrayInputStream stream(bytes);
            stream.ReadUInt8();
            CGameAssetRef nameRef;
            nameRef.Init(stream);
            preview->name = ReadGameString(toc, nameRef);
            enemyPreviews[key] = std::move(preview);
        }
        EnemyPreview &preview = *enemyPreviews[key];
        if (originalRegion != nullptr) {
            const MovieRegion &region = *originalRegion;
            if (region.index == 2) {
                // CMenuMeshOption::TextCallback :176114: two font-0 lines.
                movies.Text(preview.name, region.x + (region.width - movies.TextWidth(preview.name, 0)) / 2,
                    region.y, 0, 1, 0, region.alpha);
                const std::string kills = movies.NamedString("IDS_WRAPUP_KILLS") + std::to_string(casualty.count);
                return movies.Text(kills, region.x + (region.width - movies.TextWidth(kills, 0)) / 2,
                    region.y + movies.TextHeight(0), 0, 1, 0, region.alpha);
            }
            if (region.index != 1) { return true; }
            const int body = EnemyPartConfig(preview.model, 0);
            if (body < 0) { return true; }
            if (preview.lastTick != 0) { preview.model.enemy.Update(static_cast<int>(clock - preview.lastTick)); }
            preview.lastTick = clock;
            // CEnemy::GetBoundsInternal :67314: union of integer XY boxes,
            // all scaled by PART 0 inverse extent * 100; attachment is ignored.
            const float units = preview.model.configs[body]->mesh.GetBounds().inverseExtent * 100;
            int left = 0, top = 0, right = 0, bottom = 0;
            bool bounded = false;
            for (unsigned part = 0; part < preview.model.enemy.GetPartCount(); ++part) {
                const int config = EnemyPartConfig(preview.model, part);
                if (config < 0) { continue; }
                const auto &bounds = preview.model.configs[config]->mesh.GetBounds();
                const int width = static_cast<int>((bounds.maxX - bounds.minX) * units);
                const int height = static_cast<int>((bounds.maxY - bounds.minY) * units);
                if (width == 0 || height == 0) { continue; }
                const int x1 = static_cast<int>(bounds.centerX) - width / 2;
                const int y1 = static_cast<int>(bounds.centerY) - height / 2;
                if (!bounded) { left = x1; top = y1; right = x1 + width; bottom = y1 + height; bounded = true; }
                else { left = std::min(left, x1); top = std::min(top, y1); right = std::max(right, x1 + width); bottom = std::max(bottom, y1 + height); }
            }
            if (!bounded) { return false; }
            const float fit = std::min(region.width / (right - left), region.height / (bottom - top)) * preview.data.uiScalePercent / 100;
            float projection3D[16], translation[16], scaling[16], tilt[16], facing[16], first[16], next[16], model[16];
            Matrix4dOrthoTopLeft(kMenuWidth, kMenuHeight, 32767, projection3D);
            projection3D[11] = -1;
            // CEnemy::DrawUI :68451 anchors at center/bottom; no viewport crop.
            const float originX = static_cast<float>(static_cast<int>(region.x + region.width / 2));
            const float originY = static_cast<float>(static_cast<int>(region.y + region.height));
            Matrix4dTranslation(originX, originY, -500, translation);
            Matrix4dScale(fit, scaling);
            Matrix4dRotationX(3.14159265f * 0.5f, tilt);
            Matrix4dRotationZ(3.14159265f, facing);
            Matrix4dMultiply(projection3D, translation, first);
            Matrix4dMultiply(first, scaling, next);
            Matrix4dMultiply(next, tilt, first);
            Matrix4dMultiply(first, facing, model);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_DEPTH_BUFFER_BIT);
            DrawEnemyModel(preview.model, imageProgram, model);
            glDisable(GL_DEPTH_TEST);
            return true;
        }
        return false;
    }

    CWindow ownedWindow;
    CWindow &window;
    // Menu time; the integration harness advances it instead of real ticks.
    std::uint64_t clock = 0;
    // Under the harness the real pointer must not scroll anything, or a stray
    // drag over the window moves a list out from under a scripted click.
    bool scripted = false;
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
    void DrawPlanetOriginal(unsigned index, const MovieRegion &region) {
        images.Begin();
        for (const auto &quad : planetQuads[index]) {
            images.AddTransformedQuad(*quad.page, region.x + region.width / 2 + quad.offsetX,
                region.y + region.height / 2 + quad.offsetY, float(quad.Width()), float(quad.Height()),
                quad.source, quad.flipHorizontal, quad.flipVertical, quad.blend, 0, 0, 1, 1, 0, region.alpha, quad.rotateTexture);
        }
        images.Upload();
        images.Draw(imageProgram, movies.CurrentProjection());
    }

    /** Original PlanetCallback uses native sprite pixels and a centered hit box. */
    MovieRegion DrawPlanetThumb(unsigned index, const MovieRegion &region, float fade = 1) {
        const auto &quads = planetThumbs[index];
        float left = 0, top = 0, right = 0, bottom = 0;
        bool first = true;
        images.Begin();
        const float centerX = region.x + region.width / 2;
        const float centerY = region.y + region.height / 2;
        for (const auto &quad : quads) {
            if (first) {
                left = right = static_cast<float>(quad.offsetX);
                top = bottom = static_cast<float>(quad.offsetY);
                first = false;
            }
            left = std::min(left, float(quad.offsetX));
            top = std::min(top, float(quad.offsetY));
            right = std::max(right, float(quad.offsetX + quad.Width()));
            bottom = std::max(bottom, float(quad.offsetY + quad.Height()));
            images.AddTransformedQuad(*quad.page, centerX + quad.offsetX, centerY + quad.offsetY,
                float(quad.Width()), float(quad.Height()), quad.source, quad.flipHorizontal, quad.flipVertical,
                quad.blend, 0, 0, 1, 1, 0, region.alpha * fade, quad.rotateTexture);
        }
        images.Upload();
        images.Draw(imageProgram, movies.CurrentProjection());
        return {region.index, region.type, centerX - (right - left) / 2, centerY - (bottom - top) / 2,
            right - left, bottom - top, region.alpha};
    }

    void PlanetFlagLines(float centerX, float centerY, const MovieRegion &area) {
        // FlagPoleCallback :161369 uses native color 0x807CC9F3 and 1px lines.
        markers.Begin();
        markers.AddSegment(centerX, centerY, area.x, area.y, 1);
        markers.AddSegment(centerX, centerY, area.x + area.width, area.y, 1);
        markers.AddSegment(centerX, centerY, area.x, area.y + area.height, 1);
        markers.AddSegment(centerX, centerY, area.x + area.width, area.y + area.height, 1);
        markers.Draw(textProgram, projection, 124.0f / 255, 201.0f / 255, 243.0f / 255, area.alpha * 0.5f);
        movies.Rectangle(area.x, area.y, area.width, area.height, 0, 0, 0, area.alpha * 0.5f);
    }
    float dragX = 0, dragY = 0;
    bool pointerHeld = false, pointerPressed = false;
    bool PrepareModeEffects() {
        if (modeEffects[0]) { return true; }
        static const struct { const char *pack; int ordinals[2]; } binding =
#include "runtime/OriginalModeParticleData.inc"
        ;
        const int pack = resourceToc->GetPackIndexFromName(binding.pack);
        if (pack < 0) { return false; }
        for (unsigned index = 0; index < 2; ++index) {
            if (binding.ordinals[index] < 0) { return false; }
            modeEffectRefs[index].packHash = resourceToc->GetPack(pack)->GetPackHash();
            modeEffectRefs[index].localIndex = static_cast<std::uint8_t>(binding.ordinals[index]);
            std::vector<std::uint8_t> bytes;
            if (!resourceTables->ReadSectionResource(modeEffectRefs[index].packHash, GameSection::ParticleEffect,
                modeEffectRefs[index].localIndex, bytes)) { return false; }
            CParticleEffect effect;
            CArrayInputStream input(bytes);
            if (!effect.Init(input) || input.Available() != 0) { return false; }
            modeEffects[index] = std::make_unique<WeaponEffects>(*resourceToc, *resourceTables, imageProgram);
        }
        // Bind stops the selection burst; the second emitter runs continuously.
        return modeEffects[1]->StartPersistentEffect(modeEffectRefs[1], 0, 0) != 0;
    }
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
    void DrawModeEffects(const MovieRegion &label) {
        // LabelCallback :250137 positions the burst at label top-center and
        // the persistent glow at its center, before drawing the text itself.
        for (unsigned index = 0; index < 2; ++index) {
            float transform[16];
            std::copy(movies.CurrentProjection(), movies.CurrentProjection() + 16, transform);
            float y = label.y;
            if (index == 1) { y += label.height / 2; }
            Matrix4dTranslate(transform, label.x + label.width / 2, y);
            modeEffects[index]->Draw(transform);
        }
    }
    std::size_t ModeParticleCount() const {
        if (!modeEffects[0]) { return 0; }
        return modeEffects[0]->GetParticleCount();
    }
    void Scroll(MenuScrollMotion &motion, float &position, const MovieRegion &viewport,
        bool enabled, float maximum, float stride, unsigned duration) {
        const bool inside = MouseIn(viewport.x, viewport.y, viewport.width, viewport.height);
        float wheel = 0;
        if (enabled && inside) { wheel = window.TakeWheelDelta(); }
        bool pressed = pointerPressed && inside;
        bool held = pointerHeld;
        if (scripted && dragX != 0) { pressed = inside; held = true; }
        motion.Update(position, clock, dragX, wheel, held, pressed, enabled, maximum, stride, duration);
    }
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
    void AdvancePlayerPreview(int deltaMs) {
        CBrother &brother = equippedPreview->weapon->brother;
        brother.UpdateUI(deltaMs);
        previewAudio.Update();
        PlayPreviewSounds(brother.GetTorso().TakeSounds());
        PlayPreviewSounds(brother.GetLegs().TakeSounds());
        if (brother.TakeWeaponSwap() && previewSwapPending) {
            previewGunSlot = 1 - previewGunSlot;
            SelectPlayerUIWeapon(*equippedPreview, previewGunSlot == previewPrimarySlot);
            previewSwapPending = false;
            previewSlotChanged = true;
            std::printf("[player-ui] native-swap slot=%u state=%d torso-preserved=1\n", previewGunSlot, brother.GetStateId());
        }
    }
private:
    void PlayPreviewSounds(const std::vector<GameObjectRef> &sounds) {
        // UpdateUI :137574 uses direct WAV ordinals, not SoundEffect templates.
        for (const auto &sound : sounds) {
            const std::uint64_t key = (static_cast<std::uint64_t>(sound.packHash) << 32) | sound.localIndex;
            if (!previewAudio.HasSound(key)) {
                std::vector<std::uint8_t> bytes;
                if (!resourceTables->ReadSectionResource(sound.packHash, GameSection::Wav, sound.localIndex, bytes) ||
                    !previewAudio.Load(key, bytes)) {
                    std::printf("[player-ui-audio] failed WAV=%08x:%u\n", sound.packHash, sound.localIndex);
                    continue;
                }
            }
            if (!previewAudio.Play(key)) {
                std::printf("[player-ui-audio] playback failed WAV=%08x:%u\n", sound.packHash, sound.localIndex);
                continue;
            }
            ++previewSoundCount;
            std::printf("[player-ui-audio] WAV=%08x:%u\n", sound.packHash, sound.localIndex);
        }
    }
    CAudioPlayer previewAudio;
    std::size_t previewSoundCount = 0;
    CResTOCManager *resourceToc = nullptr;
    PackTables *resourceTables = nullptr;
    CShaderProgram textProgram;
    CShaderProgram imageProgram;
    std::array<std::unique_ptr<WeaponEffects>, 2> modeEffects;
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
}

int RunTutorialPlayCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    SurvivalGameContext context{profile, "out/tutorial-profile-check.dat", 0};
    context.tutorial = true;
    if (RunSurvival(bigDirectory, "pack2", 7, 0, -1, "", 0, false, false, true, 2, 0, &context, true) != 0) { return 1; }
    CProfileManager restored;
    restored.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!restored.LoadFromDisk(context.savePath) || !restored.tutorialCompleted || restored.tutorialSteps != 255 ||
        restored.configuration.guns[1].localIndex != 4) { return 1; }
    std::printf("[tutorial-profile-check] completed=1 steps=255 rifle=4 restored=1\n");
    // Repeat with a genuinely absent original save source. The GUI's first
    // brother choice was checked separately; now verify its native play seam.
    const auto nativePath = std::filesystem::path("out/ui-original-2026-09-09") / ("tutorial-native-" + std::to_string(GetTickCount64()));
    CProfileManager native;
    native.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!LoadNativeProfile(toc, tables, native, nativePath, nativePath / "absent-source")) { return 1; }
    native.firstLaunch = false;
    if (!native.SaveToDisk(nativePath)) { return 1; }
    const auto &level = native.nativeArchive->survivalLevels[0];
    std::vector<std::uint8_t> payload;
    if (!tables.ReadSectionResource(level.packHash, GameSection::Level, level.localIndex, payload)) { return 1; }
    CArrayInputStream input(payload);
    CLevel::Template data;
    if (!data.Init(input) || input.Available() != 0) { return 1; }
    SurvivalGameContext nativeContext{native, nativePath, 0};
    nativeContext.tutorial = true;
    if (RunSurvival(bigDirectory, tables.GetPackName(data.mapRef.packHash), data.mapRef.localIndex,
        0, -1, "", 0, false, false, true, 2, 0, &nativeContext, true) != 0) { return 1; }
    if (!native.tutorialCompleted || native.tutorialSteps != 255) { return 1; }
    const auto earnedRifle = native.configuration.guns[1];
    if (!LoadNativeProfile(toc, tables, native, nativePath, nativePath / "absent-source") || native.firstLaunch ||
        !SameObject(native.configuration.guns[1], earnedRifle) || !native.Owns(6, earnedRifle)) { return 1; }
    // Tutorial steps are a host execution trace, not an invented original flag.
    std::printf("[tutorial-profile-check] native created-without-source original-HUD completed=1 steps=255 earned-rifle-restored=1\n");
    return 0;
}

int RunProfilePlayCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<WeaponEntry> weapons;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadWeaponCatalog(toc, tables, weapons)) { return 1; }
    CProfileManager profile;
    const unsigned core = toc.GetPack(toc.GetCorePackIndex())->GetPackHash();
    profile.Reset(core, refinement);
    // Only this isolated test account gets a high-damage original gun.
    GameObjectRef gun;
    gun.packHash = weapons[65].packHash;
    gun.localIndex = static_cast<std::uint8_t>(weapons[65].ordinal);
    profile.Grant(6, gun);
    profile.configuration.guns[0] = gun;
    SurvivalGameContext context{profile, "out/game-profile-check.dat", 0};
    profile.warbucks = 50;
    context.checkControls = true;
    if (RunSurvival(bigDirectory, "pack2", 7, 0, -1, "", 0, false, false, true, 2, 0, &context) != 0) { return 1; }
    const std::uint64_t firstExperience = profile.experience;
    const std::uint64_t firstXplodium = profile.xplodium;
    CProfileManager restored;
    restored.Reset(core, refinement);
    if (!restored.LoadFromDisk(context.savePath) || restored.experience == 0 || restored.xplodium == 0 ||
        restored.clearedWaves[0] != 2 || restored.configuration.guns[0].packHash != gun.packHash) { return 1; }
    SurvivalGameContext continued{restored, context.savePath, 0};
    if (RunSurvival(bigDirectory, "pack2", 7, 0, -1, "", 0, false, false, true, 2, 2, &continued) != 0) { return 1; }
    if (restored.experience <= firstExperience || restored.xplodium <= firstXplodium || restored.clearedWaves[0] != 4) { return 1; }
    std::printf("[profile-play-check] resumed=2 completed=4 xp=%llu xplodium=%llu failures=0\n",
        restored.experience, restored.xplodium);
    return 0;
}

namespace {
bool MatchesEquipmentSlot(const StoreEntry &entry, unsigned slot,
    const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armors) {
    if (slot == 5) {
        if (entry.data.objects.empty() || entry.data.type >= 14) { return false; }
        // Zero-price consumable records are reward payloads (e.g. Bro-op
        // grenade prize), not a repeatable store purchase.
        if (entry.data.commonPrice == 0 && entry.data.rarePrice == 0) { return false; }
        for (const GameObjectTypeRef &object : entry.data.objects) {
            if (object.type != 17 || !IsPlayablePowerup(object.object)) { return false; }
        }
        return true;
    }
    if (entry.data.objects.size() != 1) { return false; }
    const GameObjectTypeRef &ref = entry.data.objects[0];
    if (slot < 2) {
        if (ref.type != 6) { return false; }
        for (const WeaponEntry &weapon : weapons) {
            if (weapon.packHash == ref.object.packHash && weapon.ordinal == ref.object.localIndex) {
                return !weapon.visualOnly && weapon.hasStoreEntry;
            }
        }
    } else {
        if (ref.type != 2) { return false; }
        for (const ArmorEntry &armor : armors) {
            if (armor.packHash == ref.object.packHash && armor.ordinal == ref.object.localIndex) {
                return armor.data.GetSlot() == kArmorSlots[slot];
            }
        }
    }
    return false;
}

GameObjectRef &Equipped(CProfileManager &profile, unsigned slot) {
    if (slot < 2) { return profile.configuration.guns[slot]; }
    return profile.configuration.armor[kArmorSlots[slot]];
}

/** Shared compact/expanded store status, distinct from the preview slot. */
bool IsStoreObjectEquipped(CProfileManager &profile, unsigned slot, const GameObjectRef &object) {
    // CStoreAggregator::GetItemStatus :155316 requests IsGunEquipped(..., -1).
    if (slot < 2) { return profile.configuration.IsGunEquipped(object) >= 0; }
    return slot < 5 && SameObject(Equipped(profile, slot), object);
}

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
bool RequireRegion(GameMenu &view, unsigned movie, unsigned index, unsigned time, MovieRegion &region, const char *what) {
    if (view.movies.Region(movie, index, time, region)) { return true; }
    std::printf("[store] missing region %u of movie %u at %u ms (%s)\n", index, movie, time, what);
    return false;
}

/** SHOP_BOX regions resolved for a card whose own origin sits at (x, y). */
bool CardRegion(GameMenu &view, unsigned card, unsigned index, const StoreCardFace &face, MovieRegion &region) {
    for (const MovieRegion &candidate : view.movies.Regions(card, face.time, face.x, face.y)) {
        if (candidate.index != index) { continue; }
        region = candidate;
        return true;
    }
    return false;
}

// Every menu button prints its label at the same size; the original never
// squeezes one to fit a narrower plate, it picks a wider plate instead.

/** Centre one original label inside a plate. */
void PlateLabel(GameMenu &view, const std::string &label, float x, float y, float width, float height) {
    view.movies.Text(label, x + (width - view.movies.TextWidth(label, 5)) * 0.5f,
        y + (height - view.movies.TextHeight(5)) * 0.5f, 5, 1);
}

/** Draw one MDS_BUTTON_STORE_ITEMS plate. Its width is the width of the button
 * movie that row names, right aligned on `right`: BUY and EQUIP take the small
 * plate, UPGRADE the large one, which is why UPGRADE reaches further left. */
bool StoreItemButton(GameMenu &view, unsigned entryIndex, float right, float y, float height, bool enabled, float alpha = 1) {
    const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", entryIndex);
    if (entry == nullptr) { return false; }
    MovieRegion label;
    if (!view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, label)) {
        std::printf("[store] missing button region: %s\n", entry->movies[0]);
        return false;
    }
    const float width = label.width;
    height = label.height;
    const float x = right - width;
    const unsigned sprite = entry->sprites[0];
    view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, x, y, width, height, alpha);
    view.movies.Text(view.movies.NamedString(entry->strings[0]),
        x + (width - view.movies.TextWidth(view.movies.NamedString(entry->strings[0]), 5)) / 2,
        y + (height - view.movies.TextHeight(5)) / 2, 5, 1, 0, alpha);
    if (!enabled || !view.Hit(x, y, width, height)) { return false; }
    view.NotePress(view.movies.Ordinal(entry->movies[0]), x, y, width, height);
    return true;
}

/** Right aligned price: the original prints the currency sprite and the number,
 * never the currency's name. */
// CreateItemCostString :157573 resolves IDS_SHOP_COMMON/RARE (Omega/delta
// glyph plus %i in this BIG). The currency icons are glyphs in font 0; drawing
// an unrelated sprite at 1.3 times the row height duplicated their layout.
std::string StoreCostText(GameMenu &view, const CStoreItem &item) {
    if (item.commonPrice == 0 && item.rarePrice == 0) { return view.movies.NamedString("IDS_SHOP_FREE"); }
    std::string text = view.movies.NamedString("IDS_SHOP_COMMON");
    unsigned amount = item.commonPrice;
    if (amount == 0) {
        text = view.movies.NamedString("IDS_SHOP_RARE");
        amount = item.rarePrice;
    }
    const std::size_t placeholder = text.find("%i");
    if (placeholder == std::string::npos) {
        std::printf("[store] unsupported currency format: %s\n", text.c_str());
        return text;
    }
    text.replace(placeholder, 2, std::to_string(amount));
    return text;
}

void DrawCardPrice(GameMenu &view, const CStoreItem &item, const MovieRegion &row, float alpha) {
    const std::string text = StoreCostText(view, item);
    view.movies.Text(text, row.x + row.width - view.movies.TextWidth(text, 0), row.y, 0, 1, 0, alpha);
}

/** A bundle is owned once every object it hands over is. Only the records with
 * the single-purchase flag are treated this way. */
bool OwnsBundle(const CProfileManager &profile, const CStoreItem &item) {
    // The historical inventory approximation below does not apply to a
    // single-purchase package: CPackageOfferMgr keeps an independent key.
    if (item.singlePurchase != 0) { return profile.IsPackagePurchased(item.resource); }
    for (const GameObjectTypeRef &object : item.objects) {
        if (!profile.Owns(object.type, object.object)) { return false; }
    }
    return !item.objects.empty();
}

const WeaponEntry *FindWeaponEntry(const std::vector<WeaponEntry> &weapons, const GameObjectRef &ref) {
    for (const WeaponEntry &weapon : weapons) {
        if (weapon.packHash == ref.packHash && weapon.ordinal == ref.localIndex) { return &weapon; }
    }
    return nullptr;
}

/** Category caption under the icon, from the weapon or armour catalogue. */
// CreateItemCategoryString :157413 indexes IDS_SHOP_SORT3 + STORE.category.
std::string StoreItemKind(GameMenu &view, const StoreEntry &item) {
    if (item.data.type > 15) { return {}; }
    return view.movies.NamedString("IDS_SHOP_SORT3", item.data.type);
}

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
    const std::vector<std::pair<std::string, std::string>> &values) {
    std::string result;
    for (std::size_t position = 0; position < text.size();) {
        if (text[position] != '#') { result += text[position++]; continue; }
        std::size_t end = position + 1;
        while (end < text.size() && std::isalpha(static_cast<unsigned char>(text[end]))) { ++end; }
        const std::string key = text.substr(position + 1, end - position - 1);
        bool found = false;
        for (const auto &value : values) {
            if (value.first != key) { continue; }
            result += value.second;
            found = true;
            break;
        }
        // Keep an unresolved token visible for diagnosis instead of deleting it.
        if (!found) { result += text.substr(position, end - position); }
        position = end;
    }
    return result;
}

/** CTextBox intersects the animated region with its parent's scissor and restores
 * it after painting (:104019). Keep the belt's clip when painting folded cards. */
class StoreRegionClip {
public:
    StoreRegionClip(GameMenu &view, const MovieRegion &area) {
        enabled = glIsEnabled(GL_SCISSOR_TEST);
        glGetIntegerv(GL_SCISSOR_BOX, previous);
        view.Clip(area.x, area.y, std::max(0.0f, area.width), std::max(0.0f, area.height));
        if (enabled) {
            GLint current[4];
            glGetIntegerv(GL_SCISSOR_BOX, current);
            const int left = std::max(previous[0], current[0]);
            const int bottom = std::max(previous[1], current[1]);
            const int right = std::min(previous[0] + previous[2], current[0] + current[2]);
            const int top = std::min(previous[1] + previous[3], current[1] + current[3]);
            glScissor(left, bottom, std::max(0, right - left), std::max(0, top - bottom));
        }
    }
    ~StoreRegionClip() {
        glScissor(previous[0], previous[1], previous[2], previous[3]);
        if (!enabled) { glDisable(GL_SCISSOR_TEST); }
    }
private:
    GLboolean enabled = GL_FALSE;
    GLint previous[4]{};
};

void DrawStoreTemplate(GameMenu &view, const std::string &text, const MovieRegion &area,
    const std::vector<std::pair<std::string, std::string>> &values, bool centered = false, float layoutWidth = 0) {
    if (text.empty() || area.alpha <= 0 || area.width <= 0 || area.height <= 0) { return; }
    if (layoutWidth <= 0) { layoutWidth = area.width; }
    const auto lines = FormatStoreText(view.movies, SubstituteStoreStats(text, values), layoutWidth);
    StoreRegionClip clip(view, area);
    float y = area.y;
    for (const StoreTextLine &line : lines) {
        float x = area.x;
        if (centered) { x += (layoutWidth - line.width) / 2; }
        for (const StoreTextRun &run : line.runs) {
            // CTextBox::paint :104170 centers mixed fonts within the line height.
            view.movies.Text(run.text, x + run.x, y + (line.height - run.height) / 2, run.font, 1, 0, area.alpha);
        }
        y += line.height;
    }
}

/** CMenuDataProvider::CreateContentMovie :149246 and CMenuStoreOption::Bind
 * :181883 bind the store-specific eight-region movie, not the upgrade popup.
 * Child time is derived from CGun mastery XP and that movie's chapter lengths. */
bool StoreMasteryTarget(const CMovie &movie, const CGun::Template &weapon, unsigned experience, unsigned &target) {
    const unsigned level = weapon.GetMasteryLevel(experience);
    target = movie.duration;
    if (level < kMaxMasteryLevel) {
        unsigned lower = 0;
        if (level > 0) { lower = weapon.GetMasteryThreshold(level - 1); }
        const unsigned upper = weapon.GetMasteryThreshold(level);
        if (upper <= lower) { return false; }
        // GetElementValueInt32 :151132 scales each unfinished tier to 0..98,
        // using 99 before integer division; 100 advances the meter too far.
        const unsigned percent = static_cast<unsigned>((static_cast<std::uint64_t>(experience - lower) * 99) / (upper - lower));
        unsigned start = 0, end = 0;
        if (!movie.GetChapterRange(level + 1, start, end)) { return false; }
        target = start + percent * (end - start) / 100;
    }
    return true;
}

bool DrawMasteryMeter(GameMenu &view, const WeaponEntry &weapon, unsigned experience,
    const MovieRegion &area, unsigned elapsed) {
    if (area.alpha <= 0) { return true; }
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_MASTERY");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned target = 0;
    if (movie == nullptr || !StoreMasteryTarget(*movie, weapon.data, experience, target)) { return false; }
    // Focus resets this movie to chapter 0; Update :181490 advances it at 2x.
    const unsigned time = static_cast<unsigned>(std::min<std::uint64_t>(target, 2ull * elapsed));
    StoreRegionClip clip(view, area);
    if (!view.movies.Draw(ordinal, time, area.x, area.y, 1024, 768, 0, area.alpha)) { return false; }
    constexpr const char *captions[] = {
        "IDS_WEAPONMASTERY_STORE_TITLE", "IDS_WEAPONMASTERY_STORE_MASTERY_CRITICAL_CHANCE",
        "IDS_WEAPONMASTERY_STORE_MASTERY_CRITICAL_CHANCE_LOW",
        "IDS_WEAPONMASTERY_STORE_MASTERY_CRITICAL_CHANCE_MED",
        "IDS_WEAPONMASTERY_STORE_MASTERY_CRITICAL_CHANCE_HIGH"};
    for (const MovieRegion &region : view.movies.Regions(ordinal, time, area.x, area.y)) {
        if (region.index >= 2 && region.index <= 4) {
            // CreateContentSprite :150114, archetype 26, star animations 5/6/7.
            // The callback draws the initialized star frame; Update :181490
            // advances the child Movie, never these three CSpritePlayers.
            view.movies.DrawSprite(26, 5 + region.index - 2, 0, region.x + region.width / 2,
                region.y + region.height / 2, 1, area.alpha * region.alpha);
        } else {
            unsigned caption = region.index;
            if (region.index >= 5) { caption = region.index - 3; }
            if (caption >= 5) { return false; }
            const std::string text = view.movies.NamedString(captions[caption]);
            view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, 1)) / 2,
                region.y, 1, 1, 0, area.alpha * region.alpha);
        }
    }
    return true;
}

/** Powerups use their own child movie; row locations and visibility live in BIG.
 * Bind :182003, GameTypeCallback :180652, GameTypeCompatibilityCallback :180688. */
bool DrawPowerupCompatibility(GameMenu &view, const CStoreItem &item, const MovieRegion &area, unsigned elapsed) {
    if (area.alpha <= 0) { return true; }
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_DEATHMATCH_ONLY_POWERUPS");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    if (movie == nullptr) { return false; }
    const unsigned time = std::min(elapsed, movie->duration);
    StoreRegionClip clip(view, area);
    if (!view.movies.Draw(ordinal, time, area.x, area.y, 1024, 768, 0, area.alpha)) { return false; }
    constexpr const char *labels[] = {"IDS_MULTIPLAYER_INACTIVE", "IDS_MULTIPLAYER_ACTIVE", "IDS_MULTIPLAYER_ACTIVE_VERSUS"};
    for (const MovieRegion &region : view.movies.Regions(ordinal, time, area.x, area.y)) {
        if (region.index < 1 || region.index > 6) { continue; }
        const unsigned mode = (region.index - 1) / 2;
        if ((region.index & 1) != 0) {
            const std::string text = view.movies.NamedString(labels[mode]);
            view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, 1)) / 2,
                region.y, 1, 1, 0, area.alpha * region.alpha);
        } else {
            // CreateContentSprite :150197 uses STORE value8 exclusion bits.
            unsigned sprite = 174;
            if ((item.value8 & (1u << mode)) == 0) { sprite = 173; }
            view.movies.DrawSprite(0, sprite, elapsed, region.x + region.width / 2,
                region.y + region.height / 2, 1, area.alpha * region.alpha);
        }
    }
    return true;
}

/** The current mastery tier's values for the card templates. */
std::vector<std::pair<std::string, std::string>> StoreStatValues(const CStoreItem &item, std::size_t mastery) {
    // CStoreAggregator::SubstituteStatsInString :156816, STORE statGroups[0..7].
    constexpr const char *keys[] = {"POWER", "DMG", "RPM", "SPD", "DEF", "ATK", "COINS", "BUCKS"};
    std::vector<std::pair<std::string, std::string>> values;
    for (unsigned stat = 0; stat < 8; ++stat) {
        const auto &column = item.statGroups[stat];
        if (mastery >= column.size()) { continue; }
        const int value = column[mastery];
        std::int64_t magnitude = value;
        if (stat != 3 && magnitude < 0) { magnitude = -magnitude; }
        std::string text = std::to_string(magnitude);
        // The speed column is a percentage offset and keeps its sign.
        if (stat == 3 && value >= 0) { text = "+" + text; }
        values.push_back({keys[stat], text});
    }
    return values;
}

/** The four category tabs. Their widths come from the button movie each
 * MDS_BUTTON_STORE_CATEGORIES row names, not from measured screenshots. */
void DrawStoreCategories(GameMenu &view, const MovieRegion &bar, MenuState &state, bool interactive) {
    // The focus overlays belong to the same button movies: small 75, medium 76,
    // large 77 and extra large 78, in the same order as the movie ordinals.
    // The actual focus art is in each button's chapter 3 (:144639).
    float x = bar.x;
    for (unsigned category = 0; category < 4; ++category) {
        const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_STORE_CATEGORIES", category);
        if (entry == nullptr) { continue; }
        const unsigned plate = view.movies.Ordinal(entry->movies[0]);
        MovieRegion touch, label;
        if (!view.movies.Region(plate, 0, 0, touch) || !view.movies.Region(plate, 1, 0, label)) { continue; }
        const float width = label.width;
        const float touchX = x + touch.x - label.x;
        const float touchY = bar.y + touch.y - label.y;
        const bool selected = category == state.shopCategory;
        unsigned sprite = entry->sprites[1];
        // The bank tab keeps its green plate whether or not it is selected.
        if (selected || category == 3) { sprite = entry->sprites[0]; }
        view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, x, bar.y, width, bar.height);
        if (selected) {
            const CMovie *buttonMovie = view.movies.GetMovie(plate);
            unsigned focusStart = 0, focusEnd = 0;
            if (buttonMovie != nullptr && buttonMovie->GetChapterRange(3, focusStart, focusEnd)) {
                view.movies.DrawFitted(plate, focusEnd, x, bar.y, width, label.height, 1);
            }
        }
        PlateLabel(view, view.movies.NamedString(entry->strings[0]), x, bar.y, width, bar.height);
        if (interactive && view.Hit(touchX, touchY, touch.width, touch.height)) {
            view.NotePress(plate, x, bar.y, width, bar.height);
            {
                state.shopCategory = category;
                if (state.page == 17) { state.page = 2; }
                state.shopScroll = 0; state.shopMotion = MenuScrollMotion{};
                state.shopFilter = 0; state.shopExclusionFilter = 0;
                state.selectedItem = -1;
                state.shopDetailOpen = false;
            }
        }
        x += width + kCategoryGap;
    }
}

/** CMenuStore::InitSortButtons binds every row in the selected MDS table. */
unsigned StoreFilterRows(unsigned category, const char *&table) {
    table = "MDS_BUTTON_STORE_SORT_GUNS";
    if (category == 1) { table = "MDS_BUTTON_STORE_SORT_ARMOR"; }
    if (category == 2) { table = "MDS_BUTTON_STORE_SORT_POWERUP"; }
    if (category == 3) { table = "MDS_BUTTON_STORE_SORT_CURRENCY"; }
    unsigned count = 0;
    while (OriginalMenuData(table, count)) { ++count; }
    return count;
}

/** Non-looping SORT_BAR playback, CMenuStore::Bind :180092 and
 * HandleTouchInput :179259. Chapter 0 holds the initial closed pose; clicks
 * play chapter 1 forward or backward without restarting the current frame.
 * Bounds come from BIG ui_movie.bt / MovieChapter, never copied timestamps.
 * CMovie::Update :109097 advances milliseconds and clamps at chapter bounds. */
bool AdvanceStoreFilter(GameMenu &view, MenuState &state, const CMovie &movie) {
    unsigned closedStart = 0, closedEnd = 0, slideStart = 0, slideEnd = 0;
    if (!movie.GetChapterRange(0, closedStart, closedEnd) ||
        !movie.GetChapterRange(1, slideStart, slideEnd)) {
        std::printf("[store-filter] missing playback chapters\n");
        return false;
    }
    if (!state.shopFilterBound) {
        state.shopFilterTime = closedEnd;
        if (state.shopFilterOpen) { state.shopFilterTime = slideEnd; }
        state.shopFilterLastTick = view.clock;
        state.shopFilterBound = true;
        std::printf("[store-filter] BIG chapters closed=%u..%u slide=%u..%u\n",
            closedStart, closedEnd, slideStart, slideEnd);
        return true;
    }
    const std::uint64_t elapsed = view.clock - state.shopFilterLastTick;
    state.shopFilterLastTick = view.clock;
    if (state.shopFilterOpen) {
        state.shopFilterTime += static_cast<unsigned>(std::min<std::uint64_t>(elapsed, slideEnd - state.shopFilterTime));
    } else if (state.shopFilterTime > slideStart) {
        state.shopFilterTime -= static_cast<unsigned>(std::min<std::uint64_t>(elapsed, state.shopFilterTime - slideStart));
    }
    return true;
}

/** The original store draws two cards per column (ItemCallback :178878) on a
 * horizontal belt and expands the focused card in place. Item identity and
 * purchases still come directly from the BIG catalog. */
/** CornerCallback :180757 and CreateContentSprite :149994 display quantity
 * in a 0:87/88 badge, with the original numeric font; no handwritten OWN label. */
void DrawStoreQuantity(GameMenu &view, const CProfileManager &profile, const GameObjectTypeRef &ref,
    const MovieRegion &area, float alpha = 1) {
    if (ref.type != 17) { return; }
    const unsigned count = profile.GetPowerupCount(ref.object);
    unsigned sprite = 87;
    if (count >= 10) { sprite = 88; }
    const std::string text = std::to_string(count);
    view.movies.DrawSprite(0, sprite, 0, area.x + area.width / 2, area.y + area.height / 2, 1, alpha * area.alpha);
    view.movies.Text(text, area.x + (area.width - view.movies.TextWidth(text, 0)) / 2,
        area.y + (area.height - view.movies.TextHeight(0)) / 2, 0, 1, 0, alpha * area.alpha);
}

/** Focus/UnFocus :181356/:181402 reverse chapter 1; Update :181486 uses 4x.
 * Keep a closing card modal until its last frame, so a click cannot buy the card
 * underneath it. Bounds are read each time from CMovie, including resource edits. */
bool AdvanceStoreCard(GameMenu &view, MenuState &state, const CMovie &movie) {
    unsigned start = 0, end = 0;
    if (!movie.GetChapterRange(1, start, end)) { return false; }
    if (!state.shopDetailOpen) { return true; }
    std::uint64_t elapsed = 0;
    if (view.clock >= state.shopDetailLastTick) { elapsed = view.clock - state.shopDetailLastTick; }
    state.shopDetailLastTick = view.clock;
    const unsigned step = static_cast<unsigned>(std::min<std::uint64_t>(end - start, elapsed * kCardPlaybackRate));
    state.shopDetailTime = std::clamp(state.shopDetailTime, start, end);
    // CMenuStore::SetupFocusInterp :179101 uses 125 ms, an original code constant.
    const float movement = static_cast<float>(elapsed) / 125;
    if (state.shopDetailClosing) {
        state.shopDetailTime -= std::min(step, state.shopDetailTime - start);
        state.shopFocusAmount = std::max(0.0f, state.shopFocusAmount - movement);
        if (state.shopDetailTime == start && state.shopFocusAmount == 0) {
            state.shopDetailOpen = false;
            state.shopDetailClosing = false;
        }
    } else {
        state.shopDetailTime += std::min(step, end - state.shopDetailTime);
        state.shopFocusAmount = std::min(1.0f, state.shopFocusAmount + movement);
    }
    return true;
}

/** GetLastFailPurchaseInfo :156610; ARM 0xD25A8/0xD25F8 confirms the total
 * price and missing balance arguments omitted by the decompiler. */
bool StoreFailureText(GameMenu &view, const MenuState &state, std::string &body) {
    const char *currencyName = "IDS_SHOP_COMMON";
    if (state.failedCurrency == 1) { currencyName = "IDS_SHOP_RARE"; }
    const unsigned amounts[] = {state.failedPrice, state.failedMissing};
    for (unsigned amount : amounts) {
        std::string currency = view.movies.NamedString(currencyName);
        const auto number = currency.find("%i");
        const auto text = body.find("%s");
        if (number == std::string::npos || text == std::string::npos) { return false; }
        currency.replace(number, 2, std::to_string(amount));
        body.replace(text, 2, currency);
    }
    return body.find('%') == std::string::npos;
}

void ShowStoreFundsPrompt(MenuState &state, const std::vector<StoreEntry> &store,
    const CProfileManager &profile, unsigned currency, unsigned price, bool inGame = true) {
    state.ShowStorePrompt("MDS_STORE_PROMPT_MOMONEY", false, false);
    state.storePromptButtons = "MDS_BUTTON_STORE_INGAME_PROMPT";
    if (!inGame) { state.storePromptButtons = "MDS_BUTTON_STORE_PROMPT"; }
    state.failedCurrency = currency;
    state.failedPrice = price;
    std::uint64_t balance = profile.coins;
    if (currency == 1) { balance = profile.warbucks; }
    state.failedMissing = 0;
    if (price > balance) { state.failedMissing = static_cast<unsigned>(price - balance); }
    state.currencyOffer = FindCurrencyOffer(store, currency, state.failedMissing);
}

bool DrawOriginalMovieButton(GameMenu &view, const OriginalMenuEntry &entry, const MovieRegion &area,
    const std::string &label, unsigned font, bool interactive, bool &pressed,
    unsigned chapter = 0, unsigned elapsed = 0, unsigned timeOverride = UINT32_MAX);

/** CMenuStore::GunSwapCallback :178863; button size comes from Movie region 1. */
bool StoreGunSwapOrigin(GameMenu &view, const MovieRegion &parent, MovieRegion &origin) {
    const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_STORE_GUN_SWAP", 0);
    MovieRegion graphic;
    if (entry == nullptr || !view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, graphic)) { return false; }
    origin = parent;
    origin.x = parent.x + parent.width - graphic.width;
    origin.y = parent.y + static_cast<int>(parent.height) / 2;
    return true;
}

/** Original button chapters own appearance, press completion and category hide. */
bool DrawStoreGunSwap(GameMenu &view, MenuState &state, const MovieRegion &parent, bool interactive) {
    const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_STORE_GUN_SWAP", 0);
    if (entry == nullptr) { return false; }
    const CMovie *movie = view.movies.GetMovie(view.movies.Ordinal(entry->movies[0]));
    unsigned showStart = 0, showEnd = 0, pressStart = 0, pressEnd = 0, idleStart = 0, idleEnd = 0;
    if (movie == nullptr || !movie->GetChapterRange(0, showStart, showEnd) ||
        !movie->GetChapterRange(1, pressStart, pressEnd) || !movie->GetChapterRange(2, idleStart, idleEnd)) { return false; }
    unsigned delta = 0;
    if (state.shopSwapLastTick != 0 && view.clock >= state.shopSwapLastTick) {
        delta = static_cast<unsigned>(view.clock - state.shopSwapLastTick);
    }
    state.shopSwapLastTick = view.clock;
    // CMenuStore::RefreshCategoryContent :179165 tests category, not filter bits.
    const bool visible = state.shopCategory == 0;
    if (visible && state.shopSwapPhase == 8) {
        state.shopSwapPhase = 0;
        state.shopSwapTime = showStart;
        delta = 0;
    } else if (!visible && state.shopSwapPhase != 1 && state.shopSwapPhase != 8) {
        state.shopSwapPhase = 1;
        state.shopSwapTime = showEnd;
        delta = 0;
    } else if (visible && state.shopSwapPhase == 1) {
        state.shopSwapPhase = 0;
        state.shopSwapTime = showStart;
        delta = 0;
    }
    if (state.shopSwapPhase == 0 || state.shopSwapPhase == 4) {
        unsigned end = showEnd;
        if (state.shopSwapPhase == 4) { end = pressEnd; }
        state.shopSwapTime = std::min(end, state.shopSwapTime + delta);
        if (state.shopSwapTime == end) {
            if (state.shopSwapPhase == 4) {
                // CMenuMovieButton::Update :144755 dispatches action 92 only
                // after chapter 1 finishes; PLAYER Flow then owns the swap.
                state.shopGunSlot = 1 - view.GetPlayerPreviewSlot();
                state.shopDetailOpen = false;
                state.shopPreview = false;
            }
            state.shopSwapPhase = 2;
            state.shopSwapTime = idleStart;
        }
    } else if (state.shopSwapPhase == 1) {
        if (delta >= state.shopSwapTime - showStart) { state.shopSwapPhase = 8; }
        else { state.shopSwapTime -= delta; }
    } else if (state.shopSwapPhase == 2) {
        state.shopSwapTime = idleStart + (state.shopSwapTime - idleStart + delta) % (idleEnd - idleStart + 1);
    }
    bool keyPressed = state.shopSwapKeyRequested;
    state.shopSwapKeyRequested = false;
    if (state.shopSwapPhase == 8) { return true; }
    MovieRegion origin;
    if (!StoreGunSwapOrigin(view, parent, origin)) { return false; }
    bool pressed = false;
    const bool enabled = interactive && state.shopSwapPhase == 2 && state.shopGunSlot == view.GetPlayerPreviewSlot();
    if (!DrawOriginalMovieButton(view, *entry, origin, std::to_string(view.GetPlayerPreviewSlot() + 1), 6,
        enabled, pressed, 0, 0, state.shopSwapTime)) { return false; }
    if (enabled && (pressed || keyPressed)) {
        state.shopSwapPhase = 4;
        state.shopSwapTime = pressStart;
    }
    return true;
}

bool CompleteOfflineIAP(std::uint64_t clock, MenuState &state, CProfileManager &profile,
    const std::vector<StoreEntry> &store, const std::filesystem::path &savePath) {
    if (!state.currencyPending || clock < state.currencyReadyAt) { return true; }
    state.currencyPending = false;
    state.storePopup.Hide();
    if (state.currencyItem < 0 || state.currencyItem >= static_cast<int>(store.size())) { return false; }
    const PurchaseResult result = profile.AcquireCurrency(store[state.currencyItem].data);
    if (result == PurchaseResult::Purchased && !profile.SaveToDisk(savePath)) { return false; }
    std::printf("[offline-iap] completed result=%u\n", static_cast<unsigned>(result));
    return true;
}

/** IAP is a standard modal prompt, layout mode 1 (visual left), no buttons.
 * CMenuSystem::ShowPopup :96455 selects fonts 0/0/1/5 and GLU_MOVIE_POPUP.
 * BindContent :207403 derives its target size from fonts and sprite bounds. */
bool DrawStorePrompt(GameMenu &view, MenuState &state) {
    if (!state.storePromptRequested && !state.storePopup.IsActive()) { return true; }
    const auto *entry = OriginalMenuData(state.storePromptTable, state.storePromptIndex);
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_POPUP");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned smallStart = 0, smallEnd = 0, largeStart = 0, largeEnd = 0;
    MovieRegion compactRegion, expandedRegion, visual;
    if (entry == nullptr || movie == nullptr || !movie->GetChapterRange(1, smallStart, smallEnd) ||
        !movie->GetChapterRange(2, largeStart, largeEnd) ||
        !view.movies.Region(ordinal, 1, smallStart, compactRegion) || !view.movies.Region(ordinal, 1, largeStart, expandedRegion)) { return false; }
    const bool hasVisual = entry->sprites[0] != UINT32_MAX;
    if (hasVisual && !view.movies.SpriteBounds(entry->sprites[0] >> 16, entry->sprites[0] & 255, visual)) { return false; }
    const std::string title = view.movies.NamedString(entry->strings[1]);
    std::string body = view.movies.NamedString(entry->strings[0]);
    if (state.storePromptButtons != nullptr && !StoreFailureText(view, state, body)) { return false; }
    const float bodyHeight = view.movies.TextHeight(0);
    float titleSpace = view.movies.TextHeight(0) + bodyHeight;
    if (!state.storePromptSideVisual) { titleSpace = view.movies.TextHeight(0) + static_cast<unsigned>(bodyHeight) / 2; }
    // GetVisualContentBounds :207100 pads by the body font's even line height.
    float visualPadding = 0;
    if (hasVisual) { visualPadding = static_cast<float>(static_cast<unsigned>(bodyHeight) & ~1u); }
    const float visualWidth = visual.width + visualPadding;
    const float visualHeight = visual.height + visualPadding;
    float textWidth = compactRegion.width;
    if (state.storePromptSideVisual) { textWidth -= visualWidth; }
    const auto lines = FormatStoreText(view.movies, body, textWidth, {0, 1, 0, 0, 0});
    float textHeight = 0;
    for (const StoreTextLine &line : lines) { textHeight += line.height; }
    if (!state.storePopup.IsActive()) {
        float contentHeight = titleSpace + textHeight + visualHeight;
        if (state.storePromptSideVisual) { contentHeight = std::max(visualHeight, titleSpace + textHeight); }
        if (!state.storePopup.Bind(*movie, compactRegion.height, expandedRegion.height, contentHeight)) { return false; }
        state.storePromptRequested = false;
        state.storePopupLastTick = view.clock;
        state.storePromptSpriteTime = 0;
        std::printf("[iap-prompt] BIG target=%u compactRegion=%.0f expandedRegion=%.0f image=%.0fx%.0f body-lines=%zu\n",
            state.storePopup.TargetTime(), compactRegion.height, expandedRegion.height, visual.width, visual.height, lines.size());
    }
    unsigned delta = 0;
    if (view.clock >= state.storePopupLastTick) { delta = static_cast<unsigned>(view.clock - state.storePopupLastTick); }
    state.storePopupLastTick = view.clock;
    state.storePopup.Update(delta);
    if (!state.storePopup.IsActive()) { return true; }
    const unsigned time = state.storePopup.MovieTime();
    if (!view.movies.Draw(ordinal, time)) { return false; }
    MovieRegion area;
    if (!view.movies.Region(ordinal, 1, time, area)) { return true; }
    const float alpha = area.alpha * state.storePopup.ContentAlpha();
    {
        StoreRegionClip clip(view, area);
        view.movies.Text(title, area.x + (area.width - view.movies.TextWidth(title, 0)) / 2, area.y, 0, 1, 0, alpha);
        float y = area.y + titleSpace;
        if (!state.storePromptSideVisual) { y += visualHeight; }
        for (const StoreTextLine &line : lines) {
            float x = area.x + (area.width - line.width) / 2;
            if (state.storePromptSideVisual) { x = area.x + visualWidth; }
            for (const StoreTextRun &run : line.runs) {
                view.movies.Text(run.text, x + run.x, y + (line.height - run.height) / 2, run.font, 1, 0, alpha);
            }
            y += line.height;
        }
        if (hasVisual && time == state.storePopup.TargetTime()) {
            // Update :207249 advances the visual only after the container reaches
            // its size target; it must not inherit time spent opening the box.
            state.storePromptSpriteTime += delta;
            float visualX = area.x + visualWidth / 2;
            float visualY = area.y + area.height / 2;
            if (!state.storePromptSideVisual) {
                visualX = area.x + area.width / 2;
                visualY = area.y + titleSpace + visualHeight / 2;
            }
            if (!view.movies.DrawSprite(entry->sprites[0] >> 16, entry->sprites[0] & 255, state.storePromptSpriteTime,
                visualX, visualY, 1, alpha)) { return false; }
        }
    }
    if (state.storePromptDismiss) {
        MovieRegion dismissal;
        const auto *dismiss = OriginalMenuData("MDS_BUTTON_POPUP_PROMPT", 0);
        if (dismiss == nullptr) { return false; }
        if (!view.movies.Region(ordinal, 2, time, dismissal)) { return true; }
        const std::string text = view.movies.NamedString(dismiss->strings[1]);
        view.movies.Text(text, dismissal.x + (dismissal.width - view.movies.TextWidth(text, 5)) / 2,
            dismissal.y + dismissal.height / 2, 5, 1, 0, alpha);
        if (state.storePopup.IsReady()) {
            MovieRegion touch;
            if (!view.movies.Region(ordinal, 0, time, touch)) { return false; }
            const bool previousInput = view.inputEnabled;
            view.inputEnabled = true;
            if (view.Hit(touch.x, touch.y, touch.width, touch.height)) { state.storePopup.Hide(); }
            view.inputEnabled = previousInput;
        }
    }
    if (state.storePromptButtons != nullptr) {
        unsigned first = 1;
        if (std::strcmp(state.storePromptButtons, "MDS_BUTTON_STORE_PROMPT") == 0) { first = 0; }
        for (unsigned index = first; index <= 2; ++index) {
            MovieRegion region, buttonBounds, popupBounds;
            const auto *button = OriginalMenuData(state.storePromptButtons, index);
            if (button == nullptr) { return false; }
            if (!view.movies.Region(ordinal, index + 2, time, region)) { continue; }
            if (!view.movies.Region(view.movies.Ordinal(button->movies[0]), 0, 0, buttonBounds) ||
                !view.movies.Region(ordinal, 0, time, popupBounds)) { return false; }
            // ButtonCallback :206404 centers within the assigned region and
            // clamps to its edges if the button would cross the popup's bounds.
            MovieRegion placed = region;
            placed.x += region.width / 2 - buttonBounds.width / 2;
            if (placed.x < popupBounds.x) { placed.x = region.x; }
            else if (placed.x + buttonBounds.width > popupBounds.x + popupBounds.width) {
                placed.x = region.x + region.width - buttonBounds.width;
            }
            placed.y += region.height / 2 - buttonBounds.height / 2;
            placed.alpha *= state.storePopup.ContentAlpha();
            const bool previousInput = view.inputEnabled;
            view.inputEnabled = state.storePopup.IsReady();
            bool pressed = false;
            if (!DrawOriginalMovieButton(view, *button, placed, view.movies.NamedString(button->strings[0]), 5,
                state.storePopup.IsReady(), pressed)) { return false; }
            view.inputEnabled = previousInput;
            if (pressed) {
                if (button->action == 45) { state.storePopup.Hide(); }
                if (button->action == 70) {
                    state.storePopup.Hide();
                    std::printf("[store] Tapjoy offers unavailable on host; no reward issued\n");
                }
                if (button->action == 71 && state.currencyOffer >= 0) {
                    state.BeginOfflineIAP(state.currencyOffer, view.clock);
                }
                return true;
            }
        }
    }
    return true;
}

/** Currency entries have no object references and no cost string. The original
 * LevelCallback :180839 therefore places BUY/CONVERT in the bottom right.
 * Focus :181402 requires a cost string, so these cards do not expand. */
bool DrawCurrencyCard(GameMenu &view, CResTOCManager &toc, PackTables &tables,
    const StoreEntry &item, unsigned index, unsigned movie, const StoreCardFace &face,
    bool enabled, MenuState &state, CProfileManager &profile, const std::filesystem::path &savePath) {
    MovieRegion name, icon, kind, price;
    if (!CardRegion(view, movie, kCardNameRegion, face, name) ||
        !CardRegion(view, movie, kCardIconRegion, face, icon) ||
        !CardRegion(view, movie, kCardCategoryRegion, face, kind) ||
        !CardRegion(view, movie, kCardPriceRegion, face, price)) { return false; }
    view.movies.Draw(movie, face.time, face.x, face.y, kMenuWidth, kMenuHeight, 0, face.alpha);
    view.Icon(toc, tables, item, icon.x, icon.y, icon.width, icon.height, face.alpha * icon.alpha, true);
    view.movies.Text(item.name, name.x, name.y, 1, 1, 0, face.alpha * name.alpha);
    view.movies.Text(StoreItemKind(view, item), kind.x, kind.y, 1, 1, 0, face.alpha * kind.alpha);
    unsigned action = kBuyButtonEntry;
    if (item.data.type == 16) { action = 6; }
    const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", action);
    MovieRegion button;
    if (entry == nullptr || !view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, button)) { return false; }
    if (StoreItemButton(view, action, price.x + price.width, price.y + price.height - button.height,
        button.height, enabled, face.alpha * price.alpha)) {
        if (item.data.value32 == 1) {
            if (item.data.type == 16 && profile.coins < item.data.commonPrice) {
                // CMenuAction :94553 checks before launching the IAP conversion.
                state.ShowStorePrompt("MDS_STORE_PROMPT_CONVERSION_SOFT_TO_HARD_FAIL", false, true);
                return true;
            }
            // CMenuAction 0x38 :94519 displays IAP wait before LaunchIAP.
            // Four seconds is the user's requested Windows offline simulation,
            // not a retail payment timer. Product ID and amounts stay in BIG.
            state.BeginOfflineIAP(static_cast<int>(index), view.clock);
            std::printf("[offline-iap] pending product=%s common=%u rare=%u\n",
                ReadGameString(toc, item.data.assets[0]).c_str(), item.data.commonPrice, item.data.rarePrice);
        } else {
            const PurchaseResult result = profile.AcquireCurrency(item.data);
            if (result == PurchaseResult::InsufficientWarbucks) {
                state.message.clear();
                state.ShowStorePrompt("MDS_STORE_PROMPT_MOMONEY_CONV", false, true);
            }
            if (result == PurchaseResult::Purchased && !profile.SaveToDisk(savePath)) { return false; }
        }
    }
    return true;
}

/** CStoreAggregator::EquipItem :156082 and SetGun/SetArmor :171658.
 * Granting inventory and equipping it are separate original menu actions.
 */
bool EquipStoreItem(CProfileManager &profile, const CStoreItem &item, const std::vector<ArmorEntry> &armors) {
    CPlayerConfiguration configuration = profile.configuration;
    unsigned gunCount = 0;
    for (const GameObjectTypeRef &object : item.objects) {
        if (object.type == 6 && gunCount < 2) {
            const unsigned slot = (profile.activeWeaponSlot + gunCount) & 1;
            ++gunCount;
            configuration.SetGun(slot, object.object);
        } else if (object.type == 2) {
            bool alreadyEquipped = false;
            for (const GameObjectRef &part : configuration.armor) {
                if (SameObject(part, object.object)) { alreadyEquipped = true; }
            }
            if (alreadyEquipped) { continue; }
            const ArmorEntry *part = nullptr;
            for (const ArmorEntry &entry : armors) {
                if (entry.packHash == object.object.packHash && entry.ordinal == object.object.localIndex) { part = &entry; break; }
            }
            if (part == nullptr || part->data.GetSlot() >= configuration.armor.size()) {
                std::printf("[store] Cannot equip armor pack=%u ordinal=%u\n", object.object.packHash, object.object.localIndex);
                return false;
            }
            configuration.armor[part->data.GetSlot()] = object.object;
        }
    }
    profile.configuration = configuration;
    return true;
}

bool DrawStore(GameMenu &view, CResTOCManager &toc, PackTables &tables, CProfileManager &profile,
    unsigned level, const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::vector<ArmorEntry> &armors, MenuState &state, const std::filesystem::path &savePath) {
    const unsigned storeMenu = view.movies.Ordinal("GLU_MOVIE_STORE_MENU");
    const unsigned storeScroll = view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL");
    const unsigned shopBox = view.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
    const unsigned sortBar = view.movies.Ordinal("GLU_MOVIE_SORT_BAR");
    const CMovie *cardMovie = view.movies.GetMovie(shopBox);
    unsigned cardStart = 0, cardEnd = 0, openStart = 0, openEnd = 0;
    if (cardMovie == nullptr || !cardMovie->GetChapterRange(1, cardStart, cardEnd) ||
        !cardMovie->GetChapterRange(2, openStart, openEnd) || !AdvanceStoreCard(view, state, *cardMovie)) { return false; }
    const CMovie *filterMovie = view.movies.GetMovie(sortBar);
    if (filterMovie == nullptr || !AdvanceStoreFilter(view, state, *filterMovie)) { return false; }
    MovieRegion content, categoryBar, playerPanel, gunSwap;
    if (!RequireRegion(view, storeMenu, kStoreContentRegion, 0, content, "content") ||
        !RequireRegion(view, storeMenu, kStoreCategoryRegion, 0, categoryBar, "categories") ||
        !RequireRegion(view, storeMenu, kStorePlayerRegion, 0, playerPanel, "player") ||
        !RequireRegion(view, storeMenu, kStoreGunSwapRegion, 0, gunSwap, "gun swap")) { return false; }
    const bool modalOpen = state.shopDetailOpen || state.shopFilterOpen || state.currencyPending ||
        state.storePromptRequested || state.storePopup.IsActive() || state.promotion.IsActive();
    DrawStoreCategories(view, categoryBar, state, !modalOpen);

    std::vector<unsigned> items;
    std::vector<unsigned> itemSlots;
    // The first column links to the local friend and currency flows, on every
    // category page. The starter bundle follows as the store's own first row.
    if (state.shopFilter == 0) {
        items.push_back(static_cast<unsigned>(store.size())); itemSlots.push_back(6);
        items.push_back(static_cast<unsigned>(store.size() + 1)); itemSlots.push_back(6);
        for (unsigned index = 0; index < store.size(); ++index) {
            if (state.shopCategory != 0 || store[index].data.singlePurchase == 0 ||
                profile.IsPackageHidden(store[index].ref)) { continue; }
            items.push_back(index);
            itemSlots.push_back(6);
            break;
        }
    }
    // CStoreItem's trailing int16 is the store's own row order; a negative value
    // keeps the record out of the list entirely.
    // Correction: OverrideItem :233074 makes owned negative-order gear visible.
    std::vector<std::pair<int, unsigned>> ordered;
    for (unsigned index = 0; index < store.size(); ++index) {
        const int order = GetStoreDisplayOrder(store[index].data, profile);
        if (order < 0 || store[index].data.value242 == 1 || store[index].data.singlePurchase != 0) { continue; }
        ordered.push_back({order, index});
    }
    std::sort(ordered.begin(), ordered.end());
    for (const std::pair<int, unsigned> &row : ordered) {
        const unsigned index = row.second;
        unsigned slot = state.shopGunSlot;
        bool matches = false;
        if (state.shopCategory == 0) {
            matches = MatchesEquipmentSlot(store[index], slot, weapons, armors);
        } else if (state.shopCategory == 1) {
            for (slot = 2; slot < 5; ++slot) {
                if (MatchesEquipmentSlot(store[index], slot, weapons, armors)) { matches = true; break; }
            }
        } else if (state.shopCategory == 3) {
            // CStoreAggregator ctor :158975 gives bank mask 0x1C000.
            slot = 7;
            matches = store[index].data.type >= 14 && store[index].data.type <= 16;
        } else {
            slot = 5;
            matches = MatchesEquipmentSlot(store[index], slot, weapons, armors);
        }
        if (!matches) { continue; }
        // InitFilteredList :159135 uses STORE.type, never the model category.
        const unsigned category = store[index].data.type;
        if ((state.shopExclusionFilter & store[index].data.value8) != 0) { continue; }
        const unsigned categoryFilter = state.shopFilter & ~kOwnedFilterBit;
        if (categoryFilter != 0 && (categoryFilter & (1u << category)) == 0) { continue; }
        if ((state.shopFilter & kOwnedFilterBit) != 0) {
            const GameObjectTypeRef &ref = store[index].data.objects[0];
            if (!profile.Owns(ref.type, ref.object)) { continue; }
        }
        items.push_back(index);
        itemSlots.push_back(slot);
    }
    const unsigned columns = static_cast<unsigned>((items.size() + 1) / 2);

    // The belt scrolls by whole columns; drag and wheel move the same pixel
    // offset and it settles back onto a column once the button is released.
    MovieRegion firstSlot, secondSlot, viewport;
    if (!RequireRegion(view, storeScroll, kFirstColumnRegion, view.storeRestTime, firstSlot, "first column") ||
        !RequireRegion(view, storeScroll, kFirstColumnRegion + 1, view.storeRestTime, secondSlot, "second column") ||
        !RequireRegion(view, storeScroll, 0, view.storeRestTime, viewport, "belt viewport")) { return false; }
    const float columnPitch = std::max(1.0f, secondSlot.x - firstSlot.x);
    const float maximumScroll = std::max(0.0f, (columns - 1.0f) * columnPitch);
    unsigned scrollStart = 0, scrollEnd = 0, nextScrollStart = 0, nextScrollEnd = 0;
    const CMovie *scrollMovie = view.movies.GetMovie(storeScroll);
    if (scrollMovie == nullptr || !scrollMovie->GetChapterRange(1, scrollStart, scrollEnd) ||
        !scrollMovie->GetChapterRange(2, nextScrollStart, nextScrollEnd)) { return false; }
    view.Scroll(state.shopMotion, state.shopScroll, viewport, !modalOpen, maximumScroll, columnPitch, nextScrollStart - scrollStart);

    int purchaseIndex = -1;
    unsigned purchaseSlot = 0;
    int focusedColumn = -1, focusedRow = 0;
    const unsigned firstColumn = static_cast<unsigned>(std::max(0.0f, std::floor(state.shopScroll / columnPitch)));
    // CMenuStore::ItemCallback :178878 places both rows inside the scroll
    // control. Its input must cover the same authored viewport as drawing.
    const bool listHover = view.MouseIn(content.x, viewport.y, content.width, viewport.height);
    // The belt has its own viewport; the content region alone cuts the second row.
    // STORE_SCROLL region 0 is the control's input rectangle, not a vertical
    // drawing clip. CMenuStore::ItemCallback :178878 and CMovieRegion::Draw
    // :109978 allow corner sprites outside it. Clip only horizontally; the
    // host framebuffer supplies the vertical boundary, just as for CMovie.
    view.Clip(content.x, 0, content.width, kMenuHeight);
    for (const auto &slot : view.movies.Regions(storeScroll, view.storeRestTime)) {
        if (slot.index < kFirstColumnRegion) { continue; }
        const unsigned column = firstColumn + slot.index - kFirstColumnRegion;
        if (column >= columns) { continue; }
        const float slotX = firstSlot.x + column * columnPitch - state.shopScroll;
        for (unsigned row = 0; row < 2; ++row) {
            const unsigned position = column * 2 + row;
            if (position >= items.size()) { break; }
            const unsigned index = items[position];
            const unsigned slotKind = itemSlots[position];
            if (state.shopDetailOpen && state.selectedItem == static_cast<int>(index)) {
                focusedColumn = static_cast<int>(column);
                focusedRow = static_cast<int>(row);
                continue;
            }
            StoreCardFace face;
            face.x = slotX;
            // ItemCallback stacks the second card at half the slot height plus five.
            face.y = slot.y + row * (slot.height / 2 + 5);
            face.alpha = slot.alpha;
            MovieRegion body;
            if (!CardRegion(view, shopBox, kCardBodyRegion, face, body)) { continue; }
            const bool cardEnabled = !modalOpen && listHover;
            if (index >= store.size()) {
                // The invite friends card art, then the free Warbucks entry.
                view.movies.Draw(shopBox, face.time, face.x, face.y, 1024, 768, 0, face.alpha);
                unsigned promoSprite = kInviteCard;
                if (index != store.size()) { promoSprite = kFreeWarbucksCard; }
                // CMenuTapjoyOption::Draw :221948 uses the sprite origin and
                // bounds, without scaling it to SHOP_BOX's user rectangle.
                view.movies.DrawSprite(5, promoSprite, 0, face.x, face.y);
                if (index != store.size()) {
                    // The money pile is art only; the original prints the words.
                    MovieRegion bounds;
                    if (!view.movies.SpriteBounds(5, promoSprite, bounds)) { view.EndClip(); return false; }
                    const float center = face.x + bounds.x + bounds.width / 2;
                    view.CenterText(kFreeCardTop, center, face.y + bounds.y, 6, 1);
                    view.CenterText(kFreeCardBottom, center, face.y + bounds.y + bounds.height - view.movies.TextHeight(6), 6, 1);
                }
                if (cardEnabled && view.Hit(body.x, body.y, body.width, body.height)) {
                    unsigned action = 125;
                    if (index != store.size()) { action = 130; }
                    if (!state.promotion.Activate(view.movies, action)) { view.EndClip(); return false; }
                    state.promotionTick = view.clock;
                    view.EndClip();
                    return true;
                }
                continue;
            }
            const StoreEntry &item = store[index];
            if (item.data.type >= 14 && item.data.type <= 16) {
                if (!DrawCurrencyCard(view, toc, tables, item, index, shopBox, face,
                    cardEnabled, state, profile, savePath)) { return false; }
                continue;
            }
            const GameObjectTypeRef &ref = item.data.objects[0];
            MovieRegion name, icon, kind, price, right;
            if (!CardRegion(view, shopBox, kCardNameRegion, face, name) ||
                !CardRegion(view, shopBox, kCardIconRegion, face, icon) ||
                !CardRegion(view, shopBox, kCardCategoryRegion, face, kind) ||
                !CardRegion(view, shopBox, kCardPriceRegion, face, price) ||
                !CardRegion(view, shopBox, kCardRightRegion, face, right)) { continue; }
            view.movies.Rectangle(body.x, body.y, body.width, body.height, 0, 0, 0, face.alpha);
            view.movies.Draw(shopBox, face.time, face.x, face.y, 1024, 768, 0, face.alpha);
            // The icon sits between the name and the category row, as on the
            // original card; region 5 alone would run under the title.
            // Correction: ThumbCallback :181036 uses region 5 and original PNG size.
            view.Icon(toc, tables, item, icon.x, icon.y, icon.width, icon.height, face.alpha * icon.alpha, true);
            view.movies.Text(item.name, name.x, name.y, 1, 1, 0, face.alpha * name.alpha);
            // LevelCallback :180799 uses region 7 for the category and price.
            view.movies.Text(StoreItemKind(view, item), price.x, price.y, 1, 1, 0, face.alpha * price.alpha);
            bool owned = profile.Owns(ref.type, ref.object);
            // A single-purchase bundle counts as owned once every part of it is.
            if (item.data.singlePurchase != 0) { owned = OwnsBundle(profile, item.data); }
            // GetItemStatus :155316 excludes consumables from the OWNED state.
            if (ref.type == 17) { owned = false; }
            bool equipped = false;
            if (slotKind < 5) { equipped = IsStoreObjectEquipped(profile, slotKind, ref.object); }
            MovieRegion stamp;
            if ((owned || equipped) && CardRegion(view, shopBox, kCardStampRegion, face, stamp)) {
                unsigned stampSprite = kOwnedStamp;
                if (equipped) { stampSprite = kEquippedStamp; }
                // ThumbCallback draws the ownership overlay centered at region 5.
                view.movies.DrawSprite(5, stampSprite, 0, icon.x + icon.width / 2,
                    icon.y + icon.height / 2, 1, face.alpha * icon.alpha);
            }
            if (!owned || slotKind == 5) { DrawCardPrice(view, item.data, price, face.alpha); }
            // The folded card prints the record's own power template. Bundles
            // leave it empty and put their promo line in the stat template.
            const WeaponEntry *cardWeapon = FindWeaponEntry(weapons, ref.object);
            unsigned cardMastery = 0;
            if (cardWeapon != nullptr) { cardMastery = cardWeapon->data.GetMasteryLevel(profile.GetWeaponExperience(ref.object)); }
            std::string rightTemplate = ReadGameString(toc, item.data.assets[5]);
            if (rightTemplate.empty()) { rightTemplate = ReadGameString(toc, item.data.assets[4]); }
            if (!rightTemplate.empty()) {
                MovieRegion templateArea = right;
                templateArea.alpha *= face.alpha;
                DrawStoreTemplate(view, rightTemplate, templateArea,
                    StoreStatValues(item.data, cardMastery), !ReadGameString(toc, item.data.assets[5]).empty());
            }
            MovieRegion quantity;
            if (CardRegion(view, shopBox, kCardBadgeRegion, face, quantity)) {
                DrawStoreQuantity(view, profile, ref, quantity, face.alpha);
            }
            // The corner region carries the bronze/silver/gold mastery badge.
            if (slotKind < 2 && cardMastery > 0) {
                MovieRegion badge;
                if (CardRegion(view, shopBox, kCardBadgeRegion, face, badge)) {
                    // CornerCallback :180786 + CreateContentSprite :150159 use 26:24..26.
                    view.movies.DrawSprite(26, 24 + cardMastery - 1, 0,
                        badge.x + badge.width / 2, badge.y + badge.height / 2, 1, face.alpha * badge.alpha);
                }
            }
            unsigned action = kBuyButtonEntry;
            if (owned && slotKind < 5) { action = kEquipButtonEntry; }
            const bool soldOut = item.data.singlePurchase != 0 && owned;
            // Only guns carry a mastery meter, and a mastered one has nothing
            // left to buy, so it keeps the plain EQUIP plate.
            if (equipped && slotKind < 2 && cardMastery < kMaxMasteryLevel) { action = kUpgradeButtonEntry; }
            const OriginalMenuEntry *actionEntry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", action);
            MovieRegion actionLabel;
            if (actionEntry == nullptr || !view.movies.Region(view.movies.Ordinal(actionEntry->movies[0]), 1, 0, actionLabel)) { return false; }
            MovieRegion buttonArea = right;
            // An owned item's cost string is empty; LevelCallback :180839 puts
            // its button on region 7. PropertiesCallback :181022 centers smaller
            // buttons, and right-aligns wider ones, inside region 4.
            if (owned && slotKind < 5) { buttonArea = price; }
            float buttonRight = buttonArea.x + buttonArea.width;
            if ((!owned || slotKind == 5) && actionLabel.width <= buttonArea.width) {
                buttonRight = buttonArea.x + (buttonArea.width + actionLabel.width) / 2;
            }
            if (!soldOut && StoreItemButton(view, action, buttonRight,
                buttonArea.y + buttonArea.height - actionLabel.height, actionLabel.height, cardEnabled, face.alpha)) {
                if (action == kUpgradeButtonEntry) {
                    state.masteryWeapon = ref.object;
                    state.Navigate(26);
                } else {
                    purchaseIndex = static_cast<int>(index);
                    purchaseSlot = slotKind;
                }
            } else if (cardEnabled && view.Hit(body.x, body.y, body.width, body.height)) {
                state.selectedItem = static_cast<int>(index);
                state.slot = slotKind;
                state.shopDetailOpen = true;
                state.shopPreview = false;
                state.shopDetailStart = view.clock;
                state.shopDetailLastTick = view.clock;
                state.shopDetailTime = cardStart;
                state.shopDetailClosing = false;
                state.shopFocusAmount = 0;
            }
        }
    }
    view.EndClip();
    // The belt's own gradient fades the far column out behind the player.
    view.movies.Draw(storeScroll, view.storeRestTime);

    const GameObjectTypeRef *preview = nullptr;
    unsigned previewSlot = state.shopGunSlot;
    if (state.shopPreview && state.selectedItem >= 0 && state.selectedItem < static_cast<int>(store.size()) &&
        state.slot < 5) {
        preview = &store[state.selectedItem].data.objects[0];
        previewSlot = state.slot;
    }
    // The original lets the player turn the model by dragging it.
    unsigned rotationDelta = 0;
    if (view.clock >= state.playerMeshLastTick) { rotationDelta = static_cast<unsigned>(view.clock - state.playerMeshLastTick); }
    state.playerMeshLastTick = view.clock;
    view.UpdateMeshRotation(state.playerMesh, rotationDelta, playerPanel, !modalOpen);
    if (!view.DrawEquippedPlayer(toc, tables, profile, weapons, armors, previewSlot, preview, &playerPanel,
        state.playerMesh.GetRadians())) { return false; }
    if (view.TakePlayerPreviewSlotChange()) {
        profile.activeWeaponSlot = view.GetPlayerPreviewSlot();
        if (!profile.SaveToDisk(savePath)) { return false; }
    }
    // MDS_BUTTON_STORE_GUN_SWAP is the round weapon slot toggle.
    if (!DrawStoreGunSwap(view, state, gunSwap, !modalOpen)) { return false; }

    // The FILTER button and its drop-down both live in GLU_MOVIE_SORT_BAR.
    const unsigned sortTime = state.shopFilterTime;
    MovieRegion sortButton, sortLabel;
    if (!RequireRegion(view, sortBar, kSortButtonRegion, sortTime, sortButton, "filter button") ||
        !RequireRegion(view, sortBar, kSortLabelRegion, sortTime, sortLabel, "filter label")) { return false; }
    view.movies.Draw(sortBar, sortTime);
    // SortLabelCallback :178777 uses font 5 at the label region's top and
    // centres its measured width. The touch rectangle is not a text layout.
    const std::string filterLabel = view.movies.NamedString("IDS_SHOP_FILTER");
    view.movies.Text(filterLabel, sortLabel.x + (sortLabel.width - view.movies.TextWidth(filterLabel, 5)) * 0.5f,
        sortLabel.y, 5, 1, 0, sortLabel.alpha);
    if (!state.shopDetailOpen && !state.currencyPending && view.Hit(sortButton.x, sortButton.y, sortButton.width, sortButton.height)) {
        state.shopFilterOpen = !state.shopFilterOpen;
    }
    // The original region callback keeps drawing while the movie reverses.
    // Only open-state buttons accept input (CMenuStore::Update :179558).
    MovieRegion sortPanel;
    if (view.movies.Region(sortBar, kSortPanelRegion, sortTime, sortPanel)) {
        const char *table = nullptr;
        const unsigned rows = StoreFilterRows(state.shopCategory, table);
        float optionY = sortPanel.y;
        for (unsigned row = 0; row < rows; ++row) {
            const OriginalMenuEntry *entry = OriginalMenuData(table, row);
            if (entry == nullptr) { return false; }
            // MDS is extracted from the original executable; it selects each
            // button movie, sprite and string. The movie supplies its geometry.
            const unsigned optionPlate = view.movies.Ordinal(entry->movies[0]);
            MovieRegion optionLabel, optionTouch;
            if (!RequireRegion(view, optionPlate, 1, 0, optionLabel, "filter option label") ||
                !RequireRegion(view, optionPlate, 0, 0, optionTouch, "filter option touch")) { return false; }
            const float optionX = sortPanel.x + (sortPanel.width - optionLabel.width) * 0.5f;
            const float y = optionY;
            optionY += optionLabel.height * kSortRowSpacing;
            unsigned bit = 0;
            unsigned mask = state.shopFilter;
            if (entry->action == 66) {
                bit = 1u << (entry->parameter - 1);
                mask = state.shopExclusionFilter;
            } else if (entry->parameter != 17) { bit = 1u << entry->parameter; }
            bool selected = mask == 0;
            if (bit != 0) { selected = (mask & bit) != 0; }
            unsigned sprite = entry->sprites[1];
            if (selected) { sprite = entry->sprites[0]; }
            view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, optionX, y, optionLabel.width, optionLabel.height);
            PlateLabel(view, view.movies.NamedString(entry->strings[0]), optionX, y, optionLabel.width, optionLabel.height);
            const float touchX = optionX + optionTouch.x - optionLabel.x;
            const float touchY = y + optionTouch.y - optionLabel.y;
            if (!state.shopFilterOpen || !view.Hit(touchX, touchY, optionTouch.width, optionTouch.height)) { continue; }
            view.NotePress(optionPlate, optionX, y, optionLabel.width, optionLabel.height);
            if (entry->action == 66) { state.shopExclusionFilter ^= bit; }
            else if (bit == 0) { state.shopFilter = 0; }
            else { state.shopFilter ^= bit; }
            state.shopScroll = 0; state.shopMotion = MenuScrollMotion{};
        }
    }

    if (state.shopDetailOpen && state.selectedItem >= 0 && state.selectedItem < static_cast<int>(store.size())) {
        const StoreEntry &item = store[state.selectedItem];
        const GameObjectTypeRef &ref = item.data.objects[0];
        StoreCardFace face;
        const unsigned elapsed = static_cast<unsigned>(view.clock - state.shopDetailStart);
        face.time = state.shopDetailTime;
        MovieRegion body, foldedBody;
        if (!CardRegion(view, shopBox, kCardBodyRegion, face, body) ||
            !view.movies.Region(shopBox, kCardBodyRegion, cardStart, foldedBody)) { return false; }
        // The card grows out of its own place on the belt and stays in the list.
        float grownX = content.x, grownY = content.y;
        if (focusedColumn >= 0) {
            MovieRegion slot;
            if (view.movies.Region(storeScroll, kFirstColumnRegion, view.storeRestTime, slot)) {
                grownX = firstSlot.x + focusedColumn * columnPitch - state.shopScroll;
                grownY = slot.y + focusedRow * (slot.height / 2 + 5);
            }
        }
        // CMenuStore::Init :180297 gets the focus center from STORE_MENU region 0:
        // centerX = x + width/2 - width/16, centerY = y + height/2.
        const float targetX = content.x + content.width / 2 - static_cast<int>(content.width) / 16;
        const float targetY = content.y + content.height / 2;
        const float centerX = grownX + foldedBody.width / 2;
        const float centerY = grownY + foldedBody.height / 2;
        face.x = centerX + (targetX - centerX) * state.shopFocusAmount - body.width / 2;
        face.y = centerY + (targetY - centerY) * state.shopFocusAmount - body.height / 2;
        if (!CardRegion(view, shopBox, kCardBodyRegion, face, body)) { return false; }
        // The old implementation used the fully open rectangle for every frame.
        // Bind :181843 only uses chapter 2 to FORMAT text; callbacks paint at the
        // current region, including its alpha and clipping, throughout expansion.
        StoreCardFace open = face;
        open.time = openStart;
        MovieRegion finalDescription, finalStats;
        if (!CardRegion(view, shopBox, kCardDescriptionRegion, open, finalDescription) ||
            !CardRegion(view, shopBox, kCardRightRegion, StoreCardFace{face.x, face.y, 1, cardStart}, finalStats)) { return false; }
        view.movies.Rectangle(body.x, body.y, body.width, body.height, 0, 0, 0, body.alpha);
        view.movies.Draw(shopBox, face.time, face.x, face.y);
        const WeaponEntry *weapon = FindWeaponEntry(weapons, ref.object);
        unsigned mastery = 0;
        if (weapon != nullptr) { mastery = weapon->data.GetMasteryLevel(profile.GetWeaponExperience(ref.object)); }
        const auto values = StoreStatValues(item.data, mastery);
        bool owned = profile.Owns(ref.type, ref.object);
        if (item.data.singlePurchase != 0) { owned = OwnsBundle(profile, item.data); }
        if (ref.type == 17) { owned = false; }
        bool equipped = false;
        if (state.slot < 5) { equipped = IsStoreObjectEquipped(profile, state.slot, ref.object); }
        MovieRegion region;
        if (CardRegion(view, shopBox, kCardIconRegion, face, region)) {
            view.Icon(toc, tables, item, region.x, region.y, region.width, region.height, region.alpha, true);
            if (owned || equipped) {
                unsigned sprite = kOwnedStamp;
                if (equipped) { sprite = kEquippedStamp; }
                view.movies.DrawSprite(5, sprite, 0, region.x + region.width / 2, region.y + region.height / 2, 1, region.alpha);
            }
        }
        if (CardRegion(view, shopBox, kCardBadgeRegion, face, region)) {
            DrawStoreQuantity(view, profile, ref, region);
        }
        if (CardRegion(view, shopBox, kCardNameRegion, face, region)) {
            view.movies.Text(item.name, region.x, region.y, 1, 1, 0, region.alpha);
        }
        if (CardRegion(view, shopBox, kCardPriceRegion, face, region)) {
            view.movies.Text(StoreItemKind(view, item), region.x, region.y, 1, 1, 0, region.alpha);
            if (!owned || state.slot == 5) { DrawCardPrice(view, item.data, region, region.alpha); }
        }
        if (CardRegion(view, shopBox, kCardRightRegion, face, region)) {
            std::string properties = ReadGameString(toc, item.data.assets[5]);
            const bool centered = !properties.empty();
            if (properties.empty()) { properties = ReadGameString(toc, item.data.assets[4]); }
            DrawStoreTemplate(view, properties, region, values, centered, finalStats.width);
        }
        if (CardRegion(view, shopBox, kCardStatsRegion, face, region)) {
            // ARMOR uses precisely the same STORE text as GUNS; do not invent a
            // DEFENSE/ATTACK/SPEED/XP/XPLODIUM list from equipment script values.
            DrawStoreTemplate(view, ReadGameString(toc, item.data.assets[4]), region, values, false, finalStats.width);
        }
        if (CardRegion(view, shopBox, kCardUpgradeRegion, face, region)) {
            if (ref.type == 6 && weapon != nullptr &&
                !DrawMasteryMeter(view, *weapon, profile.GetWeaponExperience(ref.object), region, elapsed)) { return false; }
            if (ref.type == 17 && !DrawPowerupCompatibility(view, item.data, region, elapsed)) { return false; }
        }
        if (CardRegion(view, shopBox, kCardDescriptionRegion, face, region)) {
            DrawStoreTemplate(view, ReadGameString(toc, item.data.assets[3]), region, values, false, finalDescription.width);
        }
        if (CardRegion(view, shopBox, kCardActionRegion, face, region) && region.alpha > 0) {
            const bool interactive = !state.shopDetailClosing && state.shopDetailTime == cardEnd;
            const OriginalMenuEntry *previewEntry = OriginalMenuData("MDS_BUTTON_STORE_PREVIEW", 0);
            float previewWidth = 0;
            if (previewEntry != nullptr && !owned && state.slot < 5) {
                MovieRegion preview;
                if (!view.movies.Region(view.movies.Ordinal(previewEntry->movies[0]), 1, 0, preview)) { return false; }
                previewWidth = preview.width;
                const unsigned sprite = previewEntry->sprites[0];
                view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, region.x, region.y, preview.width, preview.height, region.alpha);
                const std::string label = view.movies.NamedString(previewEntry->strings[0]);
                view.movies.Text(label, region.x + (preview.width - view.movies.TextWidth(label, 5)) / 2,
                    region.y + (preview.height - view.movies.TextHeight(5)) / 2, 5, 1, 0, region.alpha);
                if (interactive && view.Hit(region.x, region.y, preview.width, preview.height)) { state.shopPreview = !state.shopPreview; }
            }
            unsigned action = kBuyButtonEntry;
            if (owned && state.slot < 5) { action = kEquipButtonEntry; }
            if (equipped && weapon != nullptr && mastery < kMaxMasteryLevel) { action = kUpgradeButtonEntry; }
            const bool soldOut = item.data.singlePurchase != 0 && owned;
            if (!soldOut && StoreItemButton(view, action, region.x + region.width, region.y, region.height, interactive, region.alpha)) {
                if (action == kUpgradeButtonEntry) {
                    state.masteryWeapon = ref.object;
                    state.shopDetailOpen = false;
                    state.Navigate(26);
                } else {
                    purchaseIndex = state.selectedItem;
                    purchaseSlot = state.slot;
                }
            }
            // PurchaseInfoCallback :180849 reserves both button widths before
            // centering the purchase hint. The level text comes from BIG.
            const OriginalMenuEntry *actionEntry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", action);
            MovieRegion actionLabel;
            if (actionEntry == nullptr || !view.movies.Region(view.movies.Ordinal(actionEntry->movies[0]), 1, 0, actionLabel)) { return false; }
            const std::string requirement = view.movies.NamedString("IDS_SHOP_LEVEL") + " " + std::to_string(item.data.requiredLevel);
            const float middleX = region.x + previewWidth;
            const float middleWidth = region.width - previewWidth - actionLabel.width;
            view.movies.Text(requirement, middleX + (middleWidth - view.movies.TextWidth(requirement, 1)) / 2,
                region.y + (region.height - view.movies.TextHeight(1)) / 2, 1, 1, 0, region.alpha);
        }
        // Anything outside the expanded card folds it again, like the original.
        // HandleTouchInput :181298 also unfocuses on the card body; button clicks
        // are consumed first. Reverse the current chapter instead of disappearing.
        if (view.Hit(0, 0, 1024, 768) && !state.shopDetailClosing) {
            state.shopDetailClosing = true;
            state.shopPreview = false;
            state.shopDetailLastTick = view.clock;
        }
    }
    if (purchaseIndex >= 0) {
        const StoreEntry &item = store[purchaseIndex];
        const PurchaseResult result = profile.AcquireItem(item.data, level);
        // CMenuAction::DoAction 0x38 :94606 uses the original three-button
        // funds prompt. Successful purchases refresh the card without a toast.
        if (result == PurchaseResult::InsufficientCoins) {
            ShowStoreFundsPrompt(state, store, profile, 0, item.data.commonPrice, false);
        } else if (result == PurchaseResult::InsufficientWarbucks) {
            ShowStoreFundsPrompt(state, store, profile, 1, item.data.rarePrice, false);
        }
        if (result == PurchaseResult::Purchased || result == PurchaseResult::Owned) {
            if (purchaseSlot < 2) { profile.configuration.SetGun(purchaseSlot, item.data.objects[0].object); }
            else if (purchaseSlot < 5) { Equipped(profile, purchaseSlot) = item.data.objects[0].object; }
            if (item.data.singlePurchase != 0 && result == PurchaseResult::Purchased) {
                if (!EquipStoreItem(profile, item.data, armors)) { return false; }
                state.shopPreview = false;
            }
            if (!profile.SaveToDisk(savePath)) { return false; }
        }
    }
    return true;
}

/** The original mode medallions are MDS_BUTTON_MP_TOGGLE, archetype 8. */

class ModeOverlayCallbacks : public IMovieRegionCallback {
public:
    ModeOverlayCallbacks(GameMenu &menu, MenuState &state, float otherAlpha) : view(menu), state(state), otherAlpha(otherAlpha) {}
    bool DrawMovieRegion(const MovieRegion &region) override {
        if (region.index > 5) { return true; }
        const unsigned mode = region.index / 2;
        const bool selected = state.modeSelected && mode == state.gameMode;
        if (!selected && state.modePhase == 2) { return true; }
        float alpha = region.alpha;
        if (!selected) { alpha *= otherAlpha; }
        const auto *entry = OriginalMenuData("MDS_BUTTON_MP_TOGGLE", mode);
        if (entry == nullptr) { return false; }
        if (region.index % 2 == 1) {
            return view.movies.DrawSprite(entry->sprites[0] >> 16, entry->sprites[0] & 255, state.modeSpriteTime,
                region.x + region.width / 2, region.y + region.height / 2, 1, alpha);
        }
        const std::string label = view.movies.NamedString(entry->strings[0]);
        if (selected) { view.DrawModeEffects(region); }
        return view.movies.Text(label, region.x + (region.width - view.movies.TextWidth(label, 0)) / 2,
            region.y + (region.height - view.movies.TextHeight(0)) / 2, 0, 1, 0, alpha);
    }
    GameMenu &view;
    MenuState &state;
    float otherAlpha;
};

/** CMenuMovieMultiplayerOverlay :250020..250880, region callbacks 0..5,
 * font 0 and MDS_BUTTON_MP_TOGGLE. No locally fabricated online mode. */
bool DrawOriginalModeOverlay(GameMenu &view, MenuState &state) {
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_MULTIPLAYER_AND_VERSUS_MAP");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned openStart = 0, openEnd = 0, foldStart = 0, foldEnd = 0, unfoldStart = 0, unfoldEnd = 0;
    unsigned idleStart = 0, idleEnd = 0;
    if (movie == nullptr || !movie->GetChapterRange(0, openStart, openEnd) ||
        !movie->GetChapterRange(2, foldStart, foldEnd) || !movie->GetChapterRange(3, unfoldStart, unfoldEnd) ||
        !movie->GetChapterRange(4, idleStart, idleEnd)) { return false; }
    if (!state.modeBound) {
        state.modeBound = true;
        state.modeLastTick = view.clock;
        state.modeTime = openStart;
        state.modePhase = 0;
        if (!view.animateNavigation) { state.modeTime = openEnd; }
        if (state.modeSelected) { state.modePhase = 2; state.modeTime = idleStart; }
    }
    if (state.modeLastTick == 0) { state.modeLastTick = view.clock; }
    const unsigned elapsed = static_cast<unsigned>(view.clock - state.modeLastTick);
    state.modeLastTick = view.clock;
    state.modeSpriteTime += elapsed;
    if (!view.AdvanceModeEffects(elapsed)) { return false; }
    if (state.modePhase == 0) { state.modeTime = std::min(openEnd, state.modeTime + elapsed); }
    if (state.modePhase == 1) {
        state.modeTime = std::min(idleEnd, state.modeTime + elapsed);
        if (state.modeTime == idleEnd) { state.modePhase = 2; }
    } else if (state.modePhase == 2) { state.modeTime = idleStart + (state.modeTime - idleStart + elapsed) % (idleEnd - idleStart + 1); }
    else if (state.modePhase == 3) {
        state.modeTime -= std::min(elapsed, state.modeTime - foldStart);
        if (state.modeTime == foldStart) { state.modePhase = 0; }
    }
    float otherAlpha = 1;
    if (state.modePhase == 1 || state.modePhase == 3) {
        otherAlpha = std::max(0.0f, 1.0f - 2.0f * (state.modeTime - foldStart) / (foldEnd - foldStart));
    }
    ModeOverlayCallbacks callback(view, state, otherAlpha);
    if (!view.movies.Draw(ordinal, state.modeTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callback)) { return false; }
    if (state.modePhase == 1 || state.modePhase == 3 || (state.modePhase == 0 && state.modeTime < openEnd)) { return true; }
    for (const auto &region : view.movies.Regions(ordinal, state.modeTime)) {
        if (region.index > 5 || region.index % 2 != 1) { continue; }
        const unsigned mode = region.index / 2;
        if (state.modePhase == 2 && mode != state.gameMode) { continue; }
        if (!view.Hit(region.x, region.y, region.width, region.height)) { continue; }
        if (state.modePhase == 2) {
            state.modeTime = unfoldStart;
            state.modePhase = 3;
        } else if (mode == 0) {
            state.gameMode = mode;
            state.modeSelected = true;
            state.modeTime = foldStart;
            state.modePhase = 1;
            if (!view.StartModeSelectionEffect()) { return false; }
            if (state.page == 22) { state.Navigate(0, true); }
        } else {
            // SetSelection :250261 uses table 189/2 when multiplayer service
            // availability (provider 82) is false, before changing game type.
            state.ShowStorePrompt("MDS_PROMPT_MP_UNAVAILABLE", false, true, 2);
            std::printf("[mode] unavailable original prompt; game type unchanged\n");
        }
    }
    return true;
}

/** Paint dynamic planets at their original Movie layer, and retain native bounds. */
class StarPlanetCallbacks : public IMovieRegionCallback {
public:
    StarPlanetCallbacks(GameMenu &menu, MenuState &state) : view(menu), state(state) {}
    bool DrawMovieRegion(const MovieRegion &region) override {
        if (region.index == 0) { return true; }
        int index = view.PlanetForMapSlot(region.index);
        if (index < 0) { index = view.PlanetForMapSlot(0); }
        if (index < 0) { return false; }
        const float fade = std::min(1.0f, state.starFadeTime / 350.0f); // OnShow :162368.
        bounds.push_back(view.DrawPlanetThumb(index, region, fade));
        return true;
    }
    GameMenu &view;
    MenuState &state;
    std::vector<MovieRegion> bounds;
};

/** Original UpdatePosition :161040 advances along the dominant axis. */
void MoveStarPoint(float &x, float &y, float targetX, float targetY, unsigned elapsed, float speed) {
    const float dx = targetX - x, dy = targetY - y;
    const float distance = std::max(std::abs(dx), std::abs(dy));
    if (distance == 0) { return; }
    const float step = std::min(1.0f, speed * elapsed / 1000 / distance);
    x += dx * step;
    y += dy * step;
}

/** CMenuMission :161015..163482, MENU_MISSION_ROOT VA 0x402d38.
 * The map is a bounded Movie timeline, not host coordinates or depth factors. */
bool DrawOriginalStarMap(GameMenu &view, MenuState &state, const CProfileManager &profile) {
    const unsigned mapOrdinal = view.movies.Ordinal("GLU_MOVIE_MAP_PARALAX_COPY");
    const unsigned reticleOrdinal = view.movies.Ordinal("GLU_MOVIE_MAP_RETICLE");
    const unsigned flagOrdinal = view.movies.Ordinal("GLU_MOVIE_PLANET_FLAG");
    const CMovie *map = view.movies.GetMovie(mapOrdinal);
    const CMovie *reticle = view.movies.GetMovie(reticleOrdinal);
    const CMovie *flag = view.movies.GetMovie(flagOrdinal);
    unsigned reticleStart = 0, reticleEnd = 0, closeStart = 0, closeEnd = 0, flagStart = 0, flagEnd = 0;
    if (map == nullptr || reticle == nullptr || flag == nullptr ||
        !reticle->GetChapterRange(1, reticleStart, reticleEnd) || !reticle->GetChapterRange(2, closeStart, closeEnd) ||
        !flag->GetChapterRange(1, flagStart, flagEnd)) { return false; }
    if (!state.starBound) {
        state.starBound = true;
        state.starLastTick = view.clock;
        state.starFadeTime = 0;
        if (!view.animateNavigation) { state.starFadeTime = 350; }
        // SetSelectedIndex :162296 queues the first slot for the end of OnShow.
        if (state.starSelectedSlot < 1) {
            state.starSelectedSlot = 1;
            state.starLocked = false;
            unsigned chapterStart = 0, chapterEnd = 0;
            if (!map->GetChapterRange(0, chapterStart, chapterEnd)) { return false; }
            state.starTargetTime = chapterEnd;
        }
    }
    const unsigned elapsed = static_cast<unsigned>(view.clock - state.starLastTick);
    state.starLastTick = view.clock;
    state.starFadeTime = std::min(350u, state.starFadeTime + elapsed);
    MovieRegion viewport;
    if (!view.movies.Region(mapOrdinal, 0, state.starTime, viewport)) { return false; }
    bool interactive = state.page == 0 && state.modeSelected && state.modePhase == 2 && !state.starEntering && state.starFadeTime == 350;
    MovieRegion modeButton;
    if (view.movies.Region(view.movies.Ordinal("GLU_MOVIE_MULTIPLAYER_AND_VERSUS_MAP"), state.gameMode * 2 + 1, state.modeTime, modeButton) &&
        view.MouseIn(modeButton.x, modeButton.y, modeButton.width, modeButton.height)) { interactive = false; }
    if (interactive && view.MouseIn(viewport.x, viewport.y, viewport.width, viewport.height)) {
        const float wheel = view.window.TakeWheelDelta();
        if (view.dragX != 0 && elapsed != 0) {
            // Update :162434 caps drag speed at 2; 600 is the native px/sec divisor.
            state.starSpeed = std::min(2.0f, std::abs(view.dragX) / (elapsed * 0.001f) / 600);
            state.starReverse = view.dragX > 0;
            state.starTargetTime = -1;
        } else if (wheel != 0) {
            // Windows wheel selects an adjacent authored chapter boundary.
            unsigned chapter = 0;
            while (chapter + 1 < map->chapters.size() && map->chapters[chapter + 1] <= state.starTime) { ++chapter; }
            if (wheel < 0 && chapter + 1 < map->chapters.size()) { ++chapter; }
            if (wheel > 0 && chapter > 0) { --chapter; }
            state.starTargetTime = map->chapters[chapter];
        }
    }
    if (state.starTargetTime >= 0) {
        const unsigned movement = static_cast<unsigned>(elapsed * 0.75f); // Original state 2.
        const unsigned target = static_cast<unsigned>(state.starTargetTime);
        if (state.starTime < target) { state.starTime = std::min(target, state.starTime + movement); }
        else { state.starTime -= std::min(movement, state.starTime - target); }
        if (state.starTime == target) { state.starTargetTime = -1; state.starSpeed = 0; }
    } else {
        if (view.dragX == 0) {
            const float seconds = elapsed * 0.001f;
            state.starSpeed = std::max(0.0f, state.starSpeed - seconds * seconds * 125); // :162520, -dt^2/2*250.
        }
        const unsigned movement = static_cast<unsigned>(state.starSpeed * elapsed);
        if (state.starReverse) { state.starTime -= std::min(state.starTime, movement); }
        else { state.starTime = std::min(map->duration, state.starTime + movement); }
    }
    StarPlanetCallbacks callbacks(view, state);
    if (!view.movies.Draw(mapOrdinal, state.starTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callbacks)) { return false; }
    // Original hit testing scans slot order, independent of each sprite's layer.
    for (unsigned slot = 1; slot <= map->chapters.size(); ++slot) {
        for (const auto &area : callbacks.bounds) {
            if (area.index != slot) { continue; }
            if (interactive && view.MouseIn(viewport.x, viewport.y, viewport.width, viewport.height) &&
                view.Hit(area.x, area.y, area.width, area.height)) {
                if (state.starSelectedSlot == static_cast<int>(slot) && state.starLocked) {
                    state.starEntering = true;
                    state.starReticleTime = closeStart;
                    state.starSpeed = 0;
                } else {
                    state.starSelectedSlot = slot;
                    state.starLocked = false;
                    state.starFlagTime = 0;
                    unsigned chapterStart = 0, chapterEnd = 0;
                    if (!map->GetChapterRange(slot - 1, chapterStart, chapterEnd)) { return false; }
                    state.starTargetTime = chapterEnd;
                }
            }
            if (state.starSelectedSlot != static_cast<int>(slot)) { continue; }
            const float centerX = area.x + area.width / 2, centerY = area.y + area.height / 2;
            if (state.starLocked) { state.starSelectorX = centerX; state.starSelectorY = centerY; }
            else {
                MoveStarPoint(state.starSelectorX, state.starSelectorY, centerX, centerY, elapsed, kMenuWidth / 480 * 750);
                if (state.starSelectorX == centerX && state.starSelectorY == centerY) { state.starLocked = true; }
            }
        }
    }
    if (state.starSelectedSlot < 1) { return true; }
    if (state.starEntering) { state.starReticleTime = std::min(closeEnd, state.starReticleTime + elapsed); }
    else {
        state.starReticleTime += elapsed;
        if (state.starReticleTime > reticleEnd) { state.starReticleTime = reticleStart + (state.starReticleTime - reticleStart) % (reticleEnd - reticleStart + 1); }
    }
    if (!view.movies.Draw(reticleOrdinal, state.starReticleTime, state.starSelectorX, state.starSelectorY)) { return false; }
    // CrossHairsCallback :161422 draws screen-spanning native one-pixel lines.
    for (const auto &region : view.movies.Regions(reticleOrdinal, state.starReticleTime, state.starSelectorX, state.starSelectorY)) {
        if (region.index != 0) { continue; }
        view.movies.Rectangle(region.x + region.width / 2, 0, 1, kMenuHeight, 124.0f / 255, 201.0f / 255, 243.0f / 255, region.alpha * 0.5f);
        view.movies.Rectangle(0, region.y + region.height / 2, kMenuWidth, 1, 124.0f / 255, 201.0f / 255, 243.0f / 255, region.alpha * 0.5f);
    }
    int selected = view.PlanetForMapSlot(state.starSelectedSlot);
    if (selected < 0) { selected = view.PlanetForMapSlot(0); }
    if (selected < 0) { return false; }
    if (state.starLocked && !state.starEntering && state.starSelectorX >= 0 && state.starSelectorX <= kMenuWidth) {
        MovieRegion flagBounds, reticleBounds;
        if (!view.movies.Region(flagOrdinal, 0, flagEnd, flagBounds) ||
            !view.movies.Region(reticleOrdinal, 1, state.starReticleTime, reticleBounds)) { return false; }
        float offsetX = (reticleBounds.width + flagBounds.width) / 2;
        float offsetY = (reticleBounds.height + flagBounds.height) / 2;
        if (state.starSelectorX > viewport.x + viewport.width / 2) { offsetX = -offsetX; }
        if (state.starSelectorY > viewport.y + viewport.height / 2) { offsetY = -offsetY; }
        MoveStarPoint(state.starFlagX, state.starFlagY, offsetX, offsetY, elapsed, kMenuWidth / 480 * 800);
        state.starFlagTime = std::min(flagEnd, state.starFlagTime + elapsed);
        if (!view.movies.Draw(flagOrdinal, state.starFlagTime, state.starSelectorX + state.starFlagX, state.starSelectorY + state.starFlagY)) { return false; }
        for (const auto &region : view.movies.Regions(flagOrdinal, state.starFlagTime, state.starSelectorX + state.starFlagX, state.starSelectorY + state.starFlagY)) {
            if (region.index == 0) { view.PlanetFlagLines(state.starSelectorX, state.starSelectorY, region); }
            CPlayerProgress playerProgress;
            playerProgress.Bind(profile.nativeArchive->progression);
            playerProgress.SetExperience(profile.experience);
            const unsigned requiredLevel = view.planetEntries[selected].data.requiredLevel;
            const bool locked = requiredLevel > playerProgress.GetLevel();
            unsigned titleRegion = 1;
            if (locked) { titleRegion = 2; }
            if (region.index != titleRegion && !(locked && region.index == 3)) { continue; }
            std::string label = view.names[selected];
            if (region.index == 3) {
                // Planet::CreateRequirementString :170158, ARM 0xe9fd8 reads mem+82.
                const std::string format = view.movies.NamedString("IDS_PLANET_REQUIRED_LVL");
                char text[512]{};
                std::snprintf(text, sizeof(text), format.c_str(), requiredLevel);
                label = text;
            }
            MovieRegion textArea = region;
            textArea.x += (textArea.width - flagBounds.width) / 2;
            textArea.y += (textArea.height - flagBounds.height) / 2;
            textArea.width = flagBounds.width;
            textArea.height = flagBounds.height;
            const auto lines = FormatStoreText(view.movies, label, textArea.width, {1, 2, 2, 2, 2});
            float height = 0;
            for (const auto &line : lines) { height += line.height; }
            float y = textArea.y + (textArea.height - height) / 2;
            StoreRegionClip clip(view, textArea);
            for (const auto &line : lines) {
                for (const auto &run : line.runs) { view.movies.Text(run.text, textArea.x + (textArea.width - line.width) / 2 + run.x, y, run.font, 1, 0, region.alpha); }
                y += line.height;
            }
        }
    }
    if (state.starEntering && state.starReticleTime == closeEnd) {
        state.starEntering = false;
        state.planet = selected;
        state.missionScroll = 0;
        state.startingWave = -1;
        if (!view.planetEntries[selected].missions.empty()) { state.Navigate(21); }
        else { std::printf("[planet-menu] slot=%u has no mission entries\n", state.starSelectedSlot); }
        state.starReticleTime = 0;
    }
    return true;
}

/** CMissionWaveStatus collection 1003; the four live retail projections may
 * contain progress that has not reached the next disk checkpoint yet. */
unsigned NativeMissionProgress(const CProfileManager &profile, const GameObjectRef &level) {
    for (unsigned slot = 0; slot < profile.nativeArchive->survivalLevels.size(); ++slot) {
        if (SameObject(level, profile.nativeArchive->survivalLevels[slot])) { return profile.clearedWaves[slot]; }
    }
    const auto &bytes = profile.nativeArchive->records[3].payload;
    CArrayInputStream input(bytes);
    const unsigned count = input.ReadUInt32();
    for (unsigned index = 0; index < count; ++index) {
        const unsigned hash = input.ReadUInt32(), ordinal = input.ReadUInt8(), type = input.ReadUInt8();
        input.Skip(2);
        const unsigned progress = input.ReadUInt16();
        input.Skip(514);
        if (hash == level.packHash && ordinal == level.localIndex && type == 7) { return progress; }
    }
    return 0; // Original collection GetWaveProgress returns zero for absent keys.
}

bool OriginalMissionLocked(const CProfileManager &profile, const Mission &mission, const PlanetMissionInfo &info) {
    if (!mission.script.IsPresent()) { return false; }
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    if (info.requiredLevel > static_cast<int>(progress.GetLevel())) { return true; }
    for (const auto &requirement : info.prerequisites) {
        if (requirement.type == 7 && (mission.type == 1 || mission.type == 2) &&
            NativeMissionProgress(profile, requirement.object) < mission.value64) { return true; }
        if (requirement.type == 22) {
            // Mission::IsLocked checks original purchase collection type 22.
            CArrayInputStream input(profile.nativeArchive->records[2].payload);
            const unsigned count = input.ReadUInt32();
            bool found = false;
            for (unsigned index = 0; index < count; ++index) {
                const unsigned hash = input.ReadUInt32(), ordinal = input.ReadUInt8(), type = input.ReadUInt8();
                input.Skip(2);
                const unsigned quantity = input.ReadUInt8();
                input.Skip(1);
                if (type == 22 && hash == requirement.object.packHash && ordinal == requirement.object.localIndex && quantity != 0) { found = true; }
            }
            if (!found) { return true; }
        }
    }
    return false;
}

void DrawMissionText(GameMenu &view, const MovieRegion &region, const std::string &text, unsigned font, bool centered = false) {
    const auto lines = FormatStoreText(view.movies, text, region.width, {font, font, font, font, font});
    float y = region.y;
    StoreRegionClip clip(view, region);
    for (const auto &line : lines) {
        float x = region.x;
        if (centered) { x += (region.width - line.width) / 2; }
        for (const auto &run : line.runs) { view.movies.Text(run.text, x + run.x, y, run.font, 1, 0, region.alpha); }
        y += line.height;
    }
}

class MissionInfoCallbacks : public IMovieRegionCallback {
public:
    MissionInfoCallbacks(GameMenu &menu, MenuState &menuState) : view(menu), state(menuState) {}
    bool DrawMovieRegion(const MovieRegion &region) override {
        if (region.index == 0) {
            view.DrawPlanetOriginal(state.planet, region);
            // PlanetImageCallback :188957 draws shared movie 3 after the planet.
            // OnShow starts at zero, then loops chapter 1 (CMenuSystem::Init :97428).
            const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_RADIAL_WIDGET");
            const CMovie *radial = view.movies.GetMovie(ordinal);
            unsigned start = 0, end = 0;
            if (radial == nullptr || !radial->GetChapterRange(1, start, end)) { return false; }
            unsigned time = state.missionTime;
            if (time > end) { time = start + (time - start) % (end - start + 1); }
            return view.movies.Draw(ordinal, time, region.x + region.width / 2, region.y + region.height / 2,
                kMenuWidth, kMenuHeight, 0, region.alpha);
        }
        if (region.index != 1) { return true; }
        view.movies.Rectangle(region.x, region.y, region.width, region.height, 0, 0, 0, region.alpha * 0.5f);
        const float lineHeight = view.movies.TextHeight(0);
        const auto lines = FormatStoreText(view.movies, view.descriptions[state.planet], region.width, {0, 0, 0, 0, 0});
        float bodyHeight = 0;
        for (const auto &line : lines) { bodyHeight += line.height; }
        const float y = region.y + (region.height - bodyHeight - lineHeight * 2) / 2;
        view.movies.Text(view.names[state.planet], region.x + (region.width - view.movies.TextWidth(view.names[state.planet], 0)) / 2,
            y, 0, 1, 0, region.alpha);
        MovieRegion body = region;
        body.y = y + lineHeight * 2;
        body.height = bodyHeight;
        DrawMissionText(view, body, view.descriptions[state.planet], 0, true);
        return true;
    }
    GameMenu &view;
    MenuState &state;
};

/** A page transition is the authored chapter 1; chapter 2 is the next page's
 * matching pose. The wheel is a Windows adapter for one native page gesture.
 * Historical note above described the former one-page adapter. Continuous
 * control now maps arbitrary positions onto those same authored poses.
 */
/** Map continuous scroll onto the original repeating Movie chapter. */
bool ScrollMissionMovie(GameMenu &view, MenuScrollMotion &motion, float &position,
    const CMovie &movie, const MovieRegion &viewport, float stride, unsigned maximum,
    bool enabled, unsigned &page, unsigned &time, bool &moving) {
    unsigned start = 0, end = 0, next = 0, nextEnd = 0;
    if (!movie.GetChapterRange(1, start, end) || !movie.GetChapterRange(2, next, nextEnd) || stride <= 0 || next <= start) { return false; }
    // Saved progress and research jumps choose a whole option at rest.
    if (!motion.captured && motion.velocity == 0 && time == start && page != static_cast<unsigned>(position / stride)) {
        position = page * stride;
    }
    const bool opening = time < start;
    view.Scroll(motion, position, viewport, enabled && !opening, maximum * stride, stride, next - start);
    if (opening) { moving = false; return true; }
    page = static_cast<unsigned>(position / stride);
    time = start + static_cast<unsigned>((position / stride - page) * (next - start));
    moving = motion.captured || motion.velocity != 0;
    return true;
}

/** CMenuMissionOption::WaveSelectCallback :189920, invoked in Movie65 layers. */
class MissionWaveCallbacks : public IMovieRegionCallback {
public:
    MissionWaveCallbacks(GameMenu &menu, MenuState &menuState, const CProfileManager &account,
        unsigned missionIndex, bool allowTouch, bool &launch) : view(menu), state(menuState), profile(account), index(missionIndex),
        touchEnabled(allowTouch), launched(launch) {}
    bool DrawMovieRegion(const MovieRegion &region) override {
        if (region.index == 0) { return true; }
        const auto &planet = view.planetEntries[state.planet];
        const auto &mission = planet.missions[index];
        const auto &info = planet.missionInfo[index];
        const unsigned page = state.wavePage + region.index - 1;
        const unsigned progress = NativeMissionProgress(profile, mission.level);
        const bool locked = OriginalMissionLocked(profile, mission, info);
        MovieRegion tabGraphic, scrollbar;
        const auto *tab = OriginalMenuData("MDS_BUTTON_MISSION_INFO", mission.type);
        if (tab == nullptr || !view.movies.Region(view.movies.Ordinal(tab->movies[0]), 1, 0, tabGraphic) ||
            !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_SCROLLBAR_HORIZ"), 0, 0, scrollbar)) { return false; }
        // The original reads first-tab height at mem+202, not wave-button height.
        const unsigned rowStep = static_cast<unsigned>(region.height + tabGraphic.height - scrollbar.height) / 3;
        for (unsigned cell = 0; cell < 10; ++cell) {
            const unsigned localWave = page * 10 + cell;
            if (localWave >= info.waveCount) { break; }
            const unsigned wave = mission.value64 + localWave;
            unsigned status = 0;
            if (!locked && wave <= progress) {
                status = 1;
                if (state.planet < profile.perfectedWaves.size() && wave < profile.perfectedWaves[state.planet].size() &&
                    profile.perfectedWaves[state.planet].test(wave)) { status = 2; }
            }
            const auto *entry = OriginalMenuData("MDS_BUTTON_MISSION_WAVE", status);
            if (entry == nullptr) { return false; }
            MovieRegion position = region;
            position.x += (cell % 5 + 1) * std::floor(region.width / 6);
            position.y += rowStep * (cell / 5 + 1) - tabGraphic.height;
            std::string label;
            if (status != 0) { label = std::to_string(localWave + 1); }
            bool pressed = false;
            if (!DrawOriginalMovieButton(view, *entry, position, label, 6,
                touchEnabled && status != 0 && !state.missionWaveMoving && state.modePhase == 2, pressed)) { return false; }
            if (pressed) {
                state.selectedMission = planet.data.missions[index];
                state.startingWave = wave;
                state.revolution = index;
                launched = true;
            }
        }
        return true;
    }
    GameMenu &view;
    MenuState &state;
    const CProfileManager &profile;
    unsigned index;
    bool touchEnabled;
    bool &launched;
};

class MissionCardCallbacks : public IMovieRegionCallback {
public:
    MissionCardCallbacks(GameMenu &menu, MenuState &menuState, const CProfileManager &account,
        unsigned missionIndex, bool hasFocus, bool &launch) : view(menu), state(menuState), profile(account),
        index(missionIndex), focused(hasFocus), launched(launch) {}
    bool DrawMovieRegion(const MovieRegion &region) override {
        const auto &planet = view.planetEntries[state.planet];
        const auto &mission = planet.missions[index];
        const auto &info = planet.missionInfo[index];
        const bool locked = OriginalMissionLocked(profile, mission, info);
        if (region.index == 5) { view.movies.Text(info.title, region.x, region.y, 0, 1, 0, region.alpha); }
        if (region.index == 4 && (mission.type == 1 || mission.type == 2)) {
            unsigned animation = 24 + mission.value66; // Provider25 sprite3 :149869.
            if (mission.type == 2) { animation = 42 + mission.value66; }
            view.movies.DrawSprite(5, animation, state.missionTime, region.x + region.width / 2, region.y + region.height / 2, 1, region.alpha);
            if (locked) { view.movies.DrawSprite(5, 19, state.missionTime, region.x + region.width / 2, region.y + region.height / 2, 1, region.alpha); }
        }
        // CornerCallback is BX LR in the original (VA 0x10f26c).
        if (!focused || state.missionClosing) { return true; }
        if (region.index == 7 && region.alpha > 0) {
            unsigned tabs[3] = {mission.type, 3, 4};
            MovieRegion buttonAreas[3];
            float total = 8; // ButtonCallback :189858 includes two native 4px gaps.
            for (unsigned tab = 0; tab < 3; ++tab) {
                const auto *entry = OriginalMenuData("MDS_BUTTON_MISSION_INFO", tabs[tab]);
                if (entry == nullptr || !view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, buttonAreas[tab])) { return false; }
                total += buttonAreas[tab].width;
            }
            float x = region.x + (region.width - total) / 2;
            for (unsigned tab = 0; tab < 3; ++tab) {
                OriginalMenuEntry entry = *OriginalMenuData("MDS_BUTTON_MISSION_INFO", tabs[tab]);
                if (state.missionTab != tab && entry.sprites[1] != 0) { entry.sprites[0] = entry.sprites[1]; }
                MovieRegion position = region;
                position.x = x;
                bool pressed = false;
                // CMenuOptionGroup::Init :175044 assigns font 5 to slot 1.
                unsigned chapter = 2;
                if (state.missionTab == tab) { chapter = 3; }
                if (!DrawOriginalMovieButton(view, entry, position, view.movies.NamedString(entry.strings[0]), 5,
                    true, pressed, chapter, state.missionTime)) { return false; }
                if (pressed) { state.missionTab = tab; }
                x += buttonAreas[tab].width + 4;
            }
        }
        if (region.index != 8 || region.alpha == 0) { return true; }
        if (state.missionTab == 1 || state.missionTab == 2) {
            MovieRegion body = region;
            body.y += view.movies.TextHeight(1); // DetailCallback :190033.
            body.height -= view.movies.TextHeight(1);
            if (state.missionTab == 1) { DrawMissionText(view, body, info.description, 0); }
            else { DrawMissionText(view, body, info.requirements, 0); }
            return true;
        }
        if (mission.type == 2) {
            const unsigned animation = 42 + mission.value66;
            MovieRegion sprite;
            if (!view.movies.SpriteBounds(5, animation, sprite)) { return false; }
            view.movies.DrawSprite(5, animation, state.missionTime, region.x + sprite.width / 2,
                region.y + region.height / 2, 1, region.alpha);
            MovieRegion body = region;
            body.x += sprite.width;
            body.width -= sprite.width;
            body.y += (region.height - sprite.height) / 2;
            DrawMissionText(view, body, info.overview, 0);
            // CMissionHighScore::GetHighScore reads collection 1016, keyed by Mission.
            unsigned highScore = 0;
            CArrayInputStream scores(profile.nativeArchive->records[16].payload);
            const unsigned count = scores.ReadUInt32();
            for (unsigned item = 0; item < count; ++item) {
                const unsigned hash = scores.ReadUInt32(), ordinal = scores.ReadUInt8(), type = scores.ReadUInt8();
                scores.Skip(4); // Serialized key is two bytes wider than mem+0.
                const unsigned score = scores.ReadUInt32();
                const auto &ref = planet.data.missions[index];
                if (hash == ref.packHash && ordinal == ref.localIndex && type == 9) { highScore = score; }
            }
            const std::string label = view.movies.NamedString("IDS_MISSION_HIGH_SCORE") + std::to_string(highScore);
            view.movies.Text(label, region.x + (region.width - view.movies.TextWidth(label, 0)) / 2,
                region.y + view.movies.TextHeight(0) / 2, 0, 1, 0, region.alpha);
            if (!locked) {
                const auto *play = OriginalMenuData("MDS_BUTTON_PLAY", 0);
                MovieRegion graphic;
                if (play == nullptr || !view.movies.Region(view.movies.Ordinal(play->movies[0]), 1, 0, graphic)) { return false; }
                MovieRegion position = region;
                position.x = body.x + (body.width - graphic.width) / 2;
                position.y = region.y + region.height - graphic.height;
                bool pressed = false;
                if (!DrawOriginalMovieButton(view, *play, position, view.movies.NamedString(play->strings[0]), 6, true, pressed)) { return false; }
                if (pressed) { state.hordeStart = index; state.selectedMission = planet.data.missions[index]; launched = true; }
            }
            return true;
        }
        if (mission.type != 1 || info.waveCount == 0) { return false; }
        const unsigned pageCount = (info.waveCount + 9) / 10;
        const unsigned waveOrdinal = view.movies.Ordinal("GLU_MOVIE_WAVE_SELECT");
        const CMovie *waveMovie = view.movies.GetMovie(waveOrdinal);
        if (waveMovie == nullptr) { return false; }
        unsigned waveStart = 0, waveEnd = 0;
        MovieRegion waveFirst, waveNext;
        if (!waveMovie->GetChapterRange(1, waveStart, waveEnd) ||
            !view.movies.Region(waveOrdinal, 1, waveStart, waveFirst) ||
            !view.movies.Region(waveOrdinal, 2, waveStart, waveNext)) { return false; }
        if (!ScrollMissionMovie(view, state.waveMotion, state.wavePosition, *waveMovie, region,
            waveNext.x - waveFirst.x, pageCount - 1, focused && !state.missionClosing && state.modePhase == 2,
            state.wavePage, state.missionWaveTime, state.missionWaveMoving)) { return false; }
        {
            StoreRegionClip clip(view, region);
            MissionWaveCallbacks callbacks(view, state, profile, index, view.MouseIn(region.x, region.y, region.width, region.height), launched);
            if (!view.movies.Draw(waveOrdinal, state.missionWaveTime, region.x, region.y, kMenuWidth, kMenuHeight,
                0, region.alpha, &callbacks)) { return false; }
        }
        MovieRegion scrollbar;
        if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_SCROLLBAR_HORIZ"), 0, 0, scrollbar)) { return false; }
        // Native scrollbar :190079 receives GetOptionProgress over pageCount.
        const unsigned scrollOrdinal = view.movies.Ordinal("GLU_MOVIE_SCROLLBAR_HORIZ");
        const CMovie *bar = view.movies.GetMovie(scrollOrdinal);
        if (bar == nullptr) { return false; }
        unsigned barTime = 0;
        if (pageCount > 1) { barTime = static_cast<unsigned>(bar->duration * state.wavePosition / ((waveNext.x - waveFirst.x) * (pageCount - 1))); }
        return view.movies.Draw(scrollOrdinal, barTime, region.x + (region.width - scrollbar.width) / 2, region.y + region.height - 4);
    }
    GameMenu &view;
    MenuState &state;
    const CProfileManager &profile;
    unsigned index;
    bool focused;
    bool &launched;
};

class MissionListCallbacks : public IMovieRegionCallback {
public:
    MissionListCallbacks(GameMenu &menu, MenuState &menuState, const CProfileManager &account, bool &launch)
        : view(menu), state(menuState), profile(account), launched(launch) {}
    bool DrawMovieRegion(const MovieRegion &region) override {
        if (region.index == 0) { return true; }
        const unsigned index = state.missionFirst + region.index - 1;
        if (index >= view.planetEntries[state.planet].missions.size() || state.missionFocused == static_cast<int>(index)) { return true; }
        MissionCardCallbacks card(view, state, profile, index, false, launched);
        const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_MISSION_BOX");
        const CMovie *movie = view.movies.GetMovie(ordinal);
        unsigned start = 0, end = 0;
        if (movie == nullptr || !movie->GetChapterRange(0, start, end) ||
            !view.movies.Draw(ordinal, std::min(state.missionTime, end), region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &card)) { return false; }
        MovieRegion viewport;
        if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_MISSION_LIST"), 0, state.missionListTime, viewport)) { return false; }
        if (state.missionFocused < 0 && !state.missionListMoving && state.modePhase == 2 &&
            view.MouseIn(viewport.x, viewport.y, viewport.width, viewport.height) && view.Hit(region.x, region.y, region.width, region.height)) {
            state.missionFocused = index;
            state.missionClosing = false;
            state.missionCardTime = end;
            state.missionFocusTime = 0;
            state.missionFocusX = region.x + region.width / 2;
            state.missionFocusY = region.y + region.height / 2;
            state.missionTab = 0;
            state.missionScroll = 0;
            state.missionWaveTime = 0;
            state.missionWaveMoving = false;
            state.waveMotion = MenuScrollMotion{};
            state.wavePosition = 0;
            const auto &mission = view.planetEntries[state.planet].missions[index];
            const unsigned progress = NativeMissionProgress(profile, mission.level);
            state.wavePage = 0;
            if (mission.type == 1 && progress > mission.value64) { state.wavePage = (progress - mission.value64) / 10; }
            const unsigned count = view.planetEntries[state.planet].missionInfo[index].waveCount;
            if (count != 0) { state.wavePage = std::min(state.wavePage, (count - 1) / 10); }
        }
        return true;
    }
    GameMenu &view;
    MenuState &state;
    const CProfileManager &profile;
    bool &launched;
};

/** MENU_MISSION_DETAIL: main48/list49/box50; original callbacks and references. */
bool DrawOriginalMissionInfo(GameMenu &view, MenuState &state, const CProfileManager &profile, bool &launched) {
    launched = false;
    const unsigned mainOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_MENU");
    const unsigned listOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_LIST");
    const unsigned cardOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_BOX");
    const CMovie *main = view.movies.GetMovie(mainOrdinal), *list = view.movies.GetMovie(listOrdinal), *card = view.movies.GetMovie(cardOrdinal);
    unsigned start = 0, mainEnd = 0, listStart = 0, listEnd = 0, cardStart = 0, cardEnd = 0;
    if (main == nullptr || list == nullptr || card == nullptr || !main->GetChapterRange(0, start, mainEnd) ||
        !list->GetChapterRange(1, listStart, listEnd) || !card->GetChapterRange(1, cardStart, cardEnd)) { return false; }
    if (!state.missionBound) {
        state.missionBound = true;
        state.missionLastTick = view.clock;
        state.missionTime = 0;
        state.missionListTime = 0;
        state.missionFirst = 0;
        state.missionListMoving = false;
        state.missionFocused = -1;
        state.missionScroll = 0;
        state.missionPosition = 0;
        state.missionMotion = MenuScrollMotion{};
        if (!view.animateNavigation) { state.missionTime = mainEnd; state.missionListTime = listStart; }
    }
    const unsigned elapsed = static_cast<unsigned>(view.clock - state.missionLastTick);
    state.missionLastTick = view.clock;
    state.missionTime += elapsed;
    if (state.missionListTime < listStart) { state.missionListTime = std::min(listStart, state.missionListTime + elapsed); }
    const CMovie *waveMovie = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_WAVE_SELECT"));
    unsigned waveStart = 0, waveEnd = 0;
    if (waveMovie == nullptr || !waveMovie->GetChapterRange(1, waveStart, waveEnd)) { return false; }
    if (state.missionWaveTime < waveStart) { state.missionWaveTime = std::min(waveStart, state.missionWaveTime + elapsed); }
    MissionInfoCallbacks info(view, state);
    if (!view.movies.Draw(mainOrdinal, std::min(mainEnd, state.missionTime), 512, 384, kMenuWidth, kMenuHeight, 0, 1, &info)) { return false; }
    MovieRegion viewport, firstSlot, nextSlot;
    if (!view.movies.Region(listOrdinal, 0, listStart, viewport) || !view.movies.Region(listOrdinal, 1, listStart, firstSlot) ||
        !view.movies.Region(listOrdinal, 2, listStart, nextSlot)) { return false; }
    const unsigned count = static_cast<unsigned>(view.planetEntries[state.planet].missions.size());
    unsigned maximum = 0;
    if (count > 3) { maximum = count - 3; }
    if (!ScrollMissionMovie(view, state.missionMotion, state.missionPosition, *list, viewport, nextSlot.x - firstSlot.x,
        maximum, state.missionFocused < 0 && state.modePhase == 2, state.missionFirst, state.missionListTime, state.missionListMoving)) { return false; }
    {
        StoreRegionClip clip(view, viewport);
        MissionListCallbacks callbacks(view, state, profile, launched);
        if (!view.movies.Draw(listOrdinal, state.missionListTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callbacks)) { return false; }
    }
    if (state.missionFocused >= 0) {
        state.missionFocusTime = std::min(125u, state.missionFocusTime + elapsed);
        if (state.missionClosing) {
            state.missionCardTime = std::max(cardStart, state.missionCardTime);
            state.missionCardTime -= std::min(elapsed * 4, state.missionCardTime - cardStart);
        }
        else { state.missionCardTime = std::min(cardEnd, state.missionCardTime + elapsed * 4); }
        MovieRegion bounds;
        if (!view.movies.Region(cardOrdinal, 0, state.missionCardTime, bounds)) { return false; }
        float amount = state.missionFocusTime / 125.0f;
        if (state.missionClosing) { amount = 1 - amount; }
        const float x = state.missionFocusX + (kMenuWidth / 2 - state.missionFocusX) * amount - bounds.width / 2;
        const float y = state.missionFocusY + (kMenuHeight / 2 - state.missionFocusY) * amount - bounds.height / 2;
        MissionCardCallbacks callbacks(view, state, profile, state.missionFocused, true, launched);
        if (!view.movies.Draw(cardOrdinal, state.missionCardTime, x, y, kMenuWidth, kMenuHeight, 0, 1, &callbacks)) { return false; }
        if (state.missionClosing && state.missionFocusTime == 125) { state.missionFocused = -1; state.missionClosing = false; }
    }
    MovieRegion planetRegion;
    if (!view.movies.Region(mainOrdinal, 0, mainEnd, planetRegion)) { return false; }
    const unsigned backOrdinal = view.movies.Ordinal("GLU_MOVIE_BACK_BUTTON");
    const CMovie *back = view.movies.GetMovie(backOrdinal);
    unsigned backEnd = 0;
    if (back == nullptr || !back->GetChapterRange(0, start, backEnd)) { return false; }
    const float backX = planetRegion.x + planetRegion.width / 2, backY = planetRegion.y + planetRegion.height / 2;
    if (!view.movies.Draw(backOrdinal, std::min(state.missionTime, backEnd), backX, backY)) { return false; }
    // The authored touch-only region has visible=0 at both keyframes. The
    // original button queries it independently of drawing visibility.
    for (const auto &region : view.movies.Regions(backOrdinal, std::min(state.missionTime, backEnd), backX, backY, true)) {
        if (region.index != 0 || !view.Hit(region.x, region.y, region.width, region.height)) { continue; }
        if (state.missionFocused < 0) { state.Navigate(0, true); }
        else { state.missionClosing = true; state.missionFocusTime = 0; }
    }
    return DrawOriginalModeOverlay(view, state);
}

/** Horizontal revolution/horde cards use the same two-dimensional sprites as iOS. */

/** Returns true only after an unlocked wave/horde is explicitly launched. */

const WeaponEntry *FindMasteryWeapon(const std::vector<WeaponEntry> &weapons, const GameObjectRef &ref) {
    for (const auto &entry : weapons) {
        if (entry.packHash == ref.packHash && entry.ordinal == ref.localIndex) { return &entry; }
    }
    return nullptr;
}

const StoreEntry *FindWeaponStore(const std::vector<StoreEntry> &store, const GameObjectRef &ref) {
    for (const auto &entry : store) {
        if (entry.data.type > 6) { continue; }
        for (const auto &object : entry.data.objects) {
            if (object.type == 6 && SameObject(object.object, ref)) { return &entry; }
        }
    }
    return nullptr;
}

void BeginPostGame(MenuState &state, const SurvivalGameContext &context, const std::vector<WeaponEntry> &weapons) {
    state.result = context.result;
    state.postGameMusic = true;
    state.casualtyPage = 0;
    state.refineryTab = 0;
    state.refinementRequired = context.profile.xplodium != 0;
    state.message.clear();
    state.Navigate(27, true);
    state.postGameBound = false;
    state.postGameClosing = false;
    state.postGameUpgradePending = false;
    for (const auto &ref : context.profile.configuration.guns) {
        const WeaponEntry *weapon = FindMasteryWeapon(weapons, ref);
        if (weapon != nullptr && weapon->data.GetMasteryLevel(context.profile.GetWeaponExperience(ref)) < 3) {
            state.masteryWeapon = ref;
            if (context.profile.nativeArchive) { state.postGameUpgradePending = true; }
            else { state.Navigate(26); }
            break;
        }
    }
}

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
void CloseMastery(MenuState &state) {
    state.masteryPopup = CMenuUpgradePopup();
    if (state.history.empty()) { state.page = 27; return; }
    state.Back();
}

/** How far into GLU_MOVIE_WEAPON_UPGRADE_MASTERY the meter stands for this
 * much experience. The movie's chapters are the three cells. */
unsigned MasteryMeterTime(GameMenu &view, const WeaponEntry &weapon, unsigned experience) {
    const unsigned meter = view.movies.Ordinal("GLU_MOVIE_WEAPON_UPGRADE_MASTERY");
    CMovie *movie = view.movies.GetMovie(meter);
    if (movie == nullptr) { return 0; }
    unsigned target = 0;
    if (!CMenuUpgradePopup::StarsTarget(*movie, weapon.data, experience, target)) { return 0; }
    return target;
}

/** Bind CMenuMovieButton's original region 1 graphic/label and region 0 hit box. */
bool DrawUpgradeButton(GameMenu &view, unsigned index, const MovieRegion &area,
    const std::string &label, unsigned font, bool interactive, bool &pressed) {
    const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_STORE_UPGRADE", index);
    if (entry == nullptr) { return false; }
    return DrawOriginalMovieButton(view, *entry, area, label, font, interactive, pressed);
}

bool DrawOriginalMovieButton(GameMenu &view, const OriginalMenuEntry &entry, const MovieRegion &area,
    const std::string &label, unsigned font, bool interactive, bool &pressed, unsigned chapter, unsigned elapsed, unsigned timeOverride) {
    pressed = false;
    const unsigned ordinal = view.movies.Ordinal(entry.movies[0]);
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(chapter, start, end)) { return false; }
    unsigned time = end;
    if (chapter == 2 || chapter == 3) { time = start + elapsed % (end - start + 1); }
    if (timeOverride != UINT32_MAX) { time = timeOverride; }
    std::string text = label;
    // CMenuMovieButton::Init :144942: optional resource-authored ^fN font prefix.
    if (text.size() >= 4 && text.compare(0, 2, "^f") == 0 && text[2] >= '0' && text[2] <= '9') {
        font = static_cast<unsigned>(text[2] - '0');
        text.erase(0, 3);
    }
    bool foundGraphic = false, foundTouch = false;
    class ButtonCallback : public IMovieRegionCallback {
    public:
        ButtonCallback(GameMenu &menu, const OriginalMenuEntry &data, const std::string &caption, unsigned face,
            bool enabled, bool &hit, bool &graphic, bool &touch) : view(menu), entry(data), text(caption), font(face),
            interactive(enabled), pressed(hit), foundGraphic(graphic), foundTouch(touch) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index == 1) {
                foundGraphic = true;
                const unsigned sprite = entry.sprites[0];
                if (sprite != UINT32_MAX && !view.movies.DrawSprite(sprite >> 16, sprite & 255, 0,
                    region.x, region.y, 1, region.alpha)) { return false; }
                if (!text.empty()) {
                    view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, font)) / 2,
                        region.y + (region.height - view.movies.TextHeight(font)) / 2, font, 1, 0, region.alpha);
                }
            }
            if (region.index == 0) {
                foundTouch = true;
                if (interactive && region.alpha > 0) { pressed = view.Hit(region.x, region.y, region.width, region.height); }
            }
            return true;
        }
        GameMenu &view;
        const OriginalMenuEntry &entry;
        const std::string &text;
        unsigned font;
        bool interactive;
        bool &pressed, &foundGraphic, &foundTouch;
    } callback(view, entry, text, font, interactive, pressed, foundGraphic, foundTouch);
    // Place the content in its original type-6 layer so later glow layers cover it.
    if (!view.movies.Draw(ordinal, time, area.x, area.y, kMenuWidth, kMenuHeight, 0, area.alpha, &callback)) { return false; }
    // BACK_BUTTON has only an invisible logical region0. Input is updated
    // independently of Draw in CMenuMovieButton :144250; alpha0 is not missing data.
    for (const auto &region : view.movies.Regions(ordinal, time, area.x, area.y, true)) {
        if (region.index == 1) { foundGraphic = true; }
        if (region.index == 0 && !foundTouch) {
            foundTouch = true;
            if (interactive) { pressed = view.Hit(region.x, region.y, region.width, region.height); }
        }
    }
    if (entry.sprites[0] == UINT32_MAX && text.empty()) { foundGraphic = true; }
    return foundGraphic && foundTouch;
}

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
unsigned MenuBranchPage(unsigned page) {
    if (page == 1 || page == 17 || page == 18) { return 2; }
    if (page == 16 || page == 19 || page == 21 || page == 22 || page == 23) { return 0; }
    if (page == 8 || page == 9 || page == 11) { return 6; }
    if (page == 13) { return 5; }
    if (page == 29) { return 4; }
    return page;
}

/** CMenuNavigationBar :143357 binds Header0..16 and InfoCluster0..3.
 * NAVBAR_MAIN is extracted from native statics; all art/layout/timing is BIG. */
int GameMenu::Header(const CProfileManager &profile, const CPlayerProgress &progress, unsigned currentPage) {
    const unsigned ordinal = movies.Ordinal("GLU_MOVIE_HEADER");
    const CMovie *header = movies.GetMovie(ordinal);
    unsigned idleStart = 0, idleEnd = 0, hideStart = 0, hideEnd = 0;
    if (header == nullptr || !header->GetChapterRange(2, idleStart, idleEnd) ||
        !header->GetChapterRange(3, hideStart, hideEnd)) { return -3; }
    const auto now = clock;
    const bool visible = currentPage < 25;
    unsigned elapsed = 0;
    if (originalHeaderBound) { elapsed = static_cast<unsigned>(std::min<std::uint64_t>(now - originalHeaderTick, 1000)); }
    if (!originalHeaderBound) {
        originalHeaderBound = true;
        navigationVisible = visible;
        originalHeaderTime = 0;
        if (!visible) { originalHeaderTime = hideEnd; }
    } else if (visible != navigationVisible) {
        navigationVisible = visible;
        if (visible) {
            unsigned end = 0;
            if (!header->GetChapterRange(1, originalHeaderTime, end)) { return -3; }
            originalHeaderButtonTime = 0;
        } else { originalHeaderTime = hideStart; }
    }
    originalHeaderTick = now;
    originalHeaderTime += elapsed;
    originalHeaderButtonTime += elapsed;
    if (visible && originalHeaderTime > idleEnd) {
        originalHeaderTime = idleStart + (originalHeaderTime - idleStart) % (idleEnd - idleStart + 1);
    } else if (!visible) { originalHeaderTime = std::min(originalHeaderTime, hideEnd); }
    if (!animateNavigation) {
        originalHeaderTime = hideEnd;
        if (visible) { originalHeaderTime = idleStart; }
        originalHeaderButtonTime = idleStart;
    }
    const unsigned activePage = MenuBranchPage(currentPage);
    // Host page routing only; order and branch IDs are original NAVBAR_MAIN.
    constexpr unsigned branchPages[] = {0, 0, 2, 4, 5, 3, 6, 7};
    int choice = -1;
    class InfoCallback : public IMovieRegionCallback {
    public:
        InfoCallback(GameMenu &menu, const CPlayerProgress &experience) : view(menu), progress(experience) {
            char digits[16];
            std::snprintf(digits, sizeof(digits), "%.3u", progress.GetLevel());
            level = digits;
        }
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index == 0) {
                // Original GetPercentToNextLevel :193337 clamps to 1, including max level.
                const float fraction = std::min(1.0f, static_cast<float>(progress.GetExperienceInLevel()) / progress.GetExperienceDelta());
                // ExperienceCallback VA0xbc3fc passes ARGB 0xff0195d7.
                view.movies.Rectangle(region.x, region.y, std::floor(region.width * fraction), region.height,
                    1.0f / 255, 149.0f / 255, 215.0f / 255, region.alpha);
            } else if (region.index <= 3 && region.index <= level.size()) {
                view.movies.Text(level.substr(region.index - 1, 1), region.x,
                    region.y + region.height / 2 - std::floor(view.movies.TextHeight(7) / 2), 7, 1, 0, region.alpha);
            }
            return true;
        }
        GameMenu &view;
        const CPlayerProgress &progress;
        std::string level;
    } info(*this, progress);
    class HeaderCallback : public IMovieRegionCallback {
    public:
        HeaderCallback(GameMenu &menu, const CProfileManager &account, InfoCallback &cluster,
            unsigned selected, const unsigned *pages, bool touch, int &result) : view(menu), profile(account), info(cluster),
            activePage(selected), branchPages(pages), enabled(touch), choice(result) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index < std::size(kOriginalNavigationBranches)) {
                const unsigned branch = kOriginalNavigationBranches[region.index];
                const auto *entry = OriginalMenuData("MDS_BUTTON_TRUNK", branch - 1);
                if (entry == nullptr) { return false; }
                const CMovie *movie = view.movies.GetMovie(view.movies.Ordinal(entry->movies[0]));
                unsigned start = 0, end = 0;
                if (movie == nullptr || !movie->GetChapterRange(0, start, end)) { return false; }
                unsigned chapter = 2, time = UINT32_MAX;
                if (activePage == branchPages[branch]) { chapter = 3; }
                if (view.originalHeaderButtonTime <= end) { chapter = 0; time = view.originalHeaderButtonTime; }
                MovieRegion origin = region;
                origin.x += region.width / 2;
                origin.y += region.height / 2;
                bool pressed = false;
                if (!DrawOriginalMovieButton(view, *entry, origin, "", 0, enabled, pressed,
                    chapter, view.originalHeaderButtonTime, time)) { return false; }
                if (pressed) { choice = static_cast<int>(region.index); }
            } else if (region.index < 14) {
                const unsigned index = region.index - 7;
                if (index >= std::size(kOriginalNavigationBranches)) { return false; }
                const auto *entry = OriginalMenuData("MDS_BUTTON_TRUNK", kOriginalNavigationBranches[index] - 1);
                if (entry == nullptr) { return false; }
                const auto label = view.movies.NamedString(entry->strings[0]);
                view.movies.Text(label, region.x + region.width / 2 - std::floor(view.movies.TextWidth(label, 1) / 2),
                    region.y, 1, 1, 0, region.alpha);
            } else if (region.index == 14 || region.index == 15) {
                std::uint64_t value = profile.coins;
                if (region.index == 15) { value = profile.warbucks; }
                // Provider78/79 reads the original low 32-bit value; no invented compact notation.
                view.movies.Text(std::to_string(static_cast<std::int32_t>(value)), region.x, region.y, 0, 1, 0, region.alpha);
            } else if (region.index == 16) {
                const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_INFO_CLUSTER");
                const CMovie *movie = view.movies.GetMovie(ordinal);
                if (movie == nullptr || movie->duration == 0) { return false; }
                return view.movies.Draw(ordinal, view.originalHeaderButtonTime % movie->duration,
                    region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &info);
            }
            return true;
        }
        GameMenu &view;
        const CProfileManager &profile;
        InfoCallback &info;
        unsigned activePage;
        const unsigned *branchPages;
        bool enabled;
        int &choice;
    } callback(*this, profile, info, activePage, branchPages,
        visible && originalHeaderTime >= idleStart, choice);
    if (!movies.Draw(ordinal, originalHeaderTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callback)) { return -3; }
    return choice;
}

/** Original callbacks use each MovieRegion and bitmap font without fitting. */
void UpgradeCenteredText(GameMenu &view, const MovieRegion &region, const std::string &text, unsigned font) {
    view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, font)) / 2,
        region.y + (region.height - view.movies.TextHeight(font)) / 2, font, 1, 0, region.alpha);
}

/** CMenuUpgradePopup::DrawBodyText :392576: only changed stats, then CRIT.
 * CURRENT is the absolute STORE value; NEXT is the relative percentage change. */
void DrawUpgradeStats(GameMenu &view, const MovieRegion &area, const CStoreItem &item, unsigned level, bool next) {
    constexpr const char *titles[] = {"IDS_UPGRADE_POWER", "IDS_UPGRADE_DAMAGE", "IDS_UPGRADE_RPM", "IDS_UPGRADE_SPEED", "", ""};
    constexpr const char *tokens[] = {"POWER", "DMG", "RPM", "SPD", "DEF", "ATK"};
    constexpr const char *critical[] = {"IDS_UPGRADE_CRITICAL_CHANCE_NONE", "IDS_UPGRADE_CRITICAL_CHANCE_LOW",
        "IDS_UPGRADE_CRITICAL_CHANCE_MED", "IDS_UPGRADE_CRITICAL_CHANCE_HIGH"};
    struct Row { std::string label, value; };
    std::vector<Row> rows;
    const unsigned nextLevel = std::min(kMaxMasteryLevel, level + 1);
    const auto values = StoreStatValues(item, level);
    for (unsigned stat = 0; stat < 6; ++stat) {
        const auto &column = item.statGroups[stat];
        if (column.size() <= nextLevel) { continue; }
        std::int64_t before = column[level], after = column[nextLevel];
        if (stat == 3) { before += 100; after += 100; }
        if (before == 0) { continue; }
        const std::int64_t change = 100 * (after - before) / before;
        if (change == 0) { continue; }
        std::string value = SubstituteStoreStats(std::string("#") + tokens[stat], values);
        if (next) {
            value = std::to_string(change) + "%";
            if (change > 0) { value = "+" + value; }
        }
        rows.push_back({view.movies.NamedString(titles[stat]), value});
    }
    unsigned criticalLevel = level;
    if (next) { criticalLevel = nextLevel; }
    rows.push_back({view.movies.NamedString("IDS_UPGRADE_CRIT"), view.movies.NamedString(critical[criticalLevel])});
    const float height = view.movies.TextHeight(1);
    const int gap = static_cast<int>(area.height - height * rows.size()) / static_cast<int>(rows.size() + 1);
    float y = area.y + gap;
    for (const Row &row : rows) {
        view.movies.Text(row.label, area.x, y, 1, 1, 0, area.alpha);
        view.movies.Text(row.value, area.x + area.width - view.movies.TextWidth(row.value, 1), y, 1, 1, 0, area.alpha);
        y += height + gap;
    }
}

/** The original popup advances its own movie and stars through six states.
 * All geometry, fonts, item values, chapter times and art are read from BIG. */
bool DrawMastery(GameMenu &view, MenuState &state, CProfileManager &profile, CResTOCManager &toc,
    PackTables &tables, const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::filesystem::path &savePath, CPlayerProgress *headerProgress = nullptr) {
    const WeaponEntry *weapon = FindMasteryWeapon(weapons, state.masteryWeapon);
    const StoreEntry *item = FindWeaponStore(store, state.masteryWeapon);
    if (weapon == nullptr || item == nullptr) { CloseMastery(state); return true; }
    const unsigned popup = view.movies.Ordinal("GLU_MOVIE_UPGRADE_POPUP");
    const unsigned stars = view.movies.Ordinal("GLU_MOVIE_WEAPON_UPGRADE_MASTERY");
    const CMovie *popupMovie = view.movies.GetMovie(popup);
    const CMovie *starsMovie = view.movies.GetMovie(stars);
    if (popupMovie == nullptr || starsMovie == nullptr) { return false; }
    if (!state.masteryPopup.IsBound()) {
        if (!state.masteryPopup.Bind(*popupMovie, *starsMovie, weapon->data, profile.GetWeaponExperience(state.masteryWeapon))) { return false; }
        state.masteryOpened = view.clock;
    } else {
        unsigned delta = 0;
        if (view.clock >= state.masteryOpened) { delta = static_cast<unsigned>(view.clock - state.masteryOpened); }
        state.masteryOpened = view.clock;
        if (!state.storePromptRequested && !state.storePopup.IsActive()) { state.masteryPopup.Update(delta); }
    }
    if (state.masteryPopup.GetState() == CMenuUpgradePopup::State::Closed) { CloseMastery(state); return true; }
    const bool interactive = state.masteryPopup.GetState() == CMenuUpgradePopup::State::Ready &&
        !state.storePromptRequested && !state.storePopup.IsActive();
    const unsigned experience = state.masteryPopup.DisplayExperience();
    const unsigned level = weapon->data.GetMasteryLevel(experience);
    const unsigned nextLevel = std::min(kMaxMasteryLevel, level + 1);
    const float backdrop = state.masteryPopup.BackdropAlpha() / 255.0f;
    view.movies.Rectangle(0, 0, kMenuWidth, kMenuHeight, 0, 0, 0, backdrop);
    // CMenuSystem::Draw :96847 draws BetweenMenuAndHud before the HUD,
    // then draws the popup last. Update :96904 consumes all underlying input.
    if (headerProgress != nullptr) {
        const bool click = view.ExchangeClick(false);
        unsigned headerPage = 2;
        if (state.refinementRequired) { headerPage = 25; }
        if (view.Header(profile, *headerProgress, headerPage) == -3) { return false; }
        view.ExchangeClick(click);
    }
    if (!view.movies.Draw(popup, state.masteryPopup.MovieTime())) { return false; }
    bool buyPressed = false, closePressed = false, swapPressed = false;
    const WeaponEntry *other = nullptr;
    // ShowForGuns :394113 prepares both distinct equipped guns below gold.
    // The same popup and swap action are used from the store and the refinery.
    for (const GameObjectRef &gun : profile.configuration.guns) {
        if (SameObject(gun, state.masteryWeapon)) { continue; }
        const WeaponEntry *candidate = FindMasteryWeapon(weapons, gun);
        if (candidate != nullptr && candidate->data.GetMasteryLevel(profile.GetWeaponExperience(gun)) < kMaxMasteryLevel &&
            FindWeaponStore(store, gun) != nullptr) { other = candidate; break; }
    }
    for (const MovieRegion &region : view.movies.Regions(popup, state.masteryPopup.MovieTime())) {
        if (region.alpha <= 0) { continue; }
        switch (region.index) {
        case kUpgradePortraitRegion: {
            MovieRegion bounds;
            const unsigned animation = kBrotherPortrait + profile.playerBrother;
            if (!view.movies.SpriteBounds(0, animation, bounds) ||
                !view.movies.DrawSprite(0, animation, 0, region.x + region.width - bounds.width,
                    region.y + region.height - bounds.height, 1, region.alpha)) { return false; }
            break;
        }
        case kUpgradeCloseRegion:
            if (!DrawUpgradeButton(view, 0, region, "", 0, interactive, closePressed)) { return false; }
            break;
        case kUpgradeMeterRegion:
            if (!view.movies.Draw(stars, state.masteryPopup.StarsTime(), region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha)) { return false; }
            break;
        case kUpgradeCurrentHeaderRegion:
            UpgradeCenteredText(view, region, view.movies.NamedString("IDS_UPGRADE_CURRENT_LEVEL_TITLE"), 0);
            break;
        case kUpgradeNextHeaderRegion: {
            constexpr const char *titles[] = {"IDS_UPGRADE_NEXT_LEVEL_TITLE_BRONZE", "IDS_UPGRADE_NEXT_LEVEL_TITLE_SILVER", "IDS_UPGRADE_NEXT_LEVEL_TITLE_GOLD"};
            UpgradeCenteredText(view, region, view.movies.NamedString(titles[std::min(level, 2u)]), 0);
            break;
        }
        case kUpgradeCurrentColumnRegion:
            DrawUpgradeStats(view, region, item->data, level, false);
            break;
        case kUpgradeNextColumnRegion:
            DrawUpgradeStats(view, region, item->data, level, true);
            break;
        case kUpgradeIconRegion:
            view.Icon(toc, tables, *item, region.x, region.y, region.width, region.height, region.alpha, true);
            break;
        case kUpgradeTitleRegion:
            UpgradeCenteredText(view, region, view.movies.NamedString("IDS_UPGRADE_TITLE"), 11);
            break;
        case kUpgradeBuyRegion:
            if (level < kMaxMasteryLevel) {
                const std::string label = SubstituteStoreStats(view.movies.NamedString("IDS_UPGRADE_BUY_BUCKS"), StoreStatValues(item->data, nextLevel));
                if (!DrawUpgradeButton(view, 2, region, label, 0, interactive, buyPressed)) { return false; }
            }
            break;
        case 10:
            if (other != nullptr) {
                MovieRegion center = region;
                center.x += static_cast<int>(center.width) / 2;
                center.y += static_cast<int>(center.height) / 2;
                if (!DrawUpgradeButton(view, 1, center, "", 6, interactive, swapPressed)) { return false; }
            }
            break;
        case kUpgradeNameRegion:
            UpgradeCenteredText(view, region, item->name, 1);
            break;
        }
    }
    if (closePressed) { state.masteryPopup.Hide(); }
    if (swapPressed && other != nullptr) {
        state.masteryWeapon.packHash = other->packHash;
        state.masteryWeapon.localIndex = other->ordinal;
        if (!state.masteryPopup.SelectGun(*starsMovie, other->data, profile.GetWeaponExperience(state.masteryWeapon))) { return false; }
    }
    if (buyPressed && level < kMaxMasteryLevel) {
        const auto &prices = item->data.statGroups[7];
        if (prices.size() <= nextLevel || prices[nextLevel] < 0) { return false; }
        const unsigned price = static_cast<unsigned>(prices[nextLevel]);
        if (profile.warbucks >= price) {
            const unsigned threshold = weapon->data.GetMasteryThreshold(level);
            profile.warbucks -= price;
            profile.AddWeaponExperience(state.masteryWeapon, threshold - experience, weapon->data.GetMasteryLimit());
            if (!profile.SaveToDisk(savePath) || !state.masteryPopup.PerformUpgrade(*starsMovie, weapon->data, threshold)) { return false; }
            std::printf("[upgrade] weapon=%s xp=%u price=%u target=%u\n", item->name.c_str(), threshold, price, state.masteryPopup.TargetTime());
        } else {
            // BuyAction :393339 shows modal category11, button mode1/table154.
            ShowStoreFundsPrompt(state, store, profile, 1, price);
        }
    }
    if (state.masteryPopup.FlashAlpha() > 0) {
        // Original CMenuUpgradePopup::Draw constants :21160-21162, not resource values.
        constexpr unsigned colors[3][3] = {{240, 184, 155}, {217, 217, 217}, {254, 240, 125}};
        const unsigned index = level - 1;
        if (index >= 3) { return false; }
        view.movies.Rectangle(0, 0, kMenuWidth, kMenuHeight, colors[index][0] / 255.0f, colors[index][1] / 255.0f,
            colors[index][2] / 255.0f, state.masteryPopup.FlashAlpha() / 255.0f);
    }
    return true;
}

/** Resource printf substitution for the original CGame result strings. */
std::string PostGameFormat(GameMenu &view, const char *name, const std::vector<std::string> &values) {
    std::string text = view.movies.NamedString(name);
    std::size_t cursor = 0;
    for (const auto &value : values) {
        cursor = text.find('%', cursor);
        if (cursor == std::string::npos || cursor + 1 >= text.size()) { return {}; }
        const char type = text[cursor + 1];
        if (type != 'i' && type != 'd' && type != 'u' && type != 's') {
            std::printf("[postgame] unsupported format resource=%s value=%s\n", name, text.c_str());
            return {};
        }
        text.replace(cursor, 2, value);
        cursor += value.size();
    }
    return text;
}

class PostGameCardCallbacks : public IMovieRegionCallback {
public:
    PostGameCardCallbacks(GameMenu &menu, const OriginalMenuEntry &data, const std::string &number)
        : view(menu), entry(data), value(number) {}
    bool DrawMovieRegion(const MovieRegion &region) override {
        if (region.index == 1) {
            const unsigned sprite = entry.sprites[0];
            if (sprite == UINT32_MAX) { return true; }
            return view.movies.DrawSprite(sprite >> 16, sprite & 255, 0,
                region.x + static_cast<int>(region.width) / 2, region.y + static_cast<int>(region.height) / 2, 1, region.alpha);
        }
        if (region.index == 2) { UpgradeCenteredText(view, region, value, 6); }
        if (region.index == 3) { UpgradeCenteredText(view, region, view.movies.NamedString(entry.strings[0]), 0); }
        return true;
    }
    GameMenu &view;
    const OriginalMenuEntry &entry;
    const std::string &value;
};

/** CMenuPostGame::OverviewCallback :164593; single-player default bro
 * provider93 count=3, last data index=7+Mission.type-4. */
class PostGameListCallbacks : public IMovieRegionCallback {
public:
    PostGameListCallbacks(GameMenu &menu, MenuState &selection, CResTOCManager &manager, PackTables &resources)
        : view(menu), state(selection), toc(manager), tables(resources) {}
    bool DrawMovieRegion(const MovieRegion &region) override {
        if (state.page == 28) {
            if (region.index < 1 || region.index > 4) { return true; }
            const int index = static_cast<int>(std::floor(state.postGameGalleryPosition)) + static_cast<int>(region.index) - 1;
            if (index < 0 || index >= static_cast<int>(state.result.casualties.size())) { return true; }
            class CasualtyCallback : public IMovieRegionCallback {
            public:
                CasualtyCallback(PostGameListCallbacks &owner, const EnemyCasualty &value) : list(owner), casualty(value) {}
                bool DrawMovieRegion(const MovieRegion &area) override {
                    return list.view.DrawCasualty(list.tables, list.toc, casualty, 0, &area);
                }
                PostGameListCallbacks &list;
                const EnemyCasualty &casualty;
            } callback(*this, state.result.casualties[index]);
            const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_MODEL_GALLERY_ITEM");
            const auto *movie = view.movies.GetMovie(ordinal);
            if (movie == nullptr) { return false; }
            return view.movies.Draw(ordinal, std::min(state.postGameItemTime, movie->duration),
                region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &callback);
        }
        if (region.index < 1 || region.index > 2) { return true; }
        const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_WRAPUP_BOX");
        MovieRegion bounds;
        const auto *movie = view.movies.GetMovie(ordinal);
        if (movie == nullptr || !view.movies.Region(ordinal, 0, 0, bounds)) { return false; }
        const unsigned first = (region.index - 1) * 2;
        for (unsigned index = first; index < std::min(3u, first + 2); ++index) {
            unsigned icon = index;
            std::uint64_t amount = state.result.xplodium;
            if (index == 1) { amount = state.result.experience; }
            if (index == 2) {
                icon = 4;
                amount = state.result.perfectWaves;
                if (state.result.horde) { icon = 5; amount = state.result.bestKillStreak; }
            }
            const auto *entry = OriginalMenuData("MDS_ICON_POSTGAME", icon);
            if (entry == nullptr) { return false; }
            float x = region.x;
            if (index == 1) { x += region.width - bounds.width; }
            if (index == 2) { x += static_cast<int>(region.width) / 2 - static_cast<int>(bounds.width) / 2; }
            const std::string value = std::to_string(amount);
            PostGameCardCallbacks callback(view, *entry, value);
            if (!view.movies.Draw(ordinal, std::min(state.postGameItemTime, movie->duration),
                x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &callback)) { return false; }
        }
        return true;
    }
    GameMenu &view;
    MenuState &state;
    CResTOCManager &toc;
    PackTables &tables;
};

/** MENU_POST_GAME_WRAPUP VA0x403350, CMenuPostGame :164559..166204.
 * Native menu/provider logic below; layouts, fonts and artwork stay in BIG. */
bool DrawOriginalPostGame(GameMenu &view, MenuState &state, CResTOCManager &toc, PackTables &tables,
    const CProfileManager &profile) {
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_WRAPUP_SCREEN");
    const auto *movie = view.movies.GetMovie(ordinal);
    unsigned idleStart = 0, idleEnd = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, idleStart, idleEnd)) { return false; }
    if (!state.postGameBound) {
        state.postGameBound = true;
        state.postGameTime = 0;
        state.postGameItemTime = 0;
        state.postGameCloseTime = 0;
        state.postGameLastTick = view.clock;
        state.postGameGalleryPosition = 0;
        if (state.result.casualties.size() <= 2) { state.postGameGalleryPosition = -1; }
        state.postGameGalleryVelocity = 0;
        // Provider74 walks flattened ENEMY order, not the order of first kills.
        for (std::size_t item = 1; item < state.result.casualties.size(); ++item) {
            std::size_t cursor = item;
            while (cursor > 0) {
                const auto &left = state.result.casualties[cursor - 1].resource;
                const auto &right = state.result.casualties[cursor].resource;
                const int leftPack = toc.GetPackIndexFromHash(left.packHash);
                const int rightPack = toc.GetPackIndexFromHash(right.packHash);
                if (leftPack < rightPack || (leftPack == rightPack && left.localIndex <= right.localIndex)) { break; }
                std::swap(state.result.casualties[cursor - 1], state.result.casualties[cursor]);
                --cursor;
            }
        }
    }
    const unsigned delta = static_cast<unsigned>(view.clock - state.postGameLastTick);
    state.postGameDelta = delta;
    state.postGameLastTick = view.clock;
    state.postGameTime += delta;
    state.postGameItemTime += delta;
    if (state.postGameTime > idleEnd) { state.postGameTime = idleStart + (state.postGameTime - idleStart) % (idleEnd - idleStart + 1); }
    const bool ready = state.postGameTime >= idleStart && !state.postGameClosing;
    const auto *back = OriginalMenuData("MDS_BUTTON_POSTGAME_BACK", 0);
    if (back == nullptr) { return false; }
    const auto *backMovie = view.movies.GetMovie(view.movies.Ordinal(back->movies[0]));
    unsigned exitStart = 0, exitEnd = 0;
    unsigned hideStart = 0, hideEnd = 0;
    if (backMovie == nullptr || !backMovie->GetChapterRange(1, exitStart, exitEnd) ||
        !backMovie->GetChapterRange(0, hideStart, hideEnd)) { return false; }
    const unsigned pressedDuration = exitEnd - exitStart + 1;
    if (state.postGameClosing) {
        state.postGameCloseTime += delta;
        if (state.postGameCloseTime >= pressedDuration + hideEnd - hideStart + 1) {
            // DoAction38 :94446 chooses original menu20 if ore remains, 19 otherwise.
            unsigned target = 0;
            if (profile.xplodium != 0) { target = 3; }
            state.postGameMusic = false;
            state.Navigate(target, true);
            return true;
        }
    }
    class MainCallbacks : public IMovieRegionCallback {
    public:
        MainCallbacks(GameMenu &menu, MenuState &selection, CResTOCManager &manager, PackTables &resources, bool enabled)
            : view(menu), state(selection), toc(manager), tables(resources), interactive(enabled) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index == 1) {
                const auto *first = OriginalMenuData("MDS_BUTTON_POSTGAME_INFO", 0);
                if (first == nullptr) { return false; }
                MovieRegion bounds;
                if (!view.movies.Region(view.movies.Ordinal(first->movies[0]), 0, 0, bounds)) { return false; }
                // CategoryCallback ARM native gap=2; CMenuMovieButton origin is top-left.
                float x = region.x + static_cast<int>(region.width) / 2 - static_cast<int>((bounds.width + 2) * 2) / 2;
                for (unsigned index = 0; index < 2; ++index) {
                    const auto *entry = OriginalMenuData("MDS_BUTTON_POSTGAME_INFO", index);
                    if (entry == nullptr) { return false; }
                    MovieRegion origin = region;
                    origin.x = x;
                    bool pressed = false;
                    unsigned chapter = 2;
                    if (state.page == 27 + index) { chapter = 3; }
                    unsigned time = UINT32_MAX;
                    if (state.postGameClosing) {
                        // OnExit -> Hide reverses chapter0, not chapter1's press burst.
                        const auto *button = view.movies.GetMovie(view.movies.Ordinal(entry->movies[0]));
                        const auto *back = OriginalMenuData("MDS_BUTTON_POSTGAME_BACK", 0);
                        if (back == nullptr) { return false; }
                        const auto *backMovie = view.movies.GetMovie(view.movies.Ordinal(back->movies[0]));
                        unsigned pressStart = 0, pressEnd = 0;
                        if (backMovie == nullptr || !backMovie->GetChapterRange(1, pressStart, pressEnd)) { return false; }
                        unsigned begin = 0, end = 0;
                        if (button == nullptr || !button->GetChapterRange(0, begin, end)) { return false; }
                        if (state.postGameCloseTime > pressEnd - pressStart) {
                            chapter = 0;
                            const unsigned elapsed = state.postGameCloseTime - (pressEnd - pressStart + 1);
                            time = end - std::min(elapsed, end - begin);
                        }
                    }
                    if (!DrawOriginalMovieButton(view, *entry, origin, view.movies.NamedString(entry->strings[0]), 5,
                        interactive, pressed, chapter, state.postGameItemTime, time)) { return false; }
                    if (pressed) { state.page = 27 + index; }
                    x += bounds.width + 2;
                }
            } else if (region.index == 2) {
                std::string title;
                const auto &result = state.result;
                if (result.horde) {
                    const char *name = "IDS_WRAPUP_SCORE";
                    if (result.score != 0 && result.score == result.highScore) { name = "IDS_WRAPUP_NEW_HIGH_SCORE"; }
                    title = PostGameFormat(view, name, {std::to_string(result.score)});
                } else if (result.wavesPerRevolution > 0) {
                    unsigned revolution = result.wave / result.wavesPerRevolution + 1;
                    unsigned wave = result.wave % result.wavesPerRevolution + 1;
                    if (result.wave == result.waveLimit) { revolution = result.waveLimit / result.wavesPerRevolution + 1; wave = result.wavesPerRevolution; }
                    title = PostGameFormat(view, "IDS_WRAPUP_REVOLUTION_WAVE", {std::to_string(revolution), std::to_string(wave)});
                }
                UpgradeCenteredText(view, region, title, 6);
            } else if (region.index == 3) {
                std::string text;
                if (state.result.horde) {
                    const unsigned seconds = state.result.stopwatchMs / 1000;
                    char time[32];
                    // CUtility::TimeToString flags1,1; ARM string VA0x3c3048.
                    std::snprintf(time, sizeof(time), "%.2u:%.2u:%.2u", seconds / 3600, seconds / 60 % 60, seconds % 60);
                    text = PostGameFormat(view, "IDS_WRAPUP_SURVIVAL_TIME", {time});
                } else { text = PostGameFormat(view, "IDS_WRAPUP_WAVE_CLEARED", {std::to_string(state.result.waves)}); }
                UpgradeCenteredText(view, region, text, 0);
            } else if (region.index == 5) {
                const std::string text = view.movies.NamedString("IDS_WRAPUP_TOTAL_KILLS") + std::to_string(static_cast<std::uint16_t>(state.result.kills));
                view.movies.Text(text, region.x, region.y + (region.height - view.movies.TextHeight(0)) / 2, 0, 1, 0, region.alpha);
            } else if (region.index == 4) {
                PostGameListCallbacks callback(view, state, toc, tables);
                const char *name = "GLU_MOVIE_WRAPUP_MENU_SCROLL";
                if (state.page == 28) { name = "GLU_MOVIE_WRAPUP_GALLERY"; }
                const unsigned list = view.movies.Ordinal(name);
                const auto *listMovie = view.movies.GetMovie(list);
                unsigned start = 0, end = 0;
                if (listMovie == nullptr || !listMovie->GetChapterRange(1, start, end)) { return false; }
                // Show selects the final overview row then settles on row0.
                unsigned time = std::min(state.postGameItemTime, start);
                if (state.page == 27) { view.Clip(0, region.y, kMenuWidth, region.height); }
                else {
                    unsigned secondStart = 0, secondEnd = 0;
                    if (!listMovie->GetChapterRange(2, secondStart, secondEnd) || secondStart <= start) { return false; }
                    const unsigned duration = secondStart - start;
                    // CalculateBaseVelocity :141749 averages the authored travel
                    // of ALL type>=2 regions between chapter1 and chapter2.
                    float distance = 0;
                    unsigned changed = 0;
                    for (const auto &first : view.movies.Regions(list, start)) {
                        if (first.type < 2) { continue; }
                        MovieRegion last;
                        if (!view.movies.Region(list, first.index, secondStart, last)) { return false; }
                        const int travel = static_cast<int>(first.x + first.width / 2 - last.x - last.width / 2);
                        if (travel != 0) { distance += travel; ++changed; }
                    }
                    if (changed == 0 || distance == 0) { return false; }
                    distance = static_cast<float>(std::abs(static_cast<int>(distance) / static_cast<int>(changed)));
                    const float seconds = state.postGameDelta / 1000.0f;
                    if (interactive && view.MouseIn(region.x, region.y, region.width, region.height)) {
                        // Wheel is the Windows adapter for one original list option.
                        state.postGameGalleryPosition -= view.window.TakeWheelDelta();
                        if (view.dragX != 0 && seconds > 0) {
                            const float speed = view.dragX / seconds / (distance * 1000 / duration);
                            state.postGameGalleryVelocity = std::clamp(-speed, -5.0f, 5.0f);
                        }
                    }
                    if (interactive && seconds > 0) {
                        state.postGameGalleryPosition += state.postGameGalleryVelocity * state.postGameDelta / duration;
                        if (!view.window.IsLeftMouseDown()) {
                            const float slowing = 250000.0f / duration * seconds * seconds / 2;
                            if (state.postGameGalleryVelocity > 0) { state.postGameGalleryVelocity = std::max(0.0f, state.postGameGalleryVelocity - slowing); }
                            else { state.postGameGalleryVelocity = std::min(0.0f, state.postGameGalleryVelocity + slowing); }
                        }
                    }
                    // SetBoundsOptions(1,1), Init offset=1; a single enemy is centered.
                    const float maximum = static_cast<float>(std::max(0, static_cast<int>(state.result.casualties.size()) - 2) - 1);
                    const float minimum = std::min(0.0f, maximum);
                    state.postGameGalleryPosition = std::clamp(state.postGameGalleryPosition, minimum, maximum);
                    const float fraction = state.postGameGalleryPosition - std::floor(state.postGameGalleryPosition);
                    time = start + static_cast<unsigned>(fraction * duration);
                }
                const bool drawn = view.movies.Draw(list, time, region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &callback);
                if (state.page == 27) { view.EndClip(); }
                return drawn;
            }
            return true;
        }
        GameMenu &view;
        MenuState &state;
        CResTOCManager &toc;
        PackTables &tables;
        bool interactive;
    } callback(view, state, toc, tables, ready);
    if (!view.movies.Draw(ordinal, state.postGameTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callback)) { return false; }
    MovieRegion position;
    if (!view.movies.Region(ordinal, 0, state.postGameTime, position)) { return false; }
    position.x += static_cast<int>(position.width) / 2;
    position.y += static_cast<int>(position.height) / 2;
    bool pressed = false;
    unsigned time = UINT32_MAX;
    unsigned chapter = 0;
    if (ready) { chapter = 2; }
    if (state.postGameClosing) {
        chapter = 1;
        time = exitStart + state.postGameCloseTime;
        if (state.postGameCloseTime >= pressedDuration) {
            chapter = 0;
            time = hideEnd - std::min(state.postGameCloseTime - pressedDuration, hideEnd - hideStart);
        }
    }
    if (!DrawOriginalMovieButton(view, *back, position, {}, 0, ready, pressed, chapter, state.postGameItemTime, time)) { return false; }
    if (pressed) { state.postGameClosing = true; state.postGameCloseTime = 0; }
    if (ready && state.postGameUpgradePending) {
        state.postGameUpgradePending = false;
        state.Navigate(26);
    }
    return true;
}

/** Standard intervals occupy 6..11; premium intervals occupy 0..5. */

/** Original menu provider 69; strings are BIG resources except the native
 * GetTimeIntervalString printf patterns at ARM VA 0x3c4980/0x3c49c4. */
std::string RefineryNumber(GameMenu &view, const char *name, std::uint64_t value) {
    std::string text = view.movies.NamedString(name);
    std::size_t offset = text.find("%i");
    if (offset == std::string::npos) { offset = text.find("%d"); }
    if (offset == std::string::npos) {
        std::printf("[refinery] unsupported number format resource=%s text=%s\n", name, text.c_str());
        return {};
    }
    text.replace(offset, 2, std::to_string(value));
    offset = text.find("%%");
    if (offset != std::string::npos) { text.replace(offset, 2, "%"); }
    return text;
}

bool SetRefineryStatus(GameMenu &view, MenuState &state, unsigned slot, unsigned chapter) {
    const auto *entry = OriginalMenuData("MDS_BUTTON_XPLODIUM_METER", slot);
    if (entry == nullptr) { return false; }
    const auto *movie = view.movies.GetMovie(view.movies.Ordinal(entry->movies[1]));
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(chapter, start, end)) { return false; }
    state.refineryStatusChapter[slot] = chapter;
    state.refineryStatusTime[slot] = start;
    return true;
}

/** CMenuGameResources::Init/Bind :172652/172835, MENU_GAME_RESOURCES
 * VA0x402d50. Original provider SLOT_PHASE_OFFSETS={0,6}, COUNT={6,6}.
 * Resource dimensions, durations, text, sprite geometry and prices stay BIG. */
class RefineryCallbacks : public IMovieRegionCallback {
public:
    RefineryCallbacks(GameMenu &menu, MenuState &selection, CProfileManager &account,
        const CRefinementManager::Template &resources, unsigned cells, bool ready)
        : view(menu), state(selection), profile(account), data(resources), count(cells), interactive(ready) {}

    bool DrawMovieRegion(const MovieRegion &region) override {
        if (region.index < count) { return Meter(region); }
        if (region.index < count * 2) { return MeterInfo(region); }
        if (region.index == count * 2) { return Xplodium(region); }
        if (region.index == count * 2 + 1) {
            DrawMissionText(view, region, view.movies.NamedString("IDS_RESMAN_SIDEBARINFO"), 1);
        }
        if (region.index == count * 2 + 4) { return Categories(region); }
        return true;
    }

    bool Categories(const MovieRegion &region) {
        // CategoryButtonCallback :172186 draws entry1 then entry0, with 4px
        // between them and total width 2*graphicWidth+8, exactly as the source.
        const auto *first = OriginalMenuData("MDS_BUTTON_REFINE_SLOT_CATEGORY", 1);
        if (first == nullptr) { return false; }
        const unsigned movie = view.movies.Ordinal(first->movies[0]);
        MovieRegion graphic;
        if (!view.movies.Region(movie, 1, 0, graphic)) { return false; }
        float x = region.x + region.width / 2 - (graphic.width * 2 + 8) / 2;
        for (unsigned position = 0; position < 2; ++position) {
            const auto *entry = OriginalMenuData("MDS_BUTTON_REFINE_SLOT_CATEGORY", 1 - position);
            if (entry == nullptr) { return false; }
            MovieRegion area = region;
            area.x = x;
            bool pressed = false;
            unsigned chapter = 2;
            if (state.refineryTab == entry->index) { chapter = 3; }
            if (!DrawOriginalMovieButton(view, *entry, area, view.movies.NamedString(entry->strings[0]), 5,
                interactive && state.refineryTransfer < 0, pressed, chapter, state.refineryElapsed)) { return false; }
            if (pressed && state.refineryTab != entry->index) {
                // DoAction :95106 forwards argument4 (entry index), not the
                // table's parameter. Refresh(76) changes the category only.
                state.refineryTab = entry->index;
            }
            x += graphic.width + 4;
        }
        return true;
    }

    bool Xplodium(const MovieRegion &region) {
        const auto *entry = OriginalMenuData("MDS_ICON_STANDARD", 0);
        if (entry == nullptr) { return false; }
        const unsigned sprite = entry->sprites[0];
        MovieRegion bounds;
        if (!view.movies.SpriteBounds(sprite >> 16, sprite & 255, bounds)) { return false; }
        if (!view.movies.DrawSprite(sprite >> 16, sprite & 255, state.refineryElapsed,
            region.x + bounds.width / 2, region.y + region.height / 2, 1, region.alpha)) { return false; }
        const float x = region.x + bounds.width;
        view.movies.Text(view.movies.NamedString(entry->strings[0]), x, region.y, 0, 1, 0, region.alpha);
        std::string amount = std::to_string(static_cast<std::int32_t>(profile.xplodium));
        if (state.refineryTransfer >= 0 && profile.refinery.slots[state.refineryTransfer].state == 1) { amount = "0"; }
        view.movies.Text(amount, x, region.y + region.height - view.movies.TextHeight(0), 0, 1, 0, region.alpha);
        return true;
    }

    bool MeterInfo(const MovieRegion &region) {
        const unsigned slot = state.refineryTab * count + region.index - count;
        // CreateContentSprite(69,1) :149765 uses core archetype0,
        // SG_TEXTBOX_ANIM=138 at VA0x3c4946, independent of the slot state.
        if (!view.movies.DrawSprite(0, 138, 0, region.x, region.y, 1, region.alpha)) { return false; }
        std::string title;
        if (data.minutes[slot] == 0) { title = view.movies.NamedString("IDS_RESMAN_INTERVAL0"); }
        else if (data.minutes[slot] < 60) { title = std::to_string(data.minutes[slot]) + " MINS"; }
        else { title = std::to_string(data.minutes[slot] / 60) + " HRS"; }
        unsigned font = 1;
        std::string detail = RefineryNumber(view, "IDS_RESMAN_YIELD", data.efficiencyPercent[slot]);
        const auto &record = profile.refinery.slots[slot];
        std::uint64_t amount = profile.xplodium;
        if (record.state == 2 || record.state == 3) { amount = record.amount; }
        // Native offline profile has no validated friend power. Provider 69/3
        // alternates only on enabled meters, and only for a nonzero payout.
        if (data.minutes[slot] == 0 && amount != 0 && (state.refineryElapsed / 2000) % 2 != 0) {
            const auto payout = static_cast<std::uint64_t>(std::floor(static_cast<float>(amount) * data.efficiencyPercent[slot] / 100.0f + 0.5f));
            detail = RefineryNumber(view, "IDS_SHOP_COMMON", payout);
            font = 0;
        }
        if (title.empty() || detail.empty()) { return false; }
        view.movies.Text(title, region.x + (region.width - view.movies.TextWidth(title, 0)) / 2, region.y, 0, 1, 0, region.alpha);
        view.movies.Text(detail, region.x + (region.width - view.movies.TextWidth(detail, font)) / 2,
            region.y + region.height - view.movies.TextHeight(font), font, 1, 0, region.alpha);
        return true;
    }

    bool Meter(const MovieRegion &region) {
        const unsigned slot = state.refineryTab * count + region.index;
        const auto &record = profile.refinery.slots[slot];
        const auto *entry = OriginalMenuData("MDS_BUTTON_XPLODIUM_METER", slot);
        if (entry == nullptr) { return false; }
        const float x = region.x + region.width / 2, y = region.y + region.height / 2;
        const bool enabled = data.minutes[slot] == 0;
        const unsigned fill = view.movies.Ordinal("GLU_MOVIE_BUCKET_FILL");
        const unsigned status = view.movies.Ordinal(entry->movies[1]);
        if (!view.movies.Draw(fill, state.refineryFillTime[slot], x, y)) { return false; }
        {
            // The native button movie is centered on the meter. Its own hit
            // region owns interaction; the 220px parent is not a click target.
            MovieRegion button = region;
            button.x = x;
            button.y = y;
            bool pressed = false;
            unsigned chapter = 0;
            if (record.state != 0 && enabled) { chapter = 2; }
            // Enabled(false) sets button state6, which still draws; Draw skips
            // only state8 (:144707). Keep the original circle behind the lock.
            if (!DrawOriginalMovieButton(view, *entry, button, {}, 0,
                enabled && record.state != 0 && interactive && state.refineryTransfer < 0,
                pressed, chapter, state.refineryElapsed * 2)) { return false; }
            if (pressed && (record.state == 1 || (record.state == 3 && state.refineryStatusChapter[slot] == 3))) {
                if (record.state == 3 || profile.xplodium != 0) {
                    const unsigned main = view.movies.Ordinal("GLU_MOVIE_EXPLODIUM");
                    MovieRegion source, destination;
                    unsigned icon = 0;
                    if (record.state == 3) {
                        source.x = x;
                        source.y = y;
                        if (!view.movies.Region(main, count * 2 + 2, state.refineryTime, destination)) { return false; }
                        icon = 2;
                        state.refineryTransferAmount = profile.refinery.GetRefinementSlotYield(slot);
                    } else {
                        if (!view.movies.Region(main, count * 2, state.refineryTime, source)) { return false; }
                        const auto *image = OriginalMenuData("MDS_ICON_STANDARD", 0);
                        MovieRegion bounds;
                        if (image == nullptr || !view.movies.SpriteBounds(image->sprites[0] >> 16, image->sprites[0] & 255, bounds)) { return false; }
                        source.x += bounds.width / 2;
                        source.y += bounds.height / 2;
                        destination.x = x;
                        destination.y = y;
                        state.refineryTransferAmount = profile.xplodium;
                    }
                    const auto *image = OriginalMenuData("MDS_ICON_STANDARD", icon);
                    if (image == nullptr) { return false; }
                    state.refineryTransfer = static_cast<int>(slot);
                    state.refineryTransferTime = 0;
                    state.refineryTransferSprite = image->sprites[0];
                    state.refineryTransferX = source.x;
                    state.refineryTransferY = source.y;
                    state.refineryTargetX = destination.x;
                    state.refineryTargetY = destination.y;
                }
            }
        }
        if (!view.movies.Draw(status, state.refineryStatusTime[slot], x, y)) { return false; }
        if (record.state == 0) {
            // CreateContentSprite(69,0) :149780, original SG constants:
            // archetype4, first animation24, count6. Index chooses lock art.
            // CResourceMeter::Update advances this sprite only AFTER unlock.
            // Its initial colored chamber stays frozen while state==0.
            if (!view.movies.DrawSprite(4, 24 + slot % 6, 0, x, y, 1, region.alpha)) { return false; }
            const unsigned overlay = view.movies.Ordinal("GLU_MOVIE_CHAMBER_OVERLAY");
            const CMovie *movie = view.movies.GetMovie(overlay);
            unsigned start = 0, end = 0;
            if (movie == nullptr || !movie->GetChapterRange(0, start, end) ||
                !view.movies.Draw(overlay, start + state.refineryElapsed % (end - start + 1), x, y)) { return false; }
        }
        std::string text;
        if (!enabled) { text = view.movies.NamedString("IDS_FRIEND_OFFLINE"); }
        else if (record.state == 3) { text = view.movies.NamedString("IDS_RESMAN_COLLECT"); }
        else if (record.state == 1 && profile.xplodium != 0) { text = view.movies.NamedString("IDS_RESMAN_READY"); }
        if (!text.empty()) {
            view.movies.Text(text, x - view.movies.TextWidth(text, 0) / 2, y - view.movies.TextHeight(0) / 2, 0, 1, 0, region.alpha);
        }
        return true;
    }
    GameMenu &view;
    MenuState &state;
    CProfileManager &profile;
    const CRefinementManager::Template &data;
    unsigned count;
    bool interactive;
};

bool DrawRefinery(GameMenu &view, MenuState &state, CProfileManager &profile,
    const CRefinementManager::Template &data, const std::filesystem::path &savePath, std::int64_t now) {
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_EXPLODIUM");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    const CMovie *fill = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_BUCKET_FILL"));
    unsigned idleStart = 0, idleEnd = 0;
    if (movie == nullptr || fill == nullptr || !movie->GetChapterRange(1, idleStart, idleEnd)) { return false; }
    const auto regions = view.movies.Regions(ordinal, idleStart);
    if (regions.size() < 5 || (regions.size() - 5) % 2 != 0) { return false; }
    const unsigned count = static_cast<unsigned>((regions.size() - 5) / 2);
    if (count * 2 != data.minutes.size()) { return false; }
    if (!state.refineryBound) {
        state.refineryBound = true;
        state.refineryTab = 1;
        state.refineryTime = 0;
        state.refineryElapsed = 0;
        state.refineryLastTick = view.clock;
        for (unsigned slot = 0; slot < count * 2; ++slot) {
            unsigned chapter = 0;
            if (data.minutes[slot] == 0) { chapter = profile.refinery.slots[slot].state; }
            if (!SetRefineryStatus(view, state, slot, chapter)) { return false; }
            state.refineryFillTime[slot] = 0;
        }
    }
    const unsigned delta = static_cast<unsigned>(view.clock - state.refineryLastTick);
    state.refineryLastTick = view.clock;
    state.refineryElapsed += delta;
    const std::uint64_t next = static_cast<std::uint64_t>(state.refineryTime) + delta;
    state.refineryTime = static_cast<unsigned>(next);
    if (next > idleEnd) { state.refineryTime = idleStart + static_cast<unsigned>((next - idleStart) % (idleEnd - idleStart + 1)); }
    if (!view.animateNavigation) { state.refineryTime = idleStart; }
    if (state.refineryTransfer >= 0) {
        state.refineryTransferTime += delta;
        // CTransferEffect::Setup :174380: original linear x/y duration375ms.
        if (state.refineryTransferTime >= 375) {
            const unsigned slot = static_cast<unsigned>(state.refineryTransfer);
            const unsigned status = profile.refinery.slots[slot].state;
            bool changed = false;
            if (status == 1) {
                changed = profile.refinery.BeginRefinement(slot, slot, profile.xplodium, profile.xplodium, now);
                if (!SetRefineryStatus(view, state, slot, 2)) { return false; }
            } else if (status == 3) {
                changed = profile.refinery.CollectResources(slot, profile.coins);
                if (!SetRefineryStatus(view, state, slot, 1)) { return false; }
            }
            if (!changed || !profile.SaveToDisk(savePath)) { return false; }
            state.refineryFillTime[slot] = 0;
            state.refineryTransfer = -1;
            bool hasReady = false;
            for (unsigned index = 0; index < count * 2; ++index) {
                if (data.minutes[index] == 0 && profile.refinery.slots[index].state == 3) { hasReady = true; }
            }
            if (profile.xplodium == 0 && !hasReady) { state.refinementRequired = false; }
            std::printf("[refinery] transfer complete slot=%u old-state=%u new-state=%u\n", slot, status, profile.refinery.slots[slot].state);
        }
    }
    for (unsigned slot = 0; slot < count * 2; ++slot) {
        const auto *entry = OriginalMenuData("MDS_BUTTON_XPLODIUM_METER", slot);
        if (entry == nullptr) { return false; }
        const auto *status = view.movies.GetMovie(view.movies.Ordinal(entry->movies[1]));
        unsigned start = 0, end = 0;
        const unsigned chapter = state.refineryStatusChapter[slot];
        if (status == nullptr || !status->GetChapterRange(chapter, start, end)) { return false; }
        unsigned time = state.refineryStatusTime[slot];
        if (chapter >= 2) { time = start + (time - start + delta) % (end - start + 1); }
        else { time += std::min(delta, end - time); }
        state.refineryStatusTime[slot] = time;
        if (profile.refinery.slots[slot].state == 3) {
            state.refineryFillTime[slot] += std::min(delta * 2, fill->duration - state.refineryFillTime[slot]);
            if (chapter == 2 && state.refineryFillTime[slot] == fill->duration &&
                !SetRefineryStatus(view, state, slot, 3)) { return false; }
        }
    }
    if (!view.movies.DrawNamed("GLU_MOVIE_EXPLODIUM_BG", state.refineryElapsed)) { return false; }
    RefineryCallbacks callbacks(view, state, profile, data, count, state.refineryTime >= idleStart);
    if (!view.movies.Draw(ordinal, state.refineryTime, 512, 384, 1024, 768, 0, 1, &callbacks)) { return false; }
    if (state.refineryTransfer >= 0) {
        const float fraction = std::min(1.0f, state.refineryTransferTime / 375.0f);
        const float x = std::trunc(state.refineryTransferX + (state.refineryTargetX - state.refineryTransferX) * fraction);
        const float y = std::trunc(state.refineryTransferY + (state.refineryTargetY - state.refineryTransferY) * fraction);
        const unsigned sprite = state.refineryTransferSprite;
        MovieRegion bounds;
        float alpha = 1;
        if (state.refineryTransferTime > 188) { alpha -= std::min(0.5f, (state.refineryTransferTime - 188) / 374.0f); }
        if (!view.movies.SpriteBounds(sprite >> 16, sprite & 255, bounds) ||
            !view.movies.DrawSprite(sprite >> 16, sprite & 255, state.refineryTransferTime, x, y, 1, alpha)) { return false; }
        view.movies.Text(std::to_string(static_cast<std::int32_t>(state.refineryTransferAmount)), x + bounds.width / 2, y, 0, 1, 0, alpha);
    }
    return true;
}

/** CMenuFriends::Bind :197028 and CMenuChallenges::Bind :236612 select
 * chapter 1 while profile validity is false. Region 0 owns button 165/0,
 * region 1 owns centered font-0 text. No host flag can validate an NGS user.
 * ui_movie.bt and MENU_CHALLENGES VA 0x402eb0 identify the original Movie. */
bool DrawOriginalSocialOffline(GameMenu &view, MenuState &state, bool hasCredentials) {
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_OFFLINE_BROHOOD");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return false; }
    if (!state.socialBound) {
        state.socialBound = true;
        state.socialTime = start;
        state.socialLastTick = view.clock;
    }
    const auto elapsed = view.clock - state.socialLastTick;
    state.socialLastTick = view.clock;
    state.socialTime = start + static_cast<unsigned>((state.socialTime - start + elapsed) % (end - start + 1));
    if (!view.movies.Draw(ordinal, state.socialTime)) { return false; }
    const char *table = "MDS_OFFLINE_CHALLENGES";
    if (state.page == 4) { table = "MDS_OFFLINE_FRIENDS"; }
    // GetElementValueInt32(81) :150866: invalid+credentials=>1, absent=>2;
    // each menu subtracts one to index its two original description strings.
    unsigned description = 1;
    if (hasCredentials) { description = 0; }
    const auto *entry = OriginalMenuData(table, description);
    const auto *button = OriginalMenuData("MDS_BUTTON_CONNECTIVITY", 0);
    if (entry == nullptr || button == nullptr) { return false; }
    for (const auto &region : view.movies.Regions(ordinal, state.socialTime)) {
        if (region.index == 0) {
            bool pressed = false;
            if (!DrawOriginalMovieButton(view, *button, region, view.movies.NamedString(button->strings[0]), 6, true, pressed)) { return false; }
            if (pressed) {
                // Action 86 requests NGS connectivity. The absent remote service
                // cannot mint a validated identity, friendship or reward locally.
                std::printf("[social] connectivity action=%u unavailable; profile remains offline\n", button->action);
            }
        }
        if (region.index == 1) {
            const auto lines = FormatStoreText(view.movies, view.movies.NamedString(entry->strings[0]), region.width, {0, 0, 0, 0, 0});
            StoreRegionClip clip(view, region);
            float y = region.y;
            for (const auto &line : lines) {
                const float x = region.x + (region.width - line.width) / 2;
                for (const auto &run : line.runs) {
                    view.movies.Text(run.text, x + run.x, y, run.font, 1, 0, region.alpha);
                }
                y += line.height;
            }
        }
    }
    return true;
}

/** CMenuDataProvider::CreateContentString :151507 resolves the action only
 * when the corresponding original MDS string slot is null. */
std::string OptionsText(GameMenu &view, const CProfileManager &profile, unsigned index, unsigned slot, const char *table = "MDS_OPTIONS") {
    const auto *entry = OriginalMenuData(table, index);
    if (entry == nullptr || slot >= 4) { return {}; }
    if (entry->strings[slot][0] != 0) { return view.movies.NamedString(entry->strings[slot]); }
    const char *name = nullptr;
    switch (entry->action) {
    case 9:
        name = "IDS_SOUND_OFF_GAME";
        if (profile.soundEnabled) { name = "IDS_SOUND_ON_GAME"; }
        break;
    case 10:
        name = "IDS_MUSIC_OFF_GAME";
        if (profile.musicEnabled) { name = "IDS_MUSIC_ON_GAME"; }
        break;
    case 17:
        if (profile.options.AutoBro() == 0) { name = "IDS_BROTHER_AUTOSELECT_OFF"; }
        if (profile.options.AutoBro() == 1) { name = "IDS_BROTHER_AUTOSELECT_ON"; }
        if (profile.options.AutoBro() == 2) { name = "IDS_BROTHER_AUTOSELECT_ASK"; }
        break;
    case 18:
        // Utility::LoadAboutText :105469 and GetTimestampString :52021.
        // Bundle version metadata and remote identity are absent from this set.
        return view.movies.NamedString("IDS_ABOUT_TEXT_FORMATTED") +
            view.movies.NamedString("IDS_COPYRIGHT_TEXT_FORMATTED") +
            "\n\n\n\n\nName: GUNBROS_20121104-163000 \nNov  4 2012 16:52:55";
    case 19:
        name = "IDS_SAVE_STATUS_BODY1"; // Offline profile has not synced to NGS.
        break;
    case 20:
        name = "IDS_FACEBOOK_LOGIN"; // No remote login is synthesized.
        break;
    case 77:
        name = "IDS_NOTIF_TOGGLE_OFF_GAME";
        if (profile.options.NotificationsEnabled()) { name = "IDS_NOTIF_TOGGLE_ON_GAME"; }
        break;
    case 113:
        name = "IDS_CHALLENGE_PUSH_TOGGLE_OFF_GAME";
        if (profile.pushChallenges) { name = "IDS_CHALLENGE_PUSH_TOGGLE_ON_GAME"; }
        break;
    }
    if (name == nullptr) { return {}; }
    return view.movies.NamedString(name);
}

/** MENU_OPTIONS at original VA 0x402e50 selects LIST_MENU, list offset 2,
 * bounds 1/1, LIST_MENU_BUTTON, LIST_MENU_TEXT; all geometry stays in BIG.
 * CMenuList :140175..140657, CMenuListOption :144029..144317, ui_movie.bt. */
bool DrawOptions(GameMenu &view, MenuState &state, CProfileManager &profile, bool &saveChanged) {
    const char *table = "MDS_OPTIONS";
    if (state.page == 8) { table = "MDS_HELP"; }
    const unsigned list = view.movies.Ordinal("GLU_MOVIE_LIST_MENU");
    const unsigned button = view.movies.Ordinal("GLU_MOVIE_LIST_MENU_BUTTON");
    const unsigned textMovie = view.movies.Ordinal("GLU_MOVIE_LIST_MENU_TEXT");
    const CMovie *listData = view.movies.GetMovie(list);
    const CMovie *buttonData = view.movies.GetMovie(button);
    const CMovie *textData = view.movies.GetMovie(textMovie);
    unsigned start = 0, end = 0, focusStart = 0, focusEnd = 0, bodyStart = 0, bodyEnd = 0;
    unsigned restStart = 0, restEnd = 0;
    if (listData == nullptr || buttonData == nullptr || textData == nullptr ||
        !listData->GetChapterRange(1, start, end) || !buttonData->GetChapterRange(2, focusStart, focusEnd) ||
        !buttonData->GetChapterRange(0, restStart, restEnd) ||
        !textData->GetChapterRange(0, bodyStart, bodyEnd) || end <= start) { return false; }
    unsigned count = 0;
    while (OriginalMenuData(table, count) != nullptr) { ++count; }
    if (count < 3) { return false; }
    unsigned elapsed = 0;
    if (!state.optionsBound) {
        state.optionsBound = true;
        state.optionsOpening = 0;
        if (!view.animateNavigation) { state.optionsOpening = start; }
        state.optionsButtonTimes.assign(count, 0);
        state.optionsFocus = std::min(state.optionsFocus, count - 1);
        state.optionsButtonTimes[state.optionsFocus] = focusStart;
        state.optionsTarget = std::clamp(state.optionsScroll, 0.0f, float(count - 3));
    } else if (view.clock >= state.optionsLastTick) {
        elapsed = static_cast<unsigned>(view.clock - state.optionsLastTick);
    }
    state.optionsLastTick = view.clock;
    state.optionsOpening = std::min(start, state.optionsOpening + elapsed);
    state.optionsBodyTime = std::min(bodyEnd, state.optionsBodyTime + elapsed);
    MovieRegion input, content, scrollBar;
    if (!view.movies.Region(list, 0, start, input) || !view.movies.Region(list, 8, start, content) ||
        !view.movies.Region(list, 9, start, scrollBar)) { return false; }
    // CalculateBaseVelocity :141749 averages only the moving region centers.
    // Wheel movement is a Windows input adapter expressed in original options.
    float distance = 0;
    unsigned moving = 0;
    const auto before = view.movies.Regions(list, start);
    const auto after = view.movies.Regions(list, end);
    for (const auto &region : before) {
        if (region.type < 2) { continue; }
        for (const auto &last : after) {
            if (last.index != region.index) { continue; }
            const float difference = region.y + region.height / 2 - last.y - last.height / 2;
            if (difference != 0) { distance += difference; ++moving; }
        }
    }
    if (moving == 0 || distance == 0) { return false; }
    distance = std::abs(distance / moving);
    const float wheel = view.window.TakeWheelDelta();
    if (view.inputEnabled && state.optionsOpening == start && view.MouseIn(input.x, input.y, input.width, input.height)) {
        if (view.dragY != 0) {
            state.optionsScroll = std::clamp(state.optionsScroll - view.dragY / distance, 0.0f, float(count - 3));
            state.optionsTarget = std::round(state.optionsScroll);
        }
        if (wheel != 0) { state.optionsTarget = std::clamp(state.optionsTarget - wheel, 0.0f, float(count - 3)); }
    }
    if (view.dragY == 0) {
        const float step = float(elapsed) / (end - start + 1);
        if (state.optionsScroll < state.optionsTarget) { state.optionsScroll = std::min(state.optionsTarget, state.optionsScroll + step); }
        else { state.optionsScroll = std::max(state.optionsTarget, state.optionsScroll - step); }
    }
    state.optionsScroll = std::clamp(state.optionsScroll, 0.0f, float(count - 3));
    const int base = static_cast<int>(std::floor(state.optionsScroll)) - 1;
    unsigned time = state.optionsOpening;
    if (time == start) { time += static_cast<unsigned>((state.optionsScroll - std::floor(state.optionsScroll)) * (end - start)); }
    if (!view.movies.DrawNamed("GLU_MOVIE_BG_OPTIONS", static_cast<unsigned>(view.clock)) ||
        !view.movies.Draw(list, time)) { return false; }
    if (state.page == 8) {
        // MENU_HELP uses the same LIST_MENU but binds BACK at the widget slot.
        const auto regions = view.movies.Regions(list, time, 512, 384, true);
        if (regions.size() < 3) { return false; }
        const auto *back = OriginalMenuData("MDS_BUTTON_BACK", 0);
        for (auto region : regions) {
            if (region.index != regions.size() - 3) { continue; }
            region.x += region.width / 2;
            region.y += region.height / 2;
            bool pressed = false;
            if (!back || !DrawOriginalMovieButton(view, *back, region, {}, 0,
                state.optionsOpening == start, pressed)) { return false; }
            if (pressed) { state.Back(); return true; }
        }
    }
    int selected = -1;
    for (const auto &region : view.movies.Regions(list, time)) {
        if (region.type < 2) { continue; }
        const int index = base + static_cast<int>(region.type) - 2;
        if (index < 0 || index >= static_cast<int>(count)) { continue; }
        unsigned &buttonTime = state.optionsButtonTimes[index];
        if (state.optionsFocus == static_cast<unsigned>(index)) {
            buttonTime = focusStart + (buttonTime - focusStart + elapsed) % (focusEnd - focusStart + 1);
        } else { buttonTime = std::min(restEnd, buttonTime + elapsed); }
        const float x = region.x + static_cast<int>(region.width) / 2;
        const float y = region.y + static_cast<int>(region.height) / 2;
        if (!view.movies.Draw(button, buttonTime, x, y)) { return false; }
        for (const auto &part : view.movies.Regions(button, buttonTime, x, y)) {
            if (part.index == 1) {
                view.movies.Text(OptionsText(view, profile, index, 0, table), part.x,
                    part.y + static_cast<int>(part.height) / 2 - static_cast<int>(view.movies.TextHeight(0)) / 2,
                    0, 1, 0, region.alpha * part.alpha);
            }
            if (part.index == 0 && state.optionsOpening == start &&
                view.Hit(part.x, part.y, part.width, part.height)) { selected = index; }
        }
    }
    if (selected >= 0) {
        if (state.optionsFocus != static_cast<unsigned>(selected)) {
            state.optionsButtonTimes[state.optionsFocus] = 0;
            state.optionsFocus = selected;
            state.optionsButtonTimes[selected] = focusStart;
            state.optionsTarget = std::clamp(float(selected - 1), 0.0f, float(count - 3));
            state.optionsBodyTime = 0;
            state.optionsScrollbarTime = 0;
            state.optionsBodyScroll = 0;
        }
        const auto *entry = OriginalMenuData(table, selected);
        switch (entry->action) {
        case 9: profile.soundEnabled = !profile.soundEnabled; saveChanged = true; break;
        case 10: profile.musicEnabled = !profile.musicEnabled; saveChanged = true; break;
        case 17:
            profile.options.CycleAutoBro();
            profile.brotherEnabled = profile.options.AutoBro() != 0;
            saveChanged = true;
            break;
        case 77: profile.options.ToggleNotifications(); saveChanged = true; break;
        case 113: profile.pushChallenges = !profile.pushChallenges; saveChanged = true; break;
        case 19: saveChanged = true; break;
        case 1: state.Navigate(8); return true;
        case 79: state.Navigate(29); break;
        case 20:
            // DoAction 20 :93872 requests the platform login/logout; no account page.
            std::printf("[options] Facebook platform unavailable; profile unchanged\n");
            break;
        }
    }
    // CMenuList::Bind :140481 installs fonts 0 and 6. The original body already
    // contains its title and font controls; never synthesize a second heading.
    const auto lines = FormatStoreText(view.movies, OptionsText(view, profile, state.optionsFocus, 1, table),
        content.width - scrollBar.width, {0, 6, 0, 0, 0});
    MovieRegion pageBounds;
    if (!view.movies.Region(textMovie, 2, 0, pageBounds) || pageBounds.height <= 0) { return false; }
    std::vector<std::vector<StoreTextLine>> pages(1);
    float pageHeight = 0;
    for (const auto &line : lines) {
        if (!pages.back().empty() && pageHeight + line.height > pageBounds.height) {
            pages.push_back({});
            pageHeight = 0;
        }
        pages.back().push_back(line);
        pageHeight += line.height;
    }
    const float maxPage = static_cast<float>(pages.size() - 1);
    if (view.inputEnabled && view.MouseIn(content.x, content.y, content.width, content.height)) {
        state.optionsBodyScroll = std::clamp(state.optionsBodyScroll - view.dragY / pageBounds.height - wheel,
            0.0f, maxPage);
    }
    state.optionsBodyScroll = std::clamp(state.optionsBodyScroll, 0.0f, maxPage);
    const int firstPage = static_cast<int>(std::floor(state.optionsBodyScroll));
    unsigned textStart = 0, textEnd = 0;
    if (!textData->GetChapterRange(1, textStart, textEnd)) { return false; }
    unsigned textTime = state.optionsBodyTime;
    if (state.optionsBodyTime == bodyEnd) {
        textTime = textStart + static_cast<unsigned>((state.optionsBodyScroll - firstPage) * (textEnd - textStart));
    }
    if (!view.movies.Draw(textMovie, textTime, content.x, content.y)) { return false; }
    MovieRegion animatedContent;
    if (!view.movies.Region(list, 8, time, animatedContent)) { return false; }
    {
        StoreRegionClip clip(view, animatedContent);
        for (const auto &region : view.movies.Regions(textMovie, textTime, content.x, content.y)) {
            if (region.type < 2) { continue; }
            const int pageIndex = firstPage + static_cast<int>(region.type) - 2;
            if (pageIndex < 0 || pageIndex >= static_cast<int>(pages.size())) { continue; }
            float y = region.y;
            for (const auto &line : pages[pageIndex]) {
                for (const auto &run : line.runs) {
                    view.movies.Text(run.text, region.x + run.x, y + (line.height - run.height) / 2,
                        run.font, 1, 0, animatedContent.alpha * region.alpha);
                }
                y += line.height;
            }
        }
    }
    if (pages.size() > 1) {
        // ScrollBarCallback :140195 centers the original scrollbar by its
        // own region-0 height. SetProgress :221526 scales by Movie duration.
        const auto *entry = OriginalMenuData("MDS_SCROLLBARS", 1);
        if (entry == nullptr) { return false; }
        const unsigned ordinal = view.movies.Ordinal(entry->movies[0]);
        const CMovie *bar = view.movies.GetMovie(ordinal);
        MovieRegion bounds;
        if (bar == nullptr || !view.movies.Region(ordinal, 0, 0, bounds)) { return false; }
        const unsigned target = static_cast<unsigned>(bar->duration * state.optionsBodyScroll / maxPage);
        const unsigned advance = static_cast<unsigned>(elapsed * 1.2f); // SetItemCount(1) :221427 => 2 - 4/5.
        if (state.optionsScrollbarTime < target) { state.optionsScrollbarTime = std::min(target, state.optionsScrollbarTime + advance); }
        else { state.optionsScrollbarTime -= std::min(advance, state.optionsScrollbarTime - target); }
        if (!view.movies.Draw(ordinal, state.optionsScrollbarTime, scrollBar.x,
            scrollBar.y + static_cast<int>(scrollBar.height) / 2 - static_cast<int>(bounds.height) / 2)) { return false; }
    }
    return true;
}

/** CMenuPlayerSelect :194878..195343; original Movie70 owns both portraits,
 * highlights, chapter timings, title and touch rectangles. */
bool DrawOriginalPlayerSelect(GameMenu &view, MenuState &state, CProfileManager &profile,
    const std::filesystem::path &savePath, bool &launchTutorial) {
    launchTutorial = false;
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_PLAYER_SELECT");
    const auto *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(state.playerSelectChapter, start, end)) { return false; }
    if (!state.playerSelectBound) {
        state.playerSelectBound = true;
        state.playerSelectReady = false;
        state.playerSelection = -1;
        state.playerSelectChapter = 1;
        state.playerSelectTime = 0;
        state.playerSelectLastTick = view.clock;
        if (!movie->GetChapterRange(1, start, end)) { return false; }
    }
    const unsigned delta = static_cast<unsigned>(view.clock - state.playerSelectLastTick);
    state.playerSelectLastTick = view.clock;
    const auto next = static_cast<std::uint64_t>(state.playerSelectTime) + delta;
    const bool finished = next > end;
    if (finished && state.playerSelection < 0) { state.playerSelectReady = true; }
    state.playerSelectTime = static_cast<unsigned>(std::min<std::uint64_t>(next, end));
    if (!view.animateNavigation && state.playerSelection < 0) { state.playerSelectTime = end; }
    if (finished && state.playerSelection >= 0) {
        state.playerSelectBound = false;
        if (state.page == 25) { launchTutorial = true; }
        else { state.Navigate(6, true); }
        return true;
    }
    class SelectTitle : public IMovieRegionCallback {
    public:
        explicit SelectTitle(GameMenu &menu) : view(menu) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index != 0) { return true; }
            const auto title = view.movies.NamedString("IDS_PLAYER_SELECT");
            return view.movies.Text(title, region.x + (region.width - view.movies.TextWidth(title, 6)) / 2,
                region.y + (region.height - view.movies.TextHeight(6)) / 2, 6, 1, 0, region.alpha);
        }
        GameMenu &view;
    } title(view);
    view.movies.Rectangle(0, 0, kMenuWidth, kMenuHeight, 0, 0, 0);
    if (!view.movies.Draw(ordinal, state.playerSelectTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &title)) { return false; }
    const bool ready = state.playerSelection < 0 && (state.playerSelectReady || !view.animateNavigation);
    for (unsigned index = 0; index < 2; ++index) {
        MovieRegion bounds;
        // Bind/GetUserRegion(...,true) retains the logical, unscaled rectangles.
        if (!view.movies.Region(ordinal, index + 1, 0, bounds)) { return false; }
        if (ready && view.Hit(bounds.x, bounds.y, bounds.width, bounds.height)) {
            state.playerSelection = static_cast<int>(index);
            state.playerSelectChapter = 2;
            if (index != 0) { state.playerSelectChapter = 4; }
            if (!movie->GetChapterRange(state.playerSelectChapter, start, end)) { return false; }
            state.playerSelectTime = start;
            // CMenuAction 0x4E :94882 sets both brothers and clears firstLaunch.
            profile.playerBrother = index;
            profile.firstLaunch = false;
            if (!profile.SaveToDisk(savePath)) { return false; }
        }
    }
    view.inputEnabled = false; // Native selection owns input until its chapter completes.
    return true;
}

/** CMenuGreeting callbacks :207876..208207. Local daily rewards are the
 * user-authorized clock adapter; social data keeps the native offline branch. */
class GreetingCallbacks : public IMovieRegionCallback {
public:
    GreetingCallbacks(GameMenu &menu, MenuState &state, CResTOCManager &toc, PackTables &tables,
        const CDailyBonusTracking &daily, const CProfileManager &profile, bool interactive) :
        view(menu), state(state), toc(toc), tables(tables), daily(daily), profile(profile), interactive(interactive) {}
    bool DrawMovieRegion(const MovieRegion &region) override {
        if (region.index <= 2) {
            const auto *entry = OriginalMenuData("MDS_GREETING_STRINGS", region.index);
            if (entry == nullptr) { return false; }
            const auto text = view.movies.NamedString(entry->strings[0]);
            return view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, 6)) / 2,
                region.y, 6, 1, 0, region.alpha);
        }
        if (region.index == 3 || region.index == 4) {
            const auto *entry = OriginalMenuData("MDS_BUTTON_GREETING_REDIRECTS", region.index - 3);
            if (entry == nullptr) { return false; }
            MovieRegion origin = region;
            origin.x += region.width / 2;
            origin.y += region.height / 2;
            bool pressed = false;
            if (!DrawOriginalMovieButton(view, *entry, origin, {}, 0, interactive, pressed)) { return false; }
            if (pressed) {
                if (entry->parameter == 4) { state.Navigate(5, true); }
                else if (entry->parameter == 3) { state.Navigate(4, true); }
                else { return false; }
            }
        }
        if (region.index == 5 || region.index == 7) {
            unsigned index = 0;
            if (region.index == 7) { index = 1; }
            const auto *entry = OriginalMenuData("MDS_OFFLINE_GREETING", index);
            if (entry == nullptr) { return false; }
            MovieRegion box = region;
            box.height *= 5; // Original offline callback allocates five rows.
            DrawStoreTemplate(view, "^f4" + view.movies.NamedString(entry->strings[0]), box, {});
        }
        if (region.index >= 9 && region.index <= 13) {
            const unsigned index = region.index - 9;
            if (index >= daily.prizes.size()) { return false; }
            const auto &prize = daily.prizes[index];
            StoreEntry icon;
            icon.data.assets[1] = prize.image;
            if (!view.Icon(toc, tables, icon, region.x, region.y, region.width, region.height,
                region.alpha, false, true)) { return false; }
            // CreateRewardQuantityString :209417, currency priority and XP text.
            std::string quantity;
            if (prize.warbucks != 0) { quantity = "X" + std::to_string(prize.warbucks); }
            else if (prize.coins != 0) { quantity = "X" + std::to_string(prize.coins); }
            else if (prize.experience != 0) { quantity = std::to_string(prize.experience); }
            if (!quantity.empty() && !view.movies.Text(quantity,
                region.x + (region.width - view.movies.TextWidth(quantity, 0)) / 2,
                region.y + region.height - view.movies.TextHeight(0), 0, 1, 0, region.alpha)) { return false; }
        }
        if (region.index >= 14 && region.index <= 18 && profile.dailyConsecutiveDays != 0) {
            const unsigned reward = (profile.dailyConsecutiveDays - 1) % static_cast<unsigned>(daily.prizes.size());
            if (region.index - 14 <= reward && !view.movies.DrawSprite(7, 1, state.greetingElapsed,
                region.x + region.width / 2, region.y + region.height / 2, 1, region.alpha)) { return false; }
        }
        return true;
    }
    GameMenu &view;
    MenuState &state;
    CResTOCManager &toc;
    PackTables &tables;
    const CDailyBonusTracking &daily;
    const CProfileManager &profile;
    bool interactive;
};

bool DrawOriginalGreeting(GameMenu &view, MenuState &state, CResTOCManager &toc, PackTables &tables,
    CProfileManager &profile, const CDailyBonusTracking &daily, const std::vector<StoreEntry> &store,
    CPlayerProgress &progress, const std::filesystem::path &savePath, std::int64_t seconds) {
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_WELCOME_NEW");
    const auto *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, start, end) || daily.prizes.empty()) { return false; }
    if (!state.greetingBound) {
        state.greetingBound = true;
        state.greetingClosing = false;
        state.greetingExitRequested = false;
        state.greetingTime = 0;
        state.greetingElapsed = 0;
        state.greetingLastTick = view.clock;
        daily.RefreshUsageData(profile, static_cast<std::uint32_t>(seconds));
    }
    const unsigned delta = static_cast<unsigned>(view.clock - state.greetingLastTick);
    state.greetingLastTick = view.clock;
    state.greetingElapsed += delta;
    if (state.greetingExitRequested && !state.greetingClosing) {
        // OnExit :208279 invokes action95 once. SetChapter(1,true) seeks the
        // chapter START before SetReverse(true); playback then ends at zero.
        if (daily.IsBonusAvailable(profile, seconds)) {
            if (!daily.CommitBonus(profile, seconds, store) || !profile.SaveToDisk(savePath)) { return false; }
            progress.SetExperience(profile.experience);
        }
        state.greetingClosing = true;
        state.greetingTime = start;
    } else if (state.greetingClosing) {
        state.greetingTime -= std::min(delta, state.greetingTime);
        if (state.greetingTime == 0) {
            state.greetingBound = false;
            state.Navigate(state.greetingTarget, true);
            return true;
        }
    } else {
        state.greetingTime += delta;
        if (state.greetingTime > end) { state.greetingTime = start + (state.greetingTime - start) % (end - start + 1); }
        if (!view.animateNavigation) { state.greetingTime = start; }
    }
    view.movies.Rectangle(0, 0, kMenuWidth, kMenuHeight, 0, 0, 0);
    const bool interactive = state.greetingTime >= start && !state.greetingClosing;
    GreetingCallbacks callback(view, state, toc, tables, daily, profile, interactive);
    if (!view.movies.Draw(ordinal, state.greetingTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callback)) { return false; }
    if (!interactive) { view.inputEnabled = false; }
    return true;
}

/** User-authorized cht advances the native elapsed-day accumulator. The saved
 * launch timestamp stays on the real clock, so restarting cannot underflow it. */
void AdvanceDailyDebugDay(CProfileManager &profile, const CDailyBonusTracking &daily, std::uint32_t now) {
    if (!profile.nativeArchive) { ++profile.dailyDayOffset; return; }
    daily.RefreshUsageData(profile, now);
    profile.dailyConsecutiveSeconds += 86400;
    profile.dailyConsecutiveDays = profile.dailyConsecutiveSeconds / 86400 + 1;
}

/** Returns selected planet, -1 for quit, -2 after capture, -3 on failure. */
int ShowGameMenu(CResTOCManager &toc, PackTables &tables, CProfileManager &profile,
    const CPlayerProgress::Template &progressData, const CRefinementManager::Template &refinement,
    const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::vector<ArmorEntry> &armors, MenuState &state, const std::filesystem::path &savePath,
    const std::string &capturePath, const std::vector<MenuTestClick> *testClicks = nullptr, bool originalProfile = false, CWindow *sharedWindow = nullptr, bool testTransitions = false, MenuTransitionTrace *transitionTrace = nullptr, CBGM *sharedMusic = nullptr) {
    // CGunBros owns CBGM across loading, gameplay and postgame menus.
    GameMenu view(sharedWindow);
    if (!view.window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return -3; }
    CBGM ownedMusic;
    if (sharedMusic == nullptr) { sharedMusic = &ownedMusic; }
    CBGM &music = *sharedMusic;
    music.SetEnabled(profile.musicEnabled);
    music.SetPaused(false);
    if (!state.postGameMusic && !music.Play(0)) { return -3; }
    CAudioPlayer::SetEffectsEnabled(profile.soundEnabled);
    if (!view.Open(toc, tables, &profile, state.page == 14, &music)) { return -3; }
    view.animateNavigation = capturePath.empty() || testTransitions;
    view.scripted = testClicks != nullptr;
    // A new view has a new clock (including deterministic capture sessions).
    state.shopFilterBound = false;
    state.optionsBound = false;
    state.socialBound = false;
    state.starBound = false;
    state.modeBound = false;
    CPlayerProgress progress;
    progress.Bind(progressData);
    progress.SetExperience(profile.experience);
    unsigned testFrame = 0;
    std::uint64_t testClock = 0;
    MenuWipe wipe;
    std::uint64_t wipeLastTick = 0;
    std::uint64_t frameTicks = view.window.GetTicksMs();
    float smoothFrameMs = 16.7f;
    CDailyBonusTracking daily;
    if (!daily.Load(toc, tables)) { return -3; }
    if (profile.nativeArchive) {
        daily.RefreshUsageData(profile, static_cast<std::uint32_t>(CurrentSeconds()));
        if (!profile.SaveToDisk(savePath)) { return -3; }
    }
    while (view.window.PumpEvents()) {
        const auto ticks = view.window.GetTicksMs();
        smoothFrameMs = smoothFrameMs * 0.9f + static_cast<float>(ticks - frameTicks) * 0.1f;
        frameTicks = ticks;
        if (testClicks != nullptr && testFrame < testClicks->size()) { testClock += (*testClicks)[testFrame].advanceMs; }
        std::uint64_t menuClock = ticks;
        if (testClicks != nullptr) { menuClock = testClock; }
        view.clock = menuClock;
        unsigned wipeDelta = 0;
        if (wipeLastTick != 0) { wipeDelta = static_cast<unsigned>(menuClock - wipeLastTick); }
        wipeLastTick = menuClock;
        wipe.Update(wipeDelta);
        music.Update();
        bool activate = false;
        const bool wasPostGameMusic = state.postGameMusic;
        const unsigned previousPage = state.page;
        const unsigned previousCategory = state.shopCategory;
        for (std::string cheat = view.window.TakeCheatCode(); !cheat.empty(); cheat = view.window.TakeCheatCode()) {
            if (cheat == "chm") { profile.coins += 5000; profile.warbucks += 500; state.message = "COINS +5000 / WARBUCKS +500"; }
            if (cheat == "cht") {
                AdvanceDailyDebugDay(profile, daily, static_cast<std::uint32_t>(CurrentSeconds()));
                state.Navigate(24);
            }
            if (cheat == "chd") { GameHostSettings().debugMode = !GameHostSettings().debugMode; }
            if (cheat == "chc") { GameHostSettings().isConnected = !GameHostSettings().isConnected; }
            if (cheat == "chh") { state.Navigate(24); }
            if (cheat == "chw") { profile.clearedWaves.fill(500); state.message = "ALL WAVES UNLOCKED"; }
            if (!profile.SaveToDisk(savePath)) { return -3; }
            std::printf("[cheat] %s\n", cheat.c_str());
        }
        for (KeyCode key = view.window.TakeKeyPress(); key != KeyCode::None; key = view.window.TakeKeyPress()) {
            if (wipe.IsActive()) { continue; }
            if (state.page == 14) {
                if (key == KeyCode::Space || key == KeyCode::Enter) { activate = true; }
                continue;
            }
            if (state.currencyPending || state.refineryTransfer >= 0) { continue; }
            if (state.promotion.IsActive()) {
                if (key == KeyCode::Escape) { state.promotion.Dismiss(); }
                continue;
            }
            if (profile.nativeArchive && (state.page == 25 || state.page == 29)) { continue; }
            if (state.storePromptRequested || state.storePopup.IsActive()) {
                // Desktop Escape follows the modal's authored dismissal mode.
                if (key == KeyCode::Escape && (state.storePromptDismiss || state.storePromptButtons != nullptr) && state.storePopup.IsReady()) {
                    state.storePopup.Hide();
                }
                continue;
            }
            if (state.page >= 26 || state.refinementRequired) {
                if (key == KeyCode::Escape) {
                    if (state.page == 26) { state.masteryPopup.Hide(); }
                    else if (state.page == 27 || state.page == 28) {
                        if (profile.nativeArchive) { state.postGameClosing = true; state.postGameCloseTime = 0; }
                        else { state.page = 3; }
                    }
                }
                continue;
            }
            if (key == KeyCode::Space || key == KeyCode::Enter) { activate = true; }
            if (key == KeyCode::Escape) {
                if (profile.nativeArchive && state.page == 21 && state.missionFocused >= 0) {
                    state.missionClosing = true;
                    state.missionFocusTime = 0;
                }
                else if (state.page != 0) { state.Back(); }
                else { return -1; } // Windows Escape closes through the normal save path.
            }
            if (state.page == 0 && key == KeyCode::Down) { state.planet = (state.planet + 1) % 4; }
            if (state.page == 0 && key == KeyCode::Up) { state.planet = (state.planet + 3) % 4; }
            if (key == KeyCode::P) { state.Navigate(0); }
            if (key == KeyCode::E) { state.Navigate(2); }
            if (key == KeyCode::B) { state.Navigate(2); }
            if (key == KeyCode::F) { state.Navigate(3); }
            if (key == KeyCode::Q && state.page == 2) { state.shopSwapKeyRequested = true; }
        }
        if (previousPage != state.page) { state.itemPage = 0; state.selectedItem = -1; state.message.clear(); }
        const std::int64_t now = CurrentSeconds();
        profile.refinery.UpdateRefinement(now);
        view.Begin(state.page);
        view.inputEnabled = !state.currencyPending && !state.storePromptRequested && !state.storePopup.IsActive() && !state.promotion.IsActive() && !wipe.IsActive();
        if (testClicks != nullptr && testFrame < testClicks->size()) { view.SetTestClick((*testClicks)[testFrame]); }
        if (wipe.IsActive()) {
            view.ExchangeClick(false);
            view.dragX = view.dragY = 0;
            view.window.TakeWheelDelta();
        }
        const bool masteryFrame = state.page == 26;
        bool masteryClick = false;
        if (masteryFrame) {
            // Preserve the invoking menu under the modal. Its input is consumed
            // by CMenuSystem::Update :96904; no purchase/navigation may leak through.
            state.page = 27;
            if (!state.history.empty()) { state.page = state.history.back(); }
            view.inputEnabled = false;
            masteryClick = view.ExchangeClick(false);
        }
        if (state.page == 27 || state.page == 28) {
            if (profile.nativeArchive) {
                if (!DrawOriginalPostGame(view, state, toc, tables, profile)) { return -3; }
            } else { return -3; }
        }
        if (state.page == 24 && profile.nativeArchive) {
            if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, savePath, now)) { return -3; }
        }
        if (state.page == 0 || state.page == 22) {
            if (!profile.nativeArchive) {
                std::printf("[menu] star map requires native progression records\n");
                return -3;
            }
            if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state)) { return -3; }
        }
        if (state.page == 21) {
            if (!profile.nativeArchive) { return -3; }
            bool launch = false;
            if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return -3; }
            if (launch) { return static_cast<int>(state.planet); }
        }

        if (!CompleteOfflineIAP(menuClock, state, profile, store, savePath)) { return -3; }
        if (state.page == 2 || state.page == 17 || state.page == 18) {
            if (state.page != 2) { state.shopCategory = 3; }
            if (!DrawStore(view, toc, tables, profile, progress.GetLevel(), store, weapons, armors, state, savePath)) { return -3; }
        }
        if (state.page == 3) {
            if (!DrawRefinery(view, state, profile, refinement, savePath, now)) { return -3; }
        }
        bool saveChanged = false;
        if (state.page == 25 || state.page == 29) {
            bool launchTutorial = false;
            if (!DrawOriginalPlayerSelect(view, state, profile, savePath, launchTutorial)) { return -3; }
            if (launchTutorial) { return 5; }
        } else if (state.page == 4 || state.page == 5 || state.page == 11 || state.page == 13) {
            const bool credentials = std::filesystem::exists(savePath / "Credentials.dat");
            if (!DrawOriginalSocialOffline(view, state, credentials)) { return -3; }
        } else if (state.page == 6 || state.page == 8) {
            if (!DrawOptions(view, state, profile, saveChanged)) { return -3; }
        } else if (state.page == 14) {
            if (!view.TitleImage()) { return -3; }
            // User-requested desktop prompt, not an authored Movie/text.
            // The glyphs themselves come from the original BIG font11.
            if ((view.clock / 600) % 2 == 0) {
                view.CenterText("TAP TO CONTINUE", kMenuWidth * 0.5f, kMenuHeight * 0.88f, 11, 1.0f);
            }
            if (view.Hit(0, 0, kMenuWidth, kMenuHeight) || activate) {
                unsigned nextPage = 25;
                if (!profile.firstLaunch) { nextPage = 24; }
                state.Navigate(nextPage);
            }
        }
        if (saveChanged) {
            music.SetEnabled(profile.musicEnabled);
            CAudioPlayer::SetEffectsEnabled(profile.soundEnabled);
            if (!profile.SaveToDisk(savePath)) { return -3; }
        }
        if (masteryFrame) {
            state.page = 26;
            view.ExchangeClick(masteryClick);
            view.inputEnabled = !state.storePromptRequested && !state.storePopup.IsActive();
        }
        int navigation = -1;
        if (state.currencyPending || state.refineryTransfer >= 0) { view.Hit(0, 0, 1024, 768); }
        if (state.page != 14 && state.page != 26) {
            unsigned headerPage = state.page;
            if (state.page == 29) { headerPage = 4; }
            if (state.page == 17 || state.page == 18) { headerPage = 2; }
            if (state.refinementRequired) { headerPage = 25; }
            navigation = view.Header(profile, progress, headerPage);
            if (navigation == -3) { return -3; }
        }
        constexpr unsigned navigationPages[] = {0, 4, 5, 2, 3, 6, 7};
        if (navigation >= 7) {
            state.currencyTab = static_cast<unsigned>(navigation - 7);
            state.currencyPage = 0;
            state.Navigate(17);
            state.shopFilter = 1u << (state.currencyTab + 14);
        } else if (navigation >= 0) {
            if (navigation == 6) {
                // CMenuAction21 :93929 requests PlayHaven "more_games" only.
                // Without publisher offers it leaves the current menu intact.
                std::printf("[navigation] more_games unavailable: no publisher service\n");
            } else { state.Navigate(navigationPages[navigation], true); }
            state.itemPage = 0;
            state.selectedItem = -1;
            state.message.clear();
        }
        if (!state.message.empty()) { view.Text(450, 738, state.message, 1.45f, 0.93f, 0.74f, 0.33f); }
        if (state.page == 26 && !DrawMastery(view, state, profile, toc, tables, store, weapons, savePath, &progress)) { return -3; }
        if (!DrawStorePrompt(view, state)) { return -3; }
        if (state.promotion.IsActive()) {
            state.promotion.Update(static_cast<unsigned>(view.clock - state.promotionTick));
            state.promotionTick = view.clock;
            if (!state.promotion.Draw(view.movies)) { return -3; }
            // Consume every release inside the modal, including outside its buttons.
            const auto cursor = view.Cursor();
            if (view.ExchangeClick(false)) {
                const unsigned action = state.promotion.Click(cursor.first, cursor.second);
                if (action != 0 && action != 45) {
                    std::printf("[promotion] original action=%u unavailable on host; no account changes\n", action);
                }
            }
        }
        if (GameHostSettings().debugMode) {
            char debug[160];
            std::snprintf(debug, sizeof(debug), "FPS %.1f / %.1f MS / PAGE %u / NET %u", 1000.0f / std::max(0.1f, smoothFrameMs),
                smoothFrameMs, state.page, GameHostSettings().isConnected);
            view.movies.Rectangle(2, 135, 620, 22, 0, 0, 0, 0.8f);
            view.movies.Text(debug, 7, 138, 0, 0.65f);
        }
        // The pressed plate's burst plays above whatever the click opened.
        if (wasPostGameMusic && !state.postGameMusic && !music.Play(0)) { return -3; }
        view.DrawPress();
        if (view.animateNavigation) {
            // Only navigation between branches sweeps; see MenuBranchPage.
            const bool changed = MenuBranchPage(state.page) != MenuBranchPage(previousPage);
            const bool popup = state.page == 26 || previousPage == 26;
            // Deterministic reproduction of first-use resource work during a frame.
            if (testClicks && testFrame < testClicks->size()) { testClock += (*testClicks)[testFrame].renderDelayMs; }
            if (changed && !popup && !wipe.IsActive()) {
                if (!wipe.Begin(view.movies)) { return -3; }
                if (transitionTrace) { ++transitionTrace->starts; }
                // Start at the first presented wipe frame, after cold resources
                // for the destination have loaded. Frame-start time is stale here.
                wipeLastTick = view.window.GetTicksMs();
                if (testClicks) { wipeLastTick = testClock; }
                std::printf("[menu-wipe] from=%u/%u to=%u/%u duration=%u first-frame=0\n",
                    previousPage, previousCategory, state.page, state.shopCategory, wipe.Duration());
            }
            if (!wipe.Draw()) { return -3; }
            if (!wipe.IsActive() && !wipe.Remember()) { return -3; }
        }
        if (transitionTrace) { transitionTrace->active = wipe.IsActive(); transitionTrace->time = wipe.Time(); }
        ++testFrame;
        if (!capturePath.empty() && (testClicks == nullptr || testFrame > testClicks->size())) {
            if (glGetError() != 0 || !view.window.SaveFrame(capturePath)) { return -3; }
            view.window.Present();
            return -2;
        }
        view.window.Present();
    }
    return -1;
}
}

/** Exercise real card resources and the same renderer/input path as --game.
 * All money, XP, mutated templates and profile writes below are test fixtures. */
int CheckStoreCards(CResTOCManager &toc, PackTables &tables, CProfileManager &profile,
    const CPlayerProgress::Template &progress, const CRefinementManager::Template &refinement,
    const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::vector<ArmorEntry> &armor) {
    MenuTestClick cardClick, purchaseClick, previewClick;
    unsigned start = 0, end = 0;
    {
        GameMenu probe;
        if (!probe.Open(toc, tables)) { return 1; }
        const CMovie *mastery = probe.movies.GetMovie(probe.movies.Ordinal("GLU_MOVIE_MASTERY"));
        if (mastery == nullptr) { return 1; }
        unsigned masteryCases = 0;
        for (const WeaponEntry &weapon : weapons) {
            GameObjectRef weaponRef;
            weaponRef.packHash = weapon.packHash;
            weaponRef.localIndex = weapon.ordinal;
            if (FindWeaponStore(store, weaponRef) == nullptr) { continue; }
            unsigned lower = 0;
            for (unsigned tier = 0; tier < kMaxMasteryLevel; ++tier) {
                const unsigned upper = weapon.data.GetMasteryThreshold(tier);
                if (upper <= lower) { return 1; }
                unsigned start = 0, end = 0, atStart = 0, belowNext = 0, atNext = 0;
                if (!mastery->GetChapterRange(tier + 1, start, end) ||
                    !StoreMasteryTarget(*mastery, weapon.data, lower, atStart) ||
                    !StoreMasteryTarget(*mastery, weapon.data, upper - 1, belowNext) ||
                    !StoreMasteryTarget(*mastery, weapon.data, upper, atNext)) { return 1; }
                // Last XP before a tier must leave the authored gap; reaching
                // the threshold jumps to the following chapter (or full duration).
                if (atStart != start || belowNext < atStart || belowNext >= end || atNext <= belowNext) { return 1; }
                if (tier == kMaxMasteryLevel - 1 && atNext != mastery->duration) { return 1; }
                CMovie changed = *mastery;
                for (unsigned &chapter : changed.chapters) { chapter += 13; }
                changed.duration += 13;
                unsigned shifted = 0;
                if (!StoreMasteryTarget(changed, weapon.data, upper - 1, shifted) || shifted != belowNext + 13) { return 1; }
                lower = upper;
                ++masteryCases;
            }
        }
        std::printf("[store-card-check] mastery tier-boundaries resource-shift cases=%u failures=0\n", masteryCases);
        MovieRegion playerRegion;
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_MENU"), kStorePlayerRegion, 0, playerRegion)) { return 1; }
        // Follow an actual authored region, including capture beyond its edges.
        CMenuMesh rotation;
        const float touchX = std::ceil(playerRegion.x + playerRegion.width / 2);
        const float touchY = std::ceil(playerRegion.y + playerRegion.height / 2);
        rotation.UpdateRotation(0, true, touchX, touchY, playerRegion.x, playerRegion.y, playerRegion.width, playerRegion.height, true);
        rotation.UpdateRotation(16, true, touchX + 100, touchY, playerRegion.x, playerRegion.y, playerRegion.width, playerRegion.height, true);
        const float expectedDegrees = 360.0f - 100.0f / playerRegion.width * 180.0f;
        if (std::abs(rotation.GetDegrees() - expectedDegrees) > 0.001f) { return 1; }
        rotation.UpdateRotation(1000, false, touchX, touchY, playerRegion.x, playerRegion.y, playerRegion.width, playerRegion.height, true);
        if (rotation.GetDegrees() != 0) { return 1; }
        rotation.UpdateRotation(16, true, playerRegion.x - 10, touchY, playerRegion.x, playerRegion.y, playerRegion.width, playerRegion.height, true);
        rotation.UpdateRotation(16, true, touchX, touchY, playerRegion.x, playerRegion.y, playerRegion.width, playerRegion.height, true);
        if (rotation.GetDegrees() != 0) { return 1; }
        std::printf("[store-card-check] original-rotation drag-origin region-width release-return outside-start failures=0\n");
        probe.verifyPlayerProjection = true;
        probe.Begin(14);
        if (!probe.DrawEquippedPlayer(toc, tables, profile, weapons, armor, 0, nullptr, &playerRegion)) { return 1; }
        // Perturb the authored region only in this fixture, across both brothers
        // and one retail gun per category. No viewport or framing constant may win.
        MovieRegion changedRegion = playerRegion;
        changedRegion.x -= 53;
        changedRegion.y += 19;
        changedRegion.width *= 0.8f;
        changedRegion.height *= 0.65f;
        for (unsigned brother = 0; brother < 2; ++brother) {
            CProfileManager modelProfile = profile;
            modelProfile.playerBrother = brother;
            std::array<bool, kWeaponCategoryCount> checked{};
            for (const WeaponEntry &weapon : weapons) {
                if (!weapon.hasStoreEntry || weapon.visualOnly || weapon.category < 0 || weapon.category >= kWeaponCategoryCount || checked[weapon.category]) { continue; }
                GameObjectRef ref;
                ref.packHash = weapon.packHash;
                ref.localIndex = weapon.ordinal;
                if (FindWeaponStore(store, ref) == nullptr) { continue; }
                modelProfile.configuration.guns[0] = ref;
                probe.Begin(14);
                std::printf("[store-player-check] brother=%u weapon=%s category=%d mutated-region=1\n", brother, weapon.name.c_str(), weapon.category);
                if (!probe.DrawEquippedPlayer(toc, tables, modelProfile, weapons, armor, 0, nullptr, &changedRegion)) { return 1; }
                checked[weapon.category] = true;
            }
            for (bool found : checked) { if (!found) { return 1; } }
        }
        // Imported equipment catches UI states absent from the simple fixtures.
        CProfileManager savedProfile = profile;
        const auto savedPath = std::filesystem::path("out/ui-original-2026-09-09") / ("model-native-" + std::to_string(GetTickCount64()));
        if (!LoadNativeProfile(toc, tables, savedProfile, savedPath, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
        for (unsigned slot = 0; slot < 2; ++slot) {
            for (unsigned phase = 0; phase < 2; ++phase) {
                probe.Begin(14);
                if (phase == 1) {
                    for (unsigned step = 0; step < 300; ++step) { probe.AdvancePlayerPreview(16); }
                }
                if (!probe.DrawEquippedPlayer(toc, tables, savedProfile, weapons, armor, slot, nullptr, &playerRegion)) { return 1; }
                const auto image = "out/ui-original-2026-09-09/native-model-" + std::to_string(slot) + "-" + std::to_string(phase) + ".png";
                if (!probe.window.SaveFrame(image)) { return 1; }
                const auto &brother = probe.GetPlayerPreview()->weapon->brother;
                std::printf("[store-player-check] native slot=%u phase=%u state=%d torso-move=%d time=%d\n", slot, phase,
                    brother.GetStateId(), brother.GetTorso().GetMoveIndex(), brother.GetTorso().GetAnimation().GetTimeMs());
            }
        }
        // Original PLAYER flow fixture: 17 -> 18 -> 19/native 3 -> 17.
        // A switch must preserve the actor and the outgoing torso until the
        // next sequence consumes the incoming gun's move overrides.
        CBrother *originalActor = &probe.GetPlayerPreview()->weapon->brother;
        for (unsigned exchange = 0; exchange < 3; ++exchange) {
            const unsigned oldSlot = probe.GetPlayerPreviewSlot();
            const unsigned targetSlot = 1 - oldSlot;
            PlayerModel &model = *probe.GetPlayerPreview();
            PlayerWeaponState *oldWeapon = model.uiActiveWeapon;
            if (oldWeapon == nullptr) { oldWeapon = model.weapon.get(); }
            probe.TakePlayerPreviewSlotChange();
            if (!probe.DrawEquippedPlayer(toc, tables, savedProfile, weapons, armor, targetSlot, nullptr, &playerRegion)) { return 1; }
            if (&model.weapon->brother != originalActor || probe.GetPlayerPreviewSlot() != oldSlot) { return 1; }
            unsigned elapsed = 0;
            while (probe.GetPlayerPreviewSlot() == oldSlot && elapsed < 10000) {
                probe.AdvancePlayerPreview(16);
                elapsed += 16;
            }
            if (probe.GetPlayerPreviewSlot() != targetSlot || originalActor->GetStateId() != 19 ||
                model.uiActiveWeapon == oldWeapon || !probe.TakePlayerPreviewSlotChange() || probe.TakePlayerPreviewSlotChange()) { return 1; }
            const CMesh *switchTorso = originalActor->GetTorso().GetAnimation().GetMesh();
            bool outgoingTorso = !originalActor->TorsoUsesWeapon();
            for (const auto &part : oldWeapon->configs) { if (&part->mesh == switchTorso) { outgoingTorso = true; } }
            if (!outgoingTorso) { return 1; }
            while (originalActor->GetStateId() != 17 && elapsed < 10000) {
                probe.AdvancePlayerPreview(16);
                elapsed += 16;
            }
            bool incomingTorso = !originalActor->TorsoUsesWeapon();
            for (const auto &part : model.uiActiveWeapon->configs) {
                if (&part->mesh == originalActor->GetTorso().GetAnimation().GetMesh()) { incomingTorso = true; }
            }
            if (originalActor->GetStateId() != 17 || !incomingTorso) { return 1; }
            savedProfile.activeWeaponSlot = targetSlot;
            if (!savedProfile.SaveToDisk(savedPath)) { return 1; }
            CProfileManager restored;
            if (!LoadNativeProfile(toc, tables, restored, savedPath, savedPath / "absent-source") || restored.activeWeaponSlot != targetSlot) { return 1; }
            probe.Begin(14);
            if (!probe.DrawEquippedPlayer(toc, tables, savedProfile, weapons, armor, targetSlot, nullptr, &playerRegion)) { return 1; }
            if (!probe.window.SaveFrame("out/ui-original-2026-09-09/native-model-swapped-" + std::to_string(exchange) + ".png")) { return 1; }
            std::printf("[store-player-check] native-swap exchange=%u slot=%u actor-preserved=1 outgoing-torso=1 incoming-torso=1 reload=1 elapsed=%u\n",
                exchange, targetSlot, elapsed);
        }
        const auto *swapEntry = OriginalMenuData("MDS_BUTTON_STORE_GUN_SWAP", 0);
        if (swapEntry == nullptr) { return 1; }
        const unsigned swapMovieId = probe.movies.Ordinal(swapEntry->movies[0]);
        const CMovie *swapMovie = probe.movies.GetMovie(swapMovieId);
        unsigned showStart = 0, showEnd = 0, pressStart = 0, pressEnd = 0;
        if (swapMovie == nullptr || !swapMovie->GetChapterRange(0, showStart, showEnd) ||
            !swapMovie->GetChapterRange(1, pressStart, pressEnd)) { return 1; }
        MovieRegion swapParent, swapOrigin;
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_MENU"), kStoreGunSwapRegion, 0, swapParent)) { return 1; }
        // Mutated parent verifies that native placement and the child touch box
        // move together. No fitted sprite or hand-written text baseline remains.
        swapParent.x -= 31;
        swapParent.y += 17;
        if (!StoreGunSwapOrigin(probe, swapParent, swapOrigin)) { return 1; }
        MovieRegion swapTouch;
        bool foundTouch = false;
        for (const auto &region : probe.movies.Regions(swapMovieId, showEnd, swapOrigin.x, swapOrigin.y, true)) {
            if (region.index == 0) { swapTouch = region; foundTouch = true; }
        }
        if (!foundTouch) { return 1; }
        MenuState swapState;
        swapState.shopGunSlot = probe.GetPlayerPreviewSlot();
        const unsigned beforeSlot = swapState.shopGunSlot;
        const MenuTestClick swapClick{swapTouch.x + swapTouch.width / 2, swapTouch.y + swapTouch.height / 2};
        probe.Begin(14); probe.clock = 1000;
        probe.SetTestClick(swapClick);
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.shopSwapPhase != 0 || swapState.shopGunSlot != beforeSlot) { return 1; }
        probe.Begin(14); probe.clock += showEnd - showStart;
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.shopSwapPhase != 2) { return 1; }
        probe.SetTestClick(swapClick);
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.shopSwapPhase != 4 || swapState.shopGunSlot != beforeSlot) { return 1; }
        probe.Begin(14); probe.clock += pressEnd - pressStart - 1;
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.shopGunSlot != beforeSlot) { return 1; }
        probe.Begin(14); ++probe.clock;
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.shopGunSlot != 1 - beforeSlot) { return 1; }
        swapState.shopFilter = 1;
        // Filtering the GUNS list must preserve the equipped-player slot button.
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.shopSwapPhase != 2) {
            std::printf("[store-player-check] FAIL filtered GUNS hides swap button phase=%u\n", swapState.shopSwapPhase);
            return 1;
        }
        swapState.shopCategory = 1;
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.shopSwapPhase != 1) { return 1; }
        probe.Begin(14); probe.clock += showEnd - showStart;
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.shopSwapPhase != 8) { return 1; }
        std::printf("[store-player-check] original-swap-button movie=%u region-shift=1 intro-gate=1 press-boundary=1 category-hide=1 failures=0\n", swapMovieId);
        const CMovie *movie = probe.movies.GetMovie(probe.movies.Ordinal("GLU_MOVIE_SHOP_BOX"));
        if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
        MovieRegion column, body;
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), 1, probe.storeRestTime, column) ||
            !probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_SHOP_BOX"), 0, start, body)) { return 1; }
        cardClick = {column.x + body.width / 4, column.y + body.height / 4};
        MovieRegion content, openBody, actions, buyLabel, previewLabel;
        const unsigned card = probe.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_MENU"), 0, 0, content) ||
            !probe.movies.Region(card, 0, end, openBody)) { return 1; }
        const StoreCardFace face{content.x + content.width / 2 - static_cast<int>(content.width) / 16 - openBody.width / 2,
            content.y + content.height / 2 - openBody.height / 2, 1, end};
        const OriginalMenuEntry *buy = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", 0);
        const OriginalMenuEntry *preview = OriginalMenuData("MDS_BUTTON_STORE_PREVIEW", 0);
        if (buy == nullptr || preview == nullptr || !CardRegion(probe, card, 10, face, actions) ||
            !probe.movies.Region(probe.movies.Ordinal(buy->movies[0]), 1, 0, buyLabel) ||
            !probe.movies.Region(probe.movies.Ordinal(preview->movies[0]), 1, 0, previewLabel)) { return 1; }
        purchaseClick = {actions.x + actions.width - buyLabel.width / 2, actions.y + buyLabel.height / 2};
        previewClick = {actions.x + previewLabel.width / 2, actions.y + previewLabel.height / 2};
        MenuState animation;
        animation.shopDetailOpen = true;
        animation.shopDetailTime = start;
        probe.clock = 60;
        if (!AdvanceStoreCard(probe, animation, *movie) || animation.shopDetailTime != start + 240) { return 1; }
        animation.shopDetailClosing = true;
        probe.clock += 10;
        if (!AdvanceStoreCard(probe, animation, *movie) || animation.shopDetailTime != start + 200) { return 1; }
        probe.clock += 1000;
        if (!AdvanceStoreCard(probe, animation, *movie) || animation.shopDetailOpen) { return 1; }
        CMovie changed = *movie;
        changed.chapters[1] += 32;
        changed.chapters[2] += 64;
        changed.duration += 64;
        animation = MenuState{};
        animation.shopDetailOpen = true;
        animation.shopDetailTime = changed.chapters[1];
        if (!AdvanceStoreCard(probe, animation, changed) || animation.shopDetailTime != end + 64) { return 1; }
        const auto mixed = FormatStoreText(probe.movies, "^f0DMG ^f112 ^f2SPD ^f3+4 ^f450\nNEXT", 1000);
        constexpr unsigned expectedFonts[] = {1, 2, 4, 3, 0};
        if (mixed.size() != 2 || mixed[0].runs.size() != 5) { return 1; }
        for (unsigned index = 0; index < 5; ++index) {
            if (mixed[0].runs[index].font != expectedFonts[index]) { return 1; }
        }
        unsigned templates = 0;
        for (const StoreEntry &entry : store) {
            for (unsigned field = 3; field <= 5; ++field) {
                const std::string original = ReadGameString(toc, entry.data.assets[field]);
                if (original.empty()) { continue; }
                ++templates;
                const std::string expanded = SubstituteStoreStats(original, StoreStatValues(entry.data, 0));
                if (expanded.find('#') != std::string::npos || expanded.find("^i") != std::string::npos) {
                    std::printf("[store-card-check] unresolved text item=%s field=%u text=%s\n", entry.name.c_str(), field, expanded.c_str());
                    return 1;
                }
            }
        }
        // Mutate an in-memory STORE value, then verify the display consumes it.
        for (const StoreEntry &entry : store) {
            if (entry.data.statGroups[1].empty()) { continue; }
            CStoreItem changedItem = entry.data;
            changedItem.statGroups[1][0] = 12345;
            if (SubstituteStoreStats("^f0DMG ^f1#DMG", StoreStatValues(changedItem, 0)) != "^f0DMG ^f112345") { return 1; }
            break;
        }
        std::printf("[store-card-check] templates=%u five-fonts newline changed-STORE changed-chapters 4x reversal failures=0\n", templates);
        std::printf("[store-card-check] currency common=%s rare=%s\n", probe.movies.NamedString("IDS_SHOP_COMMON").c_str(), probe.movies.NamedString("IDS_SHOP_RARE").c_str());
    }
    CProfileManager before = profile;
    constexpr const char *categories[] = {"guns", "armor", "powerups"};
    constexpr unsigned categoryOrder[] = {2, 0, 1};
    for (unsigned category : categoryOrder) {
        const std::string base = std::string("out/store-card-") + categories[category];
        MenuState folded, opening, opened, closing, closed;
        MenuState *states[] = {&folded, &opening, &opened, &closing, &closed};
        for (MenuState *state : states) {
            state->page = 2;
            state->shopCategory = category;
            const char *table = nullptr;
            const unsigned rows = StoreFilterRows(category, table);
            // Native STORE.type criteria; the previous fixture assumed every
            // category reused gun bit 0 and silently disabled powerup filtering.
            for (unsigned row = 2; row < rows; ++row) {
                const auto *entry = OriginalMenuData(table, row);
                if (entry->action == 65 && entry->parameter < 17) { state->shopFilter |= 1u << entry->parameter; }
                if (category != 2) { break; }
            }
        }
        const std::vector<MenuTestClick> idle = {{-100, -100}};
        const std::vector<MenuTestClick> begin = {cardClick, {-100, -100, 60}};
        const std::vector<MenuTestClick> open = {cardClick, {-100, -100, 2500}};
        const std::vector<MenuTestClick> reverse = {cardClick, {-100, -100, 2500}, {10, 700}, {-100, -100, 60}};
        const std::vector<MenuTestClick> finish = {cardClick, {-100, -100, 2500}, {10, 700}, {-100, -100, 250}};
        const std::vector<MenuTestClick> *clicks[] = {&idle, &begin, &open, &reverse, &finish};
        constexpr const char *phases[] = {"folded", "opening", "open", "closing", "closed"};
        for (unsigned phase = 0; phase < 5; ++phase) {
            const std::string image = base + "-" + phases[phase] + ".png";
            if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
                *states[phase], "out/store-card-check.dat", image.c_str(), clicks[phase]) != -2) { return 1; }
        }
        if (!opening.shopDetailOpen || opening.shopDetailTime != start + 240 || !opened.shopDetailOpen ||
            opened.shopDetailTime != end || !closing.shopDetailClosing || closing.shopDetailTime != end - 240 ||
            closed.shopDetailOpen || opened.selectedItem < 0) { return 1; }
        const StoreEntry &entry = store[opened.selectedItem];
        if (category != 0) {
            CProfileManager buyer = profile;
            buyer.coins = 2ull * entry.data.commonPrice;
            buyer.warbucks = 2ull * entry.data.rarePrice;
            MenuState previewState = opened;
            const std::vector<MenuTestClick> previewActions = {{-100, -100, 2500}, previewClick};
            if (category == 1) {
                if (ShowGameMenu(toc, tables, buyer, progress, refinement, store, weapons, armor, previewState,
                    "out/store-card-preview.dat", base + "-preview.png", &previewActions) != -2 || !previewState.shopPreview ||
                    buyer.inventory.size() != profile.inventory.size()) { return 1; }
            }
            // Exercise the real purchase failure, including its original button table.
            CProfileManager poorBuyer = profile;
            poorBuyer.coins = 0;
            poorBuyer.warbucks = 0;
            MenuState insufficient = opened;
            const std::vector<MenuTestClick> insufficientActions = {{-100, -100, 2500}, purchaseClick};
            if (ShowGameMenu(toc, tables, poorBuyer, progress, refinement, store, weapons, armor, insufficient,
                "out/store-card-insufficient.dat", base + "-insufficient.png", &insufficientActions) != -2 ||
                insufficient.failedPrice == 0 || insufficient.storePromptButtons == nullptr ||
                std::string(insufficient.storePromptButtons) != "MDS_BUTTON_STORE_PROMPT") { return 1; }
            std::printf("[store-card-check] insufficient category=%s original-three-buttons=1 failures=0\n", categories[category]);
            MenuState purchase = opened;
            std::vector<MenuTestClick> purchaseActions = {{-100, -100, 2500}, purchaseClick};
            if (category == 2) { purchaseActions.push_back(purchaseClick); }
            const std::filesystem::path path = base + "-purchase.dat";
            if (ShowGameMenu(toc, tables, buyer, progress, refinement, store, weapons, armor, purchase,
                path, base + "-purchased.png", &purchaseActions) != -2) { return 1; }
            const GameObjectTypeRef &object = entry.data.objects[0];
            if (category == 1 && (!buyer.Owns(object.type, object.object) ||
                !SameObject(Equipped(buyer, purchase.slot), object.object))) { return 1; }
            // The first powerup is the byte-checked five-charge Speed Boost pack.
            if (category == 2 && (buyer.GetPowerupCount(object.object) != 10 || buyer.warbucks != 0)) { return 1; }
            CProfileManager reloaded = profile;
            if (!reloaded.LoadFromDisk(path) || reloaded.coins != buyer.coins || reloaded.warbucks != buyer.warbucks ||
                reloaded.GetPowerupCount(object.object) != buyer.GetPowerupCount(object.object)) { return 1; }
            std::printf("[store-card-check] category=%s expanded-purchase preview reload failures=0\n", categories[category]);
        }
        std::printf("[store-card-check] category=%s item=%s folded=%s expanded=%s screenshots=5 failures=0\n",
            categories[category], entry.name.c_str(), ReadGameString(toc, entry.data.assets[5]).c_str(),
            ReadGameString(toc, entry.data.assets[4]).c_str());
    }
    if (profile.coins != before.coins || profile.warbucks != before.warbucks || profile.inventory.size() != before.inventory.size()) { return 1; }
    for (unsigned slot = 0; slot < 5; ++slot) {
        if (!SameObject(Equipped(profile, slot), Equipped(before, slot))) { return 1; }
    }
    std::printf("[store-card-check] no-purchase unchanged-loadout failures=0\n");
    // Full native menu path: actual child-button click -> authored press ->
    // PLAYER native 3 -> DrawStore save. No direct profile mutation in the driver.
    const auto nativeSwapPath = std::filesystem::path("out/ui-original-2026-09-09") /
        ("store-native-click-" + std::to_string(GetTickCount64()));
    CProfileManager nativeSwapProfile;
    if (!LoadNativeProfile(toc, tables, nativeSwapProfile, nativeSwapPath, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    const unsigned originalSlot = nativeSwapProfile.activeWeaponSlot;
    MenuTestClick nativeSwapClick;
    unsigned showDuration = 0, pressDuration = 0;
    {
        GameMenu probe;
        if (!probe.Open(toc, tables)) { return 1; }
        MovieRegion parent, origin;
        const auto *entry = OriginalMenuData("MDS_BUTTON_STORE_GUN_SWAP", 0);
        if (entry == nullptr || !probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_MENU"), kStoreGunSwapRegion, 0, parent) ||
            !StoreGunSwapOrigin(probe, parent, origin)) { return 1; }
        const unsigned movieId = probe.movies.Ordinal(entry->movies[0]);
        const CMovie *movie = probe.movies.GetMovie(movieId);
        unsigned start = 0, end = 0;
        if (movie == nullptr || !movie->GetChapterRange(0, start, end)) { return 1; }
        showDuration = end - start;
        if (!movie->GetChapterRange(1, start, end)) { return 1; }
        pressDuration = end - start;
        bool found = false;
        for (const auto &region : probe.movies.Regions(movieId, showDuration, origin.x, origin.y, true)) {
            if (region.index != 0) { continue; }
            nativeSwapClick = {region.x + region.width / 2, region.y + region.height / 2};
            found = true;
        }
        if (!found) { return 1; }
    }
    MenuState nativeSwapState;
    nativeSwapState.page = 2;
    nativeSwapState.shopGunSlot = originalSlot;
    nativeSwapState.shopFilter = kOwnedFilterBit;
    std::vector<MenuTestClick> nativeSwapActions{{-100, -100, 1}, {-100, -100, showDuration + 1},
        nativeSwapClick, {-100, -100, pressDuration + 1}};
    for (unsigned frame = 0; frame < 120; ++frame) { nativeSwapActions.push_back({-100, -100, 16}); }
    if (ShowGameMenu(toc, tables, nativeSwapProfile, progress, refinement, store, weapons, armor,
        nativeSwapState, nativeSwapPath, "out/ui-original-2026-09-09/native-store-swap-click.png", &nativeSwapActions) != -2) { return 1; }
    CProfileManager swapReloaded;
    if (nativeSwapProfile.activeWeaponSlot != 1 - originalSlot || !LoadNativeProfile(toc, tables, swapReloaded,
        nativeSwapPath, nativeSwapPath / "absent-source") || swapReloaded.activeWeaponSlot != 1 - originalSlot) { return 1; }
    std::printf("[store-card-check] native real-button filtered-GUNS authored-press player-Flow active-slot=%u saved-reload=1 failures=0\n", swapReloaded.activeWeaponSlot);
    // Visual research fixture only: IMG_0800's silver/blue rifle may be the
    // catalog's Infinity Laser. Do not replace the real account's loadout or
    // treat this unconfirmed image match as a runtime weapon rule.
    // Infinity Laser was visually rejected. Keep that attempt above recorded;
    // enumerate the original rifle/laser categories for a same-weapon match.
    // No rifle/laser matched IMG_0800. Include every original category: visual
    // appearance is not reliable evidence of the STORE classification.
    for (const WeaponEntry &weapon : weapons) {
        const std::string referenceKey = std::to_string(weapon.packHash) + "-" + std::to_string(weapon.ordinal);
        const auto referencePath = nativeSwapPath / "reference-gun-fixture" / referenceKey;
        CProfileManager reference;
        if (!LoadNativeProfile(toc, tables, reference, referencePath, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
        reference.configuration.guns[reference.activeWeaponSlot].packHash = weapon.packHash;
        reference.configuration.guns[reference.activeWeaponSlot].localIndex = static_cast<std::uint8_t>(weapon.ordinal);
        MenuState referenceState;
        referenceState.page = 2;
        referenceState.shopGunSlot = reference.activeWeaponSlot;
        const std::string referenceImage = "out/ui-original-2026-09-09/reference-gun-" + referenceKey + ".png";
        if (ShowGameMenu(toc, tables, reference, progress, refinement, store, weapons, armor, referenceState, referencePath,
            referenceImage) != -2) { return 1; }
        std::printf("[store-player-research] reference-image-candidate=%s resource=%u:%u fixture-only image-match-unconfirmed=1\n",
            weapon.name.c_str(), weapon.packHash, weapon.ordinal);
    }
    return 0;
}

/** Native save fixtures and real greeting callbacks; no original saves change. */
int RunPostGameMenuCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<PlanetEntry> planets;
    std::vector<WeaponEntry> weapons;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadPlanetCatalog(toc, tables, planets) ||
        !LoadWeaponCatalog(toc, tables, weapons)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path("out/ui-original-2026-09-09") / ("postgame-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    unsigned tested = 0;
    for (unsigned type : {1u, 2u}) {
        for (unsigned index = 0; index < planets.size(); ++index) {
            const auto &planet = planets[index];
            if (planet.missions.empty() || planet.missions[0].type != type) { continue; }
            MissionEntry mission;
            mission.resource = planet.data.missions[0];
            mission.data = planet.missions[0];
            mission.title = planet.missionInfo[0].title;
            SurvivalGameContext context{profile, path, index};
            context.mission = mission.resource;
            context.missionLevel = mission.data.level;
            if (type == 2) { context.hordeStart = 0; }
            const MissionEntry *archiveMission = nullptr;
            if (type == 2) { archiveMission = &mission; }
            const auto &map = planet.missionInfo[0].map;
            if (RunSurvival(bigDirectory, tables.GetPackName(map.packHash), map.localIndex, 0, -1, {}, 0,
                false, false, true, 2, 0, &context, true, false, archiveMission) != 0) { return 1; }
            if (context.result.kills == 0 || context.result.casualties.empty() || context.result.waves != 2) { return 1; }
            const auto coins = profile.coins, warbucks = profile.warbucks, ore = profile.xplodium;
            MenuState state;
            BeginPostGame(state, context, weapons);
            const bool popupExpected = state.postGameUpgradePending;
            if (state.page != 27) { return 1; }
            GameMenu view;
            if (!view.Open(toc, tables)) { return 1; }
            view.scripted = true;
            const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_WRAPUP_SCREEN");
            const auto *movie = view.movies.GetMovie(ordinal);
            unsigned start = 0, end = 0;
            if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
            MovieRegion tabs;
            if (!view.movies.Region(ordinal, 1, start, tabs)) { return 1; }
            const auto *tab = OriginalMenuData("MDS_BUTTON_POSTGAME_INFO", 1);
            MovieRegion tabBounds;
            if (tab == nullptr || !view.movies.Region(view.movies.Ordinal(tab->movies[0]), 0, 0, tabBounds)) { return 1; }
            const float tabX = tabs.x + static_cast<int>(tabs.width) / 2;
            MenuTestClick casualtyClick{};
            for (const auto &area : view.movies.Regions(view.movies.Ordinal(tab->movies[0]), 0, tabX, tabs.y)) {
                if (area.index == 0) { casualtyClick = {area.x + area.width / 2, area.y + area.height / 2}; }
            }
            view.Begin(27);
            view.SetTestClick(casualtyClick);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || state.page != 27) { return 1; }
            view.clock += start;
            view.Begin(27);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile)) { return 1; }
            if (popupExpected) {
                if (state.page != 26) { return 1; }
                CloseMastery(state);
                if (state.page != 27) { return 1; }
            }
            view.clock += 1000;
            view.Begin(27);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile)) { return 1; }
            const std::string prefix = "out/ui-original-2026-09-09/postgame-original-" + std::to_string(type);
            if (!view.window.SaveFrame(prefix + "-overview.png")) { return 1; }
            view.Begin(27);
            view.SetTestClick(casualtyClick);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || state.page != 28) { return 1; }
            view.Begin(28);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile)) { return 1; }
            if (!view.window.SaveFrame(prefix + "-casualties.png")) { return 1; }
            // Every actual killed enemy must resolve its own original menu model.
            for (unsigned enemy = 0; enemy < state.result.casualties.size(); ++enemy) {
                state.postGameGalleryPosition = static_cast<float>(enemy) - 1;
                view.clock += 100;
                view.Begin(28);
                if (!DrawOriginalPostGame(view, state, toc, tables, profile)) { return 1; }
            }
            MovieRegion backArea;
            const auto *back = OriginalMenuData("MDS_BUTTON_POSTGAME_BACK", 0);
            if (back == nullptr || !view.movies.Region(ordinal, 0, start, backArea)) { return 1; }
            MenuTestClick backClick{};
            for (const auto &area : view.movies.Regions(view.movies.Ordinal(back->movies[0]), 0,
                backArea.x + backArea.width / 2, backArea.y + backArea.height / 2, true)) {
                if (area.index == 0) { backClick = {area.x + area.width / 2, area.y + area.height / 2}; }
            }
            view.Begin(28);
            view.SetTestClick(backClick);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || !state.postGameClosing || state.page != 28) { return 1; }
            const auto *backMovie = view.movies.GetMovie(view.movies.Ordinal(back->movies[0]));
            unsigned exitStart = 0, exitEnd = 0;
            if (backMovie == nullptr || !backMovie->GetChapterRange(1, exitStart, exitEnd)) { return 1; }
            unsigned hideStart = 0, hideEnd = 0;
            if (!backMovie->GetChapterRange(0, hideStart, hideEnd)) { return 1; }
            view.clock += exitEnd - exitStart + 1;
            view.Begin(28);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || state.page != 28) { return 1; }
            view.clock += hideEnd - hideStart + 1;
            view.Begin(28);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || state.page != 3) { return 1; }
            if (profile.coins != coins || profile.warbucks != warbucks || profile.xplodium != ore ||
                !profile.LoadFromDisk(path) || profile.xplodium != ore) { return 1; }
            // Zero-ore routing is isolated from the actual persisted reward.
            profile.xplodium = 0;
            state.page = 28;
            state.postGameClosing = false;
            view.Begin(28);
            view.SetTestClick(backClick);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || !state.postGameClosing) { return 1; }
            view.clock += exitEnd - exitStart + hideEnd - hideStart + 2;
            view.Begin(28);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || state.page != 0 ||
                !profile.LoadFromDisk(path) || profile.xplodium != ore) { return 1; }
            std::printf("[postgame-original-check] type=%u waves=%u kills=%u casualties=%zu score=%u best=%u time=%u opening=1 tabs=1 exit=1 no-duplicate-reward=1 failures=0\n",
                type, context.result.waves, context.result.kills, context.result.casualties.size(), context.result.score,
                context.result.bestKillStreak, context.result.stopwatchMs);
            ++tested;
            break;
        }
    }
    return tested == 2 ? 0 : 1;
}

int RunPlayerSelectCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path("out/ui-original-2026-09-09") / ("select-check-" + std::to_string(GetTickCount64()));
    // Missing source exercises native constructor defaults as requested.
    if (!LoadNativeProfile(toc, tables, profile, path, {})) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_PLAYER_SELECT");
    auto *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
    const auto coins = profile.coins, warbucks = profile.warbucks;
    for (unsigned mode = 0; mode < 2; ++mode) {
        for (unsigned brother = 0; brother < 2; ++brother) {
            MenuState state;
            state.page = 25;
            if (mode != 0) { state.page = 29; }
            profile.firstLaunch = mode == 0;
            const auto initialBrother = profile.playerBrother;
            MovieRegion area;
            if (!view.movies.Region(ordinal, brother + 1, 0, area)) { return 1; }
            const MenuTestClick click{area.x + area.width / 2, area.y + area.height / 2};
            bool launch = false;
            view.Begin(state.page);
            view.inputEnabled = true;
            view.SetTestClick(click);
            if (!DrawOriginalPlayerSelect(view, state, profile, path, launch) || launch ||
                state.playerSelection != -1 || profile.playerBrother != initialBrother) { return 1; }
            view.clock += end + 1;
            view.Begin(state.page);
            view.inputEnabled = true;
            if (!DrawOriginalPlayerSelect(view, state, profile, path, launch)) { return 1; }
            if (mode == 0 && brother == 0 && !view.window.SaveFrame("out/ui-original-2026-09-09/player-select-original-ready.png")) { return 1; }
            view.inputEnabled = true;
            view.SetTestClick(click);
            if (!DrawOriginalPlayerSelect(view, state, profile, path, launch) || launch ||
                state.playerSelection != static_cast<int>(brother) || profile.playerBrother != brother || profile.firstLaunch) { return 1; }
            unsigned selectedStart = 0, selectedEnd = 0;
            if (!movie->GetChapterRange(state.playerSelectChapter, selectedStart, selectedEnd) || state.playerSelectTime != selectedStart) { return 1; }
            view.clock += (selectedEnd - selectedStart) / 2;
            view.Begin(state.page);
            view.inputEnabled = true;
            if (!DrawOriginalPlayerSelect(view, state, profile, path, launch) || launch) { return 1; }
            if (mode == 0 && !view.window.SaveFrame("out/ui-original-2026-09-09/player-select-original-" + std::to_string(brother) + ".png")) { return 1; }
            view.clock += selectedEnd - selectedStart + 1;
            view.Begin(state.page);
            if (!DrawOriginalPlayerSelect(view, state, profile, path, launch) || launch != (mode == 0)) { return 1; }
            if (mode != 0 && state.page != 6) { return 1; }
            CProfileManager reloaded;
            reloaded.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
            if (!LoadNativeProfile(toc, tables, reloaded, path, {}) || reloaded.playerBrother != brother ||
                reloaded.firstLaunch || reloaded.coins != coins || reloaded.warbucks != warbucks) { return 1; }
        }
    }
    std::printf("[player-select-check] native-new-profile=1 brothers=2 original-hit=4 opening-gate=4 chapter-gate=4 native-reload=4 failures=0\n");
    return view.movies.Failures() != 0 || glGetError() != 0;
}

int RunGreetingCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    CDailyBonusTracking daily;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadStoreCatalog(toc, tables, store) ||
        !daily.Load(toc, tables)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path("out/ui-original-2026-09-09") / ("greeting-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    const auto first = static_cast<std::uint32_t>(CurrentSeconds());
    // Explicit fixture: a new daily cycle, preserving the real inventory/wallet.
    profile.dailyLastLaunchSeconds = first;
    profile.dailyConsecutiveSeconds = 0;
    profile.dailyConsecutiveDays = 1;
    profile.dailyLastCommit = 0;
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = true;
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_WELCOME_NEW");
    auto *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
    for (unsigned day = 0; day < 7; ++day) {
        const auto seconds = first + day * 86400;
        MenuState state;
        state.page = 24;
        const auto coins = profile.coins;
        const auto warbucks = profile.warbucks;
        const auto xp = profile.experience;
        const auto awarded = profile.statistics[32];
        const auto &prize = daily.prizes[day % daily.prizes.size()];
        view.Begin(24);
        if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, path, seconds) ||
            profile.coins != coins || profile.warbucks != warbucks || profile.statistics[32] != awarded) { return 1; }
        view.clock += start;
        view.Begin(24);
        view.inputEnabled = true; // ShowGameMenu resets the frame input gate.
        if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, path, seconds) ||
            profile.coins != coins || state.greetingClosing) { return 1; }
        if (day == 0 && !view.window.SaveFrame("out/ui-original-2026-09-09/greeting-original-open.png")) { return 1; }
        MovieRegion button;
        if (!view.movies.Region(ordinal, 3 + day % 2, state.greetingTime, button)) { return 1; }
        view.SetTestClick({button.x + button.width / 2, button.y + button.height / 2});
        if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, path, seconds) ||
            !state.greetingExitRequested || profile.coins != coins) { return 1; }
        view.Begin(24);
        if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, path, seconds) ||
            !state.greetingClosing || state.greetingTime != start || profile.coins != coins + prize.coins ||
            profile.warbucks != warbucks + prize.warbucks || profile.experience != xp + prize.experience ||
            profile.statistics[32] != awarded + 1) { return 1; }
        view.clock += start / 2;
        view.Begin(24);
        if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, path, seconds) ||
            state.greetingTime != start - start / 2 || profile.statistics[32] != awarded + 1) { return 1; }
        if (day == 0 && !view.window.SaveFrame("out/ui-original-2026-09-09/greeting-original-exit.png")) { return 1; }
        view.clock += start;
        view.Begin(24);
        unsigned target = 5;
        if (day % 2 != 0) { target = 4; }
        if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, path, seconds) ||
            state.page != target || daily.CommitBonus(profile, seconds, store)) { return 1; }
    }
    CProfileManager reloaded;
    reloaded.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!LoadNativeProfile(toc, tables, reloaded, path, {}) || reloaded.dailyLastCommit != 7 ||
        reloaded.coins != profile.coins || reloaded.warbucks != profile.warbucks) { return 1; }
    const auto seconds = first + 9 * 86400;
    daily.RefreshUsageData(reloaded, seconds);
    if (reloaded.dailyConsecutiveDays != 1 || !daily.CommitBonus(reloaded, seconds, store)) { return 1; }
    AdvanceDailyDebugDay(reloaded, daily, seconds);
    if (reloaded.dailyLastLaunchSeconds != seconds || reloaded.dailyConsecutiveDays != 2 ||
        !daily.CommitBonus(reloaded, seconds, store) || !reloaded.SaveToDisk(path)) { return 1; }
    CProfileManager finalProfile;
    finalProfile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!LoadNativeProfile(toc, tables, finalProfile, path, {}) || finalProfile.dailyLastCommit != 2 ||
        daily.IsBonusAvailable(finalProfile, seconds) || view.movies.Failures() != 0 || glGetError() != 0) { return 1; }
    std::printf("[greeting-check] original-buttons=7 no-award-on-show=7 exit-only=7 reverse=7 duplicate=7 cycle=7 gap=1 cht=1 native-reload=2 failures=0\n");
    return 0;
}

int RunRefineryMenuCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path("out/ui-original-2026-09-09") / ("refinery-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = true;
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    MenuState state;
    state.page = 3;
    state.refinementRequired = true;
    // Isolated fixture amount. Native gameplay loads its actual source balance.
    profile.xplodium = 250;
    const auto coins = profile.coins;
    const auto warbucks = profile.warbucks;
    const auto now = CurrentSeconds();
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_EXPLODIUM");
    CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned idle = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, idle, end)) { return 1; }
    const auto *coin = OriginalMenuData("MDS_ICON_STANDARD", 2);
    MovieRegion clamped, lastAnimation;
    if (coin == nullptr || coin->sprites[0] != 0x0004002B ||
        !view.movies.SpriteBounds(4, 43, clamped) || !view.movies.SpriteBounds(4, 32, lastAnimation) ||
        clamped.width != lastAnimation.width || clamped.height != lastAnimation.height) { return 1; }
    MovieRegion meter;
    if (!view.movies.Region(ordinal, 0, idle, meter)) { return 1; }
    view.Begin(3);
    view.SetTestClick({meter.x + meter.width / 2, meter.y + meter.height / 2});
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refineryTransfer != -1) { return 1; }
    view.clock = 2000;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refineryTab != 1 ||
        view.Header(profile, progress, 3) == -3 || !view.window.SaveFrame("out/ui-original-2026-09-09/refinery-original-standard.png")) { return 1; }
    // Both category buttons use entry ordinal dispatch, not static parameter.
    MovieRegion category, graphic, hit;
    const auto *button = OriginalMenuData("MDS_BUTTON_REFINE_SLOT_CATEGORY", 1);
    if (button == nullptr || !view.movies.Region(ordinal, 16, state.refineryTime, category)) { return 1; }
    const unsigned categoryMovie = view.movies.Ordinal(button->movies[0]);
    if (!view.movies.Region(categoryMovie, 1, 0, graphic) || !view.movies.Region(categoryMovie, 0, 0, hit)) { return 1; }
    const float categoryX = category.x + category.width / 2 - (graphic.width * 2 + 8) / 2;
    for (unsigned tab : {0u, 1u}) {
        float x = categoryX;
        if (tab == 0) { x += graphic.width + 4; }
        view.Begin(3);
        view.SetTestClick({x + hit.x - 512 + hit.width / 2, category.y + hit.y - 384 + hit.height / 2});
        if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refineryTab != tab) {
            std::printf("[refinery-check] category failed expected=%u actual=%u\n", tab, state.refineryTab);
            return 1;
        }
        view.Begin(3);
        if (!DrawRefinery(view, state, profile, refinement, path, now) || view.Header(profile, progress, 3) == -3 ||
            !view.window.SaveFrame("out/ui-original-2026-09-09/refinery-original-tab-" + std::to_string(tab) + ".png")) { return 1; }
        for (unsigned cell = 1; cell < 6; ++cell) {
            MovieRegion offline;
            if (!view.movies.Region(ordinal, cell, state.refineryTime, offline)) { return 1; }
            const auto before = profile.refinery.slots[tab * 6 + cell].state;
            view.Begin(3);
            view.SetTestClick({offline.x + offline.width / 2, offline.y + offline.height / 2});
            if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refineryTransfer != -1 ||
                profile.refinery.slots[tab * 6 + cell].state != before || profile.xplodium != 250) { return 1; }
        }
    }
    // Move the source parent region; no old screen-space hit rectangle remains.
    const CMovie original = *movie;
    if (!view.movies.Region(ordinal, 0, state.refineryTime, meter)) { return 1; }
    for (auto &object : movie->objects) {
        if (object.type != 6 || object.frames.empty()) { continue; }
        for (auto &frame : object.frames) { frame.x -= 260; }
        break;
    }
    MovieRegion moved;
    if (!view.movies.Region(ordinal, 0, state.refineryTime, moved)) { return 1; }
    view.Begin(3);
    view.SetTestClick({meter.x + meter.width / 2, meter.y + meter.height / 2});
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refineryTransfer != -1) { return 1; }
    view.Begin(3);
    view.SetTestClick({moved.x + moved.width / 2, moved.y + moved.height / 2});
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refineryTransfer != 6 || profile.xplodium != 250) {
        std::printf("[refinery-check] moved meter click failed transfer=%d\n", state.refineryTransfer);
        return 1;
    }
    *movie = original;
    // Save halfway through transfer: the original amount has not been committed.
    view.clock += 187;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || profile.xplodium != 250 ||
        !profile.SaveToDisk(path) || !view.window.SaveFrame("out/ui-original-2026-09-09/refinery-original-transfer.png")) { return 1; }
    CProfileManager restored;
    if (!LoadNativeProfile(toc, tables, restored, path) || restored.xplodium != 250 || restored.coins != coins) { return 1; }
    view.clock += 188;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refineryTransfer != -1 ||
        profile.xplodium != 0 || profile.coins != coins || profile.refinery.slots[6].state != 3 ||
        !state.refinementRequired) { return 1; }
    if (!ReloadNativeProfile(restored, path) || restored.refinery.slots[6].state != 3 || restored.xplodium != 0) { return 1; }
    const CMovie *fill = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_BUCKET_FILL"));
    if (fill == nullptr) { return 1; }
    view.clock += fill->duration;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refineryStatusChapter[6] != 3 ||
        !view.window.SaveFrame("out/ui-original-2026-09-09/refinery-original-collect.png")) { return 1; }
    if (!view.movies.Region(ordinal, 0, state.refineryTime, meter)) { return 1; }
    const auto yield = profile.refinery.GetRefinementSlotYield(6);
    view.Begin(3);
    view.SetTestClick({meter.x + meter.width / 2, meter.y + meter.height / 2});
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refineryTransfer != 6 || profile.coins != coins) { return 1; }
    view.clock += 375;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || profile.coins != coins + yield ||
        state.refinementRequired || profile.refinery.slots[6].state != 1 || profile.warbucks != warbucks ||
        !ReloadNativeProfile(restored, path) || restored.coins != profile.coins || restored.refinery.slots[6].state != 1) { return 1; }
    view.clock += 400;
    view.Begin(3);
    view.SetTestClick({meter.x + meter.width / 2, meter.y + meter.height / 2});
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refineryTransfer != -1 ||
        profile.coins != coins + yield || glGetError() != 0) { return 1; }
    std::printf("[refinery-check] categories=2 offline=10 region-mutation transfer=375 fill collect save-reload failures=0\n");
    return 0;
}

int RunNavigationBarCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CProfileManager profile;
    const auto path = std::filesystem::path("out/ui-original-2026-09-09") / ("header-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_HEADER");
    CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(2, start, end)) { return 1; }
    MovieRegion first;
    if (!view.movies.Region(ordinal, 0, start, first)) { return 1; }
    view.Begin(2);
    view.SetTestClick({first.x + first.width / 2, first.y + first.height / 2});
    if (view.Header(profile, progress, 2) != -1) { return 1; }
    view.clock = start / 2;
    view.Begin(2);
    view.SetTestClick({first.x + first.width / 2, first.y + first.height / 2});
    if (view.Header(profile, progress, 2) != -1 ||
        !view.window.SaveFrame("out/ui-original-2026-09-09/header-original-opening.png")) { return 1; }
    view.clock = start;
    for (unsigned index = 0; index < std::size(kOriginalNavigationBranches); ++index) {
        MovieRegion area;
        if (!view.movies.Region(ordinal, index, start, area)) { return 1; }
        view.Begin(2);
        view.SetTestClick({area.x + area.width / 2, area.y + area.height / 2});
        if (view.Header(profile, progress, 2) != static_cast<int>(index)) {
            std::printf("[header-check] original button failed index=%u\n", index);
            return 1;
        }
    }
    if (!view.window.SaveFrame("out/ui-original-2026-09-09/header-original-ready.png")) { return 1; }
    // Change only the in-memory research copy; click must follow the source region.
    const CMovie original = *movie;
    for (auto &object : movie->objects) {
        if (object.type != 6 || object.frames.empty()) { continue; }
        for (auto &frame : object.frames) { frame.x += 37; frame.y += 190; }
        break;
    }
    MovieRegion moved;
    if (!view.movies.Region(ordinal, 0, start, moved) || moved.x == first.x || moved.y == first.y) { return 1; }
    view.Begin(2);
    view.SetTestClick({first.x + first.width / 2, first.y + first.height / 2});
    const int oldHit = view.Header(profile, progress, 2);
    if (oldHit != -1) { std::printf("[header-check] old position still hit=%d\n", oldHit); return 1; }
    view.Begin(2);
    view.SetTestClick({moved.x + moved.width / 2, moved.y + moved.height / 2});
    const int movedHit = view.Header(profile, progress, 2);
    if (movedHit != 0) { std::printf("[header-check] moved position hit=%d x=%.0f y=%.0f\n", movedHit, moved.x, moved.y); return 1; }
    *movie = original;
    view.animateNavigation = false;
    view.Begin(25);
    view.SetTestClick({first.x + first.width / 2, first.y + first.height / 2});
    if (view.Header(profile, progress, 25) != -1 ||
        !view.window.SaveFrame("out/ui-original-2026-09-09/header-original-hidden.png")) { return 1; }
    view.animateNavigation = true;
    view.Begin(2);
    view.SetTestClick({first.x + first.width / 2, first.y + first.height / 2});
    if (view.Header(profile, progress, 2) != -1) { return 1; }
    unsigned showStart = 0, showEnd = 0;
    if (!movie->GetChapterRange(1, showStart, showEnd)) { return 1; }
    view.clock += start - showStart;
    view.Begin(2);
    view.SetTestClick({first.x + first.width / 2, first.y + first.height / 2});
    if (view.Header(profile, progress, 2) != 0 || glGetError() != 0) { return 1; }
    std::printf("[header-check] seven native branches movie entrance hidden reentry mutated-position failures=0\n");
    return 0;
}

int RunMissionMenuCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CProfileManager profile;
    const auto path = std::filesystem::path("out/ui-original-2026-09-09") / ("mission-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = false;
    const unsigned listOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_LIST");
    const unsigned cardOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_BOX");
    const CMovie *list = view.movies.GetMovie(listOrdinal), *card = view.movies.GetMovie(cardOrdinal);
    unsigned testedWaves = 0, testedHordes = 0;
    unsigned start = 0, end = 0;
    if (list == nullptr || card == nullptr || !list->GetChapterRange(1, start, end)) { return 1; }
    for (unsigned planetIndex = 0; planetIndex < view.planetEntries.size(); ++planetIndex) {
        const auto &planet = view.planetEntries[planetIndex];
        if (planet.missions.empty()) { continue; }
        MenuState state;
        state.page = 21;
        state.planet = planetIndex;
        state.modeSelected = true;
        state.modeBound = true;
        state.modePhase = 2;
        state.modeTime = 1200;
        state.modeLastTick = view.clock;
        bool launch = false;
        view.Begin(21);
        if (!DrawOriginalMissionInfo(view, state, profile, launch) || launch) { return 1; }
        const std::string prefix = "out/ui-original-2026-09-09/mission-native-" + std::to_string(planetIndex);
        if (!view.window.SaveFrame(prefix + "-list.png")) { return 1; }
        for (unsigned index = 0; index < planet.missions.size(); ++index) {
            state.missionFocused = -1;
            state.missionFirst = std::min(index, static_cast<unsigned>(planet.missions.size() - 3));
            state.missionListTime = start;
            MovieRegion area;
            if (!view.movies.Region(listOrdinal, index - state.missionFirst + 1, start, area)) { return 1; }
            view.Begin(21);
            view.SetTestClick({area.x + area.width / 2, area.y + area.height / 2});
            if (!DrawOriginalMissionInfo(view, state, profile, launch) || launch || state.missionFocused != static_cast<int>(index)) {
                std::printf("[mission-menu-check] focus failed planet=%u index=%u actual=%d\n", planetIndex, index, state.missionFocused);
                return 1;
            }
            view.clock += 1000;
            view.Begin(21);
            if (!DrawOriginalMissionInfo(view, state, profile, launch) || launch) { return 1; }
            const auto &mission = planet.missions[index];
            const auto &info = planet.missionInfo[index];
            if (index == 0 || index + 1 == planet.missions.size()) {
                if (!view.window.SaveFrame(prefix + "-expanded-" + std::to_string(index) + ".png")) { return 1; }
            }
            std::printf("[mission-menu-check] planet=%u mission=%u type=%u level=%d threshold=%u waves=%u prereqs=%zu locked=%d title=%s\n",
                planetIndex, index, mission.type, info.requiredLevel, mission.value64, info.waveCount, info.prerequisites.size(),
                OriginalMissionLocked(profile, mission, info), info.title.c_str());
            MovieRegion cardBounds, detail;
            if (!view.movies.Region(cardOrdinal, 0, state.missionCardTime, cardBounds)) { return 1; }
            bool foundDetail = false;
            for (const auto &region : view.movies.Regions(cardOrdinal, state.missionCardTime,
                kMenuWidth / 2 - cardBounds.width / 2, kMenuHeight / 2 - cardBounds.height / 2)) {
                if (region.index == 8) { detail = region; foundDetail = true; }
            }
            if (!foundDetail) { return 1; }
            if (mission.type == 1) {
                MovieRegion pageRegion, tabGraphic, scrollbar;
                if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_WAVE_SELECT"), 1, state.missionWaveTime, pageRegion) ||
                    !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_BUTTON_LG"), 1, 0, tabGraphic) ||
                    !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_SCROLLBAR_HORIZ"), 0, 0, scrollbar)) { return 1; }
                const unsigned rowStep = static_cast<unsigned>(pageRegion.height + tabGraphic.height - scrollbar.height) / 3;
                for (unsigned localWave = 0; localWave < info.waveCount; ++localWave) {
                    state.wavePage = localWave / 10;
                    const unsigned cell = localWave % 10;
                    const float x = detail.x + (cell % 5 + 1) * std::floor(pageRegion.width / 6);
                    const float y = detail.y + rowStep * (cell / 5 + 1) - tabGraphic.height;
                    view.Begin(21);
                    view.SetTestClick({x, y});
                    if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
                    const unsigned wave = mission.value64 + localWave;
                    const bool allowed = !OriginalMissionLocked(profile, mission, info) && wave <= NativeMissionProgress(profile, mission.level);
                    if (launch != allowed || (launch && (state.startingWave != static_cast<int>(wave) || !SameObject(state.selectedMission, planet.data.missions[index])))) {
                        std::printf("[mission-menu-check] wave click failed planet=%u index=%u local=%u launch=%d allowed=%d actual=%d\n",
                            planetIndex, index, localWave, launch, allowed, state.startingWave);
                        return 1;
                    }
                    ++testedWaves;
                }
            } else if (mission.type == 2 && !OriginalMissionLocked(profile, mission, info)) {
                const auto *play = OriginalMenuData("MDS_BUTTON_PLAY", 0);
                MovieRegion graphic, art;
                if (play == nullptr || !view.movies.Region(view.movies.Ordinal(play->movies[0]), 1, 0, graphic) ||
                    !view.movies.SpriteBounds(5, 42 + mission.value66, art)) { return 1; }
                const float x = detail.x + art.width + (detail.width - art.width - graphic.width) / 2;
                const float y = detail.y + detail.height - graphic.height;
                view.Begin(21);
                view.SetTestClick({x + graphic.width / 2, y + graphic.height / 2});
                if (!DrawOriginalMissionInfo(view, state, profile, launch) || !launch || state.hordeStart != index ||
                    !SameObject(state.selectedMission, planet.data.missions[index])) { return 1; }
                ++testedHordes;
            }
        }
    }
    CProfileManager fresh;
    if (!LoadNativeProfile(toc, tables, fresh, path / "fresh")) { return 1; }
    for (const auto &planet : view.planetEntries) {
        for (unsigned index = 0; index < planet.missions.size(); ++index) {
            const auto &mission = planet.missions[index];
            const auto &info = planet.missionInfo[index];
            // Actual archive export2 must drive both level and previous-round locks.
            if (info.prerequisites.empty()) { continue; }
            if (info.requiredLevel > 1 || mission.value64 != 0) {
                if (!OriginalMissionLocked(fresh, mission, info)) { return 1; }
            }
        }
    }
    std::printf("[mission-menu-check] all authored missions focus wave-clicks=%u horde-clicks=%u fresh-profile script locks failures=0\n",
        testedWaves, testedHordes);
    return 0;
}

/** Replay input through the same PLAY callbacks as the GUI, with native saves. */
int RunPlayInteractionCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CProfileManager profile;
    const auto path = std::filesystem::path("out/play-interaction-check") / std::to_string(GetTickCount64());
    if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = true;
    MenuState state;
    unsigned failures = 0;
    for (unsigned frame = 0; frame < 150; ++frame) {
        view.clock += 16; view.Begin(0); view.SetTestClick({-1, -1});
        if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state)) { return 1; }
    }
    const bool autoSelected = state.starSelectedSlot == 1 && state.starLocked;
    if (!autoSelected) { ++failures; }
    std::printf("[play-interaction] auto-selected=%d slot=%d locked=%d failures=%u\n", autoSelected, state.starSelectedSlot, state.starLocked, failures);
    const unsigned modeOrdinal = view.movies.Ordinal("GLU_MOVIE_MULTIPLAYER_AND_VERSUS_MAP");
    MovieRegion mode;
    if (!view.movies.Region(modeOrdinal, 1, state.modeTime, mode)) { return 1; }
    view.Begin(0); view.SetTestClick({mode.x + mode.width / 2, mode.y + mode.height / 2});
    if (!DrawOriginalModeOverlay(view, state)) { return 1; }
    const unsigned before = state.modeTime;
    view.clock += 80; view.Begin(0); view.SetTestClick({-1, -1});
    if (!DrawOriginalModeOverlay(view, state)) { return 1; }
    const bool animated = state.modeTime != before && state.modePhase == 1;
    if (!animated || view.ModeParticleCount() == 0) { ++failures; }
    if (!view.window.SaveFrame((path / "mode-select.png").string())) { return 1; }
    std::printf("[play-interaction] mode-intermediate=%d time=%u..%u failures=%u\n", animated, before, state.modeTime, failures);
    view.clock += 3000; view.Begin(0); view.SetTestClick({-1, -1});
    if (!DrawOriginalModeOverlay(view, state)) { return 1; }
    state.Navigate(21);
    view.animateNavigation = false;
    bool launch = false;
    view.Begin(21); view.SetTestClick({-1, -1});
    if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
    const unsigned mainOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_MENU");
    const unsigned backOrdinal = view.movies.Ordinal("GLU_MOVIE_BACK_BUTTON");
    unsigned start = 0, end = 0;
    MovieRegion planet, back;
    if (!view.movies.GetMovie(mainOrdinal)->GetChapterRange(0, start, end) ||
        !view.movies.Region(mainOrdinal, 0, end, planet) ||
        !view.movies.GetMovie(backOrdinal)->GetChapterRange(0, start, end)) { return 1; }
    for (const auto &region : view.movies.Regions(backOrdinal, end, planet.x + planet.width / 2, planet.y + planet.height / 2, true)) {
        if (region.index == 0) { back = region; }
    }
    view.Begin(21); view.SetTestClick({std::max(1.0f, back.x + back.width / 2), back.y + back.height / 2});
    std::printf("[play-interaction] back-bounds=%.1f,%.1f %.1fx%.1f frame=%u mission=%u\n", back.x, back.y, back.width, back.height, end, state.missionTime);
    if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
    if (state.page != 0) { ++failures; }
    std::printf("[play-interaction] back-page=%u failures=%u\n", state.page, failures);
    state.Navigate(21); state.missionBound = false;
    view.Begin(21); view.SetTestClick({-1, -1});
    if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
    const unsigned listOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_LIST");
    MovieRegion viewport, first, second;
    if (!view.movies.GetMovie(listOrdinal)->GetChapterRange(1, start, end) ||
        !view.movies.Region(listOrdinal, 0, start, viewport) || !view.movies.Region(listOrdinal, 1, start, first) ||
        !view.movies.Region(listOrdinal, 2, start, second)) { return 1; }
    view.clock += 16; view.Begin(21);
    view.SetTestClick({viewport.x + viewport.width / 2, viewport.y + viewport.height / 2});
    view.ExchangeClick(false); view.dragX = -(second.x - first.x) / 8;
    if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
    const bool followsDrag = state.missionListTime != start || state.missionFirst != 0;
    if (!followsDrag) { ++failures; }
    std::printf("[play-interaction] list-follows-small-drag=%d time=%u rest=%u failures=%u\n", followsDrag, state.missionListTime, start, failures);
    const float stride = second.x - first.x;
    // Equal flicks must travel equally at 30/60-ish desktop frame cadences.
    float referenceDistance = 0;
    for (unsigned frameMs : {16u, 32u}) {
        MenuScrollMotion motion;
        float position = 0;
        motion.Update(position, 16, -stride / 2, 0, true, true, true, stride * 100, stride, end - start + 1);
        unsigned clock = 16;
        while (clock < 1616) {
            clock += frameMs;
            motion.Update(position, clock, 0, 0, false, false, true, stride * 100, stride, end - start + 1);
        }
        if (frameMs == 16) { referenceDistance = position; }
        else if (std::abs(position - referenceDistance) > 0.1f) { ++failures; }
    }
    for (unsigned frame = 0; frame < 6; ++frame) {
        view.clock += 16; view.Begin(21);
        view.SetTestClick({viewport.x + viewport.width / 2, viewport.y + viewport.height / 2});
        view.ExchangeClick(false); view.dragX = -stride / 2;
        if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
    }
    const float released = state.missionPosition;
    view.clock += 16; view.Begin(21); view.SetTestClick({-1, -1});
    if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
    if (state.missionPosition <= released || released < stride * 3) { ++failures; }
    for (unsigned frame = 0; frame < 180; ++frame) {
        view.clock += 16; view.Begin(21); view.SetTestClick({-1, -1});
        if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
    }
    const float maximum = (view.planetEntries[state.planet].missions.size() - 3) * stride;
    if (state.missionPosition > maximum || state.missionMotion.velocity != 0) { ++failures; }
    if (!view.window.SaveFrame((path / "revolutions-after-flick.png").string())) { return 1; }
    std::printf("[play-interaction] list-release=%.1f coast=%.1f maximum=%.1f failures=%u\n", released, state.missionPosition, maximum, failures);
    // Focus a real visible REV and swipe its wave selector continuously.
    MovieRegion card;
    if (!view.movies.Region(listOrdinal, 1, state.missionListTime, card)) { return 1; }
    view.Begin(21); view.SetTestClick({card.x + card.width / 2, card.y + card.height / 2});
    if (!DrawOriginalMissionInfo(view, state, profile, launch) || state.missionFocused < 0) { return 1; }
    for (unsigned frame = 0; frame < 65; ++frame) {
        view.clock += 16; view.Begin(21); view.SetTestClick({-1, -1});
        if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
    }
    const unsigned boxOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_BOX");
    MovieRegion box, waves;
    if (!view.movies.Region(boxOrdinal, 0, state.missionCardTime, box)) { return 1; }
    for (const auto &region : view.movies.Regions(boxOrdinal, state.missionCardTime,
        kMenuWidth / 2 - box.width / 2, kMenuHeight / 2 - box.height / 2)) {
        if (region.index == 8) { waves = region; }
    }
    const float waveBefore = state.wavePosition;
    view.clock += 16; view.Begin(21);
    view.SetTestClick({waves.x + waves.width / 2, waves.y + waves.height / 2});
    view.ExchangeClick(false); view.dragX = waves.width * 3;
    if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
    const float waveLeft = state.wavePosition;
    view.clock += 16; view.Begin(21);
    view.SetTestClick({waves.x + waves.width / 2, waves.y + waves.height / 2});
    view.ExchangeClick(false); view.dragX = -waves.width * 5;
    if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
    if (state.wavePosition <= waveLeft || state.wavePage < 2 || launch) { ++failures; }
    if (!view.window.SaveFrame((path / "waves-after-drag.png").string())) { return 1; }
    std::printf("[play-interaction] waves-before=%.1f left=%.1f right=%.1f page=%u failures=%u\n", waveBefore, waveLeft, state.wavePosition, state.wavePage, failures);
    // The same radial back action must close an expanded card, then the planet.
    for (unsigned press = 0; press < 2; ++press) {
        view.Begin(21); view.SetTestClick({std::max(1.0f, back.x + back.width / 2), back.y + back.height / 2});
        if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
        if (press == 0 && !state.missionClosing) { ++failures; }
        for (unsigned frame = 0; frame < 40 && state.page == 21; ++frame) {
            view.clock += 16; view.Begin(21); view.SetTestClick({-1, -1});
            if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
        }
    }
    if (state.page != 0) { ++failures; }
    std::printf("[play-interaction] nested-back-page=%u failures=%u\n", state.page, failures);
    return failures != 0;
}

int RunPlanetMenuCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path("out/ui-original-2026-09-09") /
        ("planet-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = true;
    MenuState state;
    const unsigned mapOrdinal = view.movies.Ordinal("GLU_MOVIE_MAP_PARALAX_COPY");
    const unsigned modeOrdinal = view.movies.Ordinal("GLU_MOVIE_MULTIPLAYER_AND_VERSUS_MAP");
    const CMovie *map = view.movies.GetMovie(mapOrdinal);
    const CMovie *overlay = view.movies.GetMovie(modeOrdinal);
    unsigned begin = 0, end = 0;
    if (map == nullptr || overlay == nullptr || !overlay->GetChapterRange(0, begin, end)) { return 1; }
    view.Begin(0);
    if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state)) { return 1; }
    view.clock += end;
    view.Begin(0);
    if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state) ||
        !view.window.SaveFrame("out/ui-original-2026-09-09/mode-original-expanded.png")) { return 1; }
    for (unsigned mode : {1u, 2u, 0u}) {
        MovieRegion touch;
        if (!view.movies.Region(modeOrdinal, mode * 2 + 1, state.modeTime, touch)) { return 1; }
        view.Begin(0);
        view.SetTestClick({touch.x + touch.width / 2, touch.y + touch.height / 2});
        if (!DrawOriginalModeOverlay(view, state)) { return 1; }
        if (mode != 0) {
            if (state.gameMode != 0 || state.modeSelected || !state.storePromptRequested || state.storePromptIndex != 2) { return 1; }
            if (!DrawStorePrompt(view, state)) { return 1; }
            view.clock += 1000;
            view.Begin(0);
            if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state) || !DrawStorePrompt(view, state) ||
                !view.window.SaveFrame("out/ui-original-2026-09-09/mode-original-offline.png")) { return 1; }
            state.storePopup.Hide();
            state.storePopup.Update(1000);
            state.storePopup.Update(1000);
        }
    }
    if (!state.modeSelected || state.modePhase != 1) { return 1; }
    view.clock += 150;
    view.Begin(0);
    if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state) ||
        !view.window.SaveFrame("out/ui-original-2026-09-09/mode-original-folding.png")) { return 1; }
    view.clock += overlay->duration;
    view.Begin(0);
    if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state) || state.modePhase != 2 ||
        !view.window.SaveFrame("out/ui-original-2026-09-09/mode-original-collapsed.png")) { return 1; }
    MovieRegion before, middle, after;
    if (!view.movies.Region(modeOrdinal, 1, 700, before) || !view.movies.Region(modeOrdinal, 1, 850, middle) ||
        !view.movies.Region(modeOrdinal, 1, 1000, after) || after.width != 175 || after.height != 175 ||
        std::abs(middle.x - (before.x + after.x) / 2) > 1 || std::abs(middle.y - (before.y + after.y) / 2) > 1) { return 1; }
    std::printf("[planet-check] mode-native-regions anchors-interpolate unscaled-hit-box offline-no-fake-match failures=0\n");
    for (unsigned index = 0; index < profile.clearedWaves.size(); ++index) {
        const auto &planet = view.planetEntries[index];
        if (planet.missions.empty() || planet.missions[0].type != 1 ||
            !SameObject(planet.missions[0].level, profile.nativeArchive->survivalLevels[index]) ||
            !map->GetChapterRange(planet.data.mapSlot - 1, begin, end)) { return 1; }
        // Test positions the original playback cursor at its authored stop.
        state.page = 0;
        state.starSelectedSlot = -1;
        state.starLocked = false;
        state.starTargetTime = -1;
        state.starTime = end;
        view.clock += 1000;
        MovieRegion touch;
        if (!view.movies.Region(mapOrdinal, planet.data.mapSlot, end, touch)) { return 1; }
        view.Begin(0);
        view.SetTestClick({touch.x + touch.width / 2, touch.y + touch.height / 2});
        if (!DrawOriginalStarMap(view, state, profile) || state.starSelectedSlot != static_cast<int>(planet.data.mapSlot) || state.page != 0) { return 1; }
        view.clock += 1000;
        view.Begin(0);
        if (!DrawOriginalStarMap(view, state, profile) || !state.starLocked || !DrawOriginalModeOverlay(view, state)) { return 1; }
        const std::string screenshot = "out/ui-original-2026-09-09/planet-selected-" + std::to_string(index) + ".png";
        if (!view.window.SaveFrame(screenshot)) { return 1; }
        view.Begin(0);
        view.SetTestClick({touch.x + touch.width / 2, touch.y + touch.height / 2});
        if (!DrawOriginalStarMap(view, state, profile) || !state.starEntering || state.page != 0) { return 1; }
        view.clock += 1000;
        view.Begin(0);
        if (!DrawOriginalStarMap(view, state, profile) || state.page != 21 || state.planet != index) { return 1; }
        std::printf("[planet-check] slot=%u original-thumb double-select exit-chapter host=%u level=%u:%u failures=0\n",
            planet.data.mapSlot, index, planet.missions[0].level.packHash, planet.missions[0].level.localIndex);
    }
    return 0;
}

int RunSocialOfflineCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path("out/ui-original-2026-09-09") /
        ("social-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    const auto originalCoins = profile.coins;
    const auto originalBucks = profile.warbucks;
    // Make every legacy condition pass, then prove the native boundary blocks it.
    profile.clearedWaves.fill(500);
    profile.enemyKills.fill(1000);
    for (unsigned index = 0; index < 8; ++index) {
        if (profile.ClaimActivity(index)) { return 1; }
    }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    const auto *button = OriginalMenuData("MDS_BUTTON_CONNECTIVITY", 0);
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_OFFLINE_BROHOOD");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (button == nullptr || button->action != 86 || movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
    for (unsigned page : {4u, 5u, 11u, 13u}) {
        for (bool credentials : {false, true}) {
            MenuState state;
            state.page = page;
            GameHostSettings().isConnected = credentials;
            view.Begin(page);
            if (!DrawOriginalSocialOffline(view, state, credentials)) { return 1; }
            MovieRegion area;
            if (!view.movies.Region(ordinal, 0, start, area)) { return 1; }
            bool found = false;
            MenuTestClick click;
            for (const auto &part : view.movies.Regions(view.movies.Ordinal(button->movies[0]), 0, area.x, area.y)) {
                if (part.index == 0) { click = {part.x + part.width / 2, part.y + part.height / 2}; found = true; }
            }
            if (!found) { return 1; }
            view.clock += movie->duration * 3;
            view.Begin(page);
            view.SetTestClick(click);
            if (!DrawOriginalSocialOffline(view, state, credentials) || view.ExchangeClick(false) ||
                state.page != page || state.promotion.IsActive() || state.socialTime < start || state.socialTime > end) { return 1; }
            const auto screenshot = std::filesystem::path("out/ui-original-2026-09-09") /
                ("social-original-" + std::to_string(page) + "-" + std::to_string(credentials) + ".png");
            if (!view.window.SaveFrame(screenshot.string())) { return 1; }
            std::printf("[social-check] page=%u credentials=%u original-regions retry-offline failures=0\n", page, credentials);
        }
    }
    GameHostSettings().isConnected = false;
    if (profile.coins != originalCoins || profile.warbucks != originalBucks || !profile.SaveToDisk(path) ||
        !ReloadNativeProfile(profile, path) || profile.coins != originalCoins || profile.warbucks != originalBucks) { return 1; }
    std::printf("[social-check] native-no-legacy-reward wallet-reload failures=0\n");
    return 0;
}

int RunOptionsCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path("out/ui-original-2026-09-09") /
        ("options-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = true;
    MenuState state;
    state.page = 6;
    const unsigned list = view.movies.Ordinal("GLU_MOVIE_LIST_MENU");
    const unsigned button = view.movies.Ordinal("GLU_MOVIE_LIST_MENU_BUTTON");
    const CMovie *movie = view.movies.GetMovie(list);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
    bool changed = false;
    view.Begin(6);
    if (!DrawOptions(view, state, profile, changed)) { return 1; }
    view.clock += start / 2;
    view.Begin(6);
    view.SetTestClick({240, 360});
    if (!DrawOptions(view, state, profile, changed) || changed || state.optionsOpening != start / 2 ||
        !view.window.SaveFrame("out/ui-original-2026-09-09/options-opening.png")) { return 1; }
    const unsigned order[] = {0, 1, 3, 4, 5, 7, 9, 6, 2, 8};
    for (unsigned index : order) {
        changed = false;
        state.page = 6;
        state.optionsScroll = std::clamp(float(static_cast<int>(index) - 1), 0.0f, 7.0f);
        state.optionsTarget = state.optionsScroll;
        view.clock += movie->duration;
        view.Begin(6);
        if (!DrawOptions(view, state, profile, changed)) { return 1; }
        const int base = static_cast<int>(state.optionsScroll) - 1;
        MenuTestClick click;
        bool found = false;
        for (const auto &region : view.movies.Regions(list, start)) {
            if (region.type < 2 || base + static_cast<int>(region.type) - 2 != static_cast<int>(index)) { continue; }
            for (const auto &part : view.movies.Regions(button, 0, region.x + region.width / 2, region.y + region.height / 2)) {
                if (part.index == 0) { click = {part.x + part.width / 2, part.y + part.height / 2}; found = true; }
            }
        }
        const auto before = profile;
        const auto *entry = OriginalMenuData("MDS_OPTIONS", index);
        const auto label = OptionsText(view, profile, index, 0);
        const auto body = OptionsText(view, profile, index, 1);
        if (!found || entry == nullptr || label.empty() || body.empty()) { return 1; }
        view.Begin(6);
        view.SetTestClick(click);
        if (!DrawOptions(view, state, profile, changed)) { return 1; }
        if (entry->action != 1 && state.optionsFocus != index) { return 1; }
        if (entry->action == 9 && profile.soundEnabled == before.soundEnabled) { return 1; }
        if (entry->action == 10 && profile.musicEnabled == before.musicEnabled) { return 1; }
        if (entry->action == 17 && profile.options.AutoBro() != (before.options.AutoBro() + 1) % 3) { return 1; }
        if (entry->action == 77 && profile.options.NotificationsEnabled() == before.options.NotificationsEnabled()) { return 1; }
        if (entry->action == 113 && profile.pushChallenges == before.pushChallenges) { return 1; }
        if (entry->action == 79 && state.page != 29) { return 1; }
        if (entry->action == 1 && state.page != 8) { return 1; }
        if (entry->action == 20 && state.page != 6) { return 1; }
        const auto expectedOptions = profile.options;
        const bool expectedSound = profile.soundEnabled, expectedMusic = profile.musicEnabled, expectedPush = profile.pushChallenges;
        if (!profile.SaveToDisk(path) || !profile.LoadFromDisk(path) || profile.soundEnabled != expectedSound ||
            profile.musicEnabled != expectedMusic || profile.pushChallenges != expectedPush ||
            profile.options.AutoBro() != expectedOptions.AutoBro() ||
            profile.options.NotificationsEnabled() != expectedOptions.NotificationsEnabled() ||
            profile.coins != before.coins || profile.warbucks != before.warbucks) { return 1; }
        view.clock += movie->duration;
        view.Begin(6);
        if (!DrawOptions(view, state, profile, changed) || glGetError() != 0 ||
            !view.window.SaveFrame("out/ui-original-2026-09-09/options-entry-" + std::to_string(index) + ".png")) { return 1; }
        std::printf("[options-check] index=%u action=%u label=%s body=%zu original-layout native-reload failures=0\n",
            index, entry->action, label.c_str(), body.size());
        if (index == 9) {
            MovieRegion bodyArea;
            if (!view.movies.Region(list, 8, start, bodyArea)) { return 1; }
            view.Begin(6);
            view.SetTestClick({bodyArea.x + bodyArea.width / 2, bodyArea.y + bodyArea.height / 2});
            view.dragY = -bodyArea.height / 2;
            if (!DrawOptions(view, state, profile, changed) || state.optionsBodyScroll <= 0 ||
                !view.window.SaveFrame("out/ui-original-2026-09-09/options-about-scroll.png")) { return 1; }
            view.clock += movie->duration;
            view.Begin(6);
            view.SetTestClick({bodyArea.x + bodyArea.width / 2, bodyArea.y + bodyArea.height / 2});
            view.dragY = -bodyArea.height * 10;
            if (!DrawOptions(view, state, profile, changed) ||
                !view.window.SaveFrame("out/ui-original-2026-09-09/options-about-last-page.png")) { return 1; }
            std::printf("[options-check] authored text pages scrollbar drag-clamp failures=0\n");
        }
    }
    state.page = 6;
    state.optionsFocus = 2;
    state.Navigate(8);
    view.animateNavigation = false;
    unsigned helpCount = 0;
    while (OriginalMenuData("MDS_HELP", helpCount)) { ++helpCount; }
    for (unsigned index = 0; index < helpCount; ++index) {
        state.optionsFocus = index;
        state.optionsScroll = state.optionsTarget = std::clamp(float(int(index) - 1), 0.0f, float(helpCount - 3));
        view.clock += movie->duration;
        view.Begin(8);
        if (!DrawOptions(view, state, profile, changed)) { return 1; }
        if (OptionsText(view, profile, index, 0, "MDS_HELP").empty()) { return 1; }
    }
    if (!view.window.SaveFrame("out/ui-original-2026-09-09/help-last-item.png")) { return 1; }
    const auto regions = view.movies.Regions(list, start, 512, 384, true);
    const auto *back = OriginalMenuData("MDS_BUTTON_BACK", 0);
    bool returned = false;
    for (const auto &region : regions) {
        if (region.index != regions.size() - 3) { continue; }
        MovieRegion touch;
        bool found = false;
        for (const auto &part : view.movies.Regions(view.movies.Ordinal(back->movies[0]), 0,
            region.x + region.width / 2, region.y + region.height / 2, true)) {
            if (part.index == 0) { touch = part; found = true; }
        }
        if (!found) { return 1; }
        view.Begin(8);
        view.SetTestClick({touch.x + touch.width / 2, touch.y + touch.height / 2});
        if (!DrawOptions(view, state, profile, changed)) { return 1; }
        returned = state.page == 6 && state.optionsFocus == 2;
    }
    std::printf("[options-check] help-items=%u original-back=%d\n", helpCount, returned);
    if (!returned || helpCount == 0) { return 1; }
    return 0;
}

int RunUpgradePopupCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadStoreCatalog(toc, tables, store) ||
        !LoadWeaponCatalog(toc, tables, weapons)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    MenuState state;
    state.page = 26;
    state.masteryWeapon = profile.configuration.guns[0];
    const WeaponEntry *weapon = FindMasteryWeapon(weapons, state.masteryWeapon);
    const StoreEntry *item = FindWeaponStore(store, state.masteryWeapon);
    if (weapon == nullptr || item == nullptr || item->data.statGroups[7].size() < 2) { return 1; }
    const unsigned threshold = weapon->data.GetMasteryThreshold(0);
    const unsigned initialXP = threshold / 2;
    // Explicit test-only money/XP; this profile never touches saves/ or userdata/.
    profile.warbucks = 10000;
    profile.AddWeaponExperience(state.masteryWeapon, initialXP, weapon->data.GetMasteryLimit());
    const std::filesystem::path path = "out/ui-original-2026-09-09/upgrade-profile.dat";
    if (!profile.SaveToDisk(path)) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    const unsigned popupOrdinal = view.movies.Ordinal("GLU_MOVIE_UPGRADE_POPUP");
    const CMovie *popup = view.movies.GetMovie(popupOrdinal);
    const CMovie *stars = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_WEAPON_UPGRADE_MASTERY"));
    if (popup == nullptr || stars == nullptr) { return 1; }
    unsigned openStart = 0, openEnd = 0, closeStart = 0, closeEnd = 0, idleStart = 0, idleEnd = 0;
    if (!popup->GetChapterRange(0, openStart, openEnd) || !popup->GetChapterRange(1, idleStart, idleEnd) ||
        !popup->GetChapterRange(2, closeStart, closeEnd)) { return 1; }
    unsigned initialTarget = 0, upgradedTarget = 0;
    if (!CMenuUpgradePopup::StarsTarget(*stars, weapon->data, initialXP, initialTarget) ||
        !CMenuUpgradePopup::StarsTarget(*stars, weapon->data, threshold, upgradedTarget)) { return 1; }

    CMovie changedPopup = *popup, changedStars = *stars;
    for (unsigned &chapter : changedPopup.chapters) { chapter *= 2; }
    changedPopup.duration *= 2;
    for (unsigned &chapter : changedStars.chapters) { chapter *= 3; }
    changedStars.duration *= 3;
    CMenuUpgradePopup mutation;
    if (!mutation.Bind(changedPopup, changedStars, weapon->data, initialXP)) { return 1; }
    unsigned changedStart = 0, changedEnd = 0;
    if (!changedPopup.GetChapterRange(0, changedStart, changedEnd) || mutation.TargetTime() == initialTarget) { return 1; }
    mutation.Update(changedEnd - changedStart);
    if (mutation.GetState() != CMenuUpgradePopup::State::Opening || mutation.StarsTime() != 0) { return 1; }
    mutation.Update(1);
    if (mutation.GetState() != CMenuUpgradePopup::State::Ready || mutation.StarsTime() != 0) { return 1; }
    mutation.Update(123);
    if (mutation.StarsTime() != 123) { return 1; }
    std::printf("[upgrade-check] mutated-chapters opening-no-stars 1x-fill failures=0\n");

    MovieRegion buyArea, closeArea;
    if (!view.movies.Region(popupOrdinal, kUpgradeBuyRegion, idleStart, buyArea) ||
        !view.movies.Region(popupOrdinal, kUpgradeCloseRegion, idleStart, closeArea)) { return 1; }
    const OriginalMenuEntry *buy = OriginalMenuData("MDS_BUTTON_STORE_UPGRADE", 2);
    const OriginalMenuEntry *close = OriginalMenuData("MDS_BUTTON_STORE_UPGRADE", 0);
    if (buy == nullptr || close == nullptr) { return 1; }
    MovieRegion buyTouch, closeTouch;
    bool foundBuy = false, foundClose = false;
    for (const MovieRegion &region : view.movies.Regions(view.movies.Ordinal(buy->movies[0]), 0, buyArea.x, buyArea.y)) {
        if (region.index == 0) { buyTouch = region; foundBuy = true; }
    }
    for (const MovieRegion &region : view.movies.Regions(view.movies.Ordinal(close->movies[0]), 0, closeArea.x, closeArea.y)) {
        if (region.index == 0) { closeTouch = region; foundClose = true; }
    }
    if (!foundBuy || !foundClose) { return 1; }
    struct Step { unsigned delta; const char *name; unsigned click; };
    const unsigned opening = openEnd - openStart + 1, closing = closeEnd - closeStart + 1;
    const unsigned fill = upgradedTarget - initialTarget;
    const Step steps[] = {{0, "opening", 0}, {opening / 2, "opening-half", 1},
        {opening - opening / 2, "opened", 0}, {initialTarget / 2, "fill-half", 0},
        {initialTarget, "filled", 0}, {0, "upgrade-start", 1}, {fill / 2, "upgrade-half", 1},
        {fill - fill / 2, "flash", 0}, {125, "flash-half", 0}, {125, "ready", 0},
        {0, "close", 2}, {closing / 2, "closing", 0}, {closing - closing / 2, "closed", 0}};
    unsigned index = 0;
    for (const Step &step : steps) {
        view.clock += step.delta;
        view.Begin(26);
        if (step.click == 1) { view.SetTestClick({buyTouch.x + buyTouch.width / 2, buyTouch.y + buyTouch.height / 2}); }
        if (step.click == 2) { view.SetTestClick({closeTouch.x + closeTouch.width / 2, closeTouch.y + closeTouch.height / 2}); }
        if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path)) { return 1; }
        if (index < 3 && state.masteryPopup.StarsTime() != 0) { return 1; }
        if (index < 5 && profile.GetWeaponExperience(state.masteryWeapon) != initialXP) { return 1; }
        if (index == 4 && state.masteryPopup.StarsTime() != initialTarget) { return 1; }
        if (index == 5 && state.masteryPopup.GetState() != CMenuUpgradePopup::State::Upgrading) { return 1; }
        if (index == 6 && state.masteryPopup.DisplayExperience() != initialXP) { return 1; }
        if (index == 7 && state.masteryPopup.GetState() != CMenuUpgradePopup::State::Flash) { return 1; }
        if (index == 9 && state.masteryPopup.GetState() != CMenuUpgradePopup::State::Ready) { return 1; }
        if (index == 12 && state.page == 26) { return 1; }
        const std::string capture = std::string("out/ui-original-2026-09-09/upgrade-") + step.name + ".png";
        if (glGetError() != 0 || !view.window.SaveFrame(capture)) { return 1; }
        view.window.Present();
        std::printf("[upgrade-check] phase=%s movie=%u stars=%u state=%u\n", step.name,
            state.masteryPopup.MovieTime(), state.masteryPopup.StarsTime(), static_cast<unsigned>(state.masteryPopup.GetState()));
        ++index;
    }
    CProfileManager restored;
    restored.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!restored.LoadFromDisk(path) || restored.GetWeaponExperience(state.masteryWeapon) != threshold ||
        restored.warbucks != 10000 - item->data.statGroups[7][1]) { return 1; }
    std::printf("[upgrade-check] phases=%u one-purchase opening-and-upgrading-block-input reload failures=0\n", index);
    // Reopen the real panel, then buy silver and gold. Gold closes only after
    // the star movie reaches its target and the native 250 ms flash completes.
    state.Navigate(26);
    view.Begin(26);
    if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path)) { return 1; }
    view.clock += opening;
    view.Begin(26);
    if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path)) { return 1; }
    unsigned totalPrice = static_cast<unsigned>(item->data.statGroups[7][1]);
    for (unsigned level = 2; level <= kMaxMasteryLevel; ++level) {
        view.Begin(26);
        view.SetTestClick({buyTouch.x + buyTouch.width / 2, buyTouch.y + buyTouch.height / 2});
        if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path) ||
            state.masteryPopup.GetState() != CMenuUpgradePopup::State::Upgrading) { return 1; }
        view.clock += stars->duration;
        view.Begin(26);
        if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path) ||
            state.masteryPopup.GetState() != CMenuUpgradePopup::State::Flash) { return 1; }
        view.clock += 250;
        view.Begin(26);
        if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path)) { return 1; }
        totalPrice += static_cast<unsigned>(item->data.statGroups[7][level]);
        if (level < kMaxMasteryLevel && state.masteryPopup.GetState() != CMenuUpgradePopup::State::Ready) { return 1; }
        if (level == kMaxMasteryLevel && state.masteryPopup.GetState() != CMenuUpgradePopup::State::Closing) { return 1; }
    }
    view.clock += closing;
    view.Begin(26);
    if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path) || state.page == 26 ||
        !restored.LoadFromDisk(path) || restored.GetWeaponExperience(state.masteryWeapon) != weapon->data.GetMasteryThreshold(2) ||
        restored.warbucks != 10000 - totalPrice) { return 1; }
    std::printf("[upgrade-check] reopen silver gold auto-close total-price=%u reload failures=0\n", totalPrice);
    // Exercise the nested original modal against an isolated native account.
    CProfileManager poorProfile;
    poorProfile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto fundsPath = std::filesystem::path("out/ui-original-2026-09-09") /
        ("upgrade-funds-native-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, poorProfile, fundsPath, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    poorProfile.warbucks = 0;
    poorProfile.weaponMastery.clear();
    MenuState fundsState;
    fundsState.page = 26;
    fundsState.masteryWeapon = state.masteryWeapon;
    const unsigned price = static_cast<unsigned>(item->data.statGroups[7][1]);
    const int offer = FindCurrencyOffer(store, 1, price);
    if (offer < 0 || store[offer].data.rarePrice < price) { return 1; }
    const CMovie *prompt = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_POPUP"));
    if (prompt == nullptr) { return 1; }
    const unsigned durations[] = {0, opening + 1, 0, prompt->duration, 0, prompt->duration,
        prompt->duration, 0, prompt->duration, 0, 2000, 2000, prompt->duration, prompt->duration, 0};
    for (unsigned phase = 0; phase < 15; ++phase) {
        view.clock += durations[phase];
        view.Begin(26);
        if (phase == 2 || phase == 7 || phase == 14) {
            view.SetTestClick({buyTouch.x + buyTouch.width / 2, buyTouch.y + buyTouch.height / 2});
        }
        if (phase == 4 || phase == 9) {
            unsigned buttonRegion = 3;
            if (phase == 9) { buttonRegion = 4; }
            MovieRegion touch;
            if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_POPUP"), buttonRegion,
                fundsState.storePopup.MovieTime(), touch)) { return 1; }
            view.SetTestClick({touch.x + touch.width / 2, touch.y + touch.height / 2});
        }
        if (!CompleteOfflineIAP(view.clock, fundsState, poorProfile, store, fundsPath) ||
            !DrawMastery(view, fundsState, poorProfile, toc, tables, store, weapons, fundsPath) ||
            !DrawStorePrompt(view, fundsState)) { return 1; }
        if (phase <= 10 && (poorProfile.warbucks != 0 || poorProfile.GetWeaponExperience(fundsState.masteryWeapon) != 0)) { return 1; }
        if (phase == 3 && (!fundsState.storePopup.IsReady() || fundsState.failedPrice != price ||
            fundsState.failedMissing != price || fundsState.currencyOffer != offer)) { return 1; }
        if ((phase == 6 || phase == 13) && fundsState.storePopup.IsActive()) { return 1; }
        if (phase == 9 && !fundsState.currencyPending) { return 1; }
        if (phase >= 11 && phase < 14 && poorProfile.warbucks != store[offer].data.rarePrice) { return 1; }
        if (phase == 3 || phase == 10 || phase == 14) {
            const auto capture = "out/ui-original-2026-09-09/upgrade-funds-" + std::to_string(phase) + ".png";
            if (glGetError() != 0 || !view.window.SaveFrame(capture)) { return 1; }
        }
        std::printf("[upgrade-funds-check] phase=%u pending=%u modal=%u rare=%llu xp=%u\n", phase,
            fundsState.currencyPending, fundsState.storePopup.IsActive(), poorProfile.warbucks,
            poorProfile.GetWeaponExperience(fundsState.masteryWeapon));
    }
    if (poorProfile.warbucks != store[offer].data.rarePrice - price ||
        poorProfile.GetWeaponExperience(fundsState.masteryWeapon) != threshold ||
        !poorProfile.LoadFromDisk(fundsPath) || poorProfile.warbucks != store[offer].data.rarePrice - price ||
        poorProfile.GetWeaponExperience(fundsState.masteryWeapon) != threshold) { return 1; }
    std::printf("[upgrade-funds-check] original-prompt dismiss offer wait retry-upgrade native-reload failures=0\n");
    poorProfile.configuration.guns[0] = state.masteryWeapon;
    const GameObjectRef secondGun = poorProfile.configuration.guns[1];
    const WeaponEntry *secondWeapon = FindMasteryWeapon(weapons, secondGun);
    std::printf("[upgrade-swap-check] second=%u:%u found=%u same=%u store=%u\n", secondGun.packHash, secondGun.localIndex,
        secondWeapon != nullptr, SameObject(secondGun, state.masteryWeapon), FindWeaponStore(store, secondGun) != nullptr);
    if (secondWeapon == nullptr || SameObject(secondGun, state.masteryWeapon)) { return 1; }
    // The imported second gun is already gold. Native reload preserves records
    // absent from the projected vector; reset the isolated fixture explicitly.
    poorProfile.weaponMastery.clear();
    poorProfile.AddWeaponExperience(state.masteryWeapon, threshold, weapon->data.GetMasteryLimit());
    poorProfile.AddWeaponExperience(secondGun, secondWeapon->data.GetMasteryThreshold(0) / 2, secondWeapon->data.GetMasteryLimit());
    const auto fundsBeforeSwap = poorProfile.warbucks;
    MenuState swapState;
    swapState.page = 26;
    swapState.masteryWeapon = state.masteryWeapon;
    view.Begin(26);
    if (!DrawMastery(view, swapState, poorProfile, toc, tables, store, weapons, fundsPath)) {
        std::printf("[upgrade-swap-check] opening draw failed\n"); return 1;
    }
    view.clock += opening;
    view.Begin(26);
    if (!DrawMastery(view, swapState, poorProfile, toc, tables, store, weapons, fundsPath)) {
        std::printf("[upgrade-swap-check] ready draw failed\n"); return 1;
    }
    MovieRegion swapArea, swapTouch;
    const auto *swap = OriginalMenuData("MDS_BUTTON_STORE_UPGRADE", 1);
    if (swap == nullptr || !view.movies.Region(popupOrdinal, 10, swapState.masteryPopup.MovieTime(), swapArea)) { return 1; }
    // Match DrawOriginalMovieButton: child origin is the parent region center.
    bool foundSwapTouch = false;
    for (const auto &region : view.movies.Regions(view.movies.Ordinal(swap->movies[0]), 0,
        swapArea.x + swapArea.width / 2, swapArea.y + swapArea.height / 2)) {
        if (region.index == 0) { swapTouch = region; foundSwapTouch = true; break; }
    }
    if (!foundSwapTouch) { return 1; }
    const MenuTestClick swapClick{swapTouch.x + swapTouch.width / 2, swapTouch.y + swapTouch.height / 2};
    const unsigned beforeSwapTime = swapState.masteryPopup.MovieTime();
    std::printf("[upgrade-swap-check] click=%.1f/%.1f movie=%u state=%u alpha=%.3f input=%u xp=%u limit=%u\n", swapClick.x, swapClick.y, beforeSwapTime,
        static_cast<unsigned>(swapState.masteryPopup.GetState()), swapArea.alpha, view.inputEnabled,
        poorProfile.GetWeaponExperience(secondGun), secondWeapon->data.GetMasteryLimit());
    view.Begin(26);
    view.SetTestClick(swapClick);
    if (!DrawMastery(view, swapState, poorProfile, toc, tables, store, weapons, fundsPath) ||
        !SameObject(swapState.masteryWeapon, secondGun) || swapState.masteryPopup.MovieTime() != beforeSwapTime ||
        swapState.masteryPopup.StarsTime() != 0 || poorProfile.warbucks != fundsBeforeSwap ||
        !SameObject(poorProfile.configuration.guns[0], state.masteryWeapon)) {
        std::printf("[upgrade-swap-check] selected=%u:%u time=%u stars=%u\n", swapState.masteryWeapon.packHash,
            swapState.masteryWeapon.localIndex, swapState.masteryPopup.MovieTime(), swapState.masteryPopup.StarsTime()); return 1;
    }
    view.clock += 100;
    view.Begin(26);
    if (!DrawMastery(view, swapState, poorProfile, toc, tables, store, weapons, fundsPath) ||
        swapState.masteryPopup.StarsTime() != std::min(100u, swapState.masteryPopup.TargetTime())) { return 1; }
    if (!view.window.SaveFrame("out/ui-original-2026-09-09/upgrade-swap.png")) { return 1; }
    std::printf("[upgrade-swap-check] store-entry distinct-guns stars-reset no-reopen unchanged-loadout failures=0\n");
    return 0;
}

/** Real bank card/input/prompt path, with native saves and isolated fixtures. */
int CheckBank(CResTOCManager &toc, PackTables &tables, const CPlayerProgress::Template &progress,
    const CRefinementManager::Template &refinement, const std::vector<StoreEntry> &store,
    const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armor) {
    MenuTestClick buyClick;
    MenuTestClick dismissClick;
    float columnPitch = 0, rowPitch = 0;
    std::vector<std::pair<int, unsigned>> currencies;
    for (unsigned index = 0; index < store.size(); ++index) {
        const auto &item = store[index].data;
        if (item.type >= 14 && item.type <= 16 && item.displayOrder >= 0 && item.value242 != 1) {
            currencies.push_back({item.displayOrder, index});
        }
    }
    std::sort(currencies.begin(), currencies.end());
    {
        GameMenu probe;
        if (!probe.Open(toc, tables)) { return 1; }
        MovieRegion slot, price, button;
        StoreCardFace face;
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), kFirstColumnRegion, probe.storeRestTime, slot)) { return 1; }
        MovieRegion secondColumn, promptTouch;
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), kFirstColumnRegion + 1, probe.storeRestTime, secondColumn) ||
            !probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_POPUP"), 0, 500, promptTouch)) { return 1; }
        columnPitch = secondColumn.x - slot.x;
        rowPitch = slot.height / 2 + 5;
        dismissClick = {promptTouch.x + promptTouch.width / 2, promptTouch.y + promptTouch.height / 2, 100};
        face.x = slot.x;
        face.y = slot.y;
        const auto *entry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", kBuyButtonEntry);
        if (entry == nullptr || !CardRegion(probe, probe.movies.Ordinal("GLU_MOVIE_SHOP_BOX"), kCardPriceRegion, face, price) ||
            !probe.movies.Region(probe.movies.Ordinal(entry->movies[0]), 1, 0, button)) { return 1; }
        buyClick = {price.x + price.width - button.width / 2, price.y + price.height - button.height / 2};
        constexpr unsigned parameters[] = {17, 14, 15, 16};
        for (unsigned index = 0; index < 4; ++index) {
            entry = OriginalMenuData("MDS_BUTTON_STORE_SORT_CURRENCY", index);
            if (entry == nullptr || entry->action != 65 || entry->parameter != parameters[index]) { return 1; }
            std::printf("[bank-check] original-filter row=%u label=%s parameter=%u\n", index,
                probe.movies.NamedString(entry->strings[0]).c_str(), entry->parameter);
        }
    }
    const auto root = std::filesystem::path("out/ui-original-2026-09-09") / ("bank-check-" + std::to_string(GetTickCount64()));
    for (unsigned phase = 0; phase < 9 + currencies.size(); ++phase) {
        CProfileManager profile;
        profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
        const auto path = root / std::to_string(phase);
        if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
        MenuState state;
        state.page = 2;
        state.shopCategory = 3;
        state.shopGunSlot = profile.activeWeaponSlot;
        state.shopFilter = 1u << 14;
        if (phase == 2) { state.shopFilter = 1u << 15; }
        if (phase >= 3) { state.shopFilter = 1u << 16; }
        if (phase == 4 || phase == 7 || phase == 8) { profile.coins = 0; profile.warbucks = 0; }
        const auto coins = profile.coins;
        const auto bucks = profile.warbucks;
        std::vector<StoreEntry> fixture = store;
        unsigned first = static_cast<unsigned>(fixture.size());
        if (phase >= 9) {
            first = currencies[phase - 9].second;
            state.shopFilter = 1u << fixture[first].data.type;
        }
        for (unsigned index = 0; index < fixture.size(); ++index) {
            const auto &item = fixture[index].data;
            if (item.type < 14 || item.type > 16 || item.displayOrder < 0 || item.value242 == 1 ||
                (state.shopFilter & (1u << item.type)) == 0) { continue; }
            if (phase >= 9) { continue; }
            if (phase >= 6 && item.value32 == 0) { continue; }
            if (first == fixture.size() || item.displayOrder < fixture[first].data.displayOrder) { first = index; }
        }
        if (first == fixture.size()) { return 1; }
        unsigned position = 0;
        for (const auto &row : currencies) {
            if (row.second == first) { break; }
            if ((state.shopFilter & (1u << (fixture[row.second].data.type))) != 0) { ++position; }
        }
        state.shopScroll = (position / 2) * columnPitch;
        MenuTestClick cardClick = buyClick;
        cardClick.y += (position % 2) * rowPitch;
        if (phase == 5) { fixture[first].data.commonPrice += 321; }
        std::vector<MenuTestClick> clicks = {cardClick, {30, 90, 2000}};
        if (phase >= 3) { clicks[1] = {-100, -100, 2000}; }
        if (phase != 0) { clicks.push_back({-100, -100, 4000}); clicks.push_back({-100, -100, 1000}); }
        if (phase == 8) {
            clicks.push_back(dismissClick);
            clicks.push_back({-100, -100, 2000});
            clicks.push_back({-100, -100, 2000});
        }
        const auto capture = std::filesystem::path("out/ui-original-2026-09-09") / ("bank-phase-" + std::to_string(phase) + ".png");
        if (ShowGameMenu(toc, tables, profile, progress, refinement, fixture, weapons, armor, state, path, capture.string(), &clicks) != -2 ||
            state.page != 2 || state.shopCategory != 3) { return 1; }
        if (phase == 0) {
            if (!state.currencyPending || profile.coins != coins || profile.warbucks != bucks || !state.storePopup.IsReady()) { return 1; }
        } else {
            auto expectedCoins = coins;
            auto expectedBucks = bucks;
            const auto &item = fixture[first].data;
            const bool insufficient = phase == 4 || phase == 7 || phase == 8;
            if (!insufficient) {
                if (item.type == 14) { expectedCoins += item.commonPrice; }
                else if (item.type == 15) { expectedBucks += item.rarePrice; }
                else if (item.value32 != 0) { expectedCoins -= item.commonPrice; expectedBucks += item.rarePrice; }
                else { expectedCoins += item.commonPrice; expectedBucks -= item.rarePrice; }
            }
            if (state.currencyPending || profile.coins != expectedCoins || profile.warbucks != expectedBucks) { return 1; }
            if (!insufficient && (!profile.LoadFromDisk(path) || profile.coins != expectedCoins || profile.warbucks != expectedBucks)) { return 1; }
            if ((phase == 4 || phase == 7) && !state.storePopup.IsReady()) { return 1; }
            if (phase == 8 && state.storePopup.IsActive()) { return 1; }
        }
        std::printf("[bank-check] phase=%u type=%u pending=%u common=%llu rare=%llu original-data reload failures=0\n",
            phase, fixture[first].data.type, state.currencyPending, profile.coins, profile.warbucks);
    }
    return 0;
}

/** Focused regression for the user's splash, package and clipped badge report. */
int CheckUiFeedback(CResTOCManager &toc, PackTables &tables, const CPlayerProgress::Template &progress,
    const CRefinementManager::Template &refinement, const std::vector<StoreEntry> &store,
    const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armor) {
    const auto root = std::filesystem::path("out/ui-feedback-2026-09-09");
    const auto save = root / ("profile-" + std::to_string(GetTickCount64()));
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!LoadNativeProfile(toc, tables, profile, save, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    {
        GameMenu view;
        if (!view.Open(toc, tables)) { return 1; }
        MovieRegion viewport, column, content, badge, sprite;
        const unsigned scroll = view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL");
        if (!view.movies.Region(scroll, 0, view.storeRestTime, viewport) ||
            !view.movies.Region(scroll, 1, view.storeRestTime, column) ||
            !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_MENU"), 0, 0, content)) { return 1; }
        StoreCardFace face;
        face.x = column.x;
        face.y = column.y;
        if (!CardRegion(view, view.movies.Ordinal("GLU_MOVIE_SHOP_BOX"), kCardBadgeRegion, face, badge) ||
            !view.movies.SpriteBounds(0, 88, sprite)) { return 1; }
        const float badgeTop = badge.y + badge.height / 2 + sprite.y;
        std::printf("[ui-feedback-check] content-y=%.1f viewport-y=%.1f column-y=%.1f badge-top=%.1f sprite-height=%.1f\n",
            content.y, viewport.y, column.y, badgeTop, sprite.height);
        if (badgeTop < content.y) {
            std::printf("[ui-feedback-check] first-row badge clipped by %.1f pixels failures=1\n", viewport.y - badgeTop);
            return 1;
        }
    }
    for (unsigned phase = 0; phase < 4; ++phase) {
        MenuState state;
        state.page = 14;
        profile.firstLaunch = phase == 3;
        std::vector<MenuTestClick> clicks;
        if (phase == 0) { clicks.push_back({-1, -1, 1200}); }
        if (phase == 1) { clicks.push_back({-1, -1, 600}); }
        if (phase >= 2) { clicks.push_back({12, 12, 16}); }
        const auto screenshot = root / ("splash-" + std::to_string(phase) + ".png");
        if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, save,
            screenshot.string(), &clicks) != -2) { return 1; }
        unsigned expectedPage = 14;
        if (phase == 2) { expectedPage = 24; }
        if (phase == 3) { expectedPage = 25; }
        if (state.page != expectedPage) { return 1; }
        std::printf("[ui-feedback-check] splash-phase=%u page=%u wait blink click original-entry failures=0\n", phase, state.page);
    }
    profile.firstLaunch = false;
    const StoreEntry *package = nullptr;
    unsigned packageIndex = 0;
    for (unsigned index = 0; index < store.size(); ++index) {
        if (store[index].data.singlePurchase != 0) { package = &store[index]; packageIndex = index; break; }
    }
    if (package == nullptr) { return 1; }
    MenuTestClick packageCard, buyPackage;
    {
        GameMenu view;
        if (!view.Open(toc, tables)) { return 1; }
        MovieRegion column, body, right, label;
        StoreCardFace face;
        const auto *entry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", kBuyButtonEntry);
        if (entry == nullptr || !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), 2, view.storeRestTime, column)) { return 1; }
        face.x = column.x; face.y = column.y;
        const unsigned box = view.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
        if (!CardRegion(view, box, kCardBodyRegion, face, body) || !CardRegion(view, box, kCardRightRegion, face, right) ||
            !view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, label)) { return 1; }
        packageCard = {body.x + 10, body.y + 10, 1000};
        buyPackage = {right.x + right.width - label.width / 2, right.y + right.height - label.height / 2, 1000};
        if (label.width <= right.width) { buyPackage.x = right.x + right.width / 2; }
    }
    for (unsigned phase = 0; phase < 7; ++phase) {
        MenuState state;
        state.page = 2;
        state.shopGunSlot = profile.activeWeaponSlot;
        if (phase == 0 || phase == 1) { state.shopCategory = 2; }
        if (phase == 2) { state.shopCategory = 1; }
        std::vector<MenuTestClick> clicks{{-1, -1, 1000}};
        if (phase == 1 || phase == 2 || phase == 3 || phase == 6) { clicks.push_back(packageCard); }
        if (phase == 4) { clicks.push_back(buyPackage); }
        const auto screenshot = root / ("store-" + std::to_string(phase) + ".png");
        if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, save,
            screenshot.string(), &clicks) != -2) { return 1; }
        if (phase == 3 && state.selectedItem != static_cast<int>(packageIndex)) { return 1; }
        if ((phase == 1 || phase == 2 || phase == 6) && state.selectedItem == static_cast<int>(packageIndex)) { return 1; }
        if (phase == 4 && (!profile.IsPackagePurchased(package->ref) || state.shopDetailOpen)) { return 1; }
        if (phase == 5) {
            // A disk reload in the same session must retain OWNED. Reset models
            // a new process before applying the original purchased-item override.
            profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
            if (!LoadNativeProfile(toc, tables, profile, save) || !profile.IsPackagePurchased(package->ref)) { return 1; }
        }
        std::printf("[ui-feedback-check] store-phase=%u category=%u selected=%d package-purchased=%u failures=0\n",
            phase, state.shopCategory, state.selectedItem, profile.IsPackagePurchased(package->ref));
    }
    return 0;
}

/** Fresh inventory and one live menu, including purchase from an expanded card. */
int RunPackagePurchaseCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadStoreCatalog(toc, tables, store) ||
        !LoadWeaponCatalog(toc, tables, weapons) || !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    const StoreEntry *package = nullptr;
    for (const StoreEntry &entry : store) {
        if (entry.data.singlePurchase != 0) { package = &entry; break; }
    }
    if (package == nullptr) { return 1; }
    const auto root = std::filesystem::path("out/package-purchase-check") / std::to_string(GetTickCount64());
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = false;
    const unsigned box = view.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
    MovieRegion column, content, body, right, button, actions;
    unsigned start = 0, end = 0;
    const auto *buy = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", kBuyButtonEntry);
    if (buy == nullptr || !view.movies.GetMovie(box)->GetChapterRange(1, start, end) ||
        !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), 2, view.storeRestTime, column) ||
        !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_MENU"), 0, 0, content) ||
        !view.movies.Region(view.movies.Ordinal(buy->movies[0]), 1, 0, button)) { return 1; }
    StoreCardFace folded{column.x, column.y};
    if (!CardRegion(view, box, kCardBodyRegion, folded, body) || !CardRegion(view, box, kCardRightRegion, folded, right)) { return 1; }
    const MenuTestClick openClick{body.x + 10, body.y + 10};
    MenuTestClick foldedBuy{right.x + right.width - button.width / 2, right.y + right.height - button.height / 2};
    if (button.width <= right.width) { foldedBuy.x = right.x + right.width / 2; }
    if (!view.movies.Region(box, kCardBodyRegion, end, body)) { return 1; }
    const StoreCardFace expanded{content.x + content.width / 2 - static_cast<int>(content.width) / 16 - body.width / 2,
        content.y + content.height / 2 - body.height / 2, 1, end};
    if (!CardRegion(view, box, kCardActionRegion, expanded, actions)) { return 1; }
    const MenuTestClick expandedBuy{actions.x + actions.width - button.width / 2, actions.y + button.height / 2};
    unsigned failures = 0;
    for (unsigned mode = 0; mode < 2; ++mode) {
        CProfileManager profile;
        profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
        const auto save = root / std::to_string(mode);
        if (!LoadNativeProfile(toc, tables, profile, save, root / "absent-source")) { return 1; }
        profile.coins = package->data.commonPrice;
        profile.warbucks = package->data.rarePrice;
        profile.activeWeaponSlot = mode;
        const CPlayerConfiguration initialConfiguration = profile.configuration;
        MenuState state;
        state.page = 2;
        state.shopGunSlot = mode;
        std::vector<MenuTestClick> clicks{{-1, -1, 1000}};
        if (mode == 1) { clicks.push_back(openClick); clicks.push_back({-1, -1, 1000}); clicks.push_back(expandedBuy); }
        else { clicks.push_back(foldedBuy); }
        clicks.push_back({-1, -1, 1000});
        for (const MenuTestClick &click : clicks) {
            view.clock += click.advanceMs;
            view.Begin(2); view.SetTestClick(click);
            if (!DrawStore(view, toc, tables, profile, package->data.requiredLevel, store, weapons, armor, state, save)) { return 1; }
        }
        if (!view.window.SaveFrame((save / "after-purchase.png").string())) { return 1; }
        const bool purchased = profile.IsPackagePurchased(package->ref);
        unsigned delivered = 0, equipped = 0, gear = 0;
        for (const auto &object : package->data.objects) {
            if (object.type == 17) {
                unsigned expected = 0;
                for (const auto &other : package->data.objects) {
                    if (other.type == 17 && SameObject(other.object, object.object)) { ++expected; }
                }
                if (profile.GetPowerupCount(object.object) != expected) { ++failures; }
                continue;
            }
            ++gear;
            if (profile.Owns(object.type, object.object)) { ++delivered; }
            if (object.type == 6) {
                for (const auto &gun : profile.configuration.guns) { if (SameObject(gun, object.object)) { ++equipped; break; } }
            } else if (object.type == 2) {
                for (const auto &part : profile.configuration.armor) { if (SameObject(part, object.object)) { ++equipped; break; } }
            }
        }
        if (!purchased || state.shopDetailOpen != (mode == 1) || delivered != gear || equipped != gear ||
            profile.IsPackageHidden(package->ref)) { ++failures; }
        unsigned gunIndex = 0, restoredRows = 0;
        for (const auto &object : package->data.objects) {
            if (object.type == 6 && gunIndex < 2) {
                if (!SameObject(profile.configuration.guns[(mode + gunIndex) & 1], object.object)) { ++failures; }
                ++gunIndex;
            }
            if (object.type != 2) { continue; }
            for (const ArmorEntry &part : armor) {
                if (part.packHash == object.object.packHash && part.ordinal == object.object.localIndex &&
                    !SameObject(profile.configuration.armor[part.data.GetSlot()], object.object)) { ++failures; }
            }
            for (const StoreEntry &entry : store) {
                if (entry.data.objects.size() == 1 && entry.data.objects[0].type == 2 &&
                    SameObject(entry.data.objects[0].object, object.object) && entry.data.displayOrder < 0) {
                    if (GetStoreDisplayOrder(entry.data, profile) < 0) { ++failures; }
                    ++restoredRows;
                }
            }
        }
        const auto coins = profile.coins;
        const auto warbucks = profile.warbucks;
        const auto inventory = profile.inventory.size();
        const auto acquiredConfiguration = profile.configuration;
        // Clicking the former BUY area must not acquire or equip again.
        view.clock += 1000; view.Begin(2);
        if (mode == 0) { view.SetTestClick(foldedBuy); }
        else { view.SetTestClick(expandedBuy); }
        if (!DrawStore(view, toc, tables, profile, package->data.requiredLevel, store, weapons, armor, state, save)) { return 1; }
        if (profile.AcquireItem(package->data, package->data.requiredLevel) != PurchaseResult::Owned ||
            profile.coins != coins || profile.warbucks != warbucks || profile.inventory.size() != inventory) { ++failures; }
        // Re-entering categories must preserve the session's OWNED card.
        for (unsigned category : {1u, 2u, 0u}) {
            state = MenuState{};
            state.page = 2; state.shopCategory = category; state.shopGunSlot = mode;
            view.clock += 1000;
            view.Begin(2); view.SetTestClick(openClick);
            if (!DrawStore(view, toc, tables, profile, package->data.requiredLevel, store, weapons, armor, state, save)) { return 1; }
            bool selectedPackage = state.selectedItem >= 0 && static_cast<unsigned>(state.selectedItem) < store.size() &&
                SameObject(store[state.selectedItem].ref, package->ref);
            if (selectedPackage != (category == 0)) { ++failures; }
        }
        if (!profile.LoadFromDisk(save) || profile.IsPackageHidden(package->ref)) { ++failures; }
        CProfileManager restarted;
        restarted.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
        if (!LoadNativeProfile(toc, tables, restarted, save, root / "absent-source") ||
            !restarted.IsPackageHidden(package->ref)) { return 1; }
        for (unsigned slot = 0; slot < acquiredConfiguration.guns.size(); ++slot) {
            if (!SameObject(restarted.configuration.guns[slot], acquiredConfiguration.guns[slot])) { ++failures; }
        }
        for (unsigned slot = 0; slot < acquiredConfiguration.armor.size(); ++slot) {
            if (!SameObject(restarted.configuration.armor[slot], acquiredConfiguration.armor[slot])) { ++failures; }
        }
        for (const auto &object : package->data.objects) {
            if (object.type == 17) {
                unsigned expected = 0;
                for (const auto &other : package->data.objects) {
                    if (other.type == 17 && SameObject(other.object, object.object)) { ++expected; }
                }
                if (restarted.GetPowerupCount(object.object) != expected || profile.GetPowerupCount(object.object) != expected) { ++failures; }
            } else if (!restarted.Owns(object.type, object.object)) { ++failures; }
        }
        state = MenuState{}; state.page = 2; state.shopGunSlot = mode;
        view.clock += 1000; view.Begin(2); view.SetTestClick(openClick);
        if (!DrawStore(view, toc, tables, restarted, package->data.requiredLevel, store, weapons, armor, state, save)) { return 1; }
        if (state.selectedItem >= 0 && static_cast<unsigned>(state.selectedItem) < store.size() &&
            SameObject(store[state.selectedItem].ref, package->ref)) { ++failures; }
        if (!view.window.SaveFrame((save / "after-restart.png").string())) { return 1; }
        // Owned bundle-only armor remains selectable after changing equipment.
        restarted.configuration = initialConfiguration;
        std::vector<std::pair<int, unsigned>> ownedArmor;
        for (unsigned index = 0; index < store.size(); ++index) {
            const auto &item = store[index].data;
            if (item.objects.size() != 1 || item.objects[0].type != 2 || item.value242 == 1 ||
                !restarted.Owns(2, item.objects[0].object)) { continue; }
            const int order = GetStoreDisplayOrder(item, restarted);
            if (order >= 0) { ownedArmor.push_back({order, index}); }
        }
        std::sort(ownedArmor.begin(), ownedArmor.end());
        MovieRegion firstColumn, secondColumn, equipButton;
        const auto *equipEntry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", kEquipButtonEntry);
        const unsigned scroll = view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL");
        if (equipEntry == nullptr || !view.movies.Region(scroll, kFirstColumnRegion, view.storeRestTime, firstColumn) ||
            !view.movies.Region(scroll, kFirstColumnRegion + 1, view.storeRestTime, secondColumn) ||
            !view.movies.Region(view.movies.Ordinal(equipEntry->movies[0]), 1, 0, equipButton)) { return 1; }
        unsigned armorClicks = 0;
        for (unsigned position = 0; position < ownedArmor.size(); ++position) {
            const auto &item = store[ownedArmor[position].second].data;
            if (item.displayOrder >= 0) { continue; }
            state = MenuState{}; state.page = 2; state.shopCategory = 1;
            state.shopGunSlot = mode; state.shopFilter = kOwnedFilterBit;
            state.shopScroll = (position / 2) * (secondColumn.x - firstColumn.x);
            const StoreCardFace face{firstColumn.x, firstColumn.y + (position % 2) * (firstColumn.height / 2 + 5)};
            MovieRegion price;
            if (!CardRegion(view, box, kCardPriceRegion, face, price)) { return 1; }
            view.clock += 1000; view.Begin(2);
            view.SetTestClick({price.x + price.width - equipButton.width / 2, price.y + price.height - equipButton.height / 2});
            if (!DrawStore(view, toc, tables, restarted, package->data.requiredLevel, store, weapons, armor, state, save)) { return 1; }
            ++armorClicks;
        }
        for (unsigned slot = 0; slot < acquiredConfiguration.armor.size(); ++slot) {
            if (!SameObject(restarted.configuration.armor[slot], acquiredConfiguration.armor[slot])) { ++failures; }
        }
        if (armorClicks != restoredRows) { ++failures; }
        state = MenuState{}; state.page = 2; state.shopCategory = 1; state.shopFilter = kOwnedFilterBit; state.shopGunSlot = mode;
        view.clock += 1000; view.Begin(2); view.SetTestClick({-1, -1});
        if (!DrawStore(view, toc, tables, restarted, package->data.requiredLevel, store, weapons, armor, state, save) ||
            !view.window.SaveFrame((save / "armor-re-equipped.png").string())) { return 1; }
        std::printf("[package-purchase-check] expanded=%u purchased=%u detail-open=%u gear-delivered=%u/%u gear-equipped=%u/%u failures=%u\n",
            mode, purchased, state.shopDetailOpen, delivered, gear, equipped, gear, failures);
        std::printf("[package-purchase-check] restored-armor-rows=%u session-owned restart-hidden loadout-reloaded repeat-rejected failures=%u\n", restoredRows, failures);
        std::printf("[package-purchase-check] armor-equip-clicks=%u failures=%u\n", armorClicks, failures);
    }
    return failures != 0;
}

int RunStoreTemplateCheck(const std::string &bigDirectory, bool cardsOnly, bool bankOnly, bool feedbackOnly) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CPlayerProgress::Template progress;
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    if (!LoadPlayerProgress(toc, tables, progress) || !LoadRefinementTemplate(toc, tables, refinement) ||
        !LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (bankOnly) { return CheckBank(toc, tables, progress, refinement, store, weapons, armor); }
    if (feedbackOnly) { return CheckUiFeedback(toc, tables, progress, refinement, store, weapons, armor); }
    if (cardsOnly) { return CheckStoreCards(toc, tables, profile, progress, refinement, store, weapons, armor); }
    unsigned closedStart = 0, closedEnd = 0, slideStart = 0, slideEnd = 0;
    MovieRegion closedButton, openButton, openPanel, optionLabel;
    {
        GameMenu probe;
        if (!probe.Open(toc, tables)) { return 1; }
        const unsigned ordinal = probe.movies.Ordinal("GLU_MOVIE_SORT_BAR");
        const CMovie *movie = probe.movies.GetMovie(ordinal);
        if (movie == nullptr || !movie->GetChapterRange(0, closedStart, closedEnd) ||
            !movie->GetChapterRange(1, slideStart, slideEnd)) { return 1; }
        // Byte-checked regression for this shipped asset; runtime uses its data.
        if (closedEnd != 100 || slideStart != 101 || slideEnd != 276) { return 1; }
        if (!probe.movies.Region(ordinal, kSortButtonRegion, closedEnd, closedButton) ||
            !probe.movies.Region(ordinal, kSortButtonRegion, slideEnd, openButton) ||
            !probe.movies.Region(ordinal, kSortPanelRegion, slideEnd, openPanel) ||
            !probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_BUTTON_LG"), 1, 0, optionLabel)) { return 1; }
        MenuState playback;
        if (!AdvanceStoreFilter(probe, playback, *movie)) { return 1; }
        playback.shopFilterOpen = true;
        probe.clock = (slideEnd - closedEnd) / 2;
        if (!AdvanceStoreFilter(probe, playback, *movie)) { return 1; }
        const unsigned middle = playback.shopFilterTime;
        MovieRegion movingButton;
        if (!probe.movies.Region(ordinal, kSortButtonRegion, middle, movingButton) ||
            movingButton.y <= openButton.y || movingButton.y >= closedButton.y) { return 1; }
        playback.shopFilterOpen = false;
        ++probe.clock;
        if (!AdvanceStoreFilter(probe, playback, *movie) || playback.shopFilterTime != middle - 1) { return 1; }
        playback.shopFilterOpen = true;
        probe.clock += movie->duration;
        if (!AdvanceStoreFilter(probe, playback, *movie) || playback.shopFilterTime != slideEnd) { return 1; }
        playback.shopFilterOpen = false;
        probe.clock += movie->duration;
        if (!AdvanceStoreFilter(probe, playback, *movie) || playback.shopFilterTime != slideStart) { return 1; }
        // Perturb only an in-memory copy: playback must follow changed chapters.
        // This distinguishes resource-driven bounds from the old 101/277 constants.
        CMovie changed = *movie;
        changed.chapters[1] += 20;
        changed.chapters[2] += 20;
        MenuState changedPlayback;
        if (!AdvanceStoreFilter(probe, changedPlayback, changed) || changedPlayback.shopFilterTime != closedEnd + 20) { return 1; }
        changedPlayback.shopFilterOpen = true;
        probe.clock += changed.duration;
        if (!AdvanceStoreFilter(probe, changedPlayback, changed) || changedPlayback.shopFilterTime != slideEnd + 20) { return 1; }
        unsigned unusedStart = 0, unusedEnd = 0;
        if (movie->GetChapterRange(static_cast<unsigned>(movie->chapters.size()), unusedStart, unusedEnd)) { return 1; }
        std::printf("[store-template-check] chapter-bounds moving-region reversal modified-resource failures=0\n");
    }
    const MenuTestClick openClick{closedButton.x + closedButton.width / 2, closedButton.y + closedButton.height / 2};
    const MenuTestClick closeClick{openButton.x + openButton.width / 2, openButton.y + openButton.height / 2};
    const unsigned travel = slideEnd - closedEnd;
    const float optionX = openPanel.x + openPanel.width / 2;
    const float optionY = openPanel.y + optionLabel.height / 2;
    const float rowPitch = optionLabel.height * kSortRowSpacing;
    MenuState closed, halfway, selected, closing, closedAgain;
    closed.page = halfway.page = selected.page = closing.page = closedAgain.page = 2;
    const std::filesystem::path profilePath = "out/store-template-check.dat";
    const std::vector<MenuTestClick> idle = {{-100, -100}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        closed, profilePath, "out/store-template-closed.png", &idle) != -2) { return 1; }
    const std::vector<MenuTestClick> halfwayClicks = {openClick, {-100, -100, travel / 2}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        halfway, profilePath, "out/store-template-opening.png", &halfwayClicks) != -2 ||
        halfway.shopFilterTime != closedEnd + travel / 2) { return 1; }
    // Click a row's final position before it arrives: it must not select there.
    const std::vector<MenuTestClick> selectClicks = {openClick, {optionX, optionY + 2 * rowPitch},
        {optionX, optionY + 2 * rowPitch, travel}, {optionX, optionY + 3 * rowPitch}, {-100, -100}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        selected, profilePath, "out/store-template-open.png", &selectClicks) != -2 ||
        selected.shopFilter != 3 || selected.shopFilterTime != slideEnd) { return 1; }
    const std::vector<MenuTestClick> closingClicks = {openClick, {-100, -100, travel}, closeClick, {-100, -100, travel / 2}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        closing, profilePath, "out/store-template-closing.png", &closingClicks) != -2 ||
        closing.shopFilterOpen || closing.shopFilterTime != slideEnd - travel / 2) { return 1; }
    const std::vector<MenuTestClick> closeClicks = {openClick, {-100, -100, travel}, closeClick, {-100, -100, travel}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        closedAgain, profilePath, "out/store-template-closed-again.png", &closeClicks) != -2 ||
        closedAgain.shopFilterOpen || closedAgain.shopFilterTime != slideStart) { return 1; }
    if (profile.coins != 0 || profile.warbucks != 0 || profile.inventory.size() != 4) { return 1; }
    std::printf("[store-template-check] screenshots=5 multiselect moving-hitbox close no-purchase failures=0\n");
    return CheckStoreCards(toc, tables, profile, progress, refinement, store, weapons, armor);
}

/** Retained milestone: aggregate the original menu paths, never legacy mock UI. */
int RunGameMenuCheck(const std::string &bigDirectory) {
    if (RunNavigationBarCheck(bigDirectory) != 0 || RunOptionsCheck(bigDirectory) != 0 ||
        RunSocialOfflineCheck(bigDirectory) != 0 || RunPromotionCheck(bigDirectory) != 0 ||
        RunLoadingWipeCheck(bigDirectory) != 0) { return 1; }
    std::printf("[menu-check] original header, list, social, promotions and transitions failures=0\n");
    return 0;
}

int RunPromotionCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadStoreCatalog(toc, tables, store) ||
        !LoadWeaponCatalog(toc, tables, weapons) || !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const std::filesystem::path path = "out/ui-restoration-promotion-profile";
    if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.animateNavigation = false;
    unsigned failures = 0;
    for (unsigned row = 0; row < 2; ++row) {
        MenuState state;
        state.page = 2;
        view.Begin(2);
        if (!DrawStore(view, toc, tables, profile, 200, store, weapons, armor, state, path)) { return 1; }
        MovieRegion slot, body;
        if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), 1, view.storeRestTime, slot)) { return 1; }
        StoreCardFace face;
        face.x = slot.x; face.y = slot.y + row * (slot.height / 2 + 5);
        if (!CardRegion(view, view.movies.Ordinal("GLU_MOVIE_SHOP_BOX"), 0, face, body)) { return 1; }
        view.SetTestClick({body.x + body.width / 2, body.y + body.height / 2});
        if (!DrawStore(view, toc, tables, profile, 200, store, weapons, armor, state, path)) { return 1; }
        unsigned action = 125;
        if (row == 1) { action = 130; }
        const bool opened = state.page == 2 && state.promotion.IsActive() && state.promotion.Action() == action;
        if (!opened) { ++failures; }
        std::printf("[promotion-check] card=%u page=%u modal=%d action=%u failures=%u\n",
            row, state.page, state.promotion.IsActive(), state.promotion.Action(), failures);
        if (!opened) { continue; }
        const CMovie *movie = view.movies.GetMovie(state.promotion.Ordinal());
        unsigned start = 0, end = 0;
        if (!movie->GetChapterRange(2, start, end)) { return 1; }
        if (state.promotion.Click(body.x, body.y) != 0) { ++failures; }
        state.promotion.Update(start);
        view.Begin(2);
        view.ExchangeClick(false);
        if (!DrawStore(view, toc, tables, profile, 200, store, weapons, armor, state, path)) { return 1; }
        CPlayerProgress::Template progressTemplate;
        CPlayerProgress progress;
        if (!LoadPlayerProgress(toc, tables, progressTemplate)) { return 1; }
        progress.Bind(progressTemplate); progress.SetExperience(profile.experience);
        if (view.Header(profile, progress, 2) == -3 || !state.promotion.Draw(view.movies) ||
            !state.promotion.IsReady() || state.promotion.Hits().size() != 3) { return 1; }
        const std::string capture = "out/ui-promotion-" + std::to_string(action) + ".png";
        if (!view.window.SaveFrame(capture)) { return 1; }
        const auto hit = state.promotion.Hits().back().first;
        if (state.promotion.Click(hit.x + hit.width / 2, hit.y + hit.height / 2) != 45 ||
            state.promotion.IsReady()) { ++failures; }
        state.promotion.Update(movie->duration);
        if (state.promotion.IsActive() || state.page != 2) { ++failures; }
        std::printf("[promotion-check] movie=%u chapters=%zu close-region=1 retained-store=1 failures=%u\n",
            state.promotion.Ordinal(), movie->chapters.size(), failures);
    }
    profile.coins = 0;
    for (unsigned index = 0; index < 3; ++index) {
        MenuState state;
        ShowStoreFundsPrompt(state, store, profile, 0, 1, false); // Test-only shortage.
        view.Begin(2);
        if (!DrawStorePrompt(view, state)) { return 1; }
        const unsigned popup = view.movies.Ordinal("GLU_MOVIE_POPUP");
        view.clock += view.movies.GetMovie(popup)->duration;
        view.Begin(2);
        if (!DrawStorePrompt(view, state) || !state.storePopup.IsReady()) { return 1; }
        if (index == 0 && !view.window.SaveFrame("out/ui-store-funds-original.png")) { return 1; }
        const auto *entry = OriginalMenuData("MDS_BUTTON_STORE_PROMPT", index);
        MovieRegion area;
        if (!view.movies.Region(popup, index + 2, state.storePopup.MovieTime(), area)) { return 1; }
        view.SetTestClick({area.x + area.width / 2, area.y + area.height / 2});
        if (!DrawStorePrompt(view, state)) { return 1; }
        if (entry->action == 71 && !state.currencyPending) { ++failures; }
        if (entry->action != 71 && state.storePopup.IsReady()) { ++failures; }
        std::printf("[promotion-check] funds-button=%u action=%u failures=%u\n", index, entry->action, failures);
    }
    return failures != 0;
}

int RunLoadingWipeCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CWindow window;
    if (!window.Open("Gun Bros", 1600, 1200)) { return 1; }
    MovieRenderer movies;
    auto &core = *toc.GetPack(toc.GetCorePackIndex());
    if (!movies.Init(core, core)) { return 1; }
    CRefinementManager::Template refinement;
    CProfileManager profile;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    profile.Reset(core.GetPackHash(), refinement);
    if (!LoadNativeProfile(toc, tables, profile, "out/ui-restoration-loading-profile", std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    unsigned count = 1, failures = 0;
    for (unsigned index = 0; index < count; ++index) {
        OriginalLoadingSplash splash;
        if (!splash.Init(movies, index, &profile)) { return 1; }
        count = splash.Count();
        glViewport(0, 0, 1600, 1200);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!splash.Draw(splash.IdleStart()) || splash.ImageHandle() == 0 || splash.TextHandle() == 0) { return 1; }
        if (index < 3 && !window.SaveFrame("out/ui-loading-cg-" + std::to_string(index) + ".png")) { return 1; }
    }
    MenuWipe wipe;
    glClearColor(1, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    movies.Rectangle(0, 0, 64, 64, 0, 1, 0);
    movies.Rectangle(960, 704, 64, 64, 1, 1, 1);
    if (!wipe.Remember() || !wipe.Begin(movies)) { return 1; }
    const unsigned duration = wipe.Duration();
    for (const char *name : {"GLU_MOVIE_STORE_SCROLL", "GLU_MOVIE_SHOP_BOX", "GLU_MOVIE_SPLASH"}) {
        const auto *source = movies.GetMovie(movies.Ordinal(name));
        std::printf("[ui-chapters] %s duration=%u chapters=", name, source->duration);
        for (auto time : source->chapters) { std::printf("%u,", time); }
        std::printf("\n");
    }
    for (unsigned frame = 0; frame < 3; ++frame) {
        if (frame > 0) { wipe.Update((duration + 1) / 2); }
        glClearColor(0, 0, 1, 1); glClear(GL_COLOR_BUFFER_BIT);
        if (!wipe.Draw()) { return 1; }
        std::vector<std::uint8_t> pixels(1600 * 1200 * 4);
        glReadPixels(0, 0, 1600, 1200, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        unsigned red = 0, blue = 0;
        for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
            if (pixels[pixel] > 250 && pixels[pixel + 2] < 5) { ++red; }
            if (pixels[pixel] < 5 && pixels[pixel + 2] > 250) { ++blue; }
        }
        if (frame == 0) {
            const unsigned topLeft = (1150 * 1600 + 50) * 4;
            const unsigned bottomRight = (50 * 1600 + 1550) * 4;
            if (red == 0 || pixels[topLeft] > 5 || pixels[topLeft + 1] < 250 ||
                pixels[bottomRight] < 250 || pixels[bottomRight + 1] < 250) { ++failures; }
        }
        if (frame == 1 && (red == 0 || blue == 0)) { ++failures; }
        if (frame == 2 && (red != 0 || wipe.IsActive())) { ++failures; }
        if (!window.SaveFrame("out/ui-wipe-fixture-" + std::to_string(frame) + ".png")) { return 1; }
        std::printf("[loading-wipe-check] frame=%u time=%u duration=%u old-pixels=%u new-pixels=%u failures=%u\n",
            frame, wipe.Time(), duration, red, blue, failures);
    }
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    CPlayerProgress::Template progress;
    if (!LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadArmorCatalog(toc, tables, armor) || !LoadPlayerProgress(toc, tables, progress)) { return 1; }
    const auto header = movies.Ordinal("GLU_MOVIE_HEADER");
    unsigned headerStart = 0, headerEnd = 0;
    if (!movies.GetMovie(header)->GetChapterRange(2, headerStart, headerEnd)) { return 1; }
    MovieRegion options, storeTab, play, categoryBar;
    if (!movies.Region(header, 5, headerStart, options) || !movies.Region(header, 3, headerStart, storeTab) ||
        !movies.Region(header, 0, headerStart, play) ||
        !movies.Region(movies.Ordinal("GLU_MOVIE_STORE_MENU"), kStoreCategoryRegion, 0, categoryBar)) { return 1; }
    std::vector<MenuTestClick> targets = {
        {options.x + options.width / 2, options.y + options.height / 2},
        {play.x + play.width / 2, play.y + play.height / 2}
    };
    float categoryX = categoryBar.x;
    for (unsigned index = 0; index < 4; ++index) {
        const auto *entry = OriginalMenuData("MDS_BUTTON_STORE_CATEGORIES", index);
        MovieRegion label, touch;
        if (!movies.Region(movies.Ordinal(entry->movies[0]), 1, 0, label) ||
            !movies.Region(movies.Ordinal(entry->movies[0]), 0, 0, touch)) { return 1; }
        if (index != 0) { targets.push_back({categoryX + touch.x - label.x + touch.width / 2,
            categoryBar.y + touch.y - label.y + touch.height / 2}); }
        categoryX += label.width + kCategoryGap;
    }
    for (unsigned index = 0; index < targets.size(); ++index) {
        MenuState state;
        state.page = 2;
        auto target = targets[index];
        target.advanceMs = 1;
        target.renderDelayMs = duration * 2;
        const std::vector<MenuTestClick> clicks = {{-100, -100, 1}, {-100, -100, headerEnd + 1},
            {-100, -100, headerEnd + 1}, {-100, -100, headerEnd + 1}, target,
            {storeTab.x + storeTab.width / 2, storeTab.y + storeTab.height / 2, duration / 2}};
        const std::string capture = "out/ui-wipe-menu-" + std::to_string(index) + ".png";
        MenuTransitionTrace trace;
        if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state,
            "out/ui-restoration-loading-profile", capture, &clicks, true, &window, true, &trace) != -2) { return 1; }
        const unsigned expectedPage[] = {6, 0, 2, 2, 2};
        // Targets 0 and 1 leave the STORE branch and sweep, which also swallows
        // the store click that follows. The categories are menus inside that
        // branch, so they change with no sweep and nothing to swallow.
        const bool branchChange = index < 2;
        bool wrong = state.page != expectedPage[index] || (index >= 2 && state.shopCategory != index - 1);
        if (branchChange && (!trace.active || trace.time != duration / 2 || trace.starts != 1)) { wrong = true; }
        if (!branchChange && (trace.active || trace.starts != 0)) { wrong = true; }
        if (wrong) { ++failures; }
        std::printf("[loading-wipe-check] cold-load=%u midpoint-active=%d time=%u starts=%u expected-sweep=%d\n",
            target.renderDelayMs, trace.active, trace.time, trace.starts, branchChange);
        std::printf("[loading-wipe-check] real-shell target=%u page=%u category=%u blocked-store-click=%d failures=%u\n",
            index, state.page, state.shopCategory, branchChange, failures);
    }
    // Actual planet click -> authored reticle exit -> REV page, with cold work.
    MenuState revolution;
    revolution.page = 0;
    revolution.modeSelected = true;
    const unsigned mapId = movies.Ordinal("GLU_MOVIE_MAP_PARALAX_COPY");
    const auto *mapMovie = movies.GetMovie(mapId);
    unsigned mapStart = 0, mapEnd = 0, exitStart = 0, exitEnd = 0;
    if (!mapMovie || !mapMovie->GetChapterRange(0, mapStart, mapEnd) ||
        !movies.GetMovie(movies.Ordinal("GLU_MOVIE_MAP_RETICLE"))->GetChapterRange(2, exitStart, exitEnd)) { return 1; }
    MovieRegion planet;
    if (!movies.Region(mapId, 1, mapEnd, planet)) { return 1; }
    MenuTestClick planetClick{planet.x + planet.width / 2, planet.y + planet.height / 2, 1};
    // No trailing store click: the REV list opens inside the PLAY branch with no
    // sweep to swallow it, so such a click would simply leave for the store.
    const std::vector<MenuTestClick> revolutionClicks = {{-100, -100, 1}, {-100, -100, headerEnd + 1},
        {-100, -100, headerEnd + 1}, {-100, -100, headerEnd + 1}, planetClick,
        {-100, -100, exitEnd + 1, duration * 2}, {-100, -100, duration / 2}};
    MenuTransitionTrace revolutionTrace;
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, revolution,
        "out/ui-restoration-loading-profile", "out/ui-wipe-revolution.png", &revolutionClicks,
        true, &window, true, &revolutionTrace) != -2) { return 1; }
    if (revolution.page != 21 || revolutionTrace.active || revolutionTrace.starts != 0) { ++failures; }
    std::printf("[loading-wipe-check] planet-to-REV page=%u active=%d starts=%u expected-sweep=0 failures=%u\n",
        revolution.page, revolutionTrace.active, revolutionTrace.starts, failures);
    // Startup has the original launch image and animated core 0:124 only.
    {
        LoadingScreen startup(window, movies, tables, &profile, false, true);
        if (!startup.IsValid() || !startup.CaptureFrame("out/ui-startup-loading.png", 0) ||
            !startup.CaptureFrame("out/ui-startup-loading-next.png", 200)) { return 1; }
        startup.OnResourceRead();
        startup.Finish();
        if (!startup.IsValid()) { return 1; }
    }
    std::printf("[loading-wipe-check] original-CG-and-STR-pairs=%u failures=%u\n", count, failures);
    return failures != 0;
}

/** Actual menu preview and scene handoffs, with isolated save data. */
int RunAudioTransitionsCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CPlayerProgress::Template progress;
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    if (!LoadPlayerProgress(toc, tables, progress) || !LoadRefinementTemplate(toc, tables, refinement) ||
        !LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    const auto savePath = std::filesystem::path("out/audio-transitions") / std::to_string(GetTickCount64());
    CProfileManager profile;
    if (!LoadNativeProfile(toc, tables, profile, savePath, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    CWindow window;
    if (!window.Open("Audio transition verification", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    unsigned failures = 0;
    {
        GameMenu view(&window);
        if (!view.Open(toc, tables, &profile)) { return 1; }
        view.EnableSilentPreviewAudio();
        MovieRegion panel;
        if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_MENU"), 2, 0, panel)) { return 1; }
        for (unsigned actor = 0; actor < 2; ++actor) {
            profile.playerBrother = actor;
            if (!view.DrawEquippedPlayer(toc, tables, profile, weapons, armor, 0, nullptr, &panel)) { return 1; }
            for (unsigned tick = 0; tick < 300; ++tick) { view.AdvancePlayerPreview(16); }
            for (unsigned exchange = 0; exchange < 2; ++exchange) {
                const unsigned slot = 1 - view.GetPlayerPreviewSlot();
                const auto before = view.PreviewSoundCount();
                if (!view.DrawEquippedPlayer(toc, tables, profile, weapons, armor, slot, nullptr, &panel)) { return 1; }
                for (unsigned tick = 0; tick < 300; ++tick) { view.AdvancePlayerPreview(16); }
                const auto sounds = view.PreviewSoundCount() - before;
                if (sounds == 0 || view.GetPlayerPreviewSlot() != slot || view.PreviewAudioState().voices == 0) { ++failures; }
                std::printf("[audio-transition-check] store brother=%u slot=%u sounds=%zu failures=%u\n", actor, slot, sounds, failures);
            }
        }
    }
    CBGM music;
    music.EnableSilentValidation();
    if (!music.Play(0)) { return 1; }
    MenuState state;
    state.page = 2;
    unsigned starts = CBGM::GetPlaybackStarts();
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state,
        savePath, "out/audio-transition-menu.png", nullptr, false, &window, false, nullptr, &music) != -2) { return 1; }
    if (CBGM::GetPlaybackStarts() != starts || music.GetTrack() != 0) { ++failures; }
    std::printf("[audio-transition-check] menu retained=%d extra-starts=%u failures=%u\n",
        music.GetTrack() == 0, CBGM::GetPlaybackStarts() - starts, failures);
    SurvivalGameContext context{profile, savePath, 0};
    context.music = &music;
    if (RunSurvival(bigDirectory, "pack2", 7, 0, -1, "", 0, false, false, true, 2, 0,
        &context, false, false, nullptr, false, &window) != 0) { return 1; }
    const int battleTrack = music.GetTrack();
    if (battleTrack <= 0) { ++failures; }
    starts = CBGM::GetPlaybackStarts();
    music.SetPaused(true); // Re-entering menus must resume the retained battle track.
    BeginPostGame(state, context, weapons);
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state,
        savePath, "out/audio-transition-postgame.png", nullptr, false, &window, false, nullptr, &music) != -2) { return 1; }
    if (music.GetTrack() != battleTrack || CBGM::GetPlaybackStarts() != starts) { ++failures; }
    std::printf("[audio-transition-check] postgame track=%d expected=%d extra-starts=%u failures=%u\n",
        music.GetTrack(), battleTrack, CBGM::GetPlaybackStarts() - starts, failures);
    // Drive the real authored close timer; existing postgame checks cover hitboxes.
    state.page = 27;
    state.postGameClosing = true;
    state.postGameCloseTime = 100000;
    profile.xplodium = 1; // Explicit test fixture chooses the refinery branch.
    const std::vector<MenuTestClick> closeTicks{{-100, -100, 16}, {-100, -100, 16}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state,
        savePath, "out/audio-transition-refinery.png", &closeTicks, false, &window, false, nullptr, &music) != -2) { return 1; }
    if (state.page != 3 || music.GetTrack() != 0) { ++failures; }
    std::printf("[audio-transition-check] refinery page=%u track=%d failures=%u\n", state.page, music.GetTrack(), failures);
    const auto playback = music.GetPlaybackState();
    if (playback.voices != 1 || playback.devicesOpened != 1 || playback.streamsCreated != 1 ||
        playback.queuedBytes <= 0 || playback.paused) { ++failures; }
    // Muting is a gain change; switching tracks must not silently enable music.
    music.SetEnabled(false);
    if (!music.NextTrack() || music.GetPlaybackState().volume != 0) { ++failures; }
    music.SetEnabled(true);
    if (std::abs(music.GetPlaybackState().volume - 0.3f) > 0.001f) { ++failures; }
    std::printf("[audio-transition-check] real-SDL voices=%u devices=%u streams=%u queued=%lld pause=%d settings-preserved failures=%u\n",
        playback.voices, playback.devicesOpened, playback.streamsCreated, playback.queuedBytes, playback.paused, failures);
    return failures != 0;
}

int RunSceneTransitionCheck(const std::string &bigDirectory) {
    CWindow window;
    if (!window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    const unsigned generation = window.GetSurfaceGeneration();
    const unsigned surface = window.GetSurfaceId();
    int initialX = 0, initialY = 0, initialWidth = 0, initialHeight = 0;
    window.GetPosition(initialX, initialY);
    window.GetDrawableSize(initialWidth, initialHeight);
    PNGImage pixel;
    pixel.width = 1; pixel.height = 1; pixel.pixels = {23, 45, 67, 255};
    CTexture witness;
    if (!witness.Create(pixel)) { return 1; }
    unsigned failures = 0;
    for (unsigned scene = 0; scene < 4; ++scene) {
        int result = 0;
        if (scene == 0) {
            result = RunStartupSequence("out/scene-transition-logo.png", 2000, &window);
        } else if (scene == 2) {
            result = RunSurvival(bigDirectory, "pack2", 7, 0, -1, "out/scene-transition-game.png",
                0, false, false, false, 2, 0, nullptr, false, false, nullptr, false, &window);
        } else {
            result = RunGameFrontEnd(bigDirectory, "out/scene-transition-menu.png", 2, false,
                "out/scene-transition-profile", &window);
        }
        int x = 0, y = 0, width = 0, height = 0;
        window.GetPosition(x, y);
        window.GetDrawableSize(width, height);
        const bool retained = window.GetSurfaceId() == surface && window.GetSurfaceGeneration() == generation &&
            x == initialX && y == initialY && width == initialWidth && height == initialHeight && glIsTexture(witness.GetHandle());
        std::printf("[scene-transition-check] scene=%u result=%d generation=%u retained=%d\n",
            scene, result, window.GetSurfaceGeneration(), retained);
        if (result != 0 || !retained) { ++failures; }
    }
    return failures != 0;
}

int RunGameFrontEnd(const std::string &bigDirectory, const std::string &screenshotPath, unsigned page, bool originalProfile,
    const std::string &profilePath, CWindow *sharedWindow) {
    CWindow ownedWindow;
    CWindow &window = sharedWindow ? *sharedWindow : ownedWindow;
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CPlayerProgress::Template progress;
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    {
        CWindow &loadingWindow = window;
        if (!loadingWindow.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
        MovieRenderer loadingMovies;
        CResPackTOC *core = toc.GetPack(toc.GetCorePackIndex());
        if (!loadingMovies.Init(*core, *core)) { return 1; }
        LoadingScreen loading(loadingWindow, loadingMovies, tables, nullptr, false, screenshotPath.empty() || page == 14);
        if (!loading.IsValid()) { return 1; }
        if (!LoadPlayerProgress(toc, tables, progress) || !LoadRefinementTemplate(toc, tables, refinement) ||
            !LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
            !LoadArmorCatalog(toc, tables, armor)) { return 1; }
        if (!loading.IsValid()) { return 1; }
        if (loading.Cancelled()) { return 0; }
    }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    std::filesystem::path savePath = std::filesystem::path(ASSET_ROOT) / "userdata" / "saves";
    if (!profilePath.empty()) { savePath = profilePath; }
    // Explicit .dat paths keep old research fixtures usable. Default game and
    // --original-profile both use the original numbered records exclusively.
    if (!profilePath.empty() && savePath.extension() == ".dat") {
        std::printf("[game] explicit legacy research profile: %s\n", savePath.string().c_str());
        if (!profile.LoadFromDisk(savePath)) { return 1; }
    } else {
        if (!LoadNativeProfile(toc, tables, profile, savePath, std::filesystem::path(ASSET_ROOT) / "saves")) {
            std::printf("[game] native profile cannot be loaded; files preserved: %s\n", savePath.string().c_str());
            return 1;
        }
    }
    CBGM music; // Lifetime includes every menu, loading screen and game session.
    MenuState state;
    state.shopGunSlot = profile.activeWeaponSlot;
    state.page = std::min(page, 29u);
    if (page == 0 && screenshotPath.empty()) {
        // Desktop splash requested by the user precedes EnterShell's original
        // first-launch player selection / returning-player greeting.
        state.page = 14;
    }
    if (state.page == 1) { state.page = 2; }
    if (state.page == 7 || state.page == 20 || state.page == 16) { state.page = 0; }
    if (state.page == 9 || state.page == 10) { state.page = 4; }
    if (state.page == 12) { state.page = 6; state.optionsFocus = 9; }
    if (state.page == 15) { state.page = 0; }
    if (state.page == 19 || state.page == 23) { state.page = 21; }
    while (true) {
        const int choice = ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, savePath, screenshotPath, nullptr, originalProfile, &window, false, nullptr, &music);
        if (choice == -3) { return 1; }
        if (choice == -2) { return 0; }
        if (choice < 0) { return !profile.SaveToDisk(savePath); }
        if (choice == 5) {
            SurvivalGameContext context{profile, savePath, 0};
            context.music = &music;
            context.tutorial = true;
            std::string tutorialPack = "pack2";
            unsigned tutorialMap = 7;
            if (profile.nativeArchive) {
                const auto &level = profile.nativeArchive->survivalLevels[0];
                std::vector<std::uint8_t> bytes;
                if (!tables.ReadSectionResource(level.packHash, GameSection::Level, level.localIndex, bytes)) { return 1; }
                CArrayInputStream input(bytes);
                CLevel::Template data;
                if (!data.Init(input) || input.Available() != 0) { return 1; }
                tutorialPack = tables.GetPackName(data.mapRef.packHash);
                tutorialMap = data.mapRef.localIndex;
            }
            if (RunSurvival(bigDirectory, tutorialPack, tutorialMap, 0, -1, "", 0, false, false, false, 2, 0, &context, true, false, nullptr, false, &window) != 0) { return 1; }
            if (profile.tutorialCompleted) { BeginPostGame(state, context, weapons); }
            else { state.Navigate(25, true); }
            continue;
        }
        if (choice == 4) {
            if (profile.nativeArchive) {
                std::vector<PlanetEntry> planets;
                if (!LoadPlanetCatalog(toc, tables, planets) || state.planet >= planets.size()) { return 1; }
                const auto &planet = planets[state.planet];
                if (state.hordeStart >= planet.missions.size() ||
                    !SameObject(state.selectedMission, planet.data.missions[state.hordeStart])) { return 1; }
                MissionEntry selected;
                selected.resource = state.selectedMission;
                selected.data = planet.missions[state.hordeStart];
                selected.title = planet.missionInfo[state.hordeStart].title;
                if (selected.data.type != 2 || OriginalMissionLocked(profile, selected.data, planet.missionInfo[state.hordeStart])) { return 1; }
                const auto &map = planet.missionInfo[state.hordeStart].map;
                SurvivalGameContext context{profile, savePath};
                context.music = &music;
                context.hordeStart = static_cast<int>(state.hordeStart);
                if (RunSurvival(bigDirectory, tables.GetPackName(map.packHash), map.localIndex, 0, -1, "", 0, false, false, false, 2,
                    selected.data.value64, &context, profile.brotherEnabled, false, &selected, false, &window) != 0) { return 1; }
                BeginPostGame(state, context, weapons);
                continue;
            }
            // Explicit legacy .dat study path retains its historical fixture.
            std::vector<MissionEntry> missions;
            if (!LoadMissionCatalog(toc, tables, missions)) { return 1; }
            const unsigned packHash = toc.GetPack(toc.GetPackIndexFromName("pack11"))->GetPackHash();
            const MissionEntry *selected = nullptr;
            for (const MissionEntry &mission : missions) {
                if (mission.resource.packHash == packHash && mission.resource.localIndex == state.hordeStart && mission.data.type == 2) { selected = &mission; break; }
            }
            if (selected == nullptr) { return 1; }
            SurvivalGameContext context{profile, savePath};
            context.music = &music;
            context.hordeStart = static_cast<int>(state.hordeStart);
            if (RunSurvival(bigDirectory, "pack11", 0, 0, -1, "", 0, false, false, false, 2,
                selected->data.value64, &context, profile.brotherEnabled, false, selected, false, &window) != 0) { return 1; }
            BeginPostGame(state, context, weapons);
            continue;
        }
        unsigned wave = profile.clearedWaves[choice];
        if (wave >= 500) { wave = 0; }
        if (state.startingWave >= 0) { wave = static_cast<unsigned>(state.startingWave); }
        SurvivalGameContext context{profile, savePath, static_cast<unsigned>(choice)};
        context.music = &music;
        std::string mapPack;
        unsigned mapIndex = 0;
        if (profile.nativeArchive) {
            const auto &level = profile.nativeArchive->survivalLevels[choice];
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(level.packHash, GameSection::Level, level.localIndex, bytes)) { return 1; }
            CArrayInputStream input(bytes);
            CLevel::Template data;
            if (!data.Init(input) || input.Available() != 0) { return 1; }
            mapPack = tables.GetPackName(data.mapRef.packHash);
            mapIndex = data.mapRef.localIndex;
        } else {
            // Explicit legacy research profiles retain their historical map fixture.
            mapPack = kPlanetPacks[choice];
            mapIndex = kPlanetMaps[choice];
        }
        if (RunSurvival(bigDirectory, mapPack, mapIndex, 0, -1, "", 0, false, false, false, 2, wave, &context, profile.brotherEnabled, false, nullptr, false, &window) != 0) { return 1; }
        BeginPostGame(state, context, weapons);
    }
}

/** Real BIG cards, native save copies and actual expanded-card input. */
int RunDualWeaponCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    if (!LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    const auto path = std::filesystem::path("out/dual-weapon-check") / std::to_string(GetTickCount64());
    CProfileManager profile;
    if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    std::vector<unsigned> entries;
    for (unsigned index = 0; index < store.size() && entries.size() < 3; ++index) {
        const auto &item = store[index].data;
        if (item.objects.size() != 1 || item.objects[0].type != 6 || item.singlePurchase != 0) { continue; }
        const auto &ref = item.objects[0].object;
        const auto *weapon = FindWeaponEntry(weapons, ref);
        if (weapon == nullptr || weapon->visualOnly) { continue; }
        bool duplicate = false;
        for (unsigned existing : entries) { if (SameObject(store[existing].data.objects[0].object, ref)) { duplicate = true; } }
        if (duplicate) { continue; }
        entries.push_back(index);
        profile.Grant(6, ref);
        // Gold weapons still expose EQUIP: exercise the duplicate-write guard.
        profile.AddWeaponExperience(ref, weapon->data.GetMasteryThreshold(2), weapon->data.GetMasteryThreshold(2));
    }
    if (entries.size() != 3) { return 1; }
    const GameObjectRef first = store[entries[0]].data.objects[0].object;
    const GameObjectRef second = store[entries[1]].data.objects[0].object;
    const GameObjectRef third = store[entries[2]].data.objects[0].object;
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    const unsigned card = view.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
    const CMovie *movie = view.movies.GetMovie(card);
    unsigned start = 0, end = 0;
    MovieRegion content, body, actions, label;
    const auto *equip = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", kEquipButtonEntry);
    if (movie == nullptr || equip == nullptr || !movie->GetChapterRange(1, start, end) ||
        !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_MENU"), 0, 0, content) ||
        !view.movies.Region(card, 0, end, body) ||
        !view.movies.Region(view.movies.Ordinal(equip->movies[0]), 1, 0, label)) { return 1; }
    const StoreCardFace face{content.x + content.width / 2 - static_cast<int>(content.width) / 16 - body.width / 2,
        content.y + content.height / 2 - body.height / 2, 1, end};
    if (!CardRegion(view, card, kCardActionRegion, face, actions)) { return 1; }
    unsigned failures = 0;
    for (unsigned active : {0u, 1u}) {
        profile.configuration.guns = {first, second};
        profile.activeWeaponSlot = active;
        unsigned stamps = 0;
        for (const auto &gun : profile.configuration.guns) { if (IsStoreObjectEquipped(profile, active, gun)) { ++stamps; } }
        if (stamps != 2) { ++failures; }
        for (unsigned step = 0; step < 2; ++step) {
            MenuState state;
            state.page = 2;
            state.shopGunSlot = active;
            state.slot = active;
            state.shopDetailOpen = true;
            state.shopFocusAmount = 1;
            state.shopDetailTime = end;
            state.selectedItem = entries[1 - active];
            if (step == 1) { state.selectedItem = entries[2]; }
            view.Begin(2);
            view.SetTestClick({actions.x + actions.width - label.width / 2, actions.y + label.height / 2});
            if (!DrawStore(view, toc, tables, profile, 200, store, weapons, armor, state, path)) { return 1; }
            bool correct = SameObject(profile.configuration.guns[0], first) && SameObject(profile.configuration.guns[1], second);
            if (step == 1) {
                correct = SameObject(profile.configuration.guns[active], third);
                if (active == 0) { correct = correct && SameObject(profile.configuration.guns[1], second); }
                else { correct = correct && SameObject(profile.configuration.guns[0], first); }
            }
            const bool distinct = !SameObject(profile.configuration.guns[0], profile.configuration.guns[1]);
            if (!correct || !distinct) { ++failures; }
            std::printf("[dual-weapon-check] active=%u stamps=%u step=%u correct=%d distinct=%d\n", active, stamps, step, correct, distinct);
            if (step == 0) {
                state.shopDetailOpen = false;
                view.Begin(2);
                if (!DrawStore(view, toc, tables, profile, 200, store, weapons, armor, state, path) ||
                    !view.window.SaveFrame("out/dual-weapon-slot-" + std::to_string(active) + ".png")) { return 1; }
            }
        }
        CProfileManager restored;
        if (!LoadNativeProfile(toc, tables, restored, path, path / "absent-source")) { return 1; }
        for (unsigned slot = 0; slot < 2; ++slot) {
            if (!SameObject(profile.configuration.guns[slot], restored.configuration.guns[slot])) { ++failures; }
        }
        if (restored.activeWeaponSlot != active) { ++failures; }
    }
    std::printf("[dual-weapon-check] failures=%u\n", failures);
    return failures != 0;
}
