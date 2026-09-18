#include "gun_bros_re/gameplay/map/CRenderQueue.h"
#include "gun_bros_re/gameplay/map/CCameraDrawing.h"
#include "gun_bros_re/gameplay/map/CMapEffects.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "engine/graphics/ZQuadBatch.h"
#include "engine/platform/ZGLLoader.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "engine/graphics/CMeshCamera.h"
using namespace MapDetail;

/** CRenderQueue::Compare :145029: group, then integer world Y. */
/** Order props the way CRenderQueue does: by group, then down the screen. */
bool CRenderQueue::Compare(const Item &left, const Item &right) {
    if (left.group != right.group) { return left.group < right.group; }
    return left.y < right.y;
}

/** Shared main/foreground passes for the game and permanent map viewers.
 * CRenderQueue::Draw :145123 puts actors and scenery in the SAME main pass.
 * The caller has already drawn tiles and every prop's background slot.
 */
void CRenderQueue::Draw(CMap &loaded, ZQuadBatch &batch, const ZShaderProgram &program,
                const float *mapMvp, bool showProps , CLevel *scene ,
                CBrother *brotherModel , float brotherY , int viewportWidth ) {
    std::vector<Item> items;
    items.reserve(loaded.GetResources().props.size() + loaded.GetResources().enemies.size() + loaded.GetResources().players.size());
    if (showProps) {
        for (const CProp &prop : loaded.GetResources().props) {
            Item item;
            item.group = prop.GetZOrderGroup();
            item.y = static_cast<int>(prop.y);
            item.prop = &prop;
            items.push_back(item);
        }
    }

    // The quad batch sets a blend FUNCTION per group but never touches the
    // enable, which is switched on once at start-up and stays on for the whole
    // frame. So only the function is ours to set, and it has to be set: the
    // last sprite group may have left an additive one behind. Switching
    // blending off instead is wrong twice over -- the sprites drawn afterwards
    // lose their alpha and turn into black rectangles, and a model's own
    // ground-shadow disc goes opaque white.
    for (std::size_t i = 0; i < loaded.GetResources().enemies.size(); ++i) {
        CEnemy &placed = *loaded.GetResources().enemies[i];
        const float scale = placed.GetWorldScale(placed.data->gameScale, kLevelCameraScale);

        Item item;
        item.y = static_cast<int>(placed.combat.y);
        item.enemy = &placed;
        placed.BuildGameMatrix(mapMvp, placed.combat.x, placed.combat.y, scale, 0.0f, item.matrix);
        items.push_back(item);
    }

    for (std::size_t i = 0; i < loaded.GetResources().players.size(); ++i) {
        CMap::Resources::Player &placed = loaded.GetResources().players[i];
        const float scale = placed.model->GetWorldScale(loaded.GetResources().playerTemplate->GetGameScale(), kLevelCameraScale);

        Item item;
        item.y = static_cast<int>(placed.y);
        item.player = placed.model.get();
        MeshCameraBuildGameMatrix(mapMvp, placed.x, placed.y, scale,
                              placed.facingDegrees, item.matrix);
        items.push_back(item);
    }
    if (scene != nullptr) {
        // CParticleSystem::QueueParticles :133841 adds individual particles,
        // allowing their authored group and world Y to interleave with actors.
        for (const auto &particle : scene->GetMapParticleItems()) {
            Item item;
            item.group = particle.group;
            item.y = particle.y;
            item.particle = particle;
            items.push_back(item);
        }
        if (brotherModel != nullptr) {
            Item item;
            item.y = static_cast<int>(brotherY);
            item.player = brotherModel;
            float world[kMatrix4dElements];
            scene->BrotherMatrix(world);
            Matrix4dMultiply(mapMvp, world, item.matrix);
            items.push_back(item);
        }
        for (const auto &actor : scene->GetEnemies()) {
            Item item;
            item.y = static_cast<int>(actor->combat.y);
            item.enemy = &*actor;
            float world[kMatrix4dElements];
            scene->EnemyMatrix(*actor, world);
            Matrix4dMultiply(mapMvp, world, item.matrix);
            // Original stun shake is a screen-pixel draw offset, never collision motion.
            item.matrix[3] += 2.0f * actor->stun.GetOffset() / viewportWidth;
            items.push_back(item);
        }
    }
    std::stable_sort(items.begin(), items.end(), CRenderQueue::Compare);
    glDisable(GL_DEPTH_TEST);
    batch.Begin();
    for (const Item &item : items) {
        if (item.prop != nullptr) {
            item.prop->DrawSlot(batch, 1);
            continue;
        }
        // Flush the preceding 2D run before submitting this model.
        if (batch.GetQuadCount() != 0) {
            batch.Upload();
            batch.Draw(program, mapMvp);
            batch.Begin();
        }
        if (item.particle.player != nullptr) {
            scene->DrawMapParticle(item.particle, mapMvp);
            continue;
        }
        // CMeshCamera::DrawHeirarchy :99263 clears depth per hierarchy.
        // Depth resolves parts of this model; cross-object order belongs to the queue.
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (item.player != nullptr) { item.player->Draw(program, item.matrix); }
        if (item.enemy != nullptr) { (*item.enemy).Draw(program, item.matrix); }
        glDisable(GL_DEPTH_TEST);
    }
    if (showProps) {
        // Explosion/shockwave z=3 and cover debris z=5 sit above bodies.
        AddParticleQuads(loaded, batch, 3, 5);
        for (const Item &item : items) {
            if (item.prop != nullptr) { item.prop->DrawSlot(batch, 2); }
        }
    }
    batch.Upload();
    batch.Draw(program, mapMvp);
    // Put back what was found: the sprite path draws flat and in order.
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
void CRenderQueue::DrawBackground(const CMap &map, ZQuadBatch &batch) {
    std::vector<Item> items;
    items.reserve(map.GetResources().props.size());
    for (const CProp &prop : map.GetResources().props) {
        Item item;
        item.group = prop.GetZOrderGroup();
        item.y = prop.GetZOrder();
        item.prop = &prop;
        items.push_back(item);
    }
    std::stable_sort(items.begin(), items.end(), Compare);
    for (const Item &item : items) { item.prop->DrawSlot(batch, 0); }
}
