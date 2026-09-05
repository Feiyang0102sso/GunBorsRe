/**
 * @file CMap.h
 * @brief One level's map: a tile set reference and a stack of layers.
 *
 * Port of CMap (src/gunbros/map.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:92498 (Init)
 *
 * Wire format:
 *   GameObjectRef   tileSetRef
 *   RequirementList requirements
 *   uint8           layerCount
 *   layers: uint8 layerType, then that layer type's own Init
 *
 * Layers draw in file order, so index 0 is the bottom.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CMAP_H
#define GUN_BROS_RE_GUN_BROS_CMAP_H

#include "gun_bros/CGameAssetRef.h"
#include "gun_bros/CLayerTile.h"

#include <cstdint>
#include <vector>

/**
 * Layer types, as switched on in CMap::Init.
 * Only Tile is parsed; see the note on CMap::Init.
 */
enum class MapLayerType : std::uint8_t {
    Tile = 0,
    Collision = 1,
    Object = 2,
    Movie = 3,
    Camera = 4,
    PathLink = 5,
    PathMesh = 6,
};

/**
 * A map, parsed as far as the tile layers go.
 *
 * Parsing stops at the first non-tile layer. That is not a shortcut we can
 * take forever -- it works because in all 22 maps across the five packs that
 * have any, the tile layers come first -- but the layer types after them
 * (collision, object placement, path meshes) each need their own parser and
 * none of them contribute to the terrain. GetUnparsedLayerCount reports what
 * was left so the gap stays visible rather than silent.
 */
class CMap {
public:
    CMap();

    bool Init(CArrayInputStream &stream);

    const GameObjectRef &GetTileSetRef() const { return m_tileSetRef; }

    std::uint32_t GetTileLayerCount() const {
        return static_cast<std::uint32_t>(m_tileLayers.size());
    }
    const CLayerTile &GetTileLayer(std::uint32_t index) const {
        return m_tileLayers[index];
    }

    /** Canvas size in tiles: the largest extent over all tile layers. */
    std::uint16_t GetCanvasWidth() const { return m_canvasWidth; }
    std::uint16_t GetCanvasHeight() const { return m_canvasHeight; }

    std::uint8_t GetDeclaredLayerCount() const { return m_declaredLayerCount; }

    /** Layers after the tile layers that this port does not read. */
    std::uint32_t GetUnparsedLayerCount() const;

    const RequirementList &GetRequirements() const { return m_requirements; }

private:
    GameObjectRef m_tileSetRef;
    RequirementList m_requirements;

    std::uint8_t m_declaredLayerCount;
    std::uint32_t m_layersRead;  // includes the ones only stepped over
    std::vector<CLayerTile> m_tileLayers;

    std::uint16_t m_canvasWidth;
    std::uint16_t m_canvasHeight;
};

#endif  // GUN_BROS_RE_GUN_BROS_CMAP_H
