/** Original layerTile.cpp: wrapping cells, authored flips and scrolling offsets. */
#include "gun_bros_re/gameplay/map/CLayerTile.h"
#include "gun_bros_re/gameplay/map/TileSet.h"
#include "engine/graphics/ZQuadBatch.h"

std::uint32_t CLayerTile::DrawBackground(ZQuadBatch &batch, const TileSet &tileSet,
    const std::vector<std::unique_ptr<ZTexture>> &textures, unsigned canvasWidth, unsigned canvasHeight) const {
    const auto &tiles = tileSet.GetTiles();
    const float drawSize = static_cast<float>(tileSet.GetDrawSize());
    std::uint32_t skipped = 0;
    // A drifted layer leaves a strip of canvas uncovered at the edge it
    // has moved away from, so it needs one more cell on that side --
    // and only that side, which is what keeps the extra cells from
    // showing up as a border on all four. A layer at rest, or one
    // exactly on a tile boundary, uncovers nothing and draws the same
    // range M3 drew.
    int firstColumn = 0;
    int firstRow = 0;
    int lastColumn = static_cast<int>(canvasWidth);
    int lastRow = static_cast<int>(canvasHeight);
    if (GetOffsetX() > 0.0f) {
        firstColumn = -1;
    }
    if (GetOffsetX() < 0.0f) {
        lastColumn = lastColumn + 1;
    }
    if (GetOffsetY() > 0.0f) {
        firstRow = -1;
    }
    if (GetOffsetY() < 0.0f) {
        lastRow = lastRow + 1;
    }

    for (int row = firstRow; row < lastRow; ++row) {
        for (int column = firstColumn; column < lastColumn; ++column) {
            // GetCell wraps by modulo and takes an unsigned index, so
            // the ring's -1 is lifted past zero first. Adding a whole
            // layer width or height leaves the wrapped result alone.
            const std::uint32_t cellColumn =
                static_cast<std::uint32_t>(column + GetWidth());
            const std::uint32_t cellRow =
                static_cast<std::uint32_t>(row + GetHeight());
            const CLayerTile::Cell &cell = GetCell(cellColumn, cellRow);

            // 255 means nothing here; the layer below shows through.
            if (cell.tileId == kEmptyTileId) {
                skipped++;
                continue;
            }
            if (cell.tileId >= tiles.size()) {
                skipped++;
                continue;
            }

            const TileSet::Rect &tile = tiles[cell.tileId];
            if (tile.imageIndex >= textures.size()) {
                skipped++;
                continue;
            }

            ZSourceRect source;
            source.x = tile.x;
            source.y = tile.y;
            source.width = tile.width;
            source.height = tile.height;

            batch.AddQuad(*textures[tile.imageIndex],
                          (column + GetOffsetX()) * drawSize,
                          (row + GetOffsetY()) * drawSize,
                          drawSize, drawSize, source,
                          (cell.flags & kTileFlagFlipHorizontal) != 0,
                          (cell.flags & kTileFlagFlipVertical) != 0,
                          ZBlendMode::Alpha);
        }
    }
    return skipped;
}
