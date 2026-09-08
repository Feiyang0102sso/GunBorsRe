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
#include "runtime/OriginalMenuData.h"
#include "runtime/SurvivalGameContext.h"
#include "gun_bros/Planet.h"
#include "gun_bros/CBGM.h"
#include "milestones/M3Map.h"
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

namespace {
constexpr const char *kPlanetPacks[] = {"pack2", "pack7", "pack9", "pack12", "pack11"};
constexpr unsigned kPlanetMaps[] = {7, 6, 0, 0, 0};
constexpr const char *kPageNames[] = {"PLANETS", "EQUIPMENT", "SHOP", "REFINERY"};
constexpr const char *kSlotNames[] = {"WEAPON 1", "WEAPON 2", "HELMET", "ARMOR", "PANTS", "ITEMS"};
constexpr unsigned kArmorSlots[] = {0, 0, 2, 1, 0};
constexpr float kMenuWidth = 1024;
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

struct MenuTestClick { float x; float y; };

/** All GL owners are destroyed before the menu window's context. */
class GameMenu {
public:
    /** Integration harness input; it still goes through rendered button hit tests. */
    void SetTestClick(const MenuTestClick &click) { mouseX = click.x; mouseY = click.y; clicked = true; }
    bool Open(CResTOCManager &toc, PackTables &tables) {
        if (!window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return false; }
        window.SetEscapeCloses(false);
        const char *directory = ASSET_ROOT "/src/gun_bros_re/shaders";
        if (!textProgram.Load(directory, "ogles_vs_mvp_constcolor", "ogles_ps_constcolor") ||
            !imageProgram.Load(directory, "ogles_vs_mvp_tex0", "ogles_ps_tex0") ||
            !markers.Create(textProgram) || !images.Create(imageProgram)) { return false; }
        Matrix4dOrthoTopLeft(kMenuWidth, kMenuHeight, 100, projection);
        CResPackTOC *core = toc.GetPack(toc.GetCorePackIndex());
        if (!movies.Init(*core, *core)) { return false; }
        for (unsigned index = 0; index < 7; ++index) {
            const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_TRUNK", index);
            if (entry == nullptr) { return false; }
            std::printf("[navigation] index=%u label=%s sprite=%u\n", index,
                movies.NamedString(entry->strings[0]).c_str(), entry->sprites[0]);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (unsigned index = 0; index < 5; ++index) {
            CResPackTOC *pack = toc.GetPack(toc.GetPackIndexFromName(kPlanetPacks[index]));
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack->GetPackHash(), GameSection::Planet, 0, payload)) { return false; }
            CArrayInputStream stream(payload);
            if (!planets[index].Init(stream)) { return false; }
            names[index] = ReadGameString(toc, planets[index].name);
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
        return true;
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
        clicked = down && !previousDown;
        previousDown = down;
        movies.Draw(47, 1600);
        if (page == 3) { movies.Draw(36, 1600); }
    }

    void Rect(float x, float y, float width, float height, float r = 0.155f, float g = 0.227f, float b = 0.29f) {
        markers.Begin();
        markers.AddRect(x, y, width, height);
        markers.Draw(textProgram, projection, r, g, b, 1);
    }

    void Text(float x, float y, const std::string &text, float size = 2,
        float r = 0.88f, float g = 0.93f, float b = 0.95f) {
        unsigned font = 0;
        float scale = size * 7 / 23.0f;
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
            float scale = std::min(0.75f, (height - 12) / 23.0f);
            const float labelWidth = movies.TextWidth(label, 0, scale);
            if (labelWidth > width - 12) { scale *= (width - 12) / labelWidth; }
            movies.Text(label, x + (width - movies.TextWidth(label, 0, scale)) * 0.5f,
                y + (height - 23 * scale) * 0.5f, 0, scale);
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

    bool WaveButton(float x, float y, float width, float height, unsigned wave, unsigned cleared, bool selected) {
        // MDS_BUTTON_MISSION_WAVE: 5:21 locked, 5:22 available, 5:23 cleared.
        unsigned animation = 21;
        if (wave == cleared) { animation = 22; }
        if (wave < cleared) { animation = 23; }
        movies.DrawSpriteFitted(5, animation, 0, x, y, width, height);
        if (selected) {
            markers.Begin();
            markers.AddOutline(x, y, width, height, 2);
            markers.Draw(textProgram, projection, 0.2f, 0.8f, 1, 1);
        }
        const std::string label = std::to_string(wave % 50 + 1);
        const float scale = std::min(0.95f, width / 80);
        movies.Text(label, x + (width - movies.TextWidth(label, 0, scale)) * 0.5f,
            y + height * 0.07f, 0, scale);
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
        Text(150, 8, std::to_string(profile.coins), 2.5f);
        Text(438, 8, std::to_string(profile.warbucks), 2.5f);
        Text(440, 44, "XPLODIUM " + std::to_string(profile.xplodium), 1.5f);
        movies.Draw(11, 1600, 697, 0);
        const float experienceFraction = static_cast<float>(progress.GetExperienceInLevel()) / std::max(1u, progress.GetExperienceDelta());
        movies.Rectangle(784, 31, 115 * experienceFraction, 10, 0.1f, 0.65f, 0.9f);
        movies.Text(std::to_string(progress.GetLevel()), 940, 9, 7, 1);
        int choice = -1;
        unsigned activePage = currentPage;
        if (currentPage == 1 || currentPage == 17 || currentPage == 18) { activePage = 2; }
        if (currentPage == 16 || currentPage == 19) { activePage = 0; }
        if (currentPage == 8 || currentPage == 9 || currentPage == 11) { activePage = 6; }
        if (currentPage == 13) { activePage = 5; }
        constexpr unsigned navigationPages[] = {0, 2, 4, 5, 3, 6, 7};
        for (const MovieRegion &region : movies.Regions(10, 1600)) {
            if (region.index >= 7) { continue; }
            const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_TRUNK", region.index);
            const unsigned sprite = entry->sprites[0];
            if (navigationPages[region.index] == activePage || region.Contains(mouseX, mouseY)) {
                // CMenuMovieButton::Focus :144634 selects chapter 3. Its
                // original cyan plate surrounds the dynamic trunk icon.
                movies.DrawFitted(14, 350, region.x, region.y, region.width, region.height, 1);
            }
            movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, region.x, region.y, region.width, region.height);
            const std::string label = movies.NamedString(entry->strings[0]);
            const float scale = std::min(0.67f, 88.0f / std::max(1.0f, movies.TextWidth(label)));
            movies.Text(label, region.x + region.width * 0.5f - movies.TextWidth(label, 0, scale) * 0.5f, 137, 0, scale);
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

    void Icon(CResTOCManager &toc, PackTables &tables, const StoreEntry &entry, float x, float y) {
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
        const float scale = std::min(66.0f / texture.GetWidth(), 66.0f / texture.GetHeight());
        const SourceRect source{0, 0, static_cast<std::uint16_t>(texture.GetWidth()), static_cast<std::uint16_t>(texture.GetHeight())};
        images.Begin();
        images.AddQuad(texture, x, y, texture.GetWidth() * scale, texture.GetHeight() * scale, source, false, false, BlendMode::Alpha);
        images.Upload();
        images.Draw(imageProgram, projection);
    }

    bool DrawEquippedPlayer(CResTOCManager &toc, PackTables &tables, const CProfileManager &profile,
        const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armors, unsigned slot,
        const GameObjectTypeRef *previewItem = nullptr) {
        unsigned gunSlot = 0;
        if (slot == 1) { gunSlot = 1; }
        // Preview substitutes only the model configuration. Ownership, currency
        // and the saved loadout remain owned by the explicit purchase action.
        CPlayerConfiguration configuration = profile.configuration;
        if (previewItem != nullptr) {
            if (previewItem->type == 6) { configuration.guns[gunSlot] = previewItem->object; }
            if (previewItem->type == 2 && slot >= 2 && slot <= 4) { configuration.armor[kArmorSlots[slot]] = previewItem->object; }
        }
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
        }
        Rect(20, 414, 180, 280, 0.035f, 0.07f, 0.10f);
        std::string previewTitle = "EQUIPPED";
        if (previewItem != nullptr) { previewTitle = "PREVIEW"; }
        Text(38, 432, previewTitle, 1.75f, 0.93f, 0.74f, 0.33f);
        const std::uint64_t now = window.GetTicksMs();
        AdvancePlayer(*equippedPreview, static_cast<int>(std::min<std::uint64_t>(now - previewTicks, 100)));
        previewTicks = now;
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        // Restrict the model's depth and long weapon geometry to its sidebar panel.
        glEnable(GL_SCISSOR_TEST);
        const int previewX = static_cast<int>(20 * width / kMenuWidth);
        const int previewY = static_cast<int>((kMenuHeight - 686) * height / kMenuHeight);
        const int previewWidth = static_cast<int>(180 * width / kMenuWidth);
        const int previewHeight = static_cast<int>(232 * height / kMenuHeight);
        glScissor(previewX, previewY, previewWidth, previewHeight);
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
        Matrix4dRotationZ(3.14159265f, facing);
        Matrix4dMultiply(normalise, centre, local);
        Matrix4dMultiply(facing, local, turned);
        Matrix4dMultiply(tilt, turned, oriented);
        Matrix4dOrthoCentred(1.6f, 1.6f * 232 / 180, 4, viewProjection);
        Matrix4dMultiply(viewProjection, oriented, model);
        DrawPlayer(*equippedPreview, imageProgram, model);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, width, height);
        return true;
    }

    CWindow window;
    MovieRenderer movies;
    std::string names[5];
private:
    CShaderProgram textProgram;
    CShaderProgram imageProgram;
    CMarkerBatch markers;
    CQuadBatch images;
    CTexture titleImage;
    float projection[16]{};
    float mouseX = 0, mouseY = 0;
    bool clicked = false, previousDown = false;
    Planet planets[5];
    std::vector<SpriteQuad> planetQuads[5];
    std::map<int, std::unique_ptr<CSpriteGlu>> spritePacks;
    std::map<std::uint64_t, std::unique_ptr<CTexture>> icons;
    std::unique_ptr<PlayerModel> equippedPreview;
    CPlayerConfiguration previewConfiguration;
    unsigned previewGunSlot = 0;
    std::uint64_t previewTicks = 0;
};
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

/** Returns selected planet, -1 for quit, -2 after capture, -3 on failure. */
int ShowGameMenu(CResTOCManager &toc, PackTables &tables, CProfileManager &profile,
    const CPlayerProgress::Template &progressData, const CRefinementManager::Template &refinement,
    const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::vector<ArmorEntry> &armors, MenuState &state, const std::filesystem::path &savePath,
    const std::string &capturePath, const std::vector<MenuTestClick> *testClicks = nullptr, bool originalProfile = false) {
    GameMenu view;
    if (!view.Open(toc, tables)) { return -3; }
    CBGM music;
    if (!music.Play(0)) { return -3; }
    music.SetEnabled(profile.musicEnabled);
    CAudioPlayer::SetEffectsEnabled(profile.soundEnabled);
    CPlayerProgress progress;
    progress.Bind(progressData);
    progress.SetExperience(profile.experience);
    unsigned testFrame = 0;
    while (view.window.PumpEvents()) {
        music.Update();
        bool activate = false;
        const unsigned previousPage = state.page;
        for (KeyCode key = view.window.TakeKeyPress(); key != KeyCode::None; key = view.window.TakeKeyPress()) {
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
        }
        if (previousPage != state.page) { state.itemPage = 0; state.selectedItem = -1; state.message.clear(); }
        const std::int64_t now = CurrentSeconds();
        profile.refinery.UpdateRefinement(now);
        view.Begin(state.page);
        if (testClicks != nullptr && testFrame < testClicks->size()) { view.SetTestClick((*testClicks)[testFrame]); }
        if (state.page == 0) {
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

        if (state.page == 19) {
            view.Text(48, 192, view.names[state.planet] + " - SELECT WAVE", 2.8f);
            const unsigned cleared = profile.clearedWaves[state.planet];
            unsigned selectedWave = cleared % 500;
            if (state.startingWave >= 0) { selectedWave = static_cast<unsigned>(state.startingWave); }
            const unsigned revolution = selectedWave / 50;
            if (view.Button(65, 240, 90, 42, "<") && revolution > 0) { state.startingWave = (revolution - 1) * 50; }
            view.Text(215, 249, "REVOLUTION " + std::to_string(revolution + 1) + " / 10", 2.3f);
            if (view.Button(867, 240, 90, 42, ">") && revolution < 9) {
                const unsigned next = (revolution + 1) * 50;
                if (next <= cleared) { state.startingWave = next; }
                else { state.message = "CLEAR THE PREVIOUS REVOLUTION TO UNLOCK"; }
            }
            for (unsigned wave = 0; wave < 50; ++wave) {
                const unsigned absolute = revolution * 50 + wave;
                if (view.WaveButton(65 + (wave % 10) * 90.0f, 300 + (wave / 10) * 72.0f,
                    80, 66, absolute, cleared, absolute == selectedWave)) {
                    if (absolute <= cleared) { state.startingWave = absolute; }
                    else { state.message = "CLEAR THE PREVIOUS WAVE TO UNLOCK"; }
                }
            }
            if (view.Button(710, 674, 265, 65, "PLAY", true) || activate) { return static_cast<int>(state.planet); }
        }

        if (state.page == 1 || state.page == 2) {
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
                view.Icon(toc, tables, item, x + 10, y + 11);
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
            view.Text(240, 169, "REFINE XPLODIUM INTO COINS", 2.7f);
            view.Text(240, 197, "Instant refining is free. Longer batches yield more coins.", 1.35f);
            view.Text(240, 222, "ADVANCED - UNLOCK WITH WARBUCKS", 1.3f);
            view.Text(624, 222, "STANDARD - USE THE PREVIOUS BATCH", 1.3f);
            for (unsigned index = 0; index < kRefinementSlotCount; ++index) {
                const float x = 240 + (index / 6) * 384.0f;
                const float y = 265 + (index % 6) * 72.0f;
                const auto &slot = profile.refinery.slots[index];
                std::string label = "REFINE ALL";
                if (slot.state == 0) { label = "UNLOCK " + std::to_string(refinement.rarePrice[index]) + " W"; }
                if (slot.state == 0 && profile.refinery.IsGated(index)) { label = "PREVIOUS BATCH FIRST"; }
                if (slot.state == 2) { label = std::to_string(std::max<std::int64_t>(0, slot.finishTime - now)) + " SEC"; }
                if (slot.state == 3) { label = "COLLECT " + std::to_string(profile.refinery.GetRefinementSlotYield(index)); }
                view.Text(x, y, std::to_string(refinement.minutes[index]) + " MIN / " + std::to_string(refinement.efficiencyPercent[index]) + "%", 1.5f);
                if (view.Button(x + 172, y - 7, 188, 45, label, slot.state == 3)) {
                    bool changed = false;
                    if (slot.state == 0) { changed = profile.refinery.UnlockSlot(index, profile.coins, profile.warbucks); }
                    else if (slot.state == 1) {
                        changed = profile.refinery.BeginRefinement(index, index, profile.xplodium, profile.xplodium, now);
                        if (changed && profile.refinery.slots[index].state == 3) { profile.refinery.CollectResources(index, profile.coins); }
                    } else if (slot.state == 3) { changed = profile.refinery.CollectResources(index, profile.coins); }
                    if (changed) {
                        state.message = "REFINERY UPDATED";
                        if (!profile.SaveToDisk(savePath)) { return -3; }
                    } else { state.message = "CHECK YOUR BALANCE OR COMPLETE THE PREVIOUS BATCH"; }
                }
            }
        }
        if (state.page >= 4 && state.page <= 18) {
            view.BodyPanel();
            bool saveChanged = false;
            if (state.page == 4) {
                view.Text(350, 207, "CHOOSE YOUR BRO", 3);
                view.movies.DrawSpriteFitted(19, 0, 0, 245, 266, 220, 275);
                view.movies.DrawSpriteFitted(19, 1, 0, 565, 266, 220, 275);
                if (view.Button(235, 553, 235, 48, "FRANCIS GUN", profile.playerBrother == 0)) { profile.playerBrother = 0; saveChanged = true; }
                if (view.Button(555, 553, 235, 48, "PERCY GUN", profile.playerBrother == 1)) { profile.playerBrother = 1; saveChanged = true; }
                std::string companion = "BROTHER: OFF";
                if (profile.brotherEnabled) { companion = "BROTHER: ON"; }
                if (view.Button(370, 624, 280, 45, companion, profile.brotherEnabled)) { profile.brotherEnabled = !profile.brotherEnabled; saveChanged = true; }
                view.Paragraph(65, 254, 155, "Your bro joins you in every survival battle with his own default armor, pistol and rifle.", 0.68f);
                if (view.Button(820, 296, 145, 48, "INVITE BROS")) { state.Navigate(9); }
                if (view.Button(820, 356, 145, 48, "BRO GIFTS")) { state.Navigate(11); }
            } else if (state.page == 5 || state.page == 11) {
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
                view.movies.Draw(86, 1600);
                view.Text(215, 215, "OPTIONS", 3.2f);
                std::string sound = "SOUND: OFF", bgm = "MUSIC: OFF", companion = "AUTOSELECT BRO: OFF";
                if (profile.soundEnabled) { sound = "SOUND: ON"; }
                if (profile.musicEnabled) { bgm = "MUSIC: ON"; }
                if (profile.brotherEnabled) { companion = "AUTOSELECT BRO: ON"; }
                if (view.Button(95, 276, 410, 45, sound)) { profile.soundEnabled = !profile.soundEnabled; saveChanged = true; }
                if (view.Button(95, 331, 410, 45, bgm)) { profile.musicEnabled = !profile.musicEnabled; saveChanged = true; }
                if (view.Button(95, 386, 410, 45, companion)) { profile.brotherEnabled = !profile.brotherEnabled; saveChanged = true; }
                if (view.Button(95, 441, 196, 45, "HELP")) { state.Navigate(8); state.detail = 0; }
                if (view.Button(310, 441, 196, 45, "ACCOUNT")) { state.Navigate(9); }
                if (view.Button(95, 496, 196, 45, "SELECT BRO")) { state.Navigate(4); }
                if (view.Button(310, 496, 196, 45, "SAVE GAME")) { saveChanged = true; state.message = "GAME SAVED"; }
                if (view.Button(95, 551, 196, 45, "ABOUT")) { state.Navigate(12); }
                if (view.Button(310, 551, 196, 45, "QUIT GAME")) { state.Navigate(15); }
                view.Text(98, 626, "Esc BACK / Enter CONFIRM", 1.6f);
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
                if (view.Button(115, 534, 355, 55, "SELECT BRO")) { state.Navigate(4); }
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
                if (view.Button(330, 620, 364, 72, "TAP TO PLAY", true) || activate) { state.Navigate(0); }
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
                    view.Icon(toc, tables, offer, 114, y);
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
                    if (view.Button(218, 546, 254, 60, "CANCEL")) { state.Back(); }
                    if (view.Button(551, 546, 254, 60, "CONFIRM", true)) {
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
        if (state.page != 14) { navigation = view.Header(profile, progress, state.page); }
        constexpr unsigned navigationPages[] = {0, 2, 4, 5, 3, 6, 7};
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
        if (state.page != 0 && state.page != 14 && view.Button(20, 708, 145, 42, "BACK")) { state.Back(); }
        if ((state.page == 1 || state.page == 2) && view.Button(780, 708, 210, 42, "GET CURRENCY")) { state.Navigate(17); }

        if (!state.message.empty()) { view.Text(450, 738, state.message, 1.45f, 0.93f, 0.74f, 0.33f); }
        ++testFrame;
        if (!capturePath.empty() && (testClicks == nullptr || testFrame >= testClicks->size())) {
            if (glGetError() != 0 || !view.window.SaveFrame(capturePath)) { return -3; }
            view.window.Present();
            return -2;
        }
        view.window.Present();
    }
    return -1;
}
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
    // Observed menu coordinates: refine; shop; Mad Dogs; buy; free rifle; equip;
    // weapon 2; owned Mad Dogs; equip; planets; Haven. Domain methods are not
    // called by this driver, so button selection and ownership wiring are tested.
    const std::vector<MenuTestClick> clicks = {
        {414, 98}, {510, 265}, {138, 98}, {620, 270}, {880, 655},
        {900, 390}, {880, 655}, {460, 180}, {620, 270}, {880, 655},
        {46, 98}, {390, 480}
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
    CProfileManager itemProfile;
    itemProfile.Reset(core, refinement);
    itemProfile.coins = 300;
    MenuState itemState;
    itemState.page = 2;
    itemState.slot = 5;
    const std::vector<MenuTestClick> itemClicks = {{885, 655}, {885, 655}, {-100, -100}};
    const std::filesystem::path itemPath = "out/menu-powerup-profile-check.dat";
    if (ShowGameMenu(toc, tables, itemProfile, progress, refinement, store, weapons, armor,
        itemState, itemPath, "out/game-menu-items-check.png", &itemClicks) != -2) { return 1; }
    GameObjectRef healthPack;
    healthPack.packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
    healthPack.localIndex = 1;
    if (itemProfile.coins != 0 || itemProfile.GetPowerupCount(healthPack) != 2) { return 1; }
    restored.Reset(core, refinement);
    if (!restored.LoadFromDisk(itemPath) || restored.GetPowerupCount(healthPack) != 2) { return 1; }
    std::printf("[menu-check] consumable-bought-twice=2 coins=0 saved=2 failures=0\n");
    CProfileManager previewProfile;
    previewProfile.Reset(core, refinement);
    const CProfileManager beforePreview = previewProfile;
    MenuState previewState;
    previewState.page = 2;
    previewState.slot = 2;
    const std::vector<MenuTestClick> previewClicks = {{620, 270}, {-100, -100}};
    if (ShowGameMenu(toc, tables, previewProfile, progress, refinement, store, weapons, armor,
        previewState, "out/menu-preview-profile.dat", "out/game-menu-preview-check.png", &previewClicks) != -2) { return 1; }
    if (previewProfile.coins != beforePreview.coins || previewProfile.warbucks != beforePreview.warbucks ||
        previewProfile.inventory.size() != beforePreview.inventory.size()) { return 1; }
    for (unsigned slot = 0; slot < kArmorSlotCount; ++slot) {
        if (!SameObject(previewProfile.configuration.armor[slot], beforePreview.configuration.armor[slot])) { return 1; }
    }
    std::printf("[menu-check] shop-preview-without-purchase loadout-unchanged=1 failures=0\n");
    CProfileManager original;
    original.Reset(core, refinement);
    if (!ImportOriginalProfile(toc, tables, original)) { return 1; }
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
    activityState.page = 5;
    const std::filesystem::path activityPath = "out/menu-activities-check.dat";
    const std::vector<MenuTestClick> activityClicks = {{780, 294}, {780, 294}, {-100, -100}};
    if (ShowGameMenu(toc, tables, activities, progress, refinement, store, weapons, armor,
        activityState, activityPath, "out/game-menu-activities-check.png", &activityClicks) != -2 ||
        activities.coins != 100 || activities.warbucks != 1 || activities.claimedActivities != 1 || activityState.page != 13) { return 1; }
    restored.Reset(core, refinement);
    if (!restored.LoadFromDisk(activityPath) || restored.ClaimActivity(0) || restored.coins != 100) { return 1; }
    MenuState optionsState;
    optionsState.page = 6;
    const std::vector<MenuTestClick> optionsClicks = {{290, 294}, {290, 350}, {290, 405}, {-100, -100}};
    if (ShowGameMenu(toc, tables, activities, progress, refinement, store, weapons, armor,
        optionsState, activityPath, "out/game-menu-options-check.png", &optionsClicks) != -2 ||
        activities.soundEnabled || activities.musicEnabled || activities.brotherEnabled) { return 1; }
    restored.Reset(core, refinement);
    if (!restored.LoadFromDisk(activityPath) || restored.soundEnabled || restored.musicEnabled || restored.brotherEnabled) { return 1; }
    MenuState nestedState;
    nestedState.page = 6;
    const std::vector<MenuTestClick> nestedClicks = {{370, 463}, {250, 561}, {880, 319},
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
    const std::vector<MenuTestClick> waveClicks = {{100, 257}, {160, 325}, {-100, -100}};
    if (ShowGameMenu(toc, tables, waveProfile, progress, refinement, store, weapons, armor,
        waveState, "out/wave-menu-check.dat", "out/game-menu-waves-check.png", &waveClicks) != -2 ||
        waveState.startingWave != 1) { return 1; }
    waveState.startingWave = 50;
    const std::vector<MenuTestClick> lockedWaveClicks = {{910, 255}, {620, 325}, {-100, -100}};
    if (ShowGameMenu(toc, tables, waveProfile, progress, refinement, store, weapons, armor,
        waveState, "out/wave-menu-check.dat", "out/game-menu-waves-locked-check.png", &lockedWaveClicks) != -2 ||
        waveState.startingWave != 50 || waveProfile.clearedWaves[0] != 55) { return 1; }
    std::printf("[menu-check] previous-revolution-replay=1 future-revolution-and-wave-locked=1\n");
    CProfileManager currencyProfile;
    currencyProfile.Reset(core, refinement);
    MenuState bankState;
    bankState.page = 17;
    const std::vector<MenuTestClick> bankClicks = {{800, 357}, {680, 575}, {-100, -100}};
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
    if (!LoadPlayerProgress(toc, tables, progress) || !LoadRefinementTemplate(toc, tables, refinement) ||
        !LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadArmorCatalog(toc, tables, armor)) { return 1; }
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
    state.page = std::min(page, 19u);
    if (page == 0 && screenshotPath.empty()) { state.page = 14; }
    while (true) {
        const int choice = ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, savePath, screenshotPath, nullptr, originalProfile);
        if (choice == -3) { return 1; }
        if (choice == -2) { return 0; }
        if (choice < 0) { return !profile.SaveToDisk(savePath); }
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
            state.message = "HORDE RECORD SAVED";
            continue;
        }
        unsigned wave = profile.clearedWaves[choice];
        if (wave >= 500) { wave = 0; }
        if (state.startingWave >= 0) { wave = static_cast<unsigned>(state.startingWave); }
        SurvivalGameContext context{profile, savePath, static_cast<unsigned>(choice)};
        if (RunSurvival(bigDirectory, kPlanetPacks[choice], kPlanetMaps[choice], 0, -1, "", 0, false, false, false, 2, wave, &context, profile.brotherEnabled) != 0) { return 1; }
        state.message = "PROGRESS SAVED";
    }
}
