#include "gun_bros_re/ui/ZMenuInternal.h"

namespace MenuDetail {

    bool ZGameMenu::Open(CResTOCManager &toc, ZPackTables &tables, const CProfileManager *profile , bool startup , CBGM *music ) {
        resourceToc = &toc;
        resourceTables = &tables;
        if (!window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return false; }
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
            const ZMenuDataEntry *entry = FindMenuData("MDS_BUTTON_TRUNK", index);
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
            const ZSpriteArchetype *large = glu.GetArchetype(planet.largeImage.archetype);
            const ZSpriteArchetype *thumb = glu.GetArchetype(planet.thumbnail.archetype);
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

    void ZGameMenu::Begin(unsigned page ) {
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

    bool ZGameMenu::TitleImage() {
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

    bool ZGameMenu::Icon(CResTOCManager &toc, ZPackTables &tables, const ZStoreEntry &entry, float x, float y, float width,
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
    bool ZGameMenu::DrawCasualty(ZPackTables &tables, CResTOCManager &toc, const ZEnemyCasualty &casualty, float x,
        const ZMovieRegion *originalRegion ) {
        const std::uint64_t key = (static_cast<std::uint64_t>(casualty.resource.packHash) << 8) | casualty.resource.localIndex;
        if (enemyPreviews.count(key) == 0) {
            auto preview = std::make_unique<EnemyPreview>();
            if (!ReadEnemyTemplate(tables, casualty.resource.packHash, casualty.resource.localIndex, casualty.name, preview->data) ||
                !LoadEnemyModel(tables, preview->data, true, &imageProgram, ZEnemySpawnMode::Menu, preview->model)) { return false; }
            // The first ENEMY asset is its localized name, before its script.
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(casualty.resource.packHash, ZGameSection::Enemy, casualty.resource.localIndex, bytes)) { return false; }
            CArrayInputStream stream(bytes);
            stream.ReadUInt8();
            CGameAssetRef nameRef;
            nameRef.Init(stream);
            preview->name = ReadGameString(toc, nameRef);
            enemyPreviews[key] = std::move(preview);
        }
        EnemyPreview &preview = *enemyPreviews[key];
        if (originalRegion != nullptr) {
            const ZMovieRegion &region = *originalRegion;
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

    /** PlanetImageCallback :188957 retains original sprite geometry and origin. */
    void ZGameMenu::DrawPlanetOriginal(unsigned index, const ZMovieRegion &region) {
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
    ZMovieRegion ZGameMenu::DrawPlanetThumb(unsigned index, const ZMovieRegion &region, float fade ) {
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

    void ZGameMenu::PlanetFlagLines(float centerX, float centerY, const ZMovieRegion &area) {
        // FlagPoleCallback :161369 uses native color 0x807CC9F3 and 1px lines.
        markers.Begin();
        markers.AddSegment(centerX, centerY, area.x, area.y, 1);
        markers.AddSegment(centerX, centerY, area.x + area.width, area.y, 1);
        markers.AddSegment(centerX, centerY, area.x, area.y + area.height, 1);
        markers.AddSegment(centerX, centerY, area.x + area.width, area.y + area.height, 1);
        markers.Draw(textProgram, projection, 124.0f / 255, 201.0f / 255, 243.0f / 255, area.alpha * 0.5f);
        movies.Rectangle(area.x, area.y, area.width, area.height, 0, 0, 0, area.alpha * 0.5f);
    }

    void ZGameMenu::PrepareParticleResources() {
        if (!particleResources) { particleResources = std::make_unique<ZParticleResources>(*resourceTables); }
        if (!particleRenderer) { particleRenderer = std::make_unique<ZSpriteRenderer>(*resourceToc, imageProgram); }
    }

    bool ZGameMenu::PrepareModeEffects() {
        if (modeEffects[0]) { return true; }
        PrepareParticleResources();
        static const struct { const char *pack; int ordinals[2]; } binding =
#include "gun_bros_re/ui/CMenuModeParticleData.inc"
        ;
        const int pack = resourceToc->GetPackIndexFromName(binding.pack);
        if (pack < 0) { return false; }
        for (unsigned index = 0; index < 2; ++index) {
            if (binding.ordinals[index] < 0) { return false; }
            modeEffectRefs[index].packHash = resourceToc->GetPack(pack)->GetPackHash();
            modeEffectRefs[index].localIndex = static_cast<std::uint8_t>(binding.ordinals[index]);
            const auto *data = particleResources->Get(modeEffectRefs[index]);
            if (data == nullptr) { return false; }
            modeEffects[index] = std::make_unique<CParticleEffectPlayer>();
            modeEffects[index]->Init(*data, particlePool);
            modeEffects[index]->SetLooping(data->GetDurationMs() == 0);
        }
        // Bind stops the selection burst; the second emitter runs continuously.
        modeEffects[0]->Stop();
        return true;
    }

    void ZGameMenu::DrawModeEffects(const ZMovieRegion &label) {
        // LabelCallback :250137 positions the burst at label top-center and
        // the persistent glow at its center, before drawing the text itself.
        for (unsigned index = 0; index < 2; ++index) {
            float transform[16];
            std::copy(movies.CurrentProjection(), movies.CurrentProjection() + 16, transform);
            float y = label.y;
            if (index == 1) { y += label.height / 2; }
            Matrix4dTranslate(transform, label.x + label.width / 2, y);
            modeEffects[index]->Draw(*particleRenderer, transform);
        }
    }

    /** CMenuPostGameOption::Bind/Update/Draw :249755..249964 owns one
     * particle player per card, behind the centered icon. */
    bool ZGameMenu::AdvancePostGameEffect(unsigned index, unsigned elapsed) {
        static const struct { const char *pack; int ordinals[7]; } binding =
#include "gun_bros_re/ui/CMenuPostGameParticleData.inc"
        ;
        static const struct { const char *pack; int ordinals[9]; } liveBinding =
#include "gun_bros_re/ui/CMenuLivePostGameParticleData.inc"
        ;
        PrepareParticleResources();
        if (index >= postGameEffects.size()) { return false; }
        if (!postGameEffects[index]) {
            const char *packName = binding.pack;
            int ordinal = 0;
            if (index < 7) { ordinal = binding.ordinals[index]; }
            else { packName = liveBinding.pack; ordinal = liveBinding.ordinals[index - 7]; }
            const int pack = resourceToc->GetPackIndexFromName(packName);
            if (pack < 0 || ordinal < 0) { return false; }
            GameObjectRef resource;
            resource.packHash = resourceToc->GetPack(pack)->GetPackHash();
            resource.localIndex = static_cast<std::uint8_t>(ordinal);
            const auto *data = particleResources->Get(resource);
            if (data == nullptr) { return false; }
            auto effect = std::make_unique<CParticleEffectPlayer>();
            effect->Init(*data, particlePool);
            // CParticleEffectPlayer's constructor enables looping (:131269).
            effect->SetLooping(true);
            postGameEffects[index] = std::move(effect);
        }
        postGameEffects[index]->Update(elapsed, particleRandom);
        return true;
    }

    bool ZGameMenu::StartRefineryEffect(unsigned slot, unsigned icon, float x, float y) {
        static const struct { const char *pack; int ordinals[4]; } binding =
#include "gun_bros_re/ui/CMenuRefineryParticleData.inc"
        ;
        if (slot >= refineryEffects.size() || icon >= std::size(binding.ordinals)) { return false; }
        const int pack = resourceToc->GetPackIndexFromName(binding.pack);
        if (pack < 0 || binding.ordinals[icon] < 0) { return false; }
        GameObjectRef resource;
        resource.packHash = resourceToc->GetPack(pack)->GetPackHash();
        resource.localIndex = static_cast<std::uint8_t>(binding.ordinals[icon]);
        PrepareParticleResources();
        const auto *data = particleResources->Get(resource);
        if (data == nullptr) { return false; }
        auto &effect = refineryEffects[slot];
        effect.player = std::make_unique<CParticleEffectPlayer>();
        // SetupTransfer :174510/:174534 uses the original ICON_STANDARD particle.
        // Positions are local to CTransferEffect::Draw, including living particles.
        effect.player->Init(*data, particlePool);
        effect.player->SetLooping(true);
        effect.x = x;
        effect.y = y;
        return true;
    }

    void ZGameMenu::AdvanceRefineryEffects(unsigned elapsed) {
        for (auto &effect : refineryEffects) {
            if (effect.player) { effect.player->Update(elapsed, particleRandom); }
        }
    }

    void ZGameMenu::MoveRefineryEffect(unsigned slot, float x, float y) {
        refineryEffects[slot].x = x;
        refineryEffects[slot].y = y;
    }

    void ZGameMenu::StopRefineryEffect(unsigned slot) {
        auto &effect = refineryEffects[slot];
        // CTransferEffect::Update :174361 calls StopSpawning, not Clear.
        // StopEffect detaches the emitter while its living particles expire.
        // Correction: that old host name now explicitly becomes StopSpawning.
        if (effect.player) { effect.player->StopSpawning(); }
    }

    void ZGameMenu::DrawRefineryEffects() {
        for (auto &effect : refineryEffects) {
            if (!effect.player) { continue; }
            float transform[16];
            std::copy(movies.CurrentProjection(), movies.CurrentProjection() + 16, transform);
            Matrix4dTranslate(transform, effect.x, effect.y);
            effect.player->Draw(*particleRenderer, transform);
        }
    }

    void ZGameMenu::Scroll(ZMenuScrollMotion &motion, float &position, const ZMovieRegion &viewport,
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
