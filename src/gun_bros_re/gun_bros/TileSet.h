/**
 * @file TileSet.h
 * @brief The tile table: which atlases a map uses, and the rect of each tile.
 *
 * Port of TileSet (the engine spells it without the C prefix).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:130310 (Init), :130436 (Load)
 *
 * Wire format:
 *   uint8 imageCount
 *   CGameAssetRef images[imageCount]              -- 8 bytes each
 *   uint8 tileCount
 *   { uint8 imageIndex, uint16 x, y, w, h } [tileCount]   -- 9 bytes each
 */

#ifndef GUN_BROS_RE_GUN_BROS_TILESET_H
#define GUN_BROS_RE_GUN_BROS_TILESET_H

#include "gun_bros/CGameAssetRef.h"

#include <cstdint>
#include <vector>

// A tile id this high means "nothing here"; CLayerTile skips those cells.
constexpr std::uint8_t kEmptyTileId = 255;

/** One tile: a rectangle on one of the atlases. */
struct TileRect {
    std::uint8_t imageIndex;
    std::uint16_t x;
    std::uint16_t y;
    std::uint16_t width;
    std::uint16_t height;
};

/**
 * The tile table for one map.
 *
 * `images` are CGameAssetRefs, so their assetIds are ordinals into the PNG
 * section -- TileSet::Load is what decides that, not the data.
 */
class TileSet {
public:
    TileSet();

    bool Init(CArrayInputStream &stream);

    const std::vector<CGameAssetRef> &GetImages() const { return m_images; }
    const std::vector<TileRect> &GetTiles() const { return m_tiles; }

    std::uint32_t GetTileCount() const {
        return static_cast<std::uint32_t>(m_tiles.size());
    }

    /**
     * The side length the engine draws tiles at.
     *
     * Taken from tile 0 and applied to every tile -- CLayerTile::GetWidth is
     * `layerWidth * tiles[0].w`, not a per-tile sum. In these packs every tile
     * is 256x256 anyway, but the rule is what the original follows.
     */
    std::uint16_t GetDrawSize() const;

private:
    std::vector<CGameAssetRef> m_images;
    std::vector<TileRect> m_tiles;
};

#endif  // GUN_BROS_RE_GUN_BROS_TILESET_H
