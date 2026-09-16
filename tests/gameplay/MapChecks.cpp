#include "gun_bros_re/debug/Capture.h"
#include "gameplay/SurvivalStudy.h"
#include "gun_bros_re/gameplay/ZMapWorldInternal.h"
#include "TestOutput.h"
using namespace MapDetail;
  // namespace

/** Ignore only sub-visible RGB rounding when comparing framebuffer captures. */
unsigned CountOcclusionPixelChanges(const std::vector<unsigned char> &first,
                                          const std::vector<unsigned char> &second) {
    unsigned changed = 0;
    for (std::size_t pixel = 0; pixel < first.size(); pixel += 4) {
        int difference = 0;
        for (unsigned channel = 0; channel < 3; ++channel) {
            difference += std::abs(static_cast<int>(first[pixel + channel]) - second[pixel + channel]);
        }
        if (difference > 24) { ++changed; }
    }
    return changed;
}

int RunMapOcclusionCheck(const std::string &bigDirectory) {
    // Fixed research scene; scenery, collision and models still come from BIG.
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    ZWindow window;
    if (!window.Open("Map occlusion check", 768, 768)) { return 1; }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ZShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    ZQuadBatch batch;
    ZQuadBatch cover;
    if (!batch.Create(program) || !cover.Create(program)) { return 1; }
    unsigned failures = 0;
    unsigned occludedPixels = 0;
    const char *packs[] = {"pack2", "pack7", "pack9", "pack12"};
    const unsigned maps[] = {7, 6, 0, 0};
    for (unsigned map = 0; map < 4; ++map) {
        ZLoadedMap loaded;
        if (!LoadMap(toc, toc.GetPackIndexFromName(packs[map]), maps[map], loaded)) { return 1; }
        LoadProps(toc, loaded);
        LoadPlacedPlayers(toc, program, loaded);
        if (loaded.players.empty()) { return 1; }
        // Find a real obstacle whose art extends above its collision footprint.
        std::size_t selected = loaded.props.size();
        float largestOverhangArea = 0;
        const ZMapRectangle bounds = loaded.map.GetVisibleBounds();
        for (std::size_t i = 0; i < loaded.props.size(); ++i) {
            const ZPlacedProp &prop = loaded.props[i];
            if (prop.sprite->data.GetCollision().GetVertices().empty()) { continue; }
            float collisionTop = 0;
            for (const ZCollisionPoint &point : prop.sprite->data.GetCollision().GetVertices()) {
                collisionTop = std::min(collisionTop, point.y);
            }
            // Use walkable interior fixtures, not pieces of the outer map wall.
            if (prop.y + collisionTop - kPlayerCollisionRadius <= bounds.y ||
                prop.x <= bounds.x || prop.x >= bounds.x + bounds.width) { continue; }
            float top = 0, left = 0, right = 0;
            for (const ZSpriteQuad &quad : CurrentQuads(*MainSlotFor(prop), prop.main)) {
                top = std::min(top, static_cast<float>(quad.offsetY));
                left = std::min(left, static_cast<float>(quad.offsetX));
                right = std::max(right, static_cast<float>(quad.offsetX + quad.source.width));
            }
            const float overhangArea = (collisionTop - top) * (right - left);
            if (overhangArea > largestOverhangArea) { largestOverhangArea = overhangArea; selected = i; }
        }
        if (selected == loaded.props.size()) { return 1; }
        ZPlacedProp prop = loaded.props[selected];
        loaded.props.clear();
        loaded.props.push_back(prop);
        loaded.players.resize(1);
        float collisionTop = 0;
        float collisionBottom = 0;
        for (const ZCollisionPoint &point : prop.sprite->data.GetCollision().GetVertices()) {
            collisionTop = std::min(collisionTop, point.y);
            collisionBottom = std::max(collisionBottom, point.y);
        }
        cover.Begin();
        AddSpriteQuads(prop, CurrentQuads(*MainSlotFor(prop), prop.main), cover);
        cover.Upload();
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        glViewport(0, 0, width, height);
        float mvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(static_cast<float>(width), static_cast<float>(height), kMapDepthRange, mvp);
        Matrix4dTranslate(mvp, -prop.x + width * 0.5f, -prop.y + height * 0.5f);
        for (unsigned side = 0; side < 2; ++side) {
            ZPlacedPlayer &player = loaded.players[0];
            player.x = prop.x;
            player.y = prop.y + collisionTop - kPlayerCollisionRadius;
            if (side == 1) { player.y = prop.y + collisionBottom + kPlayerCollisionRadius; }
            BuildGeometry(loaded, batch, true, true, false);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            batch.Draw(program, mvp);
            DrawMapObjects(loaded, batch, program, mvp);
            std::vector<unsigned char> actual(width * height * 4);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
            const std::string path = TestOutput::Path("map-occlusion-") + std::string(packs[map]) + "-" + std::to_string(side) + ".png";
            if (!Capture::SaveFrame(window, path)) { ++failures; }
            // Independent two-object reference: background, ordered bodies, foreground.
            // This also verifies alpha holes; no rectangular occlusion mask is used.
            BuildGeometry(loaded, batch, true, true, false);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            batch.Draw(program, mvp);
            if (side == 1) { cover.Draw(program, mvp); }
            std::vector<unsigned char> withoutPlayer(actual.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, withoutPlayer.data());
            DrawMapObjects(loaded, batch, program, mvp, false);
            if (side == 0) { cover.Draw(program, mvp); }
            batch.Begin();
            AddSpriteQuads(prop, CurrentQuads(*ForegroundSlotFor(prop), prop.foreground), batch);
            batch.Upload();
            batch.Draw(program, mvp);
            std::vector<unsigned char> covered(actual.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, covered.data());
            const unsigned changed = CountOcclusionPixelChanges(actual, covered);
            if (changed != 0) { ++failures; }
            unsigned actorPixels = 0;
            if (side == 0) {
                // Replaying the old actor-last bug must visibly differ from the reference.
                DrawMapObjects(loaded, batch, program, mvp, false);
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, withoutPlayer.data());
                actorPixels = CountOcclusionPixelChanges(withoutPlayer, covered);
                occludedPixels += actorPixels;
            } else {
                actorPixels = CountOcclusionPixelChanges(withoutPlayer, covered);
                if (actorPixels < 100) { ++failures; }
            }
            std::printf("[map-occlusion-check] %s prop=%08X/%u y=%.0f player-y=%.0f side=%u covered-difference=%u failures=%u\n",
                packs[map], prop.sprite->resource.packHash, prop.sprite->resource.localIndex, prop.y, player.y, side, changed, failures);
            std::printf("[map-occlusion-check] side=%u actor-pixels=%u\n", side, actorPixels);
        }
    }
    if (occludedPixels < 1000) { ++failures; }
    std::printf("[map-occlusion-check] hidden-pixels=%u failures=%u\n", occludedPixels, failures);
    if (failures != 0) { return 1; }
    return 0;
}

/** Checkpoint the rebuilt profile only. Research harnesses pass no context. */

int RunBossCheck(const std::string &bigDirectory) {
    // Verified retail Mission -> LEVEL -> map fixtures; production selection
    // still follows the resource references inside RunSurvival.
    const char *packs[] = {"pack2", "pack7", "pack9", "pack12"};
    const unsigned maps[] = {7, 6, 0, 0};
    unsigned failures = 0;
    for (unsigned index = 0; index < 4; ++index) {
        const int result = RunSurvivalStudy(bigDirectory, packs[index], maps[index], 0, -1, "", 0,
            false, false, false, 2, 0, nullptr, false, false, nullptr, false, nullptr, false, true);
        if (result != 0) { ++failures; }
    }
    std::printf("[boss-check] maps=4 failed-maps=%u\n", failures);
    if (failures > 0) { return 1; }
    return 0;
}

int RunPlayerDeathCheck(const std::string &bigDirectory) {
    const char *packs[] = {"pack2", "pack7", "pack9", "pack12"};
    const unsigned maps[] = {7, 6, 0, 0};
    unsigned failures = 0;
    for (unsigned index = 0; index < 4; ++index) {
        if (RunSurvivalStudy(bigDirectory, packs[index], maps[index], 0, -1, "", 0,
            false, false, false, 0, 0, nullptr, true, false, nullptr, false, nullptr, false, false, true) != 0) {
            ++failures;
        }
    }
    std::printf("[death-check] maps=4 failed-maps=%u\n", failures);
    if (failures != 0) { return 1; }
    return 0;
}
