/** @file GameFrontEnd.cpp
 * @brief Connect the rebuilt offline account to actual gameplay.
 */
#define NOMINMAX
#include "runtime/GameFrontEnd.h"
#include "runtime/OriginalProfile.h"
#include "runtime/MissionCatalog.h"
#include "runtime/StoreCatalog.h"
#include "runtime/WeaponCatalog.h"
#include "runtime/ArmorCatalog.h"
#include "runtime/PowerupCatalog.h"
#include "runtime/PlayerModel.h"
#include "runtime/HudText.h"
#include "runtime/MovieRenderer.h"
#include "runtime/LoadingScreen.h"
#include "runtime/OriginalMenuData.h"
#include "runtime/SurvivalGameContext.h"
#include "runtime/HostSettings.h"
#include "gun_bros/CDailyBonusTracking.h"
#include "gun_bros/Planet.h"
#include "gun_bros/CBGM.h"
#include "milestones/M3Map.h"
#include "milestones/EnemyModel.h"
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
constexpr const char *kPageNames[] = {"PLANETS", "EQUIPMENT", "SHOP", "REFINERY"};
constexpr const char *kSlotNames[] = {"WEAPON 1", "WEAPON 2", "HELMET", "ARMOR", "PANTS", "ITEMS"};
constexpr unsigned kArmorSlots[] = {0, 0, 2, 1, 0};
constexpr float kMenuWidth = 1024;
// The store's character column starts where the black content area does; the
// original character reaches up past the category tabs on that side.
constexpr float kStoreMeshClipTop = 133;
constexpr float kMenuHeight = 768;
constexpr const char *kActivityNames[] = {"FIRST TOUR", "TARGET PRACTICE", "PRIME DEFENDER", "ARMED AND READY",
    "HAVEN PATROL", "EXTERMINATOR", "SPACE EXPLORER", "REVOLUTION"};
constexpr const char *kActivityDescriptions[] = {"Clear 5 survival waves.", "Defeat 50 enemies with your bro.",
    "Defeat 250 enemies on Cerberus Prime.", "Own 4 different weapons.", "Clear 15 waves on Haven.",
    "Defeat 1,000 enemies.", "Clear a wave on all four planets.", "Clear 50 survival waves."};

std::int64_t CurrentSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

bool SameObject(const GameObjectRef &first, const GameObjectRef &second) {
    return first.packHash == second.packHash && first.localIndex == second.localIndex;
}

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
    float shopScroll = 0;
    std::uint64_t shopDetailStart = 0;
    std::uint64_t shopDetailLastTick = 0;
    unsigned shopDetailTime = 0;
    bool shopDetailClosing = false;
    float shopFocusAmount = 0;
    // Selecting a card never previews; only the PREVIEW button does.
    bool shopPreview = false;
    float playerSpin = 0;
    // When the upgrade page opened, so its meter can run up to its real value.
    std::uint64_t masteryOpened = 0;
    unsigned shopFilter = 0;
    bool shopFilterOpen = false;
    // Each menu owns its playback cursor; cached CMovie resources stay immutable.
    unsigned shopFilterTime = 0;
    std::uint64_t shopFilterLastTick = 0;
    bool shopFilterBound = false;
    bool shopDetailOpen = false;
    unsigned gameMode = 0;
    bool modeSelected = false;
    float starPanX = 0, starPanY = 0;
    float missionScroll = 0;
    unsigned revolution = 0, wavePage = 0, missionTab = 0;
    bool currencyPending = false;
    std::uint64_t currencyReadyAt = 0;
    SurvivalResult result;
    GameObjectRef masteryWeapon;
    bool refinementRequired = false;
    unsigned refineryTab = 0, casualtyPage = 0;
    float optionsScroll = 0;
    unsigned optionsFocus = 0;
    unsigned socialTab = 0;
    bool inviteOpen = false;
    std::vector<unsigned> history;

    // Every nested page remembers its caller; trunk navigation starts a new path.
    void Navigate(unsigned target, bool root = false) {
        if (root) { history.clear(); }
        if (target == page) { return; }
        if (!root && page != 14) { history.push_back(page); }
        page = target;
    }
    void Back() {
        if (history.empty()) { page = 0; return; }
        page = history.back();
        history.pop_back();
    }
};

struct MenuTestClick { float x; float y; unsigned advanceMs = 0; };

/** All GL owners are destroyed before the menu window's context. */
class GameMenu {
public:
    /** Integration harness input; it still goes through rendered button hit tests. */
    void SetTestClick(const MenuTestClick &click) { mouseX = click.x; mouseY = click.y; clicked = true; }
    /** Temporarily route this frame's click exclusively to a modal panel. */
    bool ExchangeClick(bool enabled) { const bool previous = clicked; clicked = enabled; return previous; }
    bool Open(CResTOCManager &toc, PackTables &tables) {
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
        LoadingScreen loading(window, movies, tables);
        for (unsigned index = 0; index < 7; ++index) {
            const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_TRUNK", index);
            if (entry == nullptr) { return false; }
            std::printf("[navigation] index=%u label=%s sprite=%u\n", index,
                movies.NamedString(entry->strings[0]).c_str(), entry->sprites[0]);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (unsigned index = 0; index < 6; ++index) {
            CResPackTOC *pack = toc.GetPack(toc.GetPackIndexFromName(kPlanetPacks[index]));
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack->GetPackHash(), GameSection::Planet, 0, payload)) { return false; }
            CArrayInputStream stream(payload);
            if (!planets[index].Init(stream)) { return false; }
            names[index] = ReadGameString(toc, planets[index].name);
            descriptions[index] = ReadGameString(toc, planets[index].description);
            const CGameSpriteGluRef &sprite = planets[index].largeImage;
            const int spritePack = toc.GetPackIndexFromHash(sprite.packHash);
            if (spritePacks.count(spritePack) == 0) {
                auto glu = std::make_unique<CSpriteGlu>();
                if (!glu->Init(*toc.GetPack(spritePack))) { return false; }
                spritePacks[spritePack] = std::move(glu);
            }
            CSpriteGlu &glu = *spritePacks[spritePack];
            const CSpriteGluArchetype *archetype = glu.GetArchetype(sprite.archetype);
            if (archetype == nullptr) { return false; }
            CSpriteIterator iterator(glu, *archetype);
            if (!iterator.Expand(sprite.animation, 0, planetQuads[index])) { return false; }
        }
        return !loading.Cancelled();
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
        dragDistance += std::abs(dragX) + std::abs(dragY);
        // CMenuMission handles selection on release; dragging must never enter a planet.
        clicked = !down && previousDown && dragDistance < 9;
        previousDown = down;
        if (scripted) {
            dragX = 0;
            dragY = 0;
            dragDistance = 0;
            window.TakeWheelDelta();
        }
        movies.Draw(47, 1600);
        if (page == 3) { movies.Draw(36, 1600); }
        if (page == 2) { movies.Rectangle(0, 132, 1024, 627, 0, 0, 0); }
    }

    void Rect(float x, float y, float width, float height, float r = 0.155f, float g = 0.227f, float b = 0.29f) {
        markers.Begin();
        markers.AddRect(x, y, width, height);
        markers.Draw(textProgram, projection, r, g, b, 1);
    }

    void Text(float x, float y, const std::string &text, float size = 2,
        float r = 0.88f, float g = 0.93f, float b = 0.95f) {
        unsigned font = 1;
        float scale = size * 7 / 18.0f;
        if (r > 0.9f && g < 0.8f) { font = 5; scale = size * 7 / 27.0f; }
        movies.Text(text, x, y, font, scale);
    }

    bool Button(float x, float y, float width, float height, const std::string &label, bool selected = false) {
        const bool hover = mouseX >= x && mouseX < x + width && mouseY >= y && mouseY < y + height;
        unsigned time = 200;
        if (hover || selected) { time = 800; }
        if (label.empty()) {
            movies.Rectangle(x + 6, y + 4, width - 12, height - 8, 0, 0, 0, 1);
            movies.DrawFitted(73, time, x, y, width, height);
        } else { movies.ButtonBackground(x, y, width, height, selected, hover); }
        if (selected && label.empty()) {
            markers.Begin();
            markers.AddOutline(x + 2, y + 2, width - 4, height - 4, 1.5f);
            markers.Draw(textProgram, projection, 0.2f, 0.7f, 1, 0.8f);
        }
        if (!label.empty()) {
            unsigned font = 5;
            if (label == "<" || label == ">") { font = 0; }
            float scale = std::min(1.0f, (height - 12) / 27.0f);
            const float labelWidth = movies.TextWidth(label, font, scale);
            if (labelWidth > width - 12) { scale *= (width - 12) / labelWidth; }
            movies.Text(label, x + (width - movies.TextWidth(label, font, scale)) * 0.5f,
                y + (height - 27 * scale) * 0.5f, font, scale);
        }
        if (hover && clicked) { clicked = false; return true; }
        return false;
    }

    bool Hit(float x, float y, float width, float height) {
        if (clicked && mouseX >= x && mouseX < x + width && mouseY >= y && mouseY < y + height) {
            clicked = false;
            return true;
        }
        return false;
    }

    void CenterText(const std::string &text, float center, float y, unsigned font = 0, float scale = 0.85f) {
        movies.Text(text, center - movies.TextWidth(text, font, scale) * 0.5f, y, font, scale);
    }

    bool Tab(float x, float y, float width, const std::string &label, bool selected) {
        unsigned sprite = 74;
        if (selected) { sprite = 73; }
        movies.DrawSpriteFitted(0, sprite, 0, x, y, width, 42);
        CenterText(label, x + width * 0.5f, y + 7, 5, 1);
        return Hit(x, y, width, 42);
    }

    void Clip(float x, float y, float width, float height) {
        int screenWidth = 0, screenHeight = 0;
        window.GetDrawableSize(screenWidth, screenHeight);
        glEnable(GL_SCISSOR_TEST);
        glScissor(static_cast<int>(x * screenWidth / 1024), static_cast<int>((768 - y - height) * screenHeight / 768),
            static_cast<int>(width * screenWidth / 1024), static_cast<int>(height * screenHeight / 768));
    }

    void EndClip() { glDisable(GL_SCISSOR_TEST); }
    bool MouseIn(float x, float y, float width, float height) const {
        return mouseX >= x && mouseX < x + width && mouseY >= y && mouseY < y + height;
    }

    bool WaveButton(float x, float y, float width, float height, unsigned wave, unsigned cleared, bool selected, bool perfect = false) {
        // MDS_BUTTON_MISSION_WAVE: 5:21 locked, 5:22 available, 5:23 cleared.
        // Current iOS WasWavePerfected narrows the gold marker to perfect clears.
        unsigned animation = 21;
        if (wave <= cleared) { animation = 22; }
        if (perfect) { animation = 23; }
        movies.DrawSpriteFitted(5, animation, 0, x, y, width, height);
        if (selected) {
            markers.Begin();
            markers.AddOutline(x, y, width, height, 2);
            markers.Draw(textProgram, projection, 0.2f, 0.8f, 1, 1);
        }
        const std::string label = std::to_string(wave % 50 + 1);
        const float scale = width / 80 * 1.15f;
        movies.Text(label, x + (width - movies.TextWidth(label, 6, scale)) * 0.5f,
            y + height * 0.07f, 6, scale);
        return Hit(x, y, width, height);
    }

    bool TitleImage() {
        if (!titleImage.IsValid()) {
            std::ifstream file(std::filesystem::path(ASSET_ROOT) / "png/Default-Landscape.png", std::ios::binary);
            if (!file) { return false; }
            std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(file), {});
            PNGImage decoded;
            if (!PNGDecode(bytes, decoded) || !titleImage.Create(decoded)) { return false; }
        }
        images.Begin();
        const SourceRect source{0, 0, static_cast<std::uint16_t>(titleImage.GetWidth()), static_cast<std::uint16_t>(titleImage.GetHeight())};
        images.AddQuad(titleImage, 0, 0, 1024, 768, source, false, false, BlendMode::Alpha);
        images.Upload();
        images.Draw(imageProgram, projection);
        return true;
    }

    void Paragraph(float x, float y, float width, const std::string &text, float scale = 0.85f) {
        std::istringstream paragraphs(text);
        std::string paragraph;
        while (std::getline(paragraphs, paragraph)) {
            std::istringstream words(paragraph);
            std::string word, line;
            while (words >> word) {
                std::string next = word;
                if (!line.empty()) { next = line + " " + word; }
                if (!line.empty() && movies.TextWidth(next, 0, scale) > width) {
                    movies.Text(line, x, y, 0, scale);
                    y += 26 * scale;
                    line = word;
                } else { line = next; }
            }
            movies.Text(line, x, y, 0, scale);
            y += 30 * scale;
        }
    }

    void BodyPanel() {
        movies.Rectangle(24, 184, 976, 510, 0.015f, 0.035f, 0.05f, 0.96f);
        // Original military panel artwork is also used by the Bro-ops dialogs.
        movies.Draw(111, 900, 512, 430);
    }

    int Header(const CProfileManager &profile, const CPlayerProgress &progress, unsigned currentPage) {
        movies.Draw(10, 1600);
        // Metric regions 14..16 belong to the original top resource strip.
        movies.Text(std::to_string(profile.coins), 100, 8, 0, 1);
        movies.Text(std::to_string(profile.warbucks), 390, 8, 0, 1);
        movies.Draw(11, 1600, 697, 0);
        float experienceFraction = 0;
        if (progress.GetLevel() < 200) {
            experienceFraction = static_cast<float>(progress.GetExperienceInLevel()) / std::max(1u, progress.GetExperienceDelta());
        }
        movies.Rectangle(784, 31, 115 * experienceFraction, 10, 0.1f, 0.65f, 0.9f);
        char level[4];
        std::snprintf(level, sizeof(level), "%03u", progress.GetLevel());
        for (unsigned digit = 0; digit < 3; ++digit) {
            movies.Text(std::string(1, level[digit]), 939 + digit * 28.0f, 11, 7, 1);
        }
        int choice = -1;
        if (currentPage >= 25) { navigationVisible = false; return choice; }
        if (!navigationVisible) { navigationStart = window.GetTicksMs(); navigationVisible = true; }
        unsigned activePage = currentPage;
        if (currentPage == 1 || currentPage == 17 || currentPage == 18) { activePage = 2; }
        if (currentPage == 16 || currentPage == 19) { activePage = 0; }
        if (currentPage == 8 || currentPage == 9 || currentPage == 11) { activePage = 6; }
        if (currentPage == 13) { activePage = 5; }
        constexpr unsigned navigationPages[] = {0, 4, 5, 2, 3, 6, 7};
        // iOS screenshots show PLAY / BROS / BRO-OPS / STORE. The compiled
        // provider table is a catalog; its storage order is not screen order.
        constexpr unsigned navigationEntries[] = {0, 2, 3, 1, 4, 5, 6};
        for (const MovieRegion &region : movies.Regions(10, 1600)) {
            if (region.index >= 7) { continue; }
            float entrance = 1;
            if (animateNavigation) {
                const float elapsed = static_cast<float>(window.GetTicksMs() - navigationStart) - region.index * 65.0f;
                if (elapsed < 0) { continue; }
                entrance = std::min(1.0f, elapsed / 220);
            }
            const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_TRUNK", navigationEntries[region.index]);
            const unsigned sprite = entry->sprites[0];
            if (navigationPages[region.index] == activePage || region.Contains(mouseX, mouseY)) {
                // GLU_MOVIE_TRUNK_BUTTONS chapter 1 is the lit plate with the
                // outline; chapter 3 is the plain resting plate. Rendering the
                // movie at both times is what settled which one the original
                // screenshots show behind the selected trunk icon.
                movies.DrawFitted(14, 150, region.x, region.y, region.width, region.height, 1);
            }
            const float bounce = std::sin(entrance * 3.14159265f) * 0.15f;
            const float iconScale = 0.65f + entrance * 0.35f + bounce;
            movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, region.x + region.width * (1 - iconScale) * 0.5f,
                region.y + 22 * (1 - entrance), region.width * iconScale, region.height * iconScale);
            const std::string label = movies.NamedString(entry->strings[0]);
            const float scale = std::min(1.0f, 88.0f / std::max(1.0f, movies.TextWidth(label, 1)));
            movies.Text(label, region.x + region.width * 0.5f - movies.TextWidth(label, 1, scale) * 0.5f, 43, 1, scale);
            if (Hit(region.x - 12, region.y - 8, region.width + 24, region.height + 32)) { choice = static_cast<int>(region.index); }
        }
        if (Hit(35, 0, 308, 65)) { choice = 7; }
        if (Hit(350, 0, 338, 65)) { choice = 8; }
        return choice;
    }

    void DrawPlanet(unsigned index, float centerX = 737, float centerY = 365, float diameter = 390) {
        const auto &quads = planetQuads[index];
        if (quads.empty()) { return; }
        float left = 100000, top = 100000, right = -100000, bottom = -100000;
        for (const SpriteQuad &quad : quads) {
            left = std::min(left, static_cast<float>(quad.offsetX));
            top = std::min(top, static_cast<float>(quad.offsetY));
            right = std::max(right, static_cast<float>(quad.offsetX + quad.Width()));
            bottom = std::max(bottom, static_cast<float>(quad.offsetY + quad.Height()));
        }
        const float scale = std::min(diameter / (right - left), diameter / (bottom - top));
        images.Begin();
        for (const SpriteQuad &quad : quads) {
            const float x = centerX + (quad.offsetX - (left + right) * 0.5f) * scale;
            const float y = centerY + (quad.offsetY - (top + bottom) * 0.5f) * scale;
            images.AddTransformedQuad(*quad.page, x, y, static_cast<float>(quad.Width()) * scale,
                static_cast<float>(quad.Height()) * scale, quad.source, quad.flipHorizontal, quad.flipVertical,
                quad.blend, 0, 0, 1, 1, 0, 1, quad.rotateTexture);
        }
        images.Upload();
        images.Draw(imageProgram, projection);
    }

    void Icon(CResTOCManager &toc, PackTables &tables, const StoreEntry &entry, float x, float y, float width,
        float height, float alpha = 1, bool originalSize = false) {
        const CGameAssetRef &ref = entry.data.assets[1];
        if (ref.assetId < 0 || ref.IsNull()) { return; }
        const std::uint64_t key = (static_cast<std::uint64_t>(ref.packHash) << 32) | static_cast<unsigned>(ref.assetId);
        if (icons.count(key) == 0) {
            std::vector<std::uint8_t> payload;
            PNGImage decoded;
            auto texture = std::make_unique<CTexture>();
            if (!tables.ReadSectionResource(ref.packHash, GameSection::Png, ref.assetId, payload) ||
                !PNGDecode(payload, decoded) || !texture->Create(decoded)) { return; }
            icons[key] = std::move(texture);
        }
        const CTexture &texture = *icons[key];
        float scale = std::min(width / texture.GetWidth(), height / texture.GetHeight());
        // CMenuStoreOption::ThumbCallback :181036 preserves the PNG dimensions.
        // A thumbnail wider than region 5 starts at its left edge.
        if (originalSize) { scale = 1; }
        const float drawnWidth = texture.GetWidth() * scale;
        const float drawnHeight = texture.GetHeight() * scale;
        float drawnX = x + (width - drawnWidth) * 0.5f;
        if (originalSize && drawnWidth > width) { drawnX = x; }
        const SourceRect source{0, 0, static_cast<std::uint16_t>(texture.GetWidth()), static_cast<std::uint16_t>(texture.GetHeight())};
        images.Begin();
        images.AddTransformedQuad(texture, drawnX, y + (height - drawnHeight) * 0.5f,
            drawnWidth, drawnHeight, source, false, false, BlendMode::Alpha, 0, 0, 1, 1, 0, alpha);
        images.Upload();
        images.Draw(imageProgram, projection);
    }

    bool DrawEquippedPlayer(CResTOCManager &toc, PackTables &tables, const CProfileManager &profile,
        const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armors, unsigned slot,
        const GameObjectTypeRef *previewItem = nullptr, const MovieRegion *storePanel = nullptr, float spin = 0) {
        unsigned gunSlot = 0;
        if (slot == 1) { gunSlot = 1; }
        // Preview substitutes only the model configuration. Ownership, currency
        // and the saved loadout remain owned by the explicit purchase action.
        CPlayerConfiguration configuration = profile.configuration;
        if (previewItem != nullptr) {
            if (previewItem->type == 6) { configuration.guns[gunSlot] = previewItem->object; }
            if (previewItem->type == 2 && slot >= 2 && slot <= 4) { configuration.armor[kArmorSlots[slot]] = previewItem->object; }
        }
        // Switching weapon slot is the original's swap, not just a rebuild.
        const bool swapped = equippedPreview != nullptr && previewGunSlot != gunSlot;
        bool changed = equippedPreview == nullptr || previewGunSlot != gunSlot;
        if (equippedPreview != nullptr && equippedPreview->brotherIndex != profile.playerBrother) { changed = true; }
        if (!SameObject(previewConfiguration.guns[gunSlot], configuration.guns[gunSlot])) { changed = true; }
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
            previewTicks = window.GetTicksMs();
            // CPlayer::OnSwapGun :101048 hands input event 5 to the player
            // script, which owns the swap animation.
            if (swapped) { equippedPreview->weapon->brother.OnSwapGun(); }
        }
        if (storePanel == nullptr) {
            Rect(20, 414, 180, 280, 0.035f, 0.07f, 0.10f);
            std::string previewTitle = "EQUIPPED";
            if (previewItem != nullptr) { previewTitle = "PREVIEW"; }
            Text(38, 432, previewTitle, 1.75f, 0.93f, 0.74f, 0.33f);
        }
        const std::uint64_t now = window.GetTicksMs();
        AdvancePlayer(*equippedPreview, static_cast<int>(std::min<std::uint64_t>(now - previewTicks, 100)));
        previewTicks = now;
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        // Restrict the model's depth and long weapon geometry to its sidebar panel.
        glEnable(GL_SCISSOR_TEST);
        // The store's own GLU_MOVIE_STORE_MENU region frames the character.
        float panelX = 20, panelBottom = 686, panelWidth = 180, panelHeight = 232;
        if (storePanel != nullptr) {
            panelX = storePanel->x;
            panelBottom = storePanel->y + storePanel->height;
            panelWidth = storePanel->width;
            panelHeight = storePanel->height;
        }
        const int previewX = static_cast<int>(panelX * width / kMenuWidth);
        const int previewY = static_cast<int>((kMenuHeight - panelBottom) * height / kMenuHeight);
        const int previewWidth = static_cast<int>(panelWidth * width / kMenuWidth);
        const int previewHeight = static_cast<int>(panelHeight * height / kMenuHeight);
        // The region frames the model, but the original character reaches above
        // and below it, so only the column and the bar above it clip.
        int clipY = previewY, clipHeight = previewHeight;
        if (storePanel != nullptr) {
            clipY = 0;
            clipHeight = static_cast<int>((kMenuHeight - kStoreMeshClipTop) * height / kMenuHeight);
        }
        glScissor(previewX, clipY, previewWidth, clipHeight);
        glViewport(previewX, previewY, previewWidth, previewHeight);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        // Same upright 90-degree menu orientation as the permanent character
        // viewer. Body bounds provide stable framing when switching weapons.
        const MeshBounds bounds = PlayerBounds(*equippedPreview);
        float centre[16], normalise[16], tilt[16], facing[16], local[16], turned[16], oriented[16], viewProjection[16], model[16];
        Matrix4dTranslation(-bounds.centerX, -bounds.centerY, -bounds.centerZ, centre);
        Matrix4dScale(bounds.inverseExtent, normalise);
        Matrix4dRotationX(3.14159265f * 0.5f, tilt);
        // The authored idle pose faces away from this camera; show the front.
        // Dragging the model adds to that turn about its own vertical axis.
        Matrix4dRotationZ(3.14159265f + spin, facing);
        Matrix4dMultiply(normalise, centre, local);
        Matrix4dMultiply(facing, local, turned);
        Matrix4dMultiply(tilt, turned, oriented);
        float cameraWidth = 1.6f;
        // Widest value that still keeps the whole figure inside its region;
        // the outstretched weapons, not the height, set the limit.
        if (storePanel != nullptr) { cameraWidth = 0.75f; }
        Matrix4dOrthoCentred(cameraWidth, cameraWidth * panelHeight / panelWidth, 4, viewProjection);
        Matrix4dMultiply(viewProjection, oriented, model);
        DrawPlayer(*equippedPreview, imageProgram, model);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, width, height);
        return true;
    }

    /** CEnemy::SpawnForUI assembles the original result-card model. */
    bool DrawCasualty(PackTables &tables, CResTOCManager &toc, const EnemyCasualty &casualty, float x) {
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
        CenterText(preview.name, x + 100, 661, 0, 0.78f);
        CenterText(std::to_string(casualty.count) + " KILLS", x + 100, 698, 0, 0.92f);
        const int config = EnemyPartConfig(preview.model, 0);
        if (config < 0) { return true; }
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        Clip(x + 10, 495, 180, 156);
        glViewport(static_cast<int>((x + 10) * width / 1024), static_cast<int>(117 * height / 768),
            static_cast<int>(180 * width / 1024), static_cast<int>(156 * height / 768));
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        const MeshBounds &bounds = preview.model.configs[config]->mesh.GetBounds();
        float centre[16], scale[16], tilt[16], facing[16], local[16], turned[16], oriented[16], projection3D[16], model[16];
        Matrix4dTranslation(-bounds.centerX, -bounds.centerY, -bounds.centerZ, centre);
        Matrix4dScale(bounds.inverseExtent, scale);
        Matrix4dRotationX(3.14159265f * 0.5f, tilt);
        Matrix4dRotationZ(3.14159265f, facing);
        Matrix4dMultiply(scale, centre, local);
        Matrix4dMultiply(facing, local, turned);
        Matrix4dMultiply(tilt, turned, oriented);
        Matrix4dOrthoCentred(1.35f, 1.17f, 4, projection3D);
        Matrix4dMultiply(projection3D, oriented, model);
        DrawEnemyModel(preview.model, imageProgram, model);
        glDisable(GL_DEPTH_TEST);
        EndClip();
        glViewport(0, 0, width, height);
        return true;
    }

    CWindow window;
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
        if (elapsed >= 300) { return; }
        movies.DrawFitted(pressMovie, 100 + static_cast<unsigned>(elapsed), pressX, pressY, pressWidth, pressHeight, 1);
    }
    MovieRenderer movies;
    std::string names[6], descriptions[6];
    float dragX = 0, dragY = 0;
    bool animateNavigation = true;
private:
    CShaderProgram textProgram;
    CShaderProgram imageProgram;
    CMarkerBatch markers;
    CQuadBatch images;
    CTexture titleImage;
    float projection[16]{};
    float mouseX = 0, mouseY = 0;
    bool clicked = false, previousDown = false;
    float dragDistance = 0;
    Planet planets[6];
    std::vector<SpriteQuad> planetQuads[6];
    std::map<int, std::unique_ptr<CSpriteGlu>> spritePacks;
    std::map<std::uint64_t, std::unique_ptr<CTexture>> icons;
    std::unique_ptr<PlayerModel> equippedPreview;
    CPlayerConfiguration previewConfiguration;
    unsigned previewGunSlot = 0;
    std::uint64_t previewTicks = 0;
    std::uint64_t navigationStart = 0;
    bool navigationVisible = false;
    struct EnemyPreview {
        EnemyTemplateData data;
        EnemyModel model;
        std::string name;
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
                return !weapon.visualOnly && !weapon.unused;
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

std::string ItemPrice(const CStoreItem &item) {
    if (item.commonPrice != 0) { return std::to_string(item.commonPrice) + " COINS"; }
    if (item.rarePrice != 0) { return std::to_string(item.rarePrice) + " WARBUCKS"; }
    return "FREE";
}

std::string PurchaseMessage(PurchaseResult result) {
    switch (result) {
    case PurchaseResult::Purchased: return "PURCHASED AND EQUIPPED";
    case PurchaseResult::Owned: return "EQUIPPED";
    case PurchaseResult::LevelLocked: return "REACH THE REQUIRED LEVEL FIRST";
    case PurchaseResult::InsufficientCoins: return "NOT ENOUGH COINS - REFINE XPLODIUM TO EARN COINS";
    case PurchaseResult::InsufficientWarbucks: return "NOT ENOUGH WARBUCKS";
    default: return "ITEM NOT AVAILABLE";
    }
}

std::string UpperLabel(std::string label) {
    for (char &letter : label) { letter = static_cast<char>(std::toupper(static_cast<unsigned char>(letter))); }
    return label;
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
constexpr unsigned kDrawnColumns = 3;
constexpr unsigned kScrollRestTime = 400;
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
constexpr unsigned kOwnedFilterBit = 1u << 7;
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
constexpr float kButtonLabelScale = 0.85f;

/** Centre one original label inside a plate. */
void PlateLabel(GameMenu &view, const std::string &label, float x, float y, float width, float height) {
    view.movies.Text(label, x + (width - view.movies.TextWidth(label, 5, kButtonLabelScale)) * 0.5f,
        y + (height - 27 * kButtonLabelScale) * 0.5f, 5, kButtonLabelScale);
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
struct StoreTextRun {
    std::string text;
    unsigned font = 1;
    float x = 0, width = 0, height = 0;
};
struct StoreTextLine {
    std::vector<StoreTextRun> runs;
    float width = 0, height = 0;
};

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

std::vector<StoreTextLine> FormatStoreText(MovieRenderer &renderer, const std::string &text, float width) {
    constexpr unsigned fonts[] = {1, 2, 4, 3, 0};
    std::vector<StoreTextLine> lines(1);
    unsigned font = fonts[0];
    float space = 0;
    for (std::size_t position = 0; position < text.size();) {
        if (text[position] == '^' && position + 2 < text.size() && text[position + 1] == 'f' &&
            text[position + 2] >= '0' && text[position + 2] <= '9') {
            font = fonts[std::min(4u, static_cast<unsigned>(text[position + 2] - '0'))];
            position += 3;
            continue;
        }
        if (text[position] == '\n') {
            lines.back().height = std::max(lines.back().height, renderer.TextHeight(font));
            lines.push_back({});
            space = 0;
            ++position;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(text[position]))) {
            space += renderer.TextWidth(" ", font);
            ++position;
            continue;
        }
        std::size_t end = position + 1;
        while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end])) && text[end] != '^') { ++end; }
        StoreTextRun run;
        run.text = text.substr(position, end - position);
        run.font = font;
        run.width = renderer.TextWidth(run.text, font);
        run.height = renderer.TextHeight(font);
        if (!lines.back().runs.empty() && lines.back().width + space + run.width > width) {
            lines.push_back({});
        }
        if (lines.back().runs.empty()) { space = 0; }
        run.x = lines.back().width + space;
        lines.back().width = run.x + run.width;
        lines.back().height = std::max(lines.back().height, run.height);
        lines.back().runs.push_back(run);
        space = 0;
        position = end;
    }
    return lines;
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
bool DrawMasteryMeter(GameMenu &view, const WeaponEntry &weapon, unsigned experience,
    const MovieRegion &area, unsigned elapsed) {
    if (area.alpha <= 0) { return true; }
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_MASTERY");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    if (movie == nullptr) { return false; }
    const unsigned level = weapon.data.GetMasteryLevel(experience);
    unsigned target = movie->duration;
    if (level < kMaxMasteryLevel) {
        unsigned lower = 0;
        if (level > 0) { lower = weapon.data.GetMasteryThreshold(level - 1); }
        const unsigned upper = weapon.data.GetMasteryThreshold(level);
        if (upper <= lower) { return false; }
        const unsigned percent = static_cast<unsigned>((static_cast<std::uint64_t>(experience - lower) * 100) / (upper - lower));
        unsigned start = 0, end = 0;
        if (!movie->GetChapterRange(level + 1, start, end)) { return false; }
        target = start + percent * (end - start) / 100;
    }
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
            view.movies.DrawSprite(26, 5 + region.index - 2, elapsed, region.x + region.width / 2,
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
            if (category == 3) { state.Navigate(17); }
            else {
                state.shopCategory = category;
                state.shopScroll = 0;
                state.shopFilter = 0;
                state.selectedItem = -1;
                state.shopDetailOpen = false;
            }
        }
        x += width + kCategoryGap;
    }
}

/** Original sort rows for the selected category: ALL, OWNED and then the
 * categories this rebuild can actually filter on. The remaining power-up sort
 * keys have no matching classification in the parsed records yet. */
unsigned StoreFilterRows(unsigned category, const char *&table) {
    if (category == 1) { table = "MDS_BUTTON_STORE_SORT_ARMOR"; return 5; }
    if (category == 2) { table = "MDS_BUTTON_STORE_SORT_POWERUP"; return 2; }
    table = "MDS_BUTTON_STORE_SORT_GUNS";
    return 9;
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
    const bool modalOpen = state.shopDetailOpen || state.shopFilterOpen;
    DrawStoreCategories(view, categoryBar, state, !modalOpen);

    std::vector<unsigned> items;
    std::vector<unsigned> itemSlots;
    // The first column links to the local friend and currency flows, on every
    // category page. The starter bundle follows as the store's own first row.
    if (state.shopFilter == 0) {
        items.push_back(static_cast<unsigned>(store.size())); itemSlots.push_back(6);
        items.push_back(static_cast<unsigned>(store.size() + 1)); itemSlots.push_back(6);
        for (unsigned index = 0; index < store.size(); ++index) {
            if (store[index].data.singlePurchase == 0) { continue; }
            items.push_back(index);
            itemSlots.push_back(6);
            break;
        }
    }
    // CStoreItem's trailing int16 is the store's own row order; a negative value
    // keeps the record out of the list entirely.
    std::vector<std::pair<int, unsigned>> ordered;
    for (unsigned index = 0; index < store.size(); ++index) {
        if (store[index].data.displayOrder < 0) { continue; }
        ordered.push_back({store[index].data.displayOrder, index});
    }
    std::sort(ordered.begin(), ordered.end());
    for (const std::pair<int, unsigned> &row : ordered) {
        const unsigned index = row.second;
        unsigned slot = state.shopGunSlot;
        int category = -1;
        bool matches = false;
        if (state.shopCategory == 0) {
            matches = MatchesEquipmentSlot(store[index], slot, weapons, armors);
            if (matches) {
                const WeaponEntry *weapon = FindWeaponEntry(weapons, store[index].data.objects[0].object);
                if (weapon != nullptr) { category = weapon->category; }
            }
        } else if (state.shopCategory == 1) {
            for (slot = 2; slot < 5; ++slot) {
                if (MatchesEquipmentSlot(store[index], slot, weapons, armors)) { matches = true; category = slot - 2; break; }
            }
        } else {
            slot = 5;
            matches = MatchesEquipmentSlot(store[index], slot, weapons, armors);
        }
        if (!matches) { continue; }
        const unsigned categoryFilter = state.shopFilter & ~kOwnedFilterBit;
        if (categoryFilter != 0 && category >= 0 && (categoryFilter & (1u << category)) == 0) { continue; }
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
    MovieRegion firstSlot, secondSlot;
    if (!RequireRegion(view, storeScroll, kFirstColumnRegion, kScrollRestTime, firstSlot, "first column") ||
        !RequireRegion(view, storeScroll, kFirstColumnRegion + 1, kScrollRestTime, secondSlot, "second column")) { return false; }
    const float columnPitch = std::max(1.0f, secondSlot.x - firstSlot.x);
    const float maximumScroll = std::max(0.0f, (columns - 1.0f) * columnPitch);
    if (!modalOpen && view.MouseIn(content.x, content.y, content.width, content.height)) {
        state.shopScroll -= view.dragX;
        state.shopScroll -= view.window.TakeWheelDelta() * columnPitch;
    }
    state.shopScroll = std::clamp(state.shopScroll, 0.0f, maximumScroll);
    if (!view.window.IsLeftMouseDown()) {
        const float settled = std::round(state.shopScroll / columnPitch) * columnPitch;
        state.shopScroll += (settled - state.shopScroll) * 0.25f;
        if (std::abs(settled - state.shopScroll) < 0.5f) { state.shopScroll = settled; }
    }

    int purchaseIndex = -1;
    unsigned purchaseSlot = 0;
    int focusedColumn = -1, focusedRow = 0;
    const unsigned firstColumn = static_cast<unsigned>(std::max(0.0f, std::floor(state.shopScroll / columnPitch)));
    const bool listHover = view.MouseIn(content.x, content.y, content.width, content.height);
    // The belt has its own viewport; the content region alone cuts the second row.
    MovieRegion viewport;
    if (!RequireRegion(view, storeScroll, 0, kScrollRestTime, viewport, "belt viewport")) { return false; }
    view.Clip(content.x, viewport.y, content.width, viewport.height);
    for (unsigned offset = 0; offset < kDrawnColumns; ++offset) {
        const unsigned column = firstColumn + offset;
        if (column >= columns) { break; }
        MovieRegion slot;
        if (!view.movies.Region(storeScroll, kFirstColumnRegion + offset, kScrollRestTime, slot)) { continue; }
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
                view.movies.DrawSpriteFitted(5, promoSprite, 0, body.x, body.y, body.width, body.height);
                if (index != store.size()) {
                    // The money pile is art only; the original prints the words.
                    view.CenterText("GET FREE", body.x + body.width * 0.5f, body.y + 12, 6, 0.85f);
                    view.CenterText("WARBUCKS", body.x + body.width * 0.5f, body.y + 108, 6, 0.7f);
                }
                if (cardEnabled && view.Hit(body.x, body.y, body.width, body.height)) {
                    if (index == store.size()) { state.Navigate(9); } else { state.Navigate(17); }
                    view.EndClip();
                    return true;
                }
                continue;
            }
            const StoreEntry &item = store[index];
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
            if (slotKind < 5) { equipped = SameObject(Equipped(profile, slotKind), ref.object); }
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
    view.movies.Draw(storeScroll, kScrollRestTime);

    const GameObjectTypeRef *preview = nullptr;
    unsigned previewSlot = state.shopGunSlot;
    if (state.shopPreview && state.selectedItem >= 0 && state.selectedItem < static_cast<int>(store.size()) &&
        state.slot < 5) {
        preview = &store[state.selectedItem].data.objects[0];
        previewSlot = state.slot;
    }
    // The original lets the player turn the model by dragging it.
    if (!modalOpen && view.MouseIn(playerPanel.x, playerPanel.y, playerPanel.width, playerPanel.height)) {
        state.playerSpin += view.dragX * 0.012f;
    }
    if (!view.DrawEquippedPlayer(toc, tables, profile, weapons, armors, previewSlot, preview, &playerPanel,
        state.playerSpin)) { return false; }
    // MDS_BUTTON_STORE_GUN_SWAP is the round weapon slot toggle.
    const OriginalMenuEntry *swapEntry = OriginalMenuData("MDS_BUTTON_STORE_GUN_SWAP", 0);
    if (swapEntry != nullptr) {
        const unsigned sprite = swapEntry->sprites[0];
        view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, gunSwap.x, gunSwap.y, gunSwap.width, gunSwap.height);
    }
    const std::string slotLabel = std::to_string(state.shopGunSlot + 1);
    view.movies.Text(slotLabel, gunSwap.x + (gunSwap.width - view.movies.TextWidth(slotLabel, 6, 0.8f)) * 0.5f,
        gunSwap.y + gunSwap.height * 0.26f, 6, 0.8f);
    if (!modalOpen && view.Hit(gunSwap.x, gunSwap.y, gunSwap.width, gunSwap.height)) {
        state.shopGunSlot = 1 - state.shopGunSlot;
        state.shopDetailOpen = false;
    }

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
    if (!state.shopDetailOpen && view.Hit(sortButton.x, sortButton.y, sortButton.width, sortButton.height)) {
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
            if (row == 1) { bit = kOwnedFilterBit; }
            if (row >= 2) { bit = 1u << (row - 2); }
            bool selected = state.shopFilter == 0;
            if (bit != 0) { selected = (state.shopFilter & bit) != 0; }
            unsigned sprite = entry->sprites[1];
            if (selected) { sprite = entry->sprites[0]; }
            view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, optionX, y, optionLabel.width, optionLabel.height);
            PlateLabel(view, view.movies.NamedString(entry->strings[0]), optionX, y, optionLabel.width, optionLabel.height);
            const float touchX = optionX + optionTouch.x - optionLabel.x;
            const float touchY = y + optionTouch.y - optionLabel.y;
            if (!state.shopFilterOpen || !view.Hit(touchX, touchY, optionTouch.width, optionTouch.height)) { continue; }
            view.NotePress(optionPlate, optionX, y, optionLabel.width, optionLabel.height);
            if (bit == 0) { state.shopFilter = 0; } else { state.shopFilter ^= bit; }
            state.shopScroll = 0;
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
            if (view.movies.Region(storeScroll, kFirstColumnRegion, kScrollRestTime, slot)) {
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
        if (state.slot < 5) { equipped = SameObject(Equipped(profile, state.slot), ref.object); }
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
        state.message = PurchaseMessage(result);
        if (result == PurchaseResult::Purchased || result == PurchaseResult::Owned) {
            if (purchaseSlot < 5) { Equipped(profile, purchaseSlot) = item.data.objects[0].object; }
            if (!profile.SaveToDisk(savePath)) { return false; }
        }
    }
    return true;
}

/** The original mode medallions are MDS_BUTTON_MP_TOGGLE, archetype 8. */
bool DrawModeButton(GameMenu &view, unsigned mode, float x, float y, float width, float height) {
    const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_MP_TOGGLE", mode);
    const unsigned sprite = entry->sprites[0];
    view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, x, y, width, height);
    const std::string label = view.movies.NamedString(entry->strings[0]);
    const float scale = std::min(width * 0.97f / std::max(1.0f, view.movies.TextWidth(label, 0)), width / 235);
    view.CenterText(label, x + width * 0.5f, y + height * 0.58f, 0, scale);
    return view.Hit(x, y, width, height);
}

void DrawStarMap(GameMenu &view, MenuState &state) {
    // Relative positions follow the user's iOS overview. Pan uses different
    // depths so the distance between near and far planets changes with drag.
    constexpr float positions[6][3] = {{156, 526, 305}, {1080, 402, 245}, {820, 249, 235},
        {680, 580, 260}, {1430, 530, 270}, {1790, 300, 230}};
    constexpr float depth[] = {1.0f, 0.86f, 0.72f, 0.9f, 0.95f, 0.8f};
    if (state.page == 0 && state.modeSelected && view.MouseIn(0, 140, 1024, 520)) {
        state.starPanX = std::clamp(state.starPanX + view.dragX + view.window.TakeWheelDelta() * 95, -1380.0f, 220.0f);
        state.starPanY = std::clamp(state.starPanY + view.dragY, -140.0f, 150.0f);
    }
    view.Clip(0, 133, 1024, 626);
    for (unsigned index = 0; index < 6; ++index) {
        const float x = positions[index][0] + state.starPanX * depth[index];
        const float y = positions[index][1] + state.starPanY * depth[index];
        const float diameter = positions[index][2];
        view.DrawPlanet(index, x, y, diameter);
        if (state.planet == index) {
            view.movies.DrawFitted(22, 1600, x - 90, y - 90, 180, 180);
            view.movies.Rectangle(x + 110, y - 205, 230, 94, 0, 0, 0, 0.55f);
            view.CenterText(view.names[index], x + 225, y - 180, 1, 0.9f);
        }
        if (state.page != 0 || !state.modeSelected || !view.MouseIn(0, 140, 1024, 520)) { continue; }
        if (view.Hit(x - diameter * 0.45f, y - diameter * 0.45f, diameter * 0.9f, diameter * 0.9f)) {
            state.planet = index;
            state.missionScroll = 0;
            state.startingWave = -1;
            if (index == 5) { state.message = "UNKNOWN PLANET"; }
            else { state.Navigate(21); }
        }
    }
    view.EndClip();
    if (state.page == 0 && state.modeSelected && DrawModeButton(view, state.gameMode, 844, 633, 172, 124)) {
        state.Navigate(22);
    }
}

void DrawMissionBackdrop(GameMenu &view, MenuState &state) {
    view.DrawPlanet(state.planet, -25, 446, 380);
    view.movies.DrawSpriteFitted(0, 31, 0, -214, 252, 427, 389);
    view.movies.Rectangle(160, 202, 640, 131, 0, 0, 0, 0.55f);
    view.movies.DrawFitted(48, 1600, 160, 202, 640, 131);
    view.CenterText(view.names[state.planet], 480, 214, 0, 0.9f);
    view.Paragraph(170, 257, 620, view.descriptions[state.planet], 0.88f);
    const int backMovie = view.movies.FindMovie("GLU_MOVIE_BACK_BUTTON");
    if (backMovie >= 0) { view.movies.DrawFitted(static_cast<unsigned>(backMovie), 300, -31, 354, 135, 198); }
    view.movies.DrawSpriteFitted(0, 125, 0, -28, 391, 110, 110);
    view.CenterText("<<", 23, 429, 0, 1.1f);
    if (view.Hit(0, 403, 90, 92)) { state.Navigate(0, true); }
    if (DrawModeButton(view, state.gameMode, 844, 633, 172, 124)) { state.Navigate(22); }
}

/** Horizontal revolution/horde cards use the same two-dimensional sprites as iOS. */
void DrawRevolutions(GameMenu &view, MenuState &state, const CProfileManager &profile, bool interactive = true) {
    DrawMissionBackdrop(view, state);
    const bool horde = state.planet == 4;
    if (interactive && view.MouseIn(165, 350, 859, 280)) {
        state.missionScroll = std::clamp(state.missionScroll - view.dragX - view.window.TakeWheelDelta() * 130, 0.0f, 1805.0f);
    }
    view.Clip(170, 360, 854, 248);
    for (unsigned index = 0; index < 10; ++index) {
        const float x = 174 + index * 260.0f - state.missionScroll;
        if (x + 250 < 170 || x > 1024) { continue; }
        view.movies.DrawFitted(39, 0, x, 403, 250, 164);
        view.movies.Rectangle(x + 4, 407, 242, 156, 0, 0, 0);
        unsigned animation = 24 + index;
        if (horde) { animation = 42 + index; }
        view.movies.DrawSpriteFitted(5, animation, 0, x + 3, 410, 244, 150);
        std::string title = "REVOLUTION ";
        if (horde) { title = "HORDE "; }
        view.movies.Text(title + std::to_string(index + 1), x + 8, 408, 0, 0.8f);
        const bool unlocked = horde || index * 50 <= profile.clearedWaves[state.planet];
        if (!unlocked) { view.movies.DrawSpriteFitted(5, 19, 0, x + 4, 478, 160, 79); }
        if (interactive && view.MouseIn(170, 360, 854, 248) && view.Hit(x, 403, 250, 164)) {
            if (!unlocked) { state.message = "CLEAR THE PREVIOUS REVOLUTION TO UNLOCK"; continue; }
            state.revolution = index;
            state.hordeStart = index;
            state.startingWave = static_cast<int>(index * 50);
            state.wavePage = 0;
            state.missionScroll = 0;
            state.missionTab = 0;
            if (horde) { state.Navigate(23); }
            else { state.Navigate(19); }
        }
    }
    view.EndClip();
    if (interactive) {
        if (view.Button(175, 586, 55, 32, "<")) { state.missionScroll = std::max(0.0f, state.missionScroll - 260); }
        if (view.Button(944, 586, 55, 32, ">")) { state.missionScroll = std::min(1805.0f, state.missionScroll + 260); }
    }
}

/** Returns true only after an unlocked wave/horde is explicitly launched. */
bool DrawMissionDetails(GameMenu &view, MenuState &state, const CProfileManager &profile, bool activate) {
    DrawRevolutions(view, state, profile, false);
    view.movies.DrawFitted(39, 1300, 177, 222, 672, 365);
    view.movies.Rectangle(181, 226, 664, 357, 0, 0, 0);
    std::string title = "REVOLUTION ";
    if (state.planet == 4) { title = "HORDE "; }
    view.movies.Text(title + std::to_string(state.revolution + 1), 188, 230, 0, 0.85f);
    if (view.Button(799, 232, 38, 33, "X")) { state.Back(); return false; }
    if (state.planet == 4) {
        view.movies.DrawSpriteFitted(5, 42 + state.hordeStart, 0, 198, 303, 250, 164);
        view.Paragraph(480, 309, 338, "SURVIVE THE HORDE. THE ENEMIES KEEP COMING UNTIL YOUR BROS FALL.", 0.85f);
        view.Text(480, 421, "BEST KILLS " + std::to_string(profile.hordeBestKills[state.hordeStart]), 2);
        view.Text(480, 458, "BEST SCORE " + std::to_string(profile.hordeBestScore[state.hordeStart]), 2);
        return view.Button(545, 516, 265, 53, "PLAY", true) || activate;
    }
    constexpr const char *tabs[] = {"WAVES", "BRIEFING", "REQUIREMENTS"};
    for (unsigned tab = 0; tab < 3; ++tab) {
        const float x = 238 + tab * 183.0f;
        unsigned button = 71;
        if (state.missionTab == tab) { button = 70; }
        view.movies.DrawSpriteFitted(0, button, 0, x, 255, 175, 38);
        const float scale = std::min(0.9f, 167 / std::max(1.0f, view.movies.TextWidth(tabs[tab], 5)));
        view.CenterText(tabs[tab], x + 87.5f, 260, 5, scale);
        if (view.Hit(x, 255, 175, 38)) { state.missionTab = tab; }
    }
    const unsigned cleared = profile.clearedWaves[state.planet];
    if (state.missionTab == 0) {
        if (view.MouseIn(230, 302, 572, 205)) {
            const float wheel = view.window.TakeWheelDelta();
            state.missionScroll += -view.dragX;
            if (wheel < 0 || state.missionScroll > 75) { state.wavePage = std::min(4u, state.wavePage + 1); state.missionScroll = 0; }
            if (wheel > 0 || state.missionScroll < -75) { if (state.wavePage > 0) { --state.wavePage; } state.missionScroll = 0; }
        }
        for (unsigned cell = 0; cell < 10; ++cell) {
            const unsigned wave = state.revolution * 50 + state.wavePage * 10 + cell;
            if (view.WaveButton(255 + cell % 5 * 109.0f, 313 + cell / 5 * 105.0f, 80, 80, wave, cleared,
                state.startingWave == static_cast<int>(wave), profile.perfectedWaves[state.planet].test(wave))) {
                if (wave <= cleared) { state.startingWave = wave; }
                else { state.message = "CLEAR THE PREVIOUS WAVE TO UNLOCK"; }
            }
        }
        if (view.Button(228, 522, 47, 31, "<") && state.wavePage > 0) { --state.wavePage; }
        if (view.Button(766, 522, 47, 31, ">") && state.wavePage < 4) { ++state.wavePage; }
        view.movies.Rectangle(300, 536, 420, 7, 0.06f, 0.05f, 0.35f);
        view.movies.Rectangle(300 + state.wavePage * 84.0f, 536, 84, 5, 0.4f, 0.8f, 1);
    } else if (state.missionTab == 1) {
        view.Paragraph(230, 336, 563, view.descriptions[state.planet], 0.95f);
        view.CenterText("SURVIVE ALL 50 WAVES", 512, 473, 0, 0.85f);
    } else {
        view.CenterText("REVOLUTION " + std::to_string(state.revolution + 1), 512, 336);
        view.CenterText("CLEAR THE PREVIOUS WAVES TO ADVANCE", 512, 408, 1, 0.9f);
        view.CenterText(std::to_string(cleared) + " / 500 WAVES CLEARED", 512, 469, 0, 0.8f);
    }
    if (view.Button(697, 610, 270, 66, "PLAY", true) || activate) {
        return state.startingWave >= 0 && static_cast<unsigned>(state.startingWave) <= cleared;
    }
    return false;
}

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
    state.casualtyPage = 0;
    state.refineryTab = 0;
    state.refinementRequired = context.profile.xplodium != 0;
    state.message.clear();
    state.Navigate(27, true);
    for (const auto &ref : context.profile.configuration.guns) {
        const WeaponEntry *weapon = FindMasteryWeapon(weapons, ref);
        if (weapon != nullptr && weapon->data.GetMasteryLevel(context.profile.GetWeaponExperience(ref)) < 3) {
            state.masteryWeapon = ref;
            state.page = 26;
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
constexpr unsigned kUpgradeSettleTime = 750;
// The player headshots the menus print next to a title.
constexpr unsigned kBrotherPortrait = 161;
// The meter fills over this long when the page opens, like the original.
constexpr unsigned kMeterFillMs = 900;

/** The upgrade popup is reached from the store as well as from the results,
 * so closing it returns to whichever page pushed it. */
void CloseMastery(MenuState &state) {
    if (state.history.empty()) { state.page = 27; return; }
    state.Back();
}

/** How far into GLU_MOVIE_WEAPON_UPGRADE_MASTERY the meter stands for this
 * much experience. The movie's chapters are the three cells. */
unsigned MasteryMeterTime(GameMenu &view, const WeaponEntry &weapon, unsigned experience) {
    const unsigned meter = view.movies.Ordinal("GLU_MOVIE_WEAPON_UPGRADE_MASTERY");
    CMovie *movie = view.movies.GetMovie(meter);
    if (movie == nullptr) { return 0; }
    const unsigned level = weapon.data.GetMasteryLevel(experience);
    if (level >= kMaxMasteryLevel || movie->chapters.size() <= level) { return movie->duration; }
    unsigned lower = 0;
    if (level > 0) { lower = weapon.data.GetMasteryThreshold(level - 1); }
    const unsigned upper = weapon.data.GetMasteryThreshold(level);
    unsigned endTime = movie->duration;
    if (movie->chapters.size() > level + 1) { endTime = movie->chapters[level + 1]; }
    const unsigned startTime = movie->chapters[level];
    return startTime + static_cast<unsigned>((static_cast<std::uint64_t>(experience - lower) *
        (endTime - startTime - 1)) / std::max(1u, upper - lower));
}

/** CMenuUpgradePopup lists the stats this tier changes plus the critical-hit
 * tier, with the real values in both columns. */
bool DrawMastery(GameMenu &view, MenuState &state, CProfileManager &profile, CResTOCManager &toc,
    PackTables &tables, const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::filesystem::path &savePath) {
    const WeaponEntry *weapon = FindMasteryWeapon(weapons, state.masteryWeapon);
    const StoreEntry *item = FindWeaponStore(store, state.masteryWeapon);
    if (weapon == nullptr || item == nullptr) { CloseMastery(state); return true; }
    const unsigned popup = view.movies.Ordinal("GLU_MOVIE_UPGRADE_POPUP");
    MovieRegion portrait, close, meterArea, currentHeader, nextHeader, currentColumn, nextColumn;
    MovieRegion iconArea, titleArea, buyArea, nameArea;
    if (!RequireRegion(view, popup, kUpgradePortraitRegion, kUpgradeSettleTime, portrait, "portrait") ||
        !RequireRegion(view, popup, kUpgradeCloseRegion, kUpgradeSettleTime, close, "close") ||
        !RequireRegion(view, popup, kUpgradeMeterRegion, kUpgradeSettleTime, meterArea, "meter") ||
        !RequireRegion(view, popup, kUpgradeCurrentHeaderRegion, kUpgradeSettleTime, currentHeader, "current header") ||
        !RequireRegion(view, popup, kUpgradeNextHeaderRegion, kUpgradeSettleTime, nextHeader, "next header") ||
        !RequireRegion(view, popup, kUpgradeCurrentColumnRegion, kUpgradeSettleTime, currentColumn, "current column") ||
        !RequireRegion(view, popup, kUpgradeNextColumnRegion, kUpgradeSettleTime, nextColumn, "next column") ||
        !RequireRegion(view, popup, kUpgradeIconRegion, kUpgradeSettleTime, iconArea, "icon") ||
        !RequireRegion(view, popup, kUpgradeTitleRegion, kUpgradeSettleTime, titleArea, "title") ||
        !RequireRegion(view, popup, kUpgradeBuyRegion, kUpgradeSettleTime, buyArea, "buy") ||
        !RequireRegion(view, popup, kUpgradeNameRegion, kUpgradeSettleTime, nameArea, "name")) { return false; }

    const unsigned experience = profile.GetWeaponExperience(state.masteryWeapon);
    const unsigned level = weapon->data.GetMasteryLevel(experience);
    const unsigned nextLevel = std::min(kMaxMasteryLevel, level + 1);
    view.movies.Rectangle(0, 56, 1024, 712, 0, 0, 0);
    view.movies.Draw(popup, kUpgradeSettleTime);
    view.CenterText(view.movies.NamedString("IDS_UPGRADE_TITLE"), titleArea.x + titleArea.width * 0.5f,
        titleArea.y + (titleArea.height - 34) * 0.5f, 0, 1.25f);
    view.movies.DrawSpriteFitted(0, kBrotherPortrait + profile.playerBrother, 0,
        portrait.x, portrait.y, portrait.width, portrait.height);
    view.movies.DrawSpriteFitted(0, 99, 0, close.x, close.y, close.width, close.height);
    if (view.Hit(close.x, close.y, close.width, close.height)) { CloseMastery(state); }

    // The meter runs up to its real position when the page opens.
    if (state.masteryOpened == 0) { state.masteryOpened = view.clock; }
    const std::uint64_t shown = std::min<std::uint64_t>(view.clock - state.masteryOpened, kMeterFillMs);
    const unsigned target = MasteryMeterTime(view, *weapon, experience);
    const unsigned meterTime = static_cast<unsigned>(target * shown / kMeterFillMs);
    const unsigned meter = view.movies.Ordinal("GLU_MOVIE_WEAPON_UPGRADE_MASTERY");
    view.movies.Draw(meter, meterTime, meterArea.x + 6, meterArea.y + (meterArea.height - 42) * 0.5f);

    view.CenterText(view.movies.NamedString("IDS_UPGRADE_CURRENT_LEVEL_TITLE"),
        currentHeader.x + currentHeader.width * 0.5f, currentHeader.y + 6, 0, 0.95f);
    constexpr const char *nextTitles[] = {"IDS_UPGRADE_NEXT_LEVEL_TITLE_BRONZE",
        "IDS_UPGRADE_NEXT_LEVEL_TITLE_SILVER", "IDS_UPGRADE_NEXT_LEVEL_TITLE_GOLD"};
    view.CenterText(view.movies.NamedString(nextTitles[std::min(2u, level)]),
        nextHeader.x + nextHeader.width * 0.5f, nextHeader.y + 6, 0, 0.95f);
    view.CenterText(item->name, nameArea.x + nameArea.width * 0.5f, nameArea.y + 4, 1, 0.9f);
    view.Icon(toc, tables, *item, iconArea.x, iconArea.y, iconArea.width, iconArea.height);

    // Every stat the record carries, with the real numbers in both columns.
    // Stat 3 is a walking-speed percentage and keeps its sign.
    constexpr const char *statKeys[] = {"IDS_UPGRADE_POWER", "IDS_UPGRADE_DAMAGE", "IDS_UPGRADE_RPM", "IDS_UPGRADE_SPEED"};
    struct UpgradeRow { std::string title, current, next; };
    std::vector<UpgradeRow> rows;
    for (unsigned stat = 0; stat < 4; ++stat) {
        const auto &values = item->data.statGroups[stat];
        if (values.size() <= nextLevel) { continue; }
        std::string current = std::to_string(values[level]);
        std::string next = std::to_string(values[nextLevel]);
        if (stat == 3) {
            if (values[level] >= 0) { current = "+" + current; }
            if (values[nextLevel] >= 0) { next = "+" + next; }
            current += "%";
            next += "%";
        }
        rows.push_back({view.movies.NamedString(statKeys[stat]), current, next});
    }
    constexpr const char *criticalKeys[] = {"IDS_UPGRADE_CRITICAL_CHANCE_NONE", "IDS_UPGRADE_CRITICAL_CHANCE_LOW",
        "IDS_UPGRADE_CRITICAL_CHANCE_MED", "IDS_UPGRADE_CRITICAL_CHANCE_HIGH"};
    rows.push_back({view.movies.NamedString("IDS_UPGRADE_CRIT"), view.movies.NamedString(criticalKeys[level]),
        view.movies.NamedString(criticalKeys[nextLevel])});
    for (unsigned row = 0; row < rows.size(); ++row) {
        const float y = currentColumn.y + (row + 1) * currentColumn.height / (rows.size() + 1) - 11;
        view.movies.Text(rows[row].title, currentColumn.x + 10, y, 1, 0.78f);
        view.movies.Text(rows[row].current,
            currentColumn.x + currentColumn.width - 10 - view.movies.TextWidth(rows[row].current, 1, 0.78f), y, 1, 0.78f);
        view.movies.Text(rows[row].title, nextColumn.x + 10, y, 1, 0.78f);
        view.movies.Text(rows[row].next,
            nextColumn.x + nextColumn.width - 10 - view.movies.TextWidth(rows[row].next, 1, 0.78f), y, 1, 0.78f);
    }
    if (level < kMaxMasteryLevel) {
        const unsigned threshold = weapon->data.GetMasteryThreshold(level);
        if (GameHostSettings().debugMode) {
            view.CenterText(std::to_string(experience) + " / " + std::to_string(threshold) + " XP",
                512, buyArea.y - 30, 0, 0.72f);
        }
        // Store stat column 7 is the Warbuck price of the next tier.
        const auto &prices = item->data.statGroups[7];
        if (prices.size() > nextLevel && prices[nextLevel] >= 0) {
            const unsigned price = static_cast<unsigned>(prices[nextLevel]);
            const std::string label = view.movies.NamedString("IDS_UPGRADE_TITLE") + "   " + std::to_string(price);
            PlateLabel(view, label, buyArea.x, buyArea.y, buyArea.width, buyArea.height);
            view.movies.DrawSpriteFitted(kCurrencyCharacter, kWarbuckIcon, 0,
                buyArea.x + buyArea.width - 34, buyArea.y + (buyArea.height - 26) * 0.5f, 26, 26);
            if (view.Hit(buyArea.x, buyArea.y, buyArea.width, buyArea.height)) {
                if (profile.warbucks >= price) {
                    profile.warbucks -= price;
                    profile.AddWeaponExperience(state.masteryWeapon, threshold - experience, weapon->data.GetMasteryLimit());
                    if (!profile.SaveToDisk(savePath)) { return false; }
                    state.message = "WEAPON UPGRADED";
                    state.masteryOpened = view.clock;
                } else { state.message = "NOT ENOUGH WARBUCKS"; }
            }
        }
    }
    return true;
}

bool DrawPostGame(GameMenu &view, MenuState &state, CResTOCManager &toc, PackTables &tables) {
    view.movies.Draw(17, 3000);
    if (view.Tab(279, 140, 231, "OVERVIEW", state.page == 27)) { state.page = 27; }
    if (view.Tab(514, 140, 231, "CASUALTIES", state.page == 28)) { state.page = 28; }
    const auto &result = state.result;
    view.CenterText("REVOLUTION " + std::to_string(result.wave / 50 + 1) + "  WAVE " + std::to_string(result.wave % 50 + 1), 512, 204, 6, 1.5f);
    view.CenterText(std::to_string(result.waves) + " WAVES CLEARED", 555, 303, 0, 1.05f);
    view.CenterText(std::to_string(result.kills) + " TARGETS SERVICED", 555, 349, 0, 1.05f);
    if (state.page == 27) {
        const std::uint64_t values[] = {result.xplodium, result.experience, result.perfectWaves};
        constexpr unsigned icons[] = {0, 1, 4};
        constexpr float xs[] = {132, 545, 338}, ys[] = {484, 484, 626};
        for (unsigned index = 0; index < 3; ++index) {
            const float x = xs[index], y = ys[index];
            view.movies.DrawFitted(20, 900, x, y, 369, 125);
            const auto *entry = OriginalMenuData("MDS_ICON_POSTGAME", icons[index]);
            const unsigned sprite = entry->sprites[0];
            view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, x + 15, y + 21, 77, 77);
            view.CenterText(std::to_string(values[index]), x + 225, y + 20, 6, 1.6f);
            view.CenterText(view.movies.NamedString(entry->strings[0]), x + 225, y + 82, 0, 0.76f);
        }
    } else {
        const unsigned pages = std::max(1u, static_cast<unsigned>((result.casualties.size() + 2) / 3));
        if (view.Button(52, 580, 90, 48, "<") && state.casualtyPage > 0) { --state.casualtyPage; }
        if (view.Button(882, 580, 90, 48, ">") && state.casualtyPage + 1 < pages) { ++state.casualtyPage; }
        for (unsigned column = 0; column < 3; ++column) {
            const unsigned index = state.casualtyPage * 3 + column;
            if (index >= result.casualties.size()) { break; }
            const float x = 181 + column * 229.0f;
            view.movies.DrawFitted(19, 900, x, 484, 200, 242);
            if (!view.DrawCasualty(tables, toc, result.casualties[index], x)) { return false; }
        }
        if (result.casualties.empty()) { view.CenterText("NO CASUALTIES", 512, 576, 0, 1); }
    }
    if (view.Button(0, 388, 142, 68, "REFINE")) { state.Navigate(3, true); }
    return true;
}

/** Standard intervals occupy 6..11; premium intervals occupy 0..5. */
bool DrawRefinery(GameMenu &view, MenuState &state, CProfileManager &profile,
    const CRefinementManager::Template &data, const std::filesystem::path &savePath, std::int64_t now) {
    if (view.Tab(451, 143, 232, "STANDARD", state.refineryTab == 0)) { state.refineryTab = 0; }
    if (view.Tab(686, 143, 232, "PREMIUM", state.refineryTab == 1)) { state.refineryTab = 1; }
    view.movies.Draw(31, 850);
    view.movies.DrawSpriteFitted(0, 85, 0, 8, 233, 49, 54);
    view.movies.Text("XPLODIUM", 56, 233, 0, 1.02f);
    view.movies.Text(std::to_string(profile.xplodium), 56, 272, 0, 0.96f);
    view.Paragraph(4, 324, 268, "To continue, click a refinery and convert Xplodium into coins.\n\nUnlock refineries by collecting from the previous refinery.", 0.85f);
    for (unsigned cell = 0; cell < 6; ++cell) {
        unsigned index = cell + 6;
        if (state.refineryTab == 1) { index = cell; }
        const auto &slot = profile.refinery.slots[index];
        const float x = 341 + (cell % 3) * 222.0f;
        const float y = 294 + (cell / 3) * 218.0f;
        const unsigned minutes = data.minutes[index];
        std::string title = "GET COINS";
        if (minutes > 0 && minutes < 60) { title = std::to_string(minutes) + " MIN"; }
        if (minutes >= 60) { title = std::to_string(minutes / 60) + " HOURS"; }
        view.movies.DrawFitted(20, 900, x - 3, y - 49, 184, 51);
        view.CenterText(title, x + 88, y - 43, 0, 0.98f);
        view.CenterText("COINS " + std::to_string(data.efficiencyPercent[index]) + "%", x + 88, y - 20, 1, 0.87f);
        view.movies.DrawSpriteFitted(4, 0, 0, x - 12, y - 10, 200, 200);
        if (cell == 0) { view.movies.DrawSpriteFitted(4, 8, 0, x + 27, y + 28, 124, 124); }
        else if (slot.state == 0 && cell == 1) { view.movies.DrawSpriteFitted(4, 7, 0, x + 27, y + 28, 124, 124); }
        else {
            view.movies.DrawSpriteFitted(4, 18 + cell, 0, x + 6, y + 6, 164, 164);
            view.movies.DrawFitted(54, 1200, x + 6, y + 6, 164, 164);
        }
        std::string action = "REFINE";
        const bool offline = cell != 0 && !GameHostSettings().isConnected;
        if (offline) { action = "OFFLINE"; }
        else if (slot.state == 0) {
            action = "LOCKED";
            if (!profile.refinery.IsGated(index)) { action = "UNLOCK W " + std::to_string(data.rarePrice[index]); }
        } else if (slot.state == 2) {
            const auto seconds = std::max<std::int64_t>(0, slot.finishTime - now);
            char clock[32];
            std::snprintf(clock, sizeof(clock), "%02lld:%02lld:%02lld", seconds / 3600, seconds / 60 % 60, seconds % 60);
            action = clock;
        } else if (slot.state == 3) { action = "COLLECT"; }
        if (cell != 0 || slot.state == 2 || slot.state == 3) { view.CenterText(action, x + 88, y + 76, 0, 0.9f); }
        if (view.Hit(x, y - 49, 180, 225)) {
            if (offline) { state.message = "INTERNET CONNECTION REQUIRED"; continue; }
            bool changed = false;
            if (slot.state == 0) { changed = profile.refinery.UnlockSlot(index, profile.coins, profile.warbucks); }
            else if (slot.state == 1) {
                changed = profile.refinery.BeginRefinement(index, index, profile.xplodium, profile.xplodium, now);
                if (changed && profile.refinery.slots[index].state == 3) { profile.refinery.CollectResources(index, profile.coins); }
            } else if (slot.state == 3) { changed = profile.refinery.CollectResources(index, profile.coins); }
            if (changed) {
                if (profile.xplodium == 0) { state.refinementRequired = false; }
                if (!profile.SaveToDisk(savePath)) { return false; }
                state.message.clear();
            } else { state.message = "CHECK YOUR BALANCE OR COMPLETE THE PREVIOUS BATCH"; }
        }
    }
    return true;
}

/** Original offline movie and local, explicitly simulated social menu states. */
bool DrawSocial(GameMenu &view, MenuState &state, const CProfileManager &profile,
    CResTOCManager &toc, PackTables &tables, const std::vector<WeaponEntry> &weapons,
    const std::vector<ArmorEntry> &armors) {
    view.movies.Rectangle(0, 134, 1024, 634, 0, 0, 0);
    const bool bros = state.page == 4;
    bool modalClick = false;
    if (state.inviteOpen) { modalClick = view.ExchangeClick(false); }
    if (!GameHostSettings().isConnected) {
        view.movies.Draw(75, 1600);
        view.movies.DrawSpriteFitted(0, 116, 0, 655, 322, 196, 57);
        view.CenterText("RETRY", 753, 331, 6, 1.3f);
        if (view.Hit(655, 322, 196, 57)) {
            GameHostSettings().Load("gunbros.cfg");
            if (!GameHostSettings().isConnected) { state.message = "CONNECTION UNAVAILABLE"; }
        }
        const char *table = "MDS_OFFLINE_CHALLENGES";
        if (bros) { table = "MDS_OFFLINE_FRIENDS"; }
        const auto *entry = OriginalMenuData(table, 1);
        if (entry == nullptr) { return false; }
        std::istringstream words(view.movies.NamedString(entry->strings[0]));
        std::string word, line;
        float y = 397;
        while (words >> word) {
            if (!line.empty() && view.movies.TextWidth(line + " " + word, 0, 0.96f) > 429) {
                view.CenterText(line, 753, y, 0, 0.96f);
                line.clear();
                y += 23;
            }
            if (!line.empty()) { line += ' '; }
            line += word;
        }
        if (!line.empty()) { view.CenterText(line, 753, y, 0, 0.96f); }
        if (view.Button(681, 648, 225, 42, "LOCAL PREVIEW")) {
            GameHostSettings().isConnected = true;
            state.message = "LOCAL PREVIEW - NO NETWORK SERVICE";
        }
        return true;
    }
    if (bros) {
        // TODO: drive this from GLU_MOVIE_OFFLINE_BROHOOD once that page is
        // rebuilt from its own regions; these are the previous measurements.
        const MovieRegion brosPanel{0, 0, 548, 152, 476, 565};
        if (!view.DrawEquippedPlayer(toc, tables, profile, weapons, armors, 0, nullptr, &brosPanel)) { return false; }
        view.movies.Text("ON DUTY BRO", 34, 154, 0, 1.0f);
        view.movies.DrawFitted(73, 800, 11, 176, 565, 61);
        unsigned portrait = 0;
        std::string name = "PERCY GUN";
        if (profile.playerBrother == 1) { portrait = 1; name = "FRANCIS GUN"; }
        view.Clip(20, 181, 49, 49);
        view.movies.DrawSpriteFitted(19, portrait, 0, 8, 178, 75, 94);
        view.EndClip();
        view.movies.Text(name, 75, 185, 0, 0.9f);
        view.movies.Text("LOCAL BRO", 75, 211, 1, 0.9f);
        view.Clip(0, 305, 575, 20);
        view.movies.Draw(55, 500);
        view.EndClip();
        constexpr const char *tabs[] = {"BROTHERS", "BRO-BUFFS", "REWARDS"};
        for (unsigned tab = 0; tab < 3; ++tab) {
            if (view.Tab(10 + tab * 207.0f, 386, 180, tabs[tab], state.socialTab == tab)) { state.socialTab = tab; }
        }
        if (state.socialTab == 0) {
            if (view.Button(165, 466, 265, 47, "SELECT BRO")) { state.Navigate(29); }
        } else if (state.socialTab == 1) {
            view.CenterText("0 / 10 BRO-BUFFS", 290, 478, 0, 0.9f);
        } else if (view.Button(153, 466, 285, 47, "LOCAL REWARDS")) { state.Navigate(11); }
        view.movies.DrawSpriteFitted(6, 0, 0, 10, 650, 565, 65);
        if (view.Hit(10, 650, 565, 65)) { state.inviteOpen = true; }
        view.movies.DrawSpriteFitted(6, 17, 0, 675, 558, 303, 152);
    } else {
        view.movies.Draw(107, 1600);
        constexpr const char *tabs[] = {"BRO-OPS", "RECRUIT", "REQUESTS"};
        for (unsigned tab = 0; tab < 3; ++tab) {
            if (view.Tab(130 + tab * 258.0f, 185, 240, tabs[tab], state.socialTab == tab)) { state.socialTab = tab; }
        }
        if (state.socialTab == 0) {
            view.CenterText("BRO-OPS", 512, 305, 6, 1.3f);
            view.CenterText("0 ACTIVE ONLINE OPERATIONS", 512, 368, 0, 0.9f);
            if (view.Button(347, 485, 330, 55, "LOCAL ACTIVITIES")) { state.Navigate(11); }
        } else {
            view.CenterText("NO REQUESTS", 512, 380, 0, 1.0f);
            if (view.Button(362, 484, 300, 55, "INVITE FRIENDS")) { state.inviteOpen = true; }
        }
    }
    if (state.inviteOpen) {
        view.ExchangeClick(modalClick);
        view.movies.Rectangle(0, 134, 1024, 634, 0, 0, 0, 0.75f);
        view.movies.Draw(111, 1100);
        view.CenterText("INVITE FRIENDS", 512, 266, 6, 1.2f);
        view.CenterText("FACEBOOK / GAME CENTER", 512, 345, 0, 0.9f);
        view.CenterText("LOCAL PREVIEW - SERVICE UNAVAILABLE", 512, 388, 0, 0.7f);
        if (view.Button(370, 479, 284, 54, "CLOSE")) { state.inviteOpen = false; }
        view.ExchangeClick(false);
    }
    return true;
}

void DrawOptions(GameMenu &view, MenuState &state, CProfileManager &profile, bool &saveChanged) {
    view.movies.Rectangle(0, 134, 1024, 634, 0, 0, 0);
    view.movies.Draw(86, 1600);
    const float wheel = view.window.TakeWheelDelta();
    if (view.MouseIn(0, 260, 430, 430)) {
        state.optionsScroll = std::clamp(state.optionsScroll - view.dragY - wheel * 85, 0.0f, 595.0f);
    }
    std::string labels[] = {"SFX OFF", "MUSIC OFF", "HELP", "AUTOBRO ASK", "CHALLENGE REQUESTS", "NOTIFICATIONS",
        "SELECT BRO", "SAVE GAME", "FACEBOOK", "ABOUT", "QUIT GAME"};
    if (profile.soundEnabled) { labels[0] = "SFX ON"; }
    if (profile.musicEnabled) { labels[1] = "MUSIC ON"; }
    if (profile.brotherEnabled) { labels[3] = "AUTOBRO ON"; }
    constexpr const char *titles[] = {"SOUND EFFECTS", "MUSIC", "HELP", "AUTOMATIC BRO", "CHALLENGE REQUESTS", "NOTIFICATIONS",
        "SELECT BRO", "SAVE STATUS", "FACEBOOK", "ABOUT", "QUIT GAME"};
    constexpr const char *bodies[] = {"Turns sound effects on or off.", "Turns music on or off.", "Learn the controls, weapons and game modes.",
        "Choose whether your local bro automatically joins your games.", "View challenges from your brotherhood.",
        "Online notifications require the original network service.", "Choose your Gun Brother.",
        "Your progress is saved on this computer.", "View your local account and brotherhood.", "Gun Bros game information.", "Save your game and quit."};
    view.Clip(30, 307, 391, 377);
    for (unsigned index = 0; index < 11; ++index) {
        const float y = 326 + index * 85.0f - state.optionsScroll;
        if (y + 67 < 307 || y > 684) { continue; }
        const float relative = (y - 326) / 85;
        float x = 135;
        if (relative > 0.5f && relative < 1.5f) { x = 152; }
        if (relative > 2.5f) { x = 78; }
        if (view.MouseIn(x, y, 220, 67)) { state.optionsFocus = index; }
        if (view.Button(x, y, 220, 67, "", state.optionsFocus == index)) {
            if (index == 0) { profile.soundEnabled = !profile.soundEnabled; saveChanged = true; }
            if (index == 1) { profile.musicEnabled = !profile.musicEnabled; saveChanged = true; }
            if (index == 2) { state.Navigate(8); state.detail = 0; }
            if (index == 3) { profile.brotherEnabled = !profile.brotherEnabled; saveChanged = true; }
            if (index == 4) { state.Navigate(5); }
            if (index == 5) { state.message = "ONLINE NOTIFICATIONS UNAVAILABLE"; }
            if (index == 6) { state.Navigate(29); }
            if (index == 7) { saveChanged = true; state.message = "GAME SAVED"; }
            if (index == 8) { state.Navigate(9); }
            if (index == 9) { state.Navigate(12); }
            if (index == 10) { state.Navigate(15); }
        }
        const float scale = std::min(1.0f, 200 / std::max(1.0f, view.movies.TextWidth(labels[index], 0)));
        view.movies.Text(labels[index], x + 10, y + 24, 0, scale);
    }
    view.EndClip();
    view.movies.Text(titles[state.optionsFocus], 442, 163, 6, 1.2f);
    view.Paragraph(445, 242, 555, bodies[state.optionsFocus], 0.88f);
}

/** Returns selected planet, -1 for quit, -2 after capture, -3 on failure. */
int ShowGameMenu(CResTOCManager &toc, PackTables &tables, CProfileManager &profile,
    const CPlayerProgress::Template &progressData, const CRefinementManager::Template &refinement,
    const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::vector<ArmorEntry> &armors, MenuState &state, const std::filesystem::path &savePath,
    const std::string &capturePath, const std::vector<MenuTestClick> *testClicks = nullptr, bool originalProfile = false) {
    GameMenu view;
    if (!view.Open(toc, tables)) { return -3; }
    view.animateNavigation = capturePath.empty();
    view.scripted = testClicks != nullptr;
    // A new view has a new clock (including deterministic capture sessions).
    state.shopFilterBound = false;
    CBGM music;
    if (!music.Play(0)) { return -3; }
    music.SetEnabled(profile.musicEnabled);
    CAudioPlayer::SetEffectsEnabled(profile.soundEnabled);
    CPlayerProgress progress;
    progress.Bind(progressData);
    progress.SetExperience(profile.experience);
    unsigned testFrame = 0;
    std::uint64_t testClock = 0;
    std::uint64_t frameTicks = view.window.GetTicksMs();
    float smoothFrameMs = 16.7f;
    CDailyBonusTracking daily;
    if (!daily.Load(toc, tables)) { return -3; }
    while (view.window.PumpEvents()) {
        const auto ticks = view.window.GetTicksMs();
        smoothFrameMs = smoothFrameMs * 0.9f + static_cast<float>(ticks - frameTicks) * 0.1f;
        frameTicks = ticks;
        if (testClicks != nullptr && testFrame < testClicks->size()) { testClock += (*testClicks)[testFrame].advanceMs; }
        std::uint64_t menuClock = ticks;
        if (testClicks != nullptr) { menuClock = testClock; }
        view.clock = menuClock;
        music.Update();
        bool activate = false;
        const unsigned previousPage = state.page;
        for (std::string cheat = view.window.TakeCheatCode(); !cheat.empty(); cheat = view.window.TakeCheatCode()) {
            if (cheat == "chm") { profile.coins += 5000; profile.warbucks += 500; state.message = "COINS +5000 / WARBUCKS +500"; }
            if (cheat == "cht") { ++profile.dailyDayOffset; state.Navigate(24); }
            if (cheat == "chd") { GameHostSettings().debugMode = !GameHostSettings().debugMode; }
            if (cheat == "chc") { GameHostSettings().isConnected = !GameHostSettings().isConnected; }
            if (cheat == "chh") { state.Navigate(24); }
            if (cheat == "chw") { profile.clearedWaves.fill(500); state.message = "ALL WAVES UNLOCKED"; }
            if (!profile.SaveToDisk(savePath)) { return -3; }
            std::printf("[cheat] %s\n", cheat.c_str());
        }
        for (KeyCode key = view.window.TakeKeyPress(); key != KeyCode::None; key = view.window.TakeKeyPress()) {
            if (state.currencyPending) { continue; }
            if (state.page >= 26 || state.refinementRequired) {
                if (key == KeyCode::Escape) {
                    if (state.page == 26) { state.page = 27; }
                    else if (state.page >= 27) { state.page = 3; }
                }
                continue;
            }
            if (key == KeyCode::Space || key == KeyCode::Enter) { activate = true; }
            if (key == KeyCode::Escape) {
                if (state.page != 0) { state.Back(); }
                else { state.Navigate(15); }
            }
            if (state.page == 0 && key == KeyCode::Down) { state.planet = (state.planet + 1) % 4; }
            if (state.page == 0 && key == KeyCode::Up) { state.planet = (state.planet + 3) % 4; }
            if (key == KeyCode::P) { state.Navigate(0); }
            if (key == KeyCode::E) { state.Navigate(1); }
            if (key == KeyCode::B) { state.Navigate(2); }
            if (key == KeyCode::F) { state.Navigate(3); }
            if (key == KeyCode::Q && state.page == 2) { state.shopGunSlot = 1 - state.shopGunSlot; state.shopDetailOpen = false; }
        }
        if (previousPage != state.page) { state.itemPage = 0; state.selectedItem = -1; state.message.clear(); }
        const std::int64_t now = CurrentSeconds();
        profile.refinery.UpdateRefinement(now);
        view.Begin(state.page);
        if (testClicks != nullptr && testFrame < testClicks->size()) { view.SetTestClick((*testClicks)[testFrame]); }
        if (state.page == 26 && !DrawMastery(view, state, profile, toc, tables, store, weapons, savePath)) { return -3; }
        if ((state.page == 27 || state.page == 28) && !DrawPostGame(view, state, toc, tables)) { return -3; }
        if (state.page == 24) {
            const auto day = LocalCalendarDay();
            if (daily.IsBonusAvailable(profile, day)) {
                if (!daily.CommitBonus(profile, day, store) || !profile.SaveToDisk(savePath)) { return -3; }
                progress.SetExperience(profile.experience);
            }
            view.movies.Rectangle(0, 132, 1024, 627, 0, 0, 0);
            view.movies.Draw(106, 1600);
            const unsigned rewardDay = (profile.dailyConsecutiveDays - 1) % static_cast<unsigned>(daily.prizes.size());
            for (const auto &region : view.movies.Regions(106, 1600)) {
                if (region.index <= 2) {
                    constexpr const char *titles[] = {"PLAY GUN BROS EVERY DAY!", "BRO-OPS", "BROTHERHOOD"};
                    const float scale = std::min(1.1f, (region.width - 10) / std::max(1.0f, view.movies.TextWidth(titles[region.index], 6)));
                    view.CenterText(titles[region.index], region.x + region.width * 0.5f, region.y, 6, scale);
                } else if (region.index == 3 || region.index == 4) {
                    unsigned entryIndex = 3;
                    unsigned targetPage = 5;
                    if (region.index == 4) { entryIndex = 2; targetPage = 4; }
                    const unsigned sprite = OriginalMenuData("MDS_BUTTON_TRUNK", entryIndex)->sprites[0];
                    view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, region.x, region.y, region.width, region.height);
                    if (view.Hit(region.x, region.y, region.width, region.height)) { state.Navigate(targetPage); }
                } else if (region.index >= 5 && region.index <= 8) {
                    constexpr const char *labels[] = {"0 BRO REQUESTS", "8 OPS AVAILABLE", "0 REWARDS AVAILABLE", "0/10 BRO-BUFFS"};
                    view.movies.Text(labels[region.index - 5], region.x, region.y, 0, 0.8f);
                } else if (region.index >= 9 && region.index <= 13) {
                    const DailyPrize &prize = daily.prizes[region.index - 9];
                    StoreEntry imageEntry;
                    imageEntry.data.assets[1] = prize.image;
                    view.Icon(toc, tables, imageEntry, region.x, region.y, region.width, region.height);
                    unsigned quantity = prize.coins;
                    if (prize.warbucks != 0) { quantity = prize.warbucks; }
                    view.CenterText("X" + std::to_string(quantity), region.x + region.width * 0.5f, region.y + region.height - 19, 0, 0.9f);
                } else if (region.index >= 14 && region.index <= 18 && region.index - 14 <= rewardDay) {
                    view.movies.DrawSpriteFitted(7, 1, 600, region.x, region.y, region.width, region.height);
                }
            }
        }
        if (state.page == 0 || state.page == 22) {
            DrawStarMap(view, state);
            if (!state.modeSelected || state.page == 22) {
                view.movies.Rectangle(0, 134, 1024, 625, 0, 0, 0, 0.45f);
                for (unsigned mode = 0; mode < 3; ++mode) {
                    if (DrawModeButton(view, mode, 137 + mode * 264.0f, 300, 225, 170)) {
                        state.gameMode = mode;
                        state.modeSelected = true;
                        state.Navigate(0, true);
                        if (mode != 0) { state.message = "LOCAL PREVIEW - NETWORK MATCHMAKING UNAVAILABLE"; }
                    }
                }
            }
        }
        if (state.page == 21) { DrawRevolutions(view, state, profile); }
        if (state.page == 19 || state.page == 23) {
            if (DrawMissionDetails(view, state, profile, activate)) { return static_cast<int>(state.planet); }
        }
        if (state.page == 20) {
            view.Text(360, 174, "SELECT PLANET", 3.2f);
            for (unsigned planet = 0; planet < 4; ++planet) {
                const float x = 145 + planet * 244.0f;
                float diameter = 180;
                if (state.planet == planet) { diameter = 226; }
                view.DrawPlanet(planet, x, 340, diameter);
                if (view.Hit(x - 114, 221, 228, 257)) {
                    state.planet = planet;
                    state.startingWave = -1;
                }
                if (view.Button(x - 115, 465, 230, 43, view.names[planet], state.planet == planet)) {
                    state.planet = planet;
                    state.startingWave = -1;
                }
            }
            const unsigned cleared = profile.clearedWaves[state.planet];
            unsigned nextWave = cleared;
            if (cleared >= 500) { nextWave = 0; }
            if (state.startingWave >= 0) { nextWave = static_cast<unsigned>(state.startingWave); }
            const unsigned revolution = nextWave / 50;
            if (view.Button(48, 523, 36, 34, "<") && revolution > 0) {
                state.startingWave = static_cast<int>((revolution - 1) * 50);
            }
            view.Text(92, 532, "REVOLUTION " + std::to_string(revolution + 1) + " / 10", 1.8f);
            if (view.Button(314, 523, 36, 34, ">") && revolution < 9) {
                const unsigned start = (revolution + 1) * 50;
                if (start <= cleared) { state.startingWave = static_cast<int>(start); }
                else { state.message = "CLEAR THE PREVIOUS REVOLUTION TO UNLOCK"; }
            }
            if (view.Button(388, 519, 238, 39, "SELECT WAVE")) { state.Navigate(19); }
            for (unsigned wave = 0; wave < 50; ++wave) {
                const float x = 50 + (wave % 25) * 37.0f;
                const float y = 569 + (wave / 25) * 38.0f;
                const unsigned absolute = revolution * 50 + wave;
                if (view.WaveButton(x, y, 33, 32, absolute, cleared, absolute == nextWave)) {
                    if (absolute <= cleared) { state.startingWave = static_cast<int>(absolute); }
                    else { state.message = "CLEAR THE PREVIOUS WAVE TO UNLOCK"; }
                }
            }
            view.Text(50, 660, std::to_string(cleared) + " / 500 WAVES CLEARED", 1.8f);
            if (view.Button(720, 660, 250, 66, "PLAY", true) || activate) { return static_cast<int>(state.planet); }
            if (view.Button(50, 709, 180, 39, "LOADOUT")) { state.Navigate(1); }
            if (view.Button(248, 709, 180, 39, "LEADERBOARDS")) { state.Navigate(10); }
        }

        if (state.page == 2) {
            if (!DrawStore(view, toc, tables, profile, progress.GetLevel(), store, weapons, armors, state, savePath)) { return -3; }
        }
        if (state.page == 1) {
            view.Text(35, 176, kPageNames[state.page], 2.8f);
            for (unsigned slot = 0; slot < 6; ++slot) {
                if (view.Button(240 + slot * 127.0f, 166, 121, 38, kSlotNames[slot], state.slot == slot)) {
                    state.slot = slot;
                    state.itemPage = 0;
                    state.selectedItem = -1;
                }
            }
            std::vector<unsigned> items;
            for (unsigned index = 0; index < store.size(); ++index) {
                if (!MatchesEquipmentSlot(store[index], state.slot, weapons, armors)) { continue; }
                const GameObjectTypeRef &ref = store[index].data.objects[0];
                if (state.page == 1) {
                    if (state.slot == 5 && profile.GetPowerupCount(ref.object) == 0) { continue; }
                    if (state.slot < 5 && !profile.Owns(ref.type, ref.object)) { continue; }
                }
                items.push_back(index);
            }
            unsigned pages = static_cast<unsigned>((items.size() + 8) / 9);
            if (state.page == 1 && state.slot < 5 && state.selectedItem < 0) {
                for (unsigned position = 0; position < items.size(); ++position) {
                    if (SameObject(Equipped(profile, state.slot), store[items[position]].data.objects[0].object)) {
                        state.selectedItem = static_cast<int>(items[position]);
                        state.itemPage = position / 9;
                        break;
                    }
                }
            }
            if (pages == 0) { pages = 1; }
            if (state.itemPage >= pages) { state.itemPage = pages - 1; }
            const float wheel = view.window.TakeWheelDelta();
            if (wheel < 0 && state.itemPage + 1 < pages) { ++state.itemPage; }
            if (wheel > 0 && state.itemPage > 0) { --state.itemPage; }
            bool selectedVisible = false;
            for (unsigned position = state.itemPage * 9; position < items.size() && position < (state.itemPage + 1) * 9; ++position) {
                if (state.selectedItem == static_cast<int>(items[position])) { selectedVisible = true; }
            }
            if (!selectedVisible && !items.empty()) { state.selectedItem = static_cast<int>(items[state.itemPage * 9]); }
            for (unsigned offset = 0; offset < 9 && state.itemPage * 9 + offset < items.size(); ++offset) {
                const unsigned index = items[state.itemPage * 9 + offset];
                const StoreEntry &item = store[index];
                const float x = 240 + (offset % 3) * 253.0f;
                const float y = 220 + (offset / 3) * 122.0f;
                if (view.Button(x, y, 239, 110, "", state.selectedItem == static_cast<int>(index))) { state.selectedItem = static_cast<int>(index); }
                view.Icon(toc, tables, item, x + 10, y + 11, 66, 66);
                std::string name = item.name;
                if (name.size() > 21) { name = name.substr(0, 19) + ".."; }
                view.Text(x + 82, y + 20, name, 1.15f);
                const GameObjectTypeRef &ref = item.data.objects[0];
                std::string price = ItemPrice(item.data);
                if (profile.Owns(ref.type, ref.object)) { price = "OWNED"; }
                if (state.slot < 5 && SameObject(Equipped(profile, state.slot), ref.object)) { price = "EQUIPPED"; }
                view.Text(x + 82, y + 53, price, 1.25f, 0.93f, 0.74f, 0.33f);
                if (state.slot == 5) {
                    view.Text(x + 82, y + 80, "PACK " + std::to_string(item.data.objects.size()) + " / OWN " +
                        std::to_string(profile.GetPowerupCount(ref.object)), 1.2f);
                }
                if (item.data.requiredLevel > 1) { view.Text(x + 82, y + 80, "LEVEL " + std::to_string(item.data.requiredLevel), 1.2f); }
            }
            if (view.Button(240, 591, 95, 32, "PREV") && state.itemPage > 0) { --state.itemPage; }
            view.Text(354, 601, std::to_string(state.itemPage + 1) + " / " + std::to_string(pages), 1.5f);
            if (view.Button(471, 591, 95, 32, "NEXT") && state.itemPage + 1 < pages) { ++state.itemPage; }
            const GameObjectTypeRef *previewItem = nullptr;
            if (state.page == 2 && state.slot < 5 && state.selectedItem >= 0 &&
                state.selectedItem < static_cast<int>(store.size()) && !items.empty()) {
                previewItem = &store[state.selectedItem].data.objects[0];
            }
            if (!view.DrawEquippedPlayer(toc, tables, profile, weapons, armors, state.slot, previewItem)) { return -3; }
            if (state.selectedItem >= 0 && state.selectedItem < static_cast<int>(store.size()) && !items.empty()) {
                const StoreEntry &selected = store[state.selectedItem];
                view.Text(240, 647, selected.name, 1.8f);
                const auto &ref = selected.data.objects[0];
                if (ref.type == 2) {
                    for (const ArmorEntry &entry : armors) {
                        if (entry.packHash != ref.object.packHash || entry.ordinal != ref.object.localIndex) { continue; }
                        CArmor attributes;
                        attributes.Bind(entry.data);
                        attributes.Equip();
                        char stats[160];
                        std::snprintf(stats, sizeof(stats), "DEF %+d%%  ATK %+d%%  MOVE %+d%%  XP %+d%%  X %+d%%",
                            attributes.GetAttribute(0), attributes.GetAttribute(1), attributes.GetAttribute(2),
                            attributes.GetAttribute(3), attributes.GetAttribute(4));
                        view.Text(240, 674, stats, 1.22f);
                        break;
                    }
                }
                std::string action = "BUY + EQUIP";
                if (profile.Owns(ref.type, ref.object)) { action = "EQUIP"; }
                if (state.slot == 5) { action = "BUY PACK"; }
                if (state.slot == 5 && state.page == 1) {
                    view.Text(240, 683, "IN GAME: G USE ITEM / F SELECT NEXT", 1.6f);
                } else if (view.Button(780, 631, 213, 55, action, true) || activate) {
                    const PurchaseResult result = profile.AcquireItem(selected.data, progress.GetLevel());
                    state.message = PurchaseMessage(result);
                    if (result == PurchaseResult::Purchased || result == PurchaseResult::Owned) {
                        if (state.slot < 5) { Equipped(profile, state.slot) = ref.object; }
                        else { state.message = "ITEMS ADDED - G USE / F SELECT IN GAME"; }
                        if (!profile.SaveToDisk(savePath)) { return -3; }
                    }
                }
            }
        }

        if (state.page == 3) {
            if (!DrawRefinery(view, state, profile, refinement, savePath, now)) { return -3; }
        }
        if ((state.page >= 4 && state.page <= 18) || state.page == 25 || state.page == 29) {
            view.BodyPanel();
            bool saveChanged = false;
            if (state.page == 4 || state.page == 5) {
                if (!DrawSocial(view, state, profile, toc, tables, weapons, armors)) { return -3; }
            } else if (state.page == 29) {
                view.Text(350, 207, "CHOOSE YOUR BRO", 3);
                std::string network = "OFFLINE / LOCAL BRO AVAILABLE";
                if (GameHostSettings().isConnected) { network = "CONNECTED PREVIEW / LOCAL BRO AVAILABLE"; }
                view.CenterText(network, 512, 244, 0, 0.65f);
                view.movies.DrawSpriteFitted(19, 0, 0, 245, 266, 220, 275);
                view.movies.DrawSpriteFitted(19, 1, 0, 565, 266, 220, 275);
                if (view.Button(235, 553, 235, 48, "PERCY GUN", profile.playerBrother == 0)) { profile.playerBrother = 0; saveChanged = true; }
                if (view.Button(555, 553, 235, 48, "FRANCIS GUN", profile.playerBrother == 1)) { profile.playerBrother = 1; saveChanged = true; }
                std::string companion = "BROTHER: OFF";
                if (profile.brotherEnabled) { companion = "BROTHER: ON"; }
                if (view.Button(370, 624, 280, 45, companion, profile.brotherEnabled)) { profile.brotherEnabled = !profile.brotherEnabled; saveChanged = true; }
                view.Paragraph(65, 254, 155, "Your bro joins you in every survival battle with his own default armor, pistol and rifle.", 0.68f);
                if (view.Button(820, 296, 145, 48, "INVITE BROS")) { state.Navigate(9); }
                if (view.Button(820, 356, 145, 48, "BRO GIFTS")) { state.Navigate(11); }
            } else if (state.page == 11) {
                std::string heading = "BRO-OPS";
                if (state.page == 11) { heading = "ACHIEVEMENTS"; }
                view.Text(360, 202, heading, 3);
                view.Text(305, 238, "OFFLINE ACTIVITIES / ONE-TIME REWARDS", 1.5f);
                for (unsigned index = 0; index < 8; ++index) {
                    const float y = 277 + index * 49.0f;
                    view.Text(115, y + 9, kActivityNames[index], 1.8f);
                    view.Text(453, y + 9, std::to_string(profile.ActivityProgress(index)) + " / " + std::to_string(CProfileManager::ActivityTarget(index)), 1.8f);
                    std::string action = "VIEW";
                    if ((profile.claimedActivities & (1u << index)) != 0) { action = "CLAIMED"; }
                    else if (profile.ActivityProgress(index) >= CProfileManager::ActivityTarget(index)) { action = "COLLECT"; }
                    if (view.Button(665, y, 233, 40, action)) {
                        if (profile.ClaimActivity(index)) { saveChanged = true; state.message = "REWARD COLLECTED"; }
                        else { state.detail = index; state.Navigate(13); }
                    }
                }
            } else if (state.page == 6) {
                DrawOptions(view, state, profile, saveChanged);
            } else if (state.page == 7) {
                view.Text(350, 210, "GAME CENTER", 3);
                if (view.Button(230, 275, 560, 60, "SURVIVAL / FOUR PLANETS")) { state.Navigate(0); }
                if (view.Button(230, 350, 560, 60, "BOKOR / HORDE")) { state.Navigate(16); }
                if (view.Button(230, 425, 560, 60, "LEADERBOARDS")) { state.Navigate(10); }
                if (view.Button(230, 500, 560, 60, "ACHIEVEMENTS")) { state.Navigate(11); }
                if (view.Button(230, 575, 560, 60, "MY ACCOUNT")) { state.Navigate(9); }
            } else if (state.page == 8) {
                const OriginalMenuEntry *help = OriginalMenuData("MDS_HELP", state.detail);
                if (help == nullptr) { return -3; }
                view.Text(95, 220, view.movies.NamedString(help->strings[0]), 2.7f);
                std::string body = view.movies.NamedString(help->strings[1]);
                if (state.detail == 0) { body = "WASD: move. Aim with the mouse and hold the left button to fire. 1 / 2: switch weapons. G: use your selected item. F: select the next item. Esc / Space: pause. Your bro follows you and attacks enemies automatically."; }
                view.Paragraph(95, 285, 820, body, 0.85f);
                if (view.Button(110, 620, 170, 45, "PREVIOUS")) { state.detail = (state.detail + 13) % 14; }
                view.Text(464, 631, std::to_string(state.detail + 1) + " / 14", 1.8f);
                if (view.Button(740, 620, 170, 45, "NEXT")) { state.detail = (state.detail + 1) % 14; }
            } else if (state.page == 9) {
                view.Text(345, 213, "MY BROTHERHOOD", 3);
                view.Text(115, 293, "LOCAL PLAYER", 2.7f);
                view.Text(115, 335, "LEVEL " + std::to_string(progress.GetLevel()), 2.2f);
                view.Paragraph(115, 391, 740, "Your local account is ready. Your equipment, survival progress and rewards are saved on this computer. Your default bro can join every battle.", 0.85f);
                if (view.Button(115, 534, 355, 55, "SELECT BRO")) { state.Navigate(29); }
                if (view.Button(550, 534, 355, 55, "MY ACHIEVEMENTS")) { state.Navigate(11); }
                if (view.Button(335, 615, 355, 48, "CONTINUE")) { state.Navigate(0); }
            } else if (state.page == 10) {
                view.Text(310, 214, "LOCAL LEADERBOARDS", 3);
                view.Text(122, 275, "PLANET", 2);
                view.Text(586, 275, "WAVES", 2);
                view.Text(770, 275, "KILLS", 2);
                for (unsigned planet = 0; planet < 4; ++planet) {
                    const float y = 337 + planet * 65.0f;
                    view.Text(120, y, view.names[planet], 2);
                    view.Text(590, y, std::to_string(profile.clearedWaves[planet]), 2);
                    view.Text(775, y, std::to_string(profile.enemyKills[planet]), 2);
                }
                view.Text(122, 629, "YOUR PERSONAL BEST / SAVED AFTER EACH WAVE", 1.8f);
            } else if (state.page == 12) {
                view.Text(363, 221, "GUN BROS", 4);
                view.Paragraph(130, 330, 750, "Original game and artwork: Glu Mobile.\nWindows reconstruction using the original game resources.\nMove, shoot, survive and watch your bro's back.\nOriginal game data: iOS 3.6.0.", 1);
                view.Text(130, 576, "CONTROLS AND GAME RULES ARE IN HELP", 1.8f);
            } else if (state.page == 13) {
                view.Text(170, 222, kActivityNames[state.detail], 3);
                view.Paragraph(170, 332, 684, kActivityDescriptions[state.detail], 1);
                view.Text(170, 434, "PROGRESS " + std::to_string(profile.ActivityProgress(state.detail)) + " / " +
                    std::to_string(CProfileManager::ActivityTarget(state.detail)), 2.5f);
                view.Text(170, 495, "OFFLINE REWARD: COINS + WARBUCKS", 1.8f);
                if (view.Button(170, 587, 280, 60, "PLAY")) { state.Navigate(0); }
                if (view.Button(575, 587, 280, 60, "COLLECT")) {
                    if (profile.ClaimActivity(state.detail)) { saveChanged = true; state.message = "REWARD COLLECTED"; }
                    else { state.message = "COMPLETE THIS ACTIVITY BEFORE COLLECTING"; }
                }
            } else if (state.page == 14) {
                if (!view.TitleImage()) { return -3; }
                if (view.Button(330, 620, 364, 72, "TAP TO PLAY", true) || activate) {
                    unsigned nextPage = 25;
                    if (profile.tutorialCompleted) { nextPage = 24; }
                    state.Navigate(nextPage);
                }
            } else if (state.page == 25) {
                view.movies.Rectangle(0, 134, 1024, 634, 0, 0, 0);
                view.movies.Draw(70, 400);
                view.CenterText("SELECT YOUR GUN BROTHER", 512, 202, 6, 1.45f);
                if (view.Hit(108, 306, 388, 448)) { profile.playerBrother = 0; if (!profile.SaveToDisk(savePath)) { return -3; } return 5; }
                if (view.Hit(529, 306, 388, 448)) { profile.playerBrother = 1; if (!profile.SaveToDisk(savePath)) { return -3; } return 5; }
            } else if (state.page == 15) {
                view.Text(342, 293, "LEAVE THE GAME?", 3);
                view.Text(260, 401, "Your progress will be saved.", 2.2f);
                if (view.Button(235, 526, 250, 66, "CONTINUE")) { state.Navigate(0); }
                if (view.Button(540, 526, 250, 66, "SAVE AND QUIT")) { return -1; }
            } else if (state.page == 16) {
                view.Text(115, 210, "BOKOR", 3.6f);
                view.DrawPlanet(4, 755, 418, 325);
                view.Paragraph(115, 270, 380, "Survive the hordes on the galaxy's toxic waste disposal planet. Choose your starting horde.", 0.8f);
                for (unsigned index = 0; index < 10; ++index) {
                    const float x = 115 + (index % 2) * 202.0f;
                    const float y = 372 + (index / 2) * 49.0f;
                    if (view.Button(x, y, 186, 40, "HORDE " + std::to_string(index + 1), state.hordeStart == index)) { state.hordeStart = index; }
                }
                view.Text(115, 635, "BEST KILLS: " + std::to_string(profile.hordeBestKills[state.hordeStart]), 1.9f);
                view.Text(115, 665, "BEST POINTS: " + std::to_string(profile.hordeBestScore[state.hordeStart]), 1.9f);
                if (view.Button(634, 625, 273, 58, "PLAY HORDE", true) || activate) { return 4; }
            } else if (state.page == 17) {
                view.Text(372, 207, "GET CURRENCY", 3);
                constexpr const char *tabs[] = {"COINS", "WAR BUCKS", "EXCHANGE"};
                for (unsigned index = 0; index < 3; ++index) {
                    if (view.Button(135 + index * 255.0f, 266, 238, 44, tabs[index], state.currencyTab == index)) {
                        state.currencyTab = index; state.currencyPage = 0;
                    }
                }
                std::vector<unsigned> offers;
                for (unsigned index = 0; index < store.size(); ++index) {
                    if (store[index].data.type == 14 + state.currencyTab) { offers.push_back(index); }
                }
                const unsigned pages = std::max(1u, static_cast<unsigned>((offers.size() + 3) / 4));
                state.currencyPage = std::min(state.currencyPage, pages - 1);
                for (unsigned row = 0; row < 4; ++row) {
                    const unsigned index = state.currencyPage * 4 + row;
                    if (index >= offers.size()) { break; }
                    const StoreEntry &offer = store[offers[index]];
                    const float y = 330 + row * 71.0f;
                    view.Icon(toc, tables, offer, 114, y, 66, 66);
                    view.Text(196, y + 17, offer.name, 1.9f);
                    if (view.Button(720, y + 8, 176, 47, "SELECT")) {
                        state.currencyItem = static_cast<int>(offers[index]); state.Navigate(18);
                    }
                }
                if (view.Button(240, 637, 130, 38, "PREVIOUS") && state.currencyPage > 0) { --state.currencyPage; }
                view.Text(478, 649, std::to_string(state.currencyPage + 1) + " / " + std::to_string(pages), 1.6f);
                if (view.Button(650, 637, 130, 38, "NEXT") && state.currencyPage + 1 < pages) { ++state.currencyPage; }
            } else if (state.page == 18) {
                if (state.currencyItem < 0 || state.currencyItem >= static_cast<int>(store.size())) { state.Back(); }
                else {
                    const StoreEntry &offer = store[state.currencyItem];
                    view.Text(340, 233, "CONFIRM CURRENCY", 2.8f);
                    view.Paragraph(175, 330, 700, offer.name, 1);
                    if (offer.data.type != 16) { view.Paragraph(175, 413, 700, "LOCAL MODE / NO REAL PAYMENT", 0.83f); }
                    else { view.Paragraph(175, 413, 700, "Exchange the displayed amounts using your current balance.", 0.83f); }
                    if (!state.currencyPending && view.Button(218, 546, 254, 60, "CANCEL")) { state.Back(); }
                    if (!state.currencyPending && view.Button(551, 546, 254, 60, "CONFIRM", true)) {
                        state.currencyPending = true;
                        state.currencyReadyAt = menuClock + 4000;
                    }
                    if (state.currencyPending && menuClock >= state.currencyReadyAt) {
                        state.currencyPending = false;
                        const PurchaseResult result = profile.AcquireCurrency(offer.data);
                        if (result == PurchaseResult::Purchased) { saveChanged = true; state.Back(); state.message = "CURRENCY ADDED"; }
                        else { state.message = PurchaseMessage(result); }
                    }
                }
            }
            if (saveChanged) {
                music.SetEnabled(profile.musicEnabled);
                CAudioPlayer::SetEffectsEnabled(profile.soundEnabled);
                if (!profile.SaveToDisk(savePath)) { return -3; }
            }
        }
        int navigation = -1;
        if (state.currencyPending) { view.Hit(0, 0, 1024, 768); }
        if (state.page != 14) {
            unsigned headerPage = state.page;
            if (state.page == 29) { headerPage = 4; }
            if (state.refinementRequired) { headerPage = 25; }
            navigation = view.Header(profile, progress, headerPage);
        }
        constexpr unsigned navigationPages[] = {0, 4, 5, 2, 3, 6, 7};
        if (navigation >= 7) {
            state.currencyTab = static_cast<unsigned>(navigation - 7);
            state.currencyPage = 0;
            state.Navigate(17);
        } else if (navigation >= 0) {
            state.Navigate(navigationPages[navigation], true);
            state.itemPage = 0;
            state.selectedItem = -1;
            state.message.clear();
        }
        // Trunk pages are reached from the navigation bar and carry no BACK.
        if (state.page != 0 && state.page != 2 && state.page != 14 && state.page != 19 && state.page != 21 && state.page != 22 &&
            state.page != 23 && state.page != 24 && (state.page < 25 || state.page == 29) && !state.refinementRequired && !state.currencyPending && !state.inviteOpen && view.Button(20, 708, 145, 42, "BACK")) { state.Back(); }
        if (state.page == 1 && view.Button(780, 708, 210, 42, "GET CURRENCY")) { state.Navigate(17); }

        if (!state.message.empty()) { view.Text(450, 738, state.message, 1.45f, 0.93f, 0.74f, 0.33f); }
        if (state.currencyPending) {
            view.movies.Rectangle(0, 132, 1024, 627, 0, 0, 0, 0.75f);
            view.movies.DrawFitted(39, 1300, 285, 302, 454, 170);
            view.CenterText("PLEASE WAIT...", 512, 365, 6, 1.25f);
        }
        if (GameHostSettings().debugMode) {
            char debug[160];
            std::snprintf(debug, sizeof(debug), "FPS %.1f / %.1f MS / PAGE %u / NET %u", 1000.0f / std::max(0.1f, smoothFrameMs),
                smoothFrameMs, state.page, GameHostSettings().isConnected);
            view.movies.Rectangle(2, 135, 620, 22, 0, 0, 0, 0.8f);
            view.movies.Text(debug, 7, 138, 0, 0.65f);
        }
        // The pressed plate's burst plays above whatever the click opened.
        view.DrawPress();
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
        const CMovie *movie = probe.movies.GetMovie(probe.movies.Ordinal("GLU_MOVIE_SHOP_BOX"));
        if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
        MovieRegion column, body;
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), 1, kScrollRestTime, column) ||
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
            state->shopFilter = 1; // Select one real category and omit promotional cards.
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
    return 0;
}

int RunStoreTemplateCheck(const std::string &bigDirectory, bool cardsOnly) {
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

int RunGameMenuCheck(const std::string &bigDirectory) {
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
    const unsigned core = toc.GetPack(toc.GetCorePackIndex())->GetPackHash();
    profile.Reset(core, refinement);
    profile.xplodium = 250; // Isolated test fixture, never the user's profile.
    const std::filesystem::path path = "out/menu-profile-check.dat";
    MenuState state;
    // Two columns in, at the original 258 pixel column pitch.
    state.shopScroll = 516;
    state.modeSelected = true;
    state.starPanX = -350;
    // Rebuilt store coordinates: refinery; refine; store; buy Mad Dogs into
    // the first weapon slot; swap slots; buy the free ER97E Elite; planets;
    // Haven. Domain methods are not called by this driver, so button placement
    // and ownership wiring are what actually gets tested.
    const std::vector<MenuTestClick> clicks = {
        {414, 98}, {429, 360}, {322, 98},
        {228, 582}, {946, 190}, {228, 380},
        {46, 98}, {779, 402}
    };
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        state, path, "out/game-menu-check.png", &clicks) != -2) { return 1; }
    if (profile.coins != 25 || profile.xplodium != 0 || profile.inventory.size() != 6 || state.planet != 1 ||
        SameObject(profile.configuration.guns[0], profile.configuration.guns[1])) {
        std::printf("[menu-check] failed coins=%llu xplodium=%llu inventory=%zu planet=%u\n",
            profile.coins, profile.xplodium, profile.inventory.size(), state.planet);
        return 1;
    }
    SurvivalGameContext context{profile, path, state.planet};
    if (RunSurvival(bigDirectory, kPlanetPacks[state.planet], kPlanetMaps[state.planet], 0, -1, "", 0,
        false, false, true, 2, 0, &context, true) != 0) { return 1; }
    CProfileManager restored;
    restored.Reset(core, refinement);
    if (!restored.LoadFromDisk(path) || restored.coins != 25 || restored.clearedWaves[1] != 2 ||
        restored.experience == 0 || restored.xplodium == 0 || restored.inventory.size() != 6) { return 1; }
    std::printf("[menu-check] refined=250 purchased=225 equipped=2 planet=Haven waves=2 failures=0\n");
    // Keep the actual two-wave outcome; exercise postgame navigation and saving.
    MenuState results;
    BeginPostGame(results, context, weapons);
    if (results.page != 26 || !results.refinementRequired || context.result.casualties.empty() || profile.weaponMastery.empty()) { return 1; }
    const unsigned earnedMastery = profile.GetWeaponExperience(results.masteryWeapon);
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        results, path, "out/fidelity-results-mastery.png") != -2) { return 1; }
    const std::vector<MenuTestClick> overviewClicks = {{871, 183}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        results, path, "out/fidelity-results-overview.png", &overviewClicks) != -2 || results.page != 27) { return 1; }
    const std::vector<MenuTestClick> casualtyClicks = {{622, 160}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        results, path, "out/fidelity-results-casualties.png", &casualtyClicks) != -2 || results.page != 28) { return 1; }
    const std::vector<MenuTestClick> refineryClicks = {{80, 414}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        results, path, "out/fidelity-results-refinery.png", &refineryClicks) != -2 || results.page != 3) { return 1; }
    const auto coinsBefore = profile.coins, oreBefore = profile.xplodium;
    const std::vector<MenuTestClick> refineClicks = {{429, 360}, {429, 360}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        results, path, "out/fidelity-results-refined.png", &refineClicks) != -2 || results.refinementRequired ||
        profile.xplodium != 0 || profile.coins != coinsBefore + oreBefore || !restored.LoadFromDisk(path) ||
        restored.GetWeaponExperience(results.masteryWeapon) != earnedMastery) { return 1; }
    profile.warbucks = 1000;
    results.page = 26;
    const WeaponEntry *masteryWeapon = FindMasteryWeapon(weapons, results.masteryWeapon);
    if (masteryWeapon == nullptr) { return 1; }
    const std::vector<MenuTestClick> upgradeClicks = {{761, 568}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        results, path, "out/fidelity-results-upgraded.png", &upgradeClicks) != -2 ||
        masteryWeapon->data.GetMasteryLevel(profile.GetWeaponExperience(results.masteryWeapon)) != 1 || profile.warbucks >= 1000) { return 1; }
    std::printf("[postgame-check] kills=%u casualties=%zu weaponXP=%u refined=%llu upgrade=1 failures=0\n",
        context.result.kills, context.result.casualties.size(), earnedMastery, oreBefore);
    CProfileManager itemProfile;
    itemProfile.Reset(core, refinement);
    itemProfile.warbucks = 10;
    MenuState itemState;
    itemState.page = 2;
    itemState.slot = 5;
    itemState.shopCategory = 2;
    // Speed Boost is the store's first power-up row: one Warbuck for five
    // charges, so two purchases leave ten.
    const std::vector<MenuTestClick> itemClicks = {{486, 582}, {486, 582}, {-100, -100}};
    const std::filesystem::path itemPath = "out/menu-powerup-profile-check.dat";
    if (ShowGameMenu(toc, tables, itemProfile, progress, refinement, store, weapons, armor,
        itemState, itemPath, "out/game-menu-items-check.png", &itemClicks) != -2) { return 1; }
    GameObjectRef speedBoost;
    speedBoost.packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
    speedBoost.localIndex = 16;
    if (itemProfile.warbucks != 8 || itemProfile.GetPowerupCount(speedBoost) != 10) {
        std::printf("[menu-check] failed warbucks=%llu speed-boost=%u\n",
            itemProfile.warbucks, itemProfile.GetPowerupCount(speedBoost));
        return 1;
    }
    restored.Reset(core, refinement);
    if (!restored.LoadFromDisk(itemPath) || restored.GetPowerupCount(speedBoost) != 10) { return 1; }
    std::printf("[menu-check] consumable-bought-twice=10 warbucks=8 saved=10 failures=0\n");
    CProfileManager previewProfile;
    previewProfile.Reset(core, refinement);
    const CProfileManager beforePreview = previewProfile;
    MenuState previewState;
    previewState.page = 2;
    previewState.slot = 2;
    previewState.shopCategory = 1;
    const std::vector<MenuTestClick> previewClicks = {{350, 500}, {-100, -100}};
    if (ShowGameMenu(toc, tables, previewProfile, progress, refinement, store, weapons, armor,
        previewState, "out/menu-preview-profile.dat", "out/game-menu-preview-check.png", &previewClicks) != -2) { return 1; }
    if (previewProfile.coins != beforePreview.coins || previewProfile.warbucks != beforePreview.warbucks ||
        previewProfile.inventory.size() != beforePreview.inventory.size()) { return 1; }
    for (unsigned slot = 0; slot < kArmorSlotCount; ++slot) {
        if (!SameObject(previewProfile.configuration.armor[slot], beforePreview.configuration.armor[slot])) { return 1; }
    }
    std::printf("[menu-check] shop-preview-without-purchase loadout-unchanged=1 failures=0\n");
    // The expanded card carries the description, the damage/rate/speed column
    // and the upgrade meter; open one and confirm the records behind it read.
    // Put one gun on its first tier so the card badge and the meter have
    // something real to draw.
    GameObjectRef starterGun;
    starterGun.packHash = core;
    starterGun.localIndex = 0;
    const WeaponEntry *starterWeapon = FindMasteryWeapon(weapons, starterGun);
    if (starterWeapon == nullptr) { return 1; }
    previewProfile.AddWeaponExperience(starterGun, starterWeapon->data.GetMasteryThreshold(0),
        starterWeapon->data.GetMasteryLimit());
    if (starterWeapon->data.GetMasteryLevel(previewProfile.GetWeaponExperience(starterGun)) != 1) { return 1; }
    MenuState detailState;
    detailState.page = 2;
    detailState.shopScroll = 258;
    const std::vector<MenuTestClick> detailClicks = {{100, 500}, {-100, -100, 400}, {-100, -100, 400}};
    if (ShowGameMenu(toc, tables, previewProfile, progress, refinement, store, weapons, armor,
        detailState, "out/menu-detail-check.dat", "out/game-menu-detail.png", &detailClicks) != -2 ||
        !detailState.shopDetailOpen || detailState.selectedItem < 0) { return 1; }
    const StoreEntry &detailItem = store[detailState.selectedItem];
    const std::string detailText = ReadGameString(toc, detailItem.data.assets[3]);
    if (detailText.empty() || detailItem.data.statGroups[1].empty() || detailItem.data.statGroups[2].empty()) {
        std::printf("[menu-check] expanded card has no description or stats for %s\n", detailItem.name.c_str());
        return 1;
    }
    std::printf("[menu-check] expanded-card item=%s description=%zu chars dmg=%d rpm=%d\n",
        detailItem.name.c_str(), detailText.size(), detailItem.data.statGroups[1][0], detailItem.data.statGroups[2][0]);
    MenuState filterState;
    filterState.page = 2;
    // FILTER plate, then the PISTOL and RIFLE rows of the original sort list.
    const std::vector<MenuTestClick> filterClicks = {{910, 740}, {900, 438, 400}, {900, 489}, {-100, -100}};
    if (ShowGameMenu(toc, tables, previewProfile, progress, refinement, store, weapons, armor,
        filterState, "out/menu-filter-check.dat", "out/game-menu-filter-check.png", &filterClicks) != -2 ||
        filterState.shopFilter != 3 || !filterState.shopFilterOpen || previewProfile.coins != 0) { return 1; }
    std::printf("[menu-check] shop-multiselect-pistol-rifle=1 no-purchase=1\n");
    CProfileManager original;
    original.Reset(core, refinement);
    if (!ImportOriginalProfile(toc, tables, original)) { return 1; }
    MenuState originalShop;
    originalShop.page = 2;
    if (ShowGameMenu(toc, tables, original, progress, refinement, store, weapons, armor,
        originalShop, "out/original-shop-check.dat", "out/original-shop-fidelity.png", nullptr, true) != -2) { return 1; }
    MenuState originalState;
    originalState.page = 1;
    originalState.slot = 2;
    if (ShowGameMenu(toc, tables, original, progress, refinement, store, weapons, armor,
        originalState, "out/original-menu-check.dat", "out/original-menu-armor.png", nullptr, true) != -2) { return 1; }
    std::printf("[menu-check] original-profile armor preview failures=0\n");
    // Exercise the actual equip button and one following frame so the cached
    // model must rebuild. Only the isolated output profile is written.
    const std::vector<MenuTestClick> helmetClicks = {{380, 270}, {880, 655}, {-100, -100}};
    const std::filesystem::path helmetPath = "out/original-menu-check.dat";
    if (ShowGameMenu(toc, tables, original, progress, refinement, store, weapons, armor,
        originalState, helmetPath, "out/original-menu-equipped-combat.png", &helmetClicks, true) != -2) { return 1; }
    const auto &helmet = original.configuration.armor[2];
    const unsigned armorPack = toc.GetPack(toc.GetPackIndexFromName("pack4"))->GetPackHash();
    if (helmet.packHash != armorPack || helmet.localIndex != 0) { return 1; }
    restored.Reset(core, refinement);
    if (!restored.LoadFromDisk(helmetPath) || !SameObject(helmet, restored.configuration.armor[2])) { return 1; }
    std::printf("[menu-check] helmet-changed=pack4:0 preview-rebuilt saved=1 failures=0\n");
    CProfileManager activities;
    activities.Reset(core, refinement);
    activities.clearedWaves[0] = 5;
    MenuState activityState;
    activityState.page = 11;
    const std::filesystem::path activityPath = "out/menu-activities-check.dat";
    const std::vector<MenuTestClick> activityClicks = {{780, 294}, {780, 294}, {-100, -100}};
    if (ShowGameMenu(toc, tables, activities, progress, refinement, store, weapons, armor,
        activityState, activityPath, "out/game-menu-activities-check.png", &activityClicks) != -2 ||
        activities.coins != 100 || activities.warbucks != 1 || activities.claimedActivities != 1 || activityState.page != 13) { return 1; }
    restored.Reset(core, refinement);
    if (!restored.LoadFromDisk(activityPath) || restored.ClaimActivity(0) || restored.coins != 100) { return 1; }
    MenuState optionsState;
    optionsState.page = 6;
    const std::vector<MenuTestClick> optionsClicks = {{250, 360}, {270, 445}, {185, 614}, {-100, -100}};
    if (ShowGameMenu(toc, tables, activities, progress, refinement, store, weapons, armor,
        optionsState, activityPath, "out/game-menu-options-check.png", &optionsClicks) != -2 ||
        activities.soundEnabled || activities.musicEnabled || activities.brotherEnabled) { return 1; }
    restored.Reset(core, refinement);
    if (!restored.LoadFromDisk(activityPath) || restored.soundEnabled || restored.musicEnabled || restored.brotherEnabled) { return 1; }
    const bool connectedBefore = GameHostSettings().isConnected;
    GameHostSettings().isConnected = false;
    MenuState socialState;
    socialState.page = 4;
    if (ShowGameMenu(toc, tables, activities, progress, refinement, store, weapons, armor,
        socialState, activityPath, "out/fidelity-social-offline.png") != -2) { return 1; }
    const std::vector<MenuTestClick> socialClicks = {{785, 669}, {270, 680}, {-100, -100}};
    if (ShowGameMenu(toc, tables, activities, progress, refinement, store, weapons, armor,
        socialState, activityPath, "out/fidelity-social-invite.png", &socialClicks) != -2 || !socialState.inviteOpen) { return 1; }
    const std::vector<MenuTestClick> closeInviteClicks = {{250, 90}, {510, 506}, {-100, -100}};
    if (ShowGameMenu(toc, tables, activities, progress, refinement, store, weapons, armor,
        socialState, activityPath, "out/fidelity-social-bros.png", &closeInviteClicks) != -2 || socialState.inviteOpen || socialState.page != 4) { return 1; }
    socialState.page = 5;
    const std::vector<MenuTestClick> requestsClicks = {{770, 205}, {-100, -100}};
    if (ShowGameMenu(toc, tables, activities, progress, refinement, store, weapons, armor,
        socialState, activityPath, "out/fidelity-social-requests.png", &requestsClicks) != -2 || socialState.socialTab != 2) { return 1; }
    GameHostSettings().isConnected = connectedBefore;
    std::printf("[menu-check] offline-bros local-preview invite-modal-close bro-ops-requests failures=0\n");
    MenuState nestedState;
    nestedState.page = 6;
    nestedState.optionsScroll = 595;
    const std::vector<MenuTestClick> nestedClicks = {{250, 445}, {250, 561}, {880, 319},
        {70, 728}, {70, 728}, {70, 728}, {-100, -100}};
    if (ShowGameMenu(toc, tables, activities, progress, refinement, store, weapons, armor,
        nestedState, activityPath, "out/game-menu-back-check.png", &nestedClicks) != -2 ||
        nestedState.page != 6 || !nestedState.history.empty()) { return 1; }
    std::printf("[menu-check] nested-options-account-bros-account-back=1 click-consumed=1\n");
    CProfileManager waveProfile;
    waveProfile.Reset(core, refinement);
    waveProfile.clearedWaves[0] = 55;
    MenuState waveState;
    waveState.page = 19;
    const std::vector<MenuTestClick> waveClicks = {{404, 350}, {-100, -100}};
    if (ShowGameMenu(toc, tables, waveProfile, progress, refinement, store, weapons, armor,
        waveState, "out/wave-menu-check.dat", "out/game-menu-waves-check.png", &waveClicks) != -2 ||
        waveState.startingWave != 1) { return 1; }
    waveState.startingWave = 50;
    waveState.revolution = 1;
    const std::vector<MenuTestClick> lockedWaveClicks = {{404, 455}, {-100, -100}};
    if (ShowGameMenu(toc, tables, waveProfile, progress, refinement, store, weapons, armor,
        waveState, "out/wave-menu-check.dat", "out/game-menu-waves-locked-check.png", &lockedWaveClicks) != -2 ||
        waveState.startingWave != 50 || waveProfile.clearedWaves[0] != 55) { return 1; }
    std::printf("[menu-check] previous-revolution-replay=1 future-revolution-and-wave-locked=1\n");
    MenuState modeState;
    const std::vector<MenuTestClick> modeClicks = {{240, 380}, {156, 526}, {290, 455}, {-100, -100}};
    if (ShowGameMenu(toc, tables, waveProfile, progress, refinement, store, weapons, armor,
        modeState, "out/wave-menu-check.dat", "out/game-menu-mission-flow.png", &modeClicks) != -2 ||
        !modeState.modeSelected || modeState.page != 19 || modeState.revolution != 0) { return 1; }
    MenuState hordeState;
    hordeState.page = 21;
    hordeState.planet = 4;
    hordeState.modeSelected = true;
    const std::vector<MenuTestClick> hordeClicks = {{550, 455}, {-100, -100}};
    if (ShowGameMenu(toc, tables, waveProfile, progress, refinement, store, weapons, armor,
        hordeState, "out/wave-menu-check.dat", "out/game-menu-horde-flow.png", &hordeClicks) != -2 ||
        hordeState.page != 23 || hordeState.hordeStart != 1) { return 1; }
    std::printf("[menu-check] mode-planet-revolution-wave=1 horde-no-wave-selection=1\n");
    CProfileManager currencyProfile;
    currencyProfile.Reset(core, refinement);
    MenuState bankState;
    bankState.page = 17;
    const std::vector<MenuTestClick> bankClicks = {{800, 357}, {680, 575}, {680, 575, 2000}, {-100, -100, 2000}, {-100, -100, 1000}};
    if (ShowGameMenu(toc, tables, currencyProfile, progress, refinement, store, weapons, armor,
        bankState, "out/currency-check.dat", "out/game-menu-currency-check.png", &bankClicks) != -2 ||
        currencyProfile.coins != 5000 || bankState.page != 17) { return 1; }
    restored.Reset(core, refinement);
    if (!restored.LoadFromDisk("out/currency-check.dat") || restored.coins != 5000) { return 1; }
    const StoreEntry *toCoins = nullptr, *toBucks = nullptr, *bucks = nullptr;
    for (const StoreEntry &entry : store) {
        if (entry.data.type == 16 && entry.data.commonPrice == 500) { toCoins = &entry; }
        if (entry.data.type == 16 && entry.data.commonPrice == 10000) { toBucks = &entry; }
        if (entry.data.type == 15 && entry.data.rarePrice == 5) { bucks = &entry; }
    }
    if (toCoins == nullptr || toBucks == nullptr || bucks == nullptr ||
        restored.AcquireCurrency(toCoins->data) != PurchaseResult::InsufficientWarbucks ||
        restored.AcquireCurrency(toBucks->data) != PurchaseResult::InsufficientCoins ||
        restored.coins != 5000 || restored.warbucks != 0) { return 1; }
    if (restored.AcquireCurrency(bucks->data) != PurchaseResult::Purchased ||
        restored.AcquireCurrency(toCoins->data) != PurchaseResult::Purchased ||
        restored.coins != 5500 || restored.warbucks != 4) { return 1; }
    restored.coins = 10000;
    if (restored.AcquireCurrency(toBucks->data) != PurchaseResult::Purchased || restored.coins != 0 || restored.warbucks != 9) { return 1; }
    std::printf("[menu-check] local-checkout=1 confirmed-once=1 exchange-both-directions=1 insufficient-balance=1\n");
    CAudioPlayer::SetEffectsEnabled(true);
    std::printf("[menu-check] navigation=7 activity-single-claim=1 settings-persisted=3 failures=0\n");
    return 0;
}

int RunGameFrontEnd(const std::string &bigDirectory, const std::string &screenshotPath, unsigned page, bool originalProfile,
    const std::string &profilePath) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CPlayerProgress::Template progress;
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    {
        CWindow loadingWindow;
        if (!loadingWindow.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
        MovieRenderer loadingMovies;
        CResPackTOC *core = toc.GetPack(toc.GetCorePackIndex());
        if (!loadingMovies.Init(*core, *core)) { return 1; }
        LoadingScreen loading(loadingWindow, loadingMovies, tables);
        if (!LoadPlayerProgress(toc, tables, progress) || !LoadRefinementTemplate(toc, tables, refinement) ||
            !LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
            !LoadArmorCatalog(toc, tables, armor)) { return 1; }
        if (loading.Cancelled()) { return 0; }
    }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    std::string profileName = "profile.dat";
    if (originalProfile) { profileName = "original-profile.dat"; }
    std::filesystem::path savePath = std::filesystem::path(ASSET_ROOT) / "userdata" / profileName;
    if (!profilePath.empty()) { savePath = profilePath; }
    if (originalProfile && !std::filesystem::exists(savePath)) {
        if (!ImportOriginalProfile(toc, tables, profile) || !profile.SaveToDisk(savePath)) { return 1; }
    }
    if (!profile.LoadFromDisk(savePath)) {
        std::printf("[game] profile cannot be loaded; original file preserved: %s\n", savePath.string().c_str());
        return 1;
    }
    MenuState state;
    state.page = std::min(page, 29u);
    if (page == 0 && screenshotPath.empty()) { state.page = 14; }
    while (true) {
        const int choice = ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, savePath, screenshotPath, nullptr, originalProfile);
        if (choice == -3) { return 1; }
        if (choice == -2) { return 0; }
        if (choice < 0) { return !profile.SaveToDisk(savePath); }
        if (choice == 5) {
            SurvivalGameContext context{profile, savePath, 0};
            context.tutorial = true;
            if (RunSurvival(bigDirectory, "pack2", 7, 0, -1, "", 0, false, false, false, 2, 0, &context, true) != 0) { return 1; }
            if (profile.tutorialCompleted) { BeginPostGame(state, context, weapons); }
            else { state.Navigate(25, true); }
            continue;
        }
        if (choice == 4) {
            std::vector<MissionEntry> missions;
            if (!LoadMissionCatalog(toc, tables, missions)) { return 1; }
            const unsigned packHash = toc.GetPack(toc.GetPackIndexFromName("pack11"))->GetPackHash();
            const MissionEntry *selected = nullptr;
            for (const MissionEntry &mission : missions) {
                if (mission.resource.packHash == packHash && mission.resource.localIndex == state.hordeStart && mission.data.type == 2) { selected = &mission; break; }
            }
            if (selected == nullptr) { return 1; }
            SurvivalGameContext context{profile, savePath};
            context.hordeStart = static_cast<int>(state.hordeStart);
            if (RunSurvival(bigDirectory, "pack11", 0, 0, -1, "", 0, false, false, false, 2,
                selected->data.value64, &context, profile.brotherEnabled, false, selected) != 0) { return 1; }
            BeginPostGame(state, context, weapons);
            continue;
        }
        unsigned wave = profile.clearedWaves[choice];
        if (wave >= 500) { wave = 0; }
        if (state.startingWave >= 0) { wave = static_cast<unsigned>(state.startingWave); }
        SurvivalGameContext context{profile, savePath, static_cast<unsigned>(choice)};
        if (RunSurvival(bigDirectory, kPlanetPacks[choice], kPlanetMaps[choice], 0, -1, "", 0, false, false, false, 2, wave, &context, profile.brotherEnabled) != 0) { return 1; }
        BeginPostGame(state, context, weapons);
    }
}
