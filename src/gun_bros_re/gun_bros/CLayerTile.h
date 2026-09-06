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

// What SetSpeed multiplies its arguments by. The sign is where a scroll's
// direction comes from: every speed a script asks for arrives positive.
// Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:126803
constexpr float kLayerSpeedScale = -0.05f;

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
 *
 * A layer can also drift. That is what makes lava flow and starfields move --
 * the only animation the terrain has, and it is not in the map data: a level
 * script asks for it at run time, through CLevel's setTileLayerSpeed.
 */
class CLayerTile {
public:
    CLayerTile();

    bool Init(CArrayInputStream &stream);

    std::uint16_t GetWidth() const { return m_width; }
    std::uint16_t GetHeight() const { return m_height; }

    /**
     * Set the drift.
     * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:126803 (SetSpeed)
     *
     * Takes the speed the caller has, and applies kLayerSpeedScale itself, as
     * the original does. Callers are level scripts by way of
     * CLevel::setTileLayerSpeed, which has already turned the script's
     * fixed-point argument into a float and nothing more.
     */
    void SetSpeed(float speedX, float speedY);

    /**
     * Move the drift on.
     * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:126770 (Update)
     *
     * The offsets keep only the fractional part of a tile: the whole part is
     * dropped rather than turned into a shift of the grid. That is seamless
     * exactly for the layers this is used on -- a field of lava or stars looks
     * the same moved along by one tile -- and it is why a scrolling layer never
     * needs its cell lookup adjusted, only its drawn position.
     */
    void Update(std::uint16_t deltaMs);

    bool IsScrolling() const { return m_speedX != 0.0f || m_speedY != 0.0f; }

    /** Drift in tiles per second, after kLayerSpeedScale. */
    float GetSpeedX() const { return m_speedX; }
    float GetSpeedY() const { return m_speedY; }

    /** Drift so far, in tiles, always within one tile of zero. */
    float GetOffsetX() const { return m_offsetX; }
    float GetOffsetY() const { return m_offsetY; }

    /**
     * Cell at a canvas position, wrapping when the layer is smaller than the
     * canvas. Both coordinates are in tiles.
     */
    const TileCell &GetCell(std::uint32_t column, std::uint32_t row) const;

private:
    std::uint16_t m_width;
    std::uint16_t m_height;
    std::vector<TileCell> m_cells;

    // Drift, in tiles per second and in tiles. Zero unless a level script asks
    // for it, which is the case for six of the twenty-two maps.
    float m_speedX;
    float m_speedY;
    float m_offsetX;
    float m_offsetY;

    // Returned for lookups on an empty layer, so callers never see a null.
    static const TileCell s_emptyCell;
};

#endif  // GUN_BROS_RE_GUN_BROS_CLAYERTILE_H
