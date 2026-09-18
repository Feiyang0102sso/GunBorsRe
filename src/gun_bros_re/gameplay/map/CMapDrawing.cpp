#include "gun_bros_re/gameplay/map/CMapResources.h"
#include "gun_bros_re/gameplay/map/CRenderQueue.h"
#include "gun_bros_re/gameplay/map/CMapEffects.h"
#include "engine/graphics/ZQuadBatch.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace MapDetail;

void CMap::DrawBackground(ZQuadBatch &batch, bool showTiles, bool showProps, bool report) const {
    const CMap &loaded = *this;
    const CMap &map = loaded;
    const TileSet &tileSet = loaded.GetResources().tileSet;

    batch.Begin();

    std::uint32_t skipped = 0;

    if (showTiles) {
        for (std::uint32_t layerIndex = 0; layerIndex < map.GetTileLayerCount();
             ++layerIndex) {
            const CLayerTile &layer = map.GetTileLayer(layerIndex);

            skipped += layer.DrawBackground(batch, tileSet, loaded.GetResources().textures,
                map.GetCanvasWidth(), map.GetCanvasHeight());
        }
    }

    const std::uint32_t tileQuads = batch.GetQuadCount();

    if (showProps) {
        CRenderQueue::DrawBackground(loaded, batch);
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

/** Move every drifting tile layer on by one frame's worth of time. */
void CMap::UpdateLayers(std::uint16_t deltaMs) {
    CMap &map = *this;
    for (std::uint32_t i = 0; i < map.GetTileLayerCount(); ++i) {
        CLayerTile &layer = map.GetTileLayer(i);
        if (!layer.IsScrolling()) {
            continue;
        }
        layer.Update(deltaMs);
    }
}
