/**
 * @file CLayerTile.h
 * @brief One grid of tiles.
 *
 * Port of CLayerTile (src/gunbros/layerTile.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:126884 (Init), :126932 (DrawBackground)
 *
 * Wire format:
 *   uint8  unused          -- read and discarded; always 1 in these packs
 *   uint16 width           -- in tiles, not pixels
 *   uint16 height
 *   { uint8 tileId, uint8 flags } cells[width * height]
 */

#ifndef GUN_BROS_RE_GUN_BROS_CLAYERTILE_H
#define GUN_BROS_RE_GUN_BROS_CLAYERTILE_H

#include "engine/CArrayInputStream.h"

#include <cstdint>
#include <vector>

// Per-cell flags.
constexpr std::uint8_t kTileFlagFlipHorizontal = 0x01;
constexpr std::uint8_t kTileFlagFlipVertical = 0x02;

/** One cell of the grid. */
struct TileCell {
    std::uint8_t tileId;  // 255 means empty, draw nothing
    std::uint8_t flags;
};

/**
 * A tile grid.
 *
 * Layers can be smaller than the map canvas; the original wraps by taking the
 * row and column modulo the layer size, so a small layer tiles endlessly under
 * a larger one. GetCell does that wrapping.
 */
class CLayerTile {
public:
    CLayerTile();

    bool Init(CArrayInputStream &stream);

    std::uint16_t GetWidth() const { return m_width; }
    std::uint16_t GetHeight() const { return m_height; }

    /**
     * Cell at a canvas position, wrapping when the layer is smaller than the
     * canvas. Both coordinates are in tiles.
     */
    const TileCell &GetCell(std::uint32_t column, std::uint32_t row) const;

private:
    std::uint16_t m_width;
    std::uint16_t m_height;
    std::vector<TileCell> m_cells;

    // Returned for lookups on an empty layer, so callers never see a null.
    static const TileCell s_emptyCell;
};

#endif  // GUN_BROS_RE_GUN_BROS_CLAYERTILE_H
