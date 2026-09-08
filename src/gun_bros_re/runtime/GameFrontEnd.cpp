/** @file GameFrontEnd.cpp
 * @brief Connect the rebuilt offline account to actual gameplay.
 */
#define NOMINMAX
#include "runtime/GameFrontEnd.h"
#include "runtime/OriginalProfile.h"
#include "runtime/StoreCatalog.h"
#include "runtime/WeaponCatalog.h"
#include "runtime/ArmorCatalog.h"
#include "runtime/PowerupCatalog.h"
#include "runtime/PlayerModel.h"
#include "runtime/HudText.h"
#include "runtime/SurvivalGameContext.h"
#include "gun_bros/Planet.h"
#include "milestones/M3Map.h"
#include "engine/CQuadBatch.h"
#include "engine/CMatrix4d.h"
#include "sprite_glu/CSpriteGlu.h"
#include "sprite_glu/CSpriteIterator.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <cstdio>

namespace {
constexpr const char *kPlanetPacks[] = {"pack2", "pack7", "pack9", "pack12"};
constexpr unsigned kPlanetMaps[] = {7, 6, 0, 0};
constexpr const char *kPageNames[] = {"PLANETS", "EQUIPMENT", "SHOP", "REFINERY"};
constexpr const char *kSlotNames[] = {"WEAPON 1", "WEAPON 2", "HELMET", "ARMOR", "PANTS", "ITEMS"};
constexpr unsigned kArmorSlots[] = {0, 0, 2, 1, 0};
constexpr float kMenuWidth = 1024;
constexpr float kMenuHeight = 768;

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
};

struct MenuTestClick { float x; float y; };

/** All GL owners are destroyed before the menu window's context. */
class GameMenu {
public:
    /** Integration harness input; it still goes through rendered button hit tests. */
    void SetTestClick(const MenuTestClick &click) { mouseX = click.x; mouseY = click.y; clicked = true; }
    bool Open(CResTOCManager &toc, PackTables &tables) {
        if (!window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return false; }
        const char *directory = ASSET_ROOT "/src/gun_bros_re/shaders";
        if (!textProgram.Load(directory, "ogles_vs_mvp_constcolor", "ogles_ps_constcolor") ||
            !imageProgram.Load(directory, "ogles_vs_mvp_tex0", "ogles_ps_tex0") ||
            !markers.Create(textProgram) || !images.Create(imageProgram)) { return false; }
        Matrix4dOrthoTopLeft(kMenuWidth, kMenuHeight, 100, projection);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (unsigned index = 0; index < 4; ++index) {
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

    void Begin() {
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
        markers.Begin();
        for (unsigned index = 0; index < 70; ++index) {
            markers.AddRect(static_cast<float>((index * 173 + 17) % 1024),
                static_cast<float>((index * 83 + 59) % 768), 1.5f, 1.5f);
        }
        markers.Draw(textProgram, projection, 0.37f, 0.46f, 0.56f, 0.35f);
    }

    void Rect(float x, float y, float width, float height, float r = 0.155f, float g = 0.227f, float b = 0.29f) {
        markers.Begin();
        markers.AddRect(x, y, width, height);
        markers.Draw(textProgram, projection, r, g, b, 1);
    }

    void Text(float x, float y, const std::string &text, float size = 2,
        float r = 0.88f, float g = 0.93f, float b = 0.95f) {
        markers.Begin();
        DrawHudText(markers, x, y, text, size);
        markers.Draw(textProgram, projection, r, g, b, 1);
    }

    bool Button(float x, float y, float width, float height, const std::string &label, bool selected = false) {
        const bool hover = mouseX >= x && mouseX < x + width && mouseY >= y && mouseY < y + height;
        float shade = 0.155f;
        if (hover) { shade = 0.25f; }
        Rect(x, y, width, height, shade, shade + 0.07f, shade + 0.12f);
        if (selected) { Rect(x, y, 4, height, 0.93f, 0.74f, 0.33f); }
        if (!label.empty()) {
            const float size = std::min(2.0f, (width - 20) / (label.size() * 6.0f));
            Text(x + 12, y + (height - size * 7) * 0.5f, label, size);
        }
        return hover && clicked;
    }

    void DrawPlanet(unsigned index) {
        const auto &quads = planetQuads[index];
        if (quads.empty()) { return; }
        float left = 100000, top = 100000, right = -100000, bottom = -100000;
        for (const SpriteQuad &quad : quads) {
            left = std::min(left, static_cast<float>(quad.offsetX));
            top = std::min(top, static_cast<float>(quad.offsetY));
            right = std::max(right, static_cast<float>(quad.offsetX + quad.Width()));
            bottom = std::max(bottom, static_cast<float>(quad.offsetY + quad.Height()));
        }
        const float scale = std::min(390 / (right - left), 380 / (bottom - top));
        images.Begin();
        for (const SpriteQuad &quad : quads) {
            const float x = 737 + (quad.offsetX - (left + right) * 0.5f) * scale;
            const float y = 365 + (quad.offsetY - (top + bottom) * 0.5f) * scale;
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
        const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armors, unsigned slot) {
        unsigned gunSlot = 0;
        if (slot == 1) { gunSlot = 1; }
        bool changed = equippedPreview == nullptr || previewGunSlot != gunSlot;
        if (!SameObject(previewConfiguration.guns[gunSlot], profile.configuration.guns[gunSlot])) { changed = true; }
        for (unsigned index = 0; index < kArmorSlotCount; ++index) {
            if (!SameObject(previewConfiguration.armor[index], profile.configuration.armor[index])) { changed = true; }
        }
        if (changed) {
            PlayerTemplateData playerTemplate;
            if (!FindPlayerTemplate(toc, tables, playerTemplate)) { return false; }
            auto candidate = std::make_unique<PlayerModel>();
            if (!BuildPlayerBody(tables, playerTemplate.moveSet, *candidate)) { return false; }
            const WeaponEntry *gun = nullptr;
            for (const WeaponEntry &entry : weapons) {
                const auto &ref = profile.configuration.guns[gunSlot];
                if (entry.packHash == ref.packHash && entry.ordinal == ref.localIndex) { gun = &entry; break; }
            }
            if (gun == nullptr || !EquipPlayerWeapon(tables, playerTemplate.script, gun->data, gun->owner, *candidate)) { return false; }
            for (unsigned index = 0; index < kArmorSlotCount; ++index) {
                const auto &ref = profile.configuration.armor[index];
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
            previewConfiguration = profile.configuration;
            previewGunSlot = gunSlot;
            previewTicks = window.GetTicksMs();
        }
        Rect(20, 414, 180, 280, 0.035f, 0.07f, 0.10f);
        Text(38, 432, "EQUIPPED", 1.75f, 0.93f, 0.74f, 0.33f);
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
    std::string names[4];
private:
    CShaderProgram textProgram;
    CShaderProgram imageProgram;
    CMarkerBatch markers;
    CQuadBatch images;
    float projection[16]{};
    float mouseX = 0, mouseY = 0;
    bool clicked = false, previousDown = false;
    Planet planets[4];
    std::vector<SpriteQuad> planetQuads[4];
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
    CPlayerProgress progress;
    progress.Bind(progressData);
    progress.SetExperience(profile.experience);
    unsigned testFrame = 0;
    while (view.window.PumpEvents()) {
        bool activate = false;
        const unsigned previousPage = state.page;
        for (KeyCode key = view.window.TakeKeyPress(); key != KeyCode::None; key = view.window.TakeKeyPress()) {
            if (key == KeyCode::Space) { activate = true; }
            if (state.page == 0 && key == KeyCode::Down) { state.planet = (state.planet + 1) % 4; }
            if (state.page == 0 && key == KeyCode::Up) { state.planet = (state.planet + 3) % 4; }
            if (key == KeyCode::P) { state.page = 0; }
            if (key == KeyCode::E) { state.page = 1; }
            if (key == KeyCode::B) { state.page = 2; }
            if (key == KeyCode::F) { state.page = 3; }
        }
        if (previousPage != state.page) { state.itemPage = 0; state.selectedItem = -1; state.message.clear(); }
        const std::int64_t now = CurrentSeconds();
        profile.refinery.UpdateRefinement(now);
        view.Begin();
        if (testClicks != nullptr && testFrame < testClicks->size()) { view.SetTestClick((*testClicks)[testFrame]); }
        view.Rect(0, 0, 1024, 94, 0.08f, 0.145f, 0.21f);
        view.Text(26, 22, "GUN BROS", 5, 0.93f, 0.74f, 0.33f);
        if (originalProfile) { view.Text(26, 67, "ORIGINAL SAVE LAB", 1.4f); }
        else { view.Text(26, 67, "SURVIVAL", 1.7f); }
        view.Text(375, 24, "LEVEL " + std::to_string(progress.GetLevel()), 2.1f);
        view.Text(375, 56, "XP " + std::to_string(progress.GetExperienceInLevel()) + "/" + std::to_string(progress.GetExperienceDelta()), 1.6f);
        view.Text(580, 24, "COINS " + std::to_string(profile.coins), 2);
        view.Text(580, 56, "XPLODIUM " + std::to_string(profile.xplodium), 1.6f);
        view.Text(811, 24, "WARBUCKS", 1.8f);
        view.Text(811, 56, std::to_string(profile.warbucks), 1.8f);
        for (unsigned page = 0; page < 4; ++page) {
            if (view.Button(20, 128 + page * 64.0f, 180, 51, kPageNames[page], state.page == page)) {
                state.page = page;
                state.itemPage = 0;
                state.selectedItem = -1;
                state.message.clear();
            }
        }
        view.Text(23, 444, "P  PLANETS\nE  EQUIPMENT\nB  SHOP\nF  REFINERY", 1.7f);
        view.Text(23, 590, "IN COMBAT\nWASD  MOVE\nMOUSE FIRE\n1/2   WEAPONS", 1.6f);
        if (view.Button(20, 704, 180, 40, "QUIT")) { return -1; }

        if (state.page == 0) {
            view.Text(240, 124, "CHOOSE YOUR PLANET", 3);
            for (unsigned planet = 0; planet < 4; ++planet) {
                if (view.Button(240, 205 + planet * 83.0f, 276, 61, view.names[planet], state.planet == planet)) { state.planet = planet; }
                view.Text(252, 269 + planet * 83.0f, std::to_string(profile.clearedWaves[planet]) + " / 500 WAVES CLEARED", 1.3f);
            }
            view.DrawPlanet(state.planet);
            view.Text(560, 563, "50 WAVES  /  10 REVOLUTIONS", 1.7f);
            const unsigned cleared = profile.clearedWaves[state.planet];
            unsigned nextWave = cleared + 1;
            if (cleared >= 500) { nextWave = 1; }
            view.Text(240, 629, "CONTINUE AT WAVE " + std::to_string(nextWave), 2);
            view.Text(240, 666, "Progress saves after waves and when you leave.", 1.5f);
            if (view.Button(754, 619, 234, 66, "DEPLOY", true) || activate) { return static_cast<int>(state.planet); }
        }

        if (state.page == 1 || state.page == 2) {
            if (!view.DrawEquippedPlayer(toc, tables, profile, weapons, armors, state.slot)) { return -3; }
            view.Text(240, 122, kPageNames[state.page], 3);
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
            view.Text(240, 124, "REFINE XPLODIUM INTO COINS", 2.7f);
            view.Text(240, 170, "Instant refining is free. Longer batches yield more coins.", 1.55f);
            view.Text(240, 201, "ADVANCED - UNLOCK WITH WARBUCKS", 1.3f);
            view.Text(624, 201, "STANDARD - USE THE PREVIOUS BATCH", 1.3f);
            for (unsigned index = 0; index < kRefinementSlotCount; ++index) {
                const float x = 240 + (index / 6) * 384.0f;
                const float y = 239 + (index % 6) * 75.0f;
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
        if (!state.message.empty()) { view.Text(240, 722, state.message, 1.45f, 0.93f, 0.74f, 0.33f); }
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
        {100, 345}, {510, 250}, {100, 280}, {620, 270}, {880, 655},
        {900, 390}, {880, 655}, {460, 180}, {620, 270}, {880, 655},
        {100, 150}, {400, 320}
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
    return 0;
}

int RunGameFrontEnd(const std::string &bigDirectory, const std::string &screenshotPath, unsigned page, bool originalProfile) {
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
    const std::filesystem::path savePath = std::filesystem::path(ASSET_ROOT) / "userdata" / profileName;
    if (originalProfile && !std::filesystem::exists(savePath)) {
        if (!ImportOriginalProfile(toc, tables, profile) || !profile.SaveToDisk(savePath)) { return 1; }
    }
    if (!profile.LoadFromDisk(savePath)) {
        std::printf("[game] profile cannot be loaded; original file preserved: %s\n", savePath.string().c_str());
        return 1;
    }
    MenuState state;
    state.page = std::min(page, 3u);
    while (true) {
        const int choice = ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, savePath, screenshotPath, nullptr, originalProfile);
        if (choice == -3) { return 1; }
        if (choice == -2) { return 0; }
        if (choice < 0) { return !profile.SaveToDisk(savePath); }
        unsigned wave = profile.clearedWaves[choice];
        if (wave >= 500) { wave = 0; }
        SurvivalGameContext context{profile, savePath, static_cast<unsigned>(choice)};
        if (RunSurvival(bigDirectory, kPlanetPacks[choice], kPlanetMaps[choice], 0, -1, "", 0, false, false, false, 2, wave, &context, true) != 0) { return 1; }
        state.message = "PROGRESS SAVED";
    }
}
