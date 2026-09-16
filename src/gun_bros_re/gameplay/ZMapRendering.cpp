#include "gun_bros_re/gameplay/ZMapWorldInternal.h"
using namespace MapDetail;

namespace MapDetail {

/** Emit one prop's slot, positioned at the prop and offset by each quad. */
void AddSpriteQuads(const ZPlacedProp &prop, const std::vector<ZSpriteQuad> &quads,
                    ZQuadBatch &batch) {
    for (std::size_t i = 0; i < quads.size(); ++i) {
        const ZSpriteQuad &quad = quads[i];

        batch.AddQuad(*quad.page, prop.x + static_cast<float>(quad.offsetX),
                      prop.y + static_cast<float>(quad.offsetY),
                      static_cast<float>(quad.source.width),
                      static_cast<float>(quad.source.height), quad.source,
                      quad.flipHorizontal, quad.flipVertical, quad.blend);

        if (prop.hitFlashRemainingMs > 0.0f) {
            const float flashAlpha = prop.hitFlashRemainingMs / 500.0f;
            batch.AddTransformedQuad(
                *quad.page, prop.x + static_cast<float>(quad.offsetX),
                prop.y + static_cast<float>(quad.offsetY),
                static_cast<float>(quad.source.width),
                static_cast<float>(quad.source.height), quad.source,
                quad.flipHorizontal, quad.flipVertical, ZBlendMode::Additive,
                prop.x, prop.y, 1.0f, 1.0f, 0.0f, flashAlpha);
        }
    }
}

/** Collect one outline per object of the given type, centred on its own x,y. */
void BuildMarkers(const ZLoadedMap &loaded, ZMarkerBatch &markers,
                  ZPlacedObjectType wanted) {
    markers.Begin();

    const CMap &map = loaded.map;
    for (std::uint32_t layer = 0; layer < map.GetObjectLayerCount(); ++layer) {
        const std::vector<ZPlacedObject> &objects =
            map.GetObjectLayer(layer).GetObjects();
        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (objects[i].objectType != static_cast<std::uint8_t>(wanted)) {
                continue;
            }
            markers.AddOutline(static_cast<float>(objects[i].x) - 0.5f * kMarkerSize,
                               static_cast<float>(objects[i].y) - 0.5f * kMarkerSize,
                               kMarkerSize, kMarkerSize, kMarkerThickness);
        }
    }
}

/** Convert world anchors and native pixel sizes to the HUD's logical canvas. */
void ProjectEnemyHealthBars(std::vector<ZCombatWorld::HealthBar> &bars,
    float cameraX, float cameraY, float zoom, int width, int height) {
    for (auto &bar : bars) {
        bar.x = (bar.x - cameraX) * zoom * 1024 / width;
        bar.y = (bar.y - cameraY) * zoom * 768 / height;
        bar.width *= 1024.0f / width;
        bar.height *= 768.0f / height;
        bar.border *= 1024.0f / width;
        // Centre in screen space, after the world anchor has been projected.
        bar.x -= bar.width * 0.5f;
    }
}

/** CRenderQueue::Compare :145029: group, then integer world Y. */
bool MapItemDrawsBefore(const ZMapRenderItem &left, const ZMapRenderItem &right) {
    if (left.group != right.group) { return left.group < right.group; }
    return left.y < right.y;
}

/** Shared main/foreground passes for the game and permanent map viewers.
 * CRenderQueue::Draw :145123 puts actors and scenery in the SAME main pass.
 * The caller has already drawn tiles and every prop's background slot.
 */
void DrawMapObjects(ZLoadedMap &loaded, ZQuadBatch &batch, const ZShaderProgram &program,
                const float *mapMvp, bool showProps , ZCombatWorld *scene ,
                ZPlayerModel *brotherModel , float brotherY , int viewportWidth ) {
    std::vector<ZMapRenderItem> items;
    items.reserve(loaded.props.size() + loaded.enemies.size() + loaded.players.size());
    if (showProps) {
        for (const ZPlacedProp &prop : loaded.props) {
            ZMapRenderItem item;
            item.group = prop.sprite->zOrderGroup;
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
    for (std::size_t i = 0; i < loaded.enemies.size(); ++i) {
        ZPlacedEnemy &placed = loaded.enemies[i];
        const float scale = EnemyModelWorldScale(*placed.model, placed.gameScale,
                                                 kLevelCameraScale);

        ZMapRenderItem item;
        item.y = static_cast<int>(placed.y);
        item.enemy = placed.model.get();
        BuildEnemyGameMatrix(*placed.model, mapMvp, placed.x, placed.y, scale,
                             0.0f, item.matrix);
        items.push_back(item);
    }

    for (std::size_t i = 0; i < loaded.players.size(); ++i) {
        ZPlacedPlayer &placed = loaded.players[i];
        const float scale = PlayerModelWorldScale(
            *placed.model, loaded.playerTemplate->gameScale, kLevelCameraScale);

        ZMapRenderItem item;
        item.y = static_cast<int>(placed.y);
        item.player = placed.model.get();
        BuildPlayerGameMatrix(mapMvp, placed.x, placed.y, scale,
                              placed.facingDegrees, item.matrix);
        items.push_back(item);
    }
    if (scene != nullptr) {
        if (brotherModel != nullptr) {
            ZMapRenderItem item;
            item.y = static_cast<int>(brotherY);
            item.player = brotherModel;
            float world[kMatrix4dElements];
            scene->BrotherMatrix(world);
            Matrix4dMultiply(mapMvp, world, item.matrix);
            items.push_back(item);
        }
        for (const auto &actor : scene->enemies) {
            ZMapRenderItem item;
            item.y = static_cast<int>(actor->model.enemy.combat.y);
            item.enemy = &actor->model;
            float world[kMatrix4dElements];
            scene->EnemyMatrix(*actor, world);
            Matrix4dMultiply(mapMvp, world, item.matrix);
            // Original stun shake is a screen-pixel draw offset, never collision motion.
            item.matrix[3] += 2.0f * actor->model.enemy.stun.GetOffset() / viewportWidth;
            items.push_back(item);
        }
    }
    std::stable_sort(items.begin(), items.end(), MapItemDrawsBefore);
    glDisable(GL_DEPTH_TEST);
    batch.Begin();
    for (const ZMapRenderItem &item : items) {
        if (item.prop != nullptr) {
            AddSpriteQuads(*item.prop, CurrentQuads(*MainSlotFor(*item.prop), item.prop->main), batch);
            continue;
        }
        // Flush the preceding 2D run before submitting this model.
        if (batch.GetQuadCount() != 0) {
            batch.Upload();
            batch.Draw(program, mapMvp);
            batch.Begin();
        }
        // CMeshCamera::DrawHeirarchy :99263 clears depth per hierarchy.
        // Depth resolves parts of this model; cross-object order belongs to the queue.
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (item.player != nullptr) { DrawPlayer(*item.player, program, item.matrix); }
        if (item.enemy != nullptr) { DrawEnemyModel(*item.enemy, program, item.matrix); }
        glDisable(GL_DEPTH_TEST);
    }
    if (showProps) {
        // Explosion/shockwave z=3 and cover debris z=5 sit above bodies.
        AddParticleQuads(loaded, batch, 3, 5);
        for (const ZPlacedProp &prop : loaded.props) {
            AddSpriteQuads(prop, CurrentQuads(*ForegroundSlotFor(prop), prop.foreground), batch);
        }
    }
    batch.Upload();
    batch.Draw(program, mapMvp);
    // Put back what was found: the sprite path draws flat and in order.
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/** How many objects of one type a map places. */
unsigned CountObjects(const ZLoadedMap &loaded, ZPlacedObjectType wanted) {
    const CMap &map = loaded.map;
    unsigned total = 0;
    for (std::uint32_t layer = 0; layer < map.GetObjectLayerCount(); ++layer) {
        const std::vector<ZPlacedObject> &objects =
            map.GetObjectLayer(layer).GetObjects();
        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (objects[i].objectType == static_cast<std::uint8_t>(wanted)) {
                total++;
            }
        }
    }
    return total;
}

void ReportSpawns(const ZLoadedMap &loaded) {
    std::printf("[m3] %u player spawns, %u enemy spawns\n",
                CountObjects(loaded, ZPlacedObjectType::Player),
                CountObjects(loaded, ZPlacedObjectType::Enemy));

    std::printf("[m4] %u collision layers in map data\n",
                loaded.map.GetCollisionLayerCount());
}

void BuildGeometry(const ZLoadedMap &loaded, ZQuadBatch &batch, bool showTiles,
                   bool showProps, bool report) {
    const CMap &map = loaded.map;
    const TileSet &tileSet = loaded.tileSet;
    const std::vector<ZTileRect> &tiles = tileSet.GetTiles();
    const float drawSize = static_cast<float>(tileSet.GetDrawSize());

    batch.Begin();

    std::uint32_t skipped = 0;

    if (showTiles) {
        for (std::uint32_t layerIndex = 0; layerIndex < map.GetTileLayerCount();
             ++layerIndex) {
            const CLayerTile &layer = map.GetTileLayer(layerIndex);

            // A drifted layer leaves a strip of canvas uncovered at the edge it
            // has moved away from, so it needs one more cell on that side --
            // and only that side, which is what keeps the extra cells from
            // showing up as a border on all four. A layer at rest, or one
            // exactly on a tile boundary, uncovers nothing and draws the same
            // range M3 drew.
            int firstColumn = 0;
            int firstRow = 0;
            int lastColumn = static_cast<int>(map.GetCanvasWidth());
            int lastRow = static_cast<int>(map.GetCanvasHeight());
            if (layer.GetOffsetX() > 0.0f) {
                firstColumn = -1;
            }
            if (layer.GetOffsetX() < 0.0f) {
                lastColumn = lastColumn + 1;
            }
            if (layer.GetOffsetY() > 0.0f) {
                firstRow = -1;
            }
            if (layer.GetOffsetY() < 0.0f) {
                lastRow = lastRow + 1;
            }

            for (int row = firstRow; row < lastRow; ++row) {
                for (int column = firstColumn; column < lastColumn; ++column) {
                    // GetCell wraps by modulo and takes an unsigned index, so
                    // the ring's -1 is lifted past zero first. Adding a whole
                    // layer width or height leaves the wrapped result alone.
                    const std::uint32_t cellColumn =
                        static_cast<std::uint32_t>(column + layer.GetWidth());
                    const std::uint32_t cellRow =
                        static_cast<std::uint32_t>(row + layer.GetHeight());
                    const ZTileCell &cell = layer.GetCell(cellColumn, cellRow);

                    // 255 means nothing here; the layer below shows through.
                    if (cell.tileId == kEmptyTileId) {
                        skipped++;
                        continue;
                    }
                    if (cell.tileId >= tiles.size()) {
                        skipped++;
                        continue;
                    }

                    const ZTileRect &tile = tiles[cell.tileId];
                    if (tile.imageIndex >= loaded.textures.size()) {
                        skipped++;
                        continue;
                    }

                    ZSourceRect source;
                    source.x = tile.x;
                    source.y = tile.y;
                    source.width = tile.width;
                    source.height = tile.height;

                    batch.AddQuad(*loaded.textures[tile.imageIndex],
                                  (column + layer.GetOffsetX()) * drawSize,
                                  (row + layer.GetOffsetY()) * drawSize,
                                  drawSize, drawSize, source,
                                  (cell.flags & kTileFlagFlipHorizontal) != 0,
                                  (cell.flags & kTileFlagFlipVertical) != 0,
                                  ZBlendMode::Alpha);
                }
            }
        }
    }

    const std::uint32_t tileQuads = batch.GetQuadCount();

    if (showProps) {
        for (std::size_t i = 0; i < loaded.props.size(); ++i) {
            const ZPlacedProp &prop = loaded.props[i];
            const ZPropSlot *background = BackgroundSlotFor(prop);
            AddSpriteQuads(prop, CurrentQuads(*background, prop.background), batch);
        }
        // Script z=2 effects sit above background scenery but below bodies.
        AddParticleQuads(loaded, batch, 0, 2);
        // Main scenery must be interleaved with actors; DrawMapObjects owns that
        // pass and the final foreground pass (CRenderQueue::Draw :145235).
    }

    batch.Upload();

    // This runs every frame now, so it only says anything when the caller has
    // just changed what is being drawn.
    if (report) {
        std::printf("[m3] %u tile quads + %u prop quads in %u draw calls "
                    "(%u cells empty or unusable)\n",
                    tileQuads, batch.GetQuadCount() - tileQuads,
                    batch.GetGroupCount(), skipped);
    }
}

/**
 * Every map in every pack, as one flat list.
 *
 * Flat rather than grouped because a pack boundary is not something the viewer
 * should make anyone think about -- the packs are a packaging detail, and a map
 * reaches across them for its props anyway. Walking the list runs off the end
 * of one pack straight into the next.
 */
std::vector<ZCatalogMap> BuildCatalog(CResTOCManager &tocManager) {
    std::vector<ZCatalogMap> catalog;

    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        CResPackTOC *pack = tocManager.GetPack(static_cast<int>(i));
        if (pack == nullptr) {
            continue;
        }

        CGameObjectPack objectPack;
        if (!objectPack.Init(*pack)) {
            continue;
        }

        const std::uint32_t mapCount =
            objectPack.GetObjectCount(ZGameSection::TileLayer);
        for (std::uint32_t m = 0; m < mapCount; ++m) {
            ZCatalogMap entry;
            entry.packIndex = static_cast<int>(i);
            entry.packName = pack->GetShortName();
            entry.mapIndex = m;
            catalog.push_back(entry);
        }
    }

    return catalog;
}
const std::vector<ZSpriteQuad> &CurrentQuads(const ZPropSlot &slot,
                                            const CSpritePlayer &player) {
    static const std::vector<ZSpriteQuad> kNothing;

    const std::uint32_t step = player.GetStep();
    if (step >= slot.quadsByStep.size()) {
        return kNothing;
    }
    return slot.quadsByStep[step];
}
}
