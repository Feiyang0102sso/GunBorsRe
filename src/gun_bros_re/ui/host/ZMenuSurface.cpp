#include "gun_bros_re/host/ZHostSettings.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/host/ZLoadingScreen.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
#include "gun_bros_re/startup/ZStartupSequence.h"

namespace MenuDetail {

    bool ZMenuSurface::Open(CResTOCManager &toc, CGunBros &tables, const CProfileManager *profile , bool startup , CBGM *music ) {
        particles.Bind(toc, tables, imageProgram);
        playerPreview.Bind(tables);
        if (!window.Open(GameHostSettings().title, kDefaultWindowWidth, kDefaultWindowHeight)) { return false; }
        window.SetEscapeCloses(false);
        window.EnableCheats(true);
        const char *directory = Paths::Shaders().c_str();
        if (!textProgram.Load(directory, "ogles_vs_mvp_constcolor", "ogles_ps_constcolor") ||
            !imageProgram.Load(directory, "ogles_vs_mvp_tex0", "ogles_ps_tex0") ||
            !markers.Create(textProgram) || !images.Create(imageProgram)) { return false; }
        Matrix4dOrthoTopLeft(kMenuWidth, kMenuHeight, 100, projection);
        CResPackTOC *core = toc.GetPack(toc.GetCorePackIndex());
        if (!movies.Init(*core, *core)) { return false; }
        const auto *belt = movies.GetMovie(movies.Ordinal("GLU_MOVIE_STORE_SCROLL"));
        unsigned beltEnd = 0;
        if (!belt || !belt->GetChapterRange(1, storeRestTime, beltEnd)) { return false; }
        ZLoadingScreen loading(window, movies, tables, profile, false, startup, music);
        if (!loading.IsValid()) { return false; }
        for (unsigned index = 0; index < 7; ++index) {
            const CMenuDataProvider::Entry *entry = CMenuDataProvider::Find("MDS_BUTTON_TRUNK", index);
            if (entry == nullptr) { return false; }
            std::printf("[navigation] index=%u label=%s sprite=%u\n", index,
                movies.NamedString(entry->strings[0]).c_str(), entry->sprites[0]);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (!planets.Load(toc, tables, imageProgram, textProgram)) { return false; }
        return loading.IsValid() && !loading.Cancelled();
    }

    void ZMenuSurface::Begin() {
        injectedClick = false;
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        glViewport(0, 0, width, height);
        // CGameApp::HandleRender :54611 sets opaque black before menu drawing.
        glClearColor(0, 0, 0, 1);
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
        pointerReleased = !down && previousDown;
        pointerHeld = down;
        dragDistance += std::abs(dragX) + std::abs(dragY);
        // CMenuMission handles selection on release; dragging must never enter a planet.
        clicked = !down && previousDown && dragDistance < 9;
        previousDown = down;

        if (scripted) {
            pointerPressed = false;
            pointerReleased = false;
            pointerHeld = false;
            dragX = 0;
            dragY = 0;
            dragDistance = 0;
            window.TakeWheelDelta();
        }

        // Backgrounds belong to each native menu's Draw/Init binding.
        // Shared Movie 4 is the star map, not a universal menu backdrop.
    }

    bool ZMenuSurface::TitleImage() {
        if (!titleImage.IsValid()) {
            if (!LoadStartupSplash(titleImage)) { return false; }
        }
        images.Begin();
        const ZSourceRect source{0, 0, static_cast<std::uint16_t>(titleImage.GetWidth()), static_cast<std::uint16_t>(titleImage.GetHeight())};
        images.AddQuad(titleImage, 0, 0, 1024, 768, source, false, false, ZBlendMode::Alpha);
        images.Upload();
        images.Draw(imageProgram, projection);
        return true;
    }

    // Historical explicit .dat research UI; native profiles use Header below.

    bool ZMenuSurface::Icon(CResTOCManager &toc, CGunBros &tables, const CStoreItem::Entry &entry, float x, float y, float width,
        float height, float alpha , bool originalSize , bool fitHeight, bool alignRight, float *renderedWidth ) {
        const CGameAssetRef &ref = entry.data.assets[1];
        if (ref.assetId < 0 || ref.IsNull()) { return false; }
        const std::uint64_t key = (static_cast<std::uint64_t>(ref.packHash) << 32) | static_cast<unsigned>(ref.assetId);
        if (icons.count(key) == 0) {
            std::vector<std::uint8_t> payload;
            ZPNGImage decoded;
            auto texture = std::make_unique<ZTexture>();
            if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Png, ref.assetId, payload) ||
                !PNGDecode(payload, decoded) || !texture->Create(decoded)) { return false; }
            icons[key] = std::move(texture);
        }
        const ZTexture &texture = *icons[key];
        float scale = std::min(width / texture.GetWidth(), height / texture.GetHeight());
        // CMenuStoreOption::ThumbCallback :181036 preserves the PNG dimensions.
        // A thumbnail wider than region 5 starts at its left edge.
        if (originalSize) { scale = 1; }
        // CMenuGreeting::BonusIconCallback :208122 scales by region HEIGHT,
        // including its 16.16 truncation, instead of fitting both dimensions.
        if (fitHeight) { scale = std::floor(height * 65536 / texture.GetHeight()) / 65536; }
        const float drawnWidth = texture.GetWidth() * scale;
        if (renderedWidth) { *renderedWidth = std::floor(drawnWidth); }
        const float drawnHeight = texture.GetHeight() * scale;
        float drawnX = x + (width - drawnWidth) * 0.5f;
        // CMenuChallenges::RewardCallback :235381 aligns the scaled image right.
        if (alignRight) { drawnX = x + width - drawnWidth; }
        if (originalSize && drawnWidth > width) { drawnX = x; }
        const ZSourceRect source{0, 0, static_cast<std::uint16_t>(texture.GetWidth()), static_cast<std::uint16_t>(texture.GetHeight())};
        images.Begin();
        images.AddTransformedQuad(texture, drawnX, y + (height - drawnHeight) * 0.5f,
            drawnWidth, drawnHeight, source, false, false, ZBlendMode::Alpha, 0, 0, 1, 1, 0, alpha);
        images.Upload();
        images.Draw(imageProgram, movies.CurrentProjection());
        return true;
    }

    /** CEnemy::SpawnForUI assembles the original result-card model. */
    bool ZMenuSurface::DrawCasualty(CGunBros &tables, CResTOCManager &toc, const CEnemyCasualty &casualty, float x,
        const ZMovieRegion *originalRegion ) {
        const std::uint64_t key = (static_cast<std::uint64_t>(casualty.resource.packHash) << 8) | casualty.resource.localIndex;
        if (enemyPreviews.count(key) == 0) {
            auto preview = std::make_unique<CMenuMeshEnemy>();
            // The first ENEMY asset is its localized name, before its script.
            if (!preview->Bind(tables, toc, casualty.resource, imageProgram)) { return false; }
            enemyPreviews[key] = std::move(preview);
        }
        CMenuMeshEnemy &preview = *enemyPreviews[key];
        if (originalRegion != nullptr) {
            const ZMovieRegion &region = *originalRegion;
            if (region.index == 2) {
                // CMenuMeshOption::TextCallback :176114: two font-0 lines.
                movies.Text(preview.GetName(), region.x + (region.width - movies.TextWidth(preview.GetName(), 0)) / 2,
                    region.y, 0, 1, 0, region.alpha);
                const std::string kills = movies.NamedString("IDS_WRAPUP_KILLS") + std::to_string(casualty.count);
                return movies.Text(kills, region.x + (region.width - movies.TextWidth(kills, 0)) / 2,
                    region.y + movies.TextHeight(0), 0, 1, 0, region.alpha);
            }
            if (region.index != 1) { return true; }
            preview.Update(clock);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_DEPTH_BUFFER_BIT);
            const bool drawn = preview.Draw(imageProgram, region.x, region.y,
                region.width, region.height, kMenuWidth, kMenuHeight);
            glDisable(GL_DEPTH_TEST);
            return drawn;
        }
        return false;
    }

    void ZMenuSurface::Scroll(ZMenuScrollMotion &motion, float &position, const ZMovieRegion &viewport,
        bool enabled, float maximum, float stride, unsigned duration) {
        const bool inside = MouseIn(viewport.x, viewport.y, viewport.width, viewport.height);
        float wheel = 0;
        if (enabled && inside) { wheel = window.TakeWheelDelta(); }
        bool pressed = pointerPressed && inside;
        bool held = pointerHeld;

        if (scripted && dragX != 0) { pressed = inside; held = true; }

        motion.Update(position, clock, dragX, wheel, held, pressed, enabled, maximum, stride, duration);
    }
}
