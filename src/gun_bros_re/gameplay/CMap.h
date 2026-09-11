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

#include "gun_bros_re/data/CGameAssetRef.h"
#include "gun_bros_re/gameplay/CCamera.h"
#include "gun_bros_re/gameplay/CLayerCamera.h"
#include "gun_bros_re/gameplay/CLayerCollision.h"
#include "gun_bros_re/gameplay/CLayerObject.h"
#include "gun_bros_re/gameplay/CLayerTile.h"
#include "gun_bros_re/gameplay/CLayerPathLink.h"
#include "gun_bros_re/gameplay/CLayerPathMesh.h"

#include <cstdint>
#include <vector>

/** Layer types, as switched on in CMap::Init. */
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
 * A map: its tile layers, its object layers, its camera bounds, and the rest
 * walked past.
 *
 * Every layer type has a parser now, so the whole stack is read rather than
 * abandoned at the first layer whose size is unknown. Movie and the two path
 * layers are stepped over without being kept. Collision is retained because
 * M4 uses it to constrain the player.
 */
class CMap {
public:
    CMap();

    bool Init(CArrayInputStream &stream);

    const GameObjectRef &GetTileSetRef() const { return m_tileSetRef; }
    CCamera &GetCamera() { return m_camera; }
    const CCamera &GetCamera() const { return m_camera; }

    std::uint32_t GetTileLayerCount() const {
        return static_cast<std::uint32_t>(m_tileLayers.size());
    }
    const CLayerTile &GetTileLayer(std::uint32_t index) const {
        return m_tileLayers[index];
    }

    /** Same layer, for the caller that sets its scroll speed or ticks it. */
    CLayerTile &GetTileLayer(std::uint32_t index) { return m_tileLayers[index]; }

    std::uint32_t GetCollisionLayerCount() const {
        return static_cast<std::uint32_t>(m_collisionLayers.size());
    }
    const CLayerCollision &GetCollisionLayer(std::uint32_t index) const {
        return m_collisionLayers[index];
    }

    /**
     * Choose the player collision layer by its complete map-layer index.
     * This is the value passed by CLevel's setCollisionLayer function.
     */
    bool SetCollisionLayer(std::uint32_t layerIndex);

    /** Collision selected by the level script, or null when none exists. */
    const CLayerCollision *GetCurrentCollisionLayer() const;
    /** CLevel native 6 selects the walls used by ordinary bullets. */
    bool SetBulletCollisionLayer(std::uint32_t layerIndex);
    const CLayerCollision *GetCurrentBulletCollisionLayer() const;

    std::uint32_t GetObjectLayerCount() const {
        return static_cast<std::uint32_t>(m_objectLayers.size());
    }
    const CLayerObject &GetObjectLayer(std::uint32_t index) const {
        return m_objectLayers[index];
    }

    std::uint32_t GetCameraLayerCount() const {
        return static_cast<std::uint32_t>(m_cameraLayers.size());
    }
    const CLayerCamera &GetCameraLayer(std::uint32_t index) const {
        return m_cameraLayers[index];
    }
    const CLayerCamera *GetCurrentCameraLayer() const {
        if (m_currentCameraLayer >= m_cameraLayers.size()) { return nullptr; }
        return &m_cameraLayers[m_currentCameraLayer];
    }

    /**
     * Choose the current camera layer by its index in the layer stack -- the
     * number setCameraLayer passes, counting every layer and not just the
     * camera ones.
     * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:92289 (SetCameraLayer),
     *            :117499 (the resolver case that calls it)
     *
     * @return false when that index is not a camera layer, and the current one
     *         is left alone.
     */
    bool SetCameraLayer(std::uint32_t layerIndex);

    /**
     * The rectangle the game can actually show, in world pixels.
     *
     * Whichever camera layer the level's script chose, or the first one until
     * it does -- a map with several only reveals which it means once its
     * script runs. Empty when the map has no camera layer at all, and then the
     * whole canvas is on show.
     */
    MapRectangle GetVisibleBounds() const;

    /**
     * Every camera rectangle merged into one, in world pixels.
     *
     * A level moves its camera between these as it progresses -- setCameraLayer
     * is called at the start and again at each stage -- so no single rectangle
     * is "the map". Their union is everything the game can ever show, which is
     * what a whole-map viewer wants: still tight enough to exclude the canvas
     * edge no camera reaches, but not cropped to whichever corner the level
     * happens to open on.
     *
     * Empty when the map has no camera layer.
     */
    MapRectangle GetCameraExtent() const;

    /** Canvas size in tiles: the largest extent over all tile layers. */
    std::uint16_t GetCanvasWidth() const { return m_canvasWidth; }
    std::uint16_t GetCanvasHeight() const { return m_canvasHeight; }

    std::uint8_t GetDeclaredLayerCount() const { return m_declaredLayerCount; }

    /** Layers abandoned because their type has no parser. Normally zero. */
    std::uint32_t GetUnparsedLayerCount() const;

    const RequirementList &GetRequirements() const { return m_requirements; }

    /** Whole map-layer index, matching level/spawner native arguments. */
    CLayerPathLink *GetPathLinkLayer(int layerIndex);
    ILayerPath *GetPathLayer(int layerIndex);
    void UnlockAllPathNodes();
    const std::vector<CLayerPathLink> &GetPathLinkLayers() const { return m_pathLinkLayers; }

private:
    CCamera m_camera;
    GameObjectRef m_tileSetRef;
    RequirementList m_requirements;

    std::uint8_t m_declaredLayerCount;
    std::uint32_t m_layersRead;  // includes the ones only stepped over
    std::vector<CLayerTile> m_tileLayers;
    std::vector<CLayerCollision> m_collisionLayers;
    std::vector<CLayerObject> m_objectLayers;
    std::vector<CLayerCamera> m_cameraLayers;
    std::vector<CLayerPathLink> m_pathLinkLayers;
    std::vector<CLayerPathMesh> m_pathMeshLayers;

    // Index into m_cameraLayers, not into the layer stack. Stays at zero until
    // a script calls setCameraLayer.
    std::uint32_t m_currentCameraLayer;
    std::uint32_t m_currentCollisionLayer;
    std::uint32_t m_currentBulletCollisionLayer = UINT32_MAX;

    std::uint16_t m_canvasWidth;
    std::uint16_t m_canvasHeight;
};

#endif  // GUN_BROS_RE_GUN_BROS_CMAP_H
