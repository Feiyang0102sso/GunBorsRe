/**
 * @file CLayerCamera.h
 * @brief The rectangles a map's camera is allowed to show.
 *
 * Port of CLayerCamera (src/gunbros/layerCamera.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:127663 (Init), :127685 (GetWidth),
 *            :127691 (GetHeight)
 *
 * Wire format -- two rectangles, eight int16 in all:
 *   int16 x, y, width, height    primary
 *   int16 x, y, width, height    secondary
 *
 * In world pixels, not tiles: GetWidth returns the width field straight, while
 * CLayerTile::GetWidth multiplies its tile count by the tile size to reach the
 * same unit.
 *
 * A level picks one of these with setCameraLayer, and the camera then never
 * leaves it -- which is why a map's tile layers may extend past it. Anything
 * out there is unreachable in the real game and only shows up in a viewer that
 * fits the whole canvas on screen.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CLAYERCAMERA_H
#define GUN_BROS_RE_GUN_BROS_CLAYERCAMERA_H

#include "engine/CArrayInputStream.h"

#include <cstdint>

/** A rectangle in world pixels. */
struct MapRectangle {
    std::int16_t x;
    std::int16_t y;
    std::int16_t width;
    std::int16_t height;

    MapRectangle() : x(0), y(0), width(0), height(0) {}

    bool IsEmpty() const { return width <= 0 || height <= 0; }
};

/** One camera layer: two rectangles. */
class CLayerCamera {
public:
    CLayerCamera();

    bool Init(CArrayInputStream &stream);

    const MapRectangle &GetPrimaryBounds() const { return m_primaryBounds; }
    const MapRectangle &GetSecondaryBounds() const { return m_secondaryBounds; }

    /** The original's accessors, both reading the primary rectangle. */
    std::int16_t GetWidth() const { return m_primaryBounds.width; }
    std::int16_t GetHeight() const { return m_primaryBounds.height; }

    /**
     * Where this layer sat in the map's layer stack.
     *
     * Not in the file: CMap fills it in while parsing. setCameraLayer names a
     * layer by its position among all layers, not among the camera ones, so
     * the number has to survive being sorted into a list of its own kind.
     */
    std::uint32_t GetLayerIndex() const { return m_layerIndex; }
    void SetLayerIndex(std::uint32_t layerIndex) { m_layerIndex = layerIndex; }

private:
    MapRectangle m_primaryBounds;
    MapRectangle m_secondaryBounds;
    std::uint32_t m_layerIndex;
};

#endif  // GUN_BROS_RE_GUN_BROS_CLAYERCAMERA_H
