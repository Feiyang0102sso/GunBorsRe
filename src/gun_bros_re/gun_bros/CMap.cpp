/**
 * @file CMap.cpp
 * @brief One level's map: a tile set reference and a stack of layers.
 */

#include "gun_bros/CMap.h"

#include <cstdio>

namespace {

/**
 * The layers that are stepped over rather than parsed.
 *
 * Terrain and objects are what gets drawn, but a layer this port does not care
 * about still has to be walked byte for byte to reach the ones after it. These
 * read exactly what the originals read and keep nothing.
 *
 * Each will become a real class when something needs its contents:
 * CLayerCollision in M5, the path layers when AI pathfinding lands.
 */

/**
 * CLayerCollision, which delegates to CCollisionData::Load.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:125241, :142446
 *
 *   uint16 vertexCount, { int32 x, int32 y }[]
 *   uint16 edgeCount,   { uint8, uint16, uint16 }[]
 */
void SkipCollisionLayer(CArrayInputStream &stream) {
    const std::uint16_t vertexCount = stream.ReadUInt16();
    stream.Skip(static_cast<std::size_t>(vertexCount) * 8);

    const std::uint16_t edgeCount = stream.ReadUInt16();
    stream.Skip(static_cast<std::size_t>(edgeCount) * 5);
}

/**
 * CLayerMovie: a CGameAssetRef and a position.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:126049
 */
void SkipMovieLayer(CArrayInputStream &stream) {
    stream.ReadUInt32();  // pack hash
    stream.ReadInt32();   // asset id
    stream.ReadInt16();   // x
    stream.ReadInt16();   // y
}

/**
 * CLayerPathLink: the node-and-link navigation graph.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:166451
 *
 *   uint8 nodeCount, uint8 linkCount, uint8 regionCount
 *   nodes:   { int16 x, int16 y, int16 radius }
 *   links:   { uint8 firstNode, uint8 secondNode }
 *   regions: uint8 n, uint8 nodeIndices[n], uint16 bounds[4]
 *
 * No map in these packs contains a region, so that branch is written from the
 * engine code alone and has never been exercised.
 */
void SkipPathLinkLayer(CArrayInputStream &stream) {
    const std::uint8_t nodeCount = stream.ReadUInt8();
    const std::uint8_t linkCount = stream.ReadUInt8();
    const std::uint8_t regionCount = stream.ReadUInt8();

    stream.Skip(static_cast<std::size_t>(nodeCount) * 6);
    stream.Skip(static_cast<std::size_t>(linkCount) * 2);

    for (std::uint8_t region = 0; region < regionCount; ++region) {
        const std::uint8_t nodeIndexCount = stream.ReadUInt8();
        stream.Skip(nodeIndexCount);
        stream.Skip(8);  // four uint16 bounds
    }
}

/**
 * CLayerPathMesh: the quad navigation mesh.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:167381
 *
 *   uint16 vertexCount, uint16 nodeCount, uint16 neighbourRefCount
 *   vertices: { int16 x, int16 y }
 *   nodes:    uint8 flags, int16 vertexIndices[4],
 *             uint8 neighbourCount, uint16 neighbours[neighbourCount]
 *
 * The third header count is the sum of every node's neighbour count, so it is
 * redundant -- the nodes are walked rather than trusted.
 */
void SkipPathMeshLayer(CArrayInputStream &stream) {
    const std::uint16_t vertexCount = stream.ReadUInt16();
    const std::uint16_t nodeCount = stream.ReadUInt16();
    stream.ReadUInt16();  // total neighbour references

    stream.Skip(static_cast<std::size_t>(vertexCount) * 4);

    for (std::uint16_t node = 0; node < nodeCount; ++node) {
        stream.ReadUInt8();  // flags; zero in every checked sample
        stream.Skip(8);      // four int16 vertex indices

        const std::uint8_t neighbourCount = stream.ReadUInt8();
        stream.Skip(static_cast<std::size_t>(neighbourCount) * 2);
    }
}

}  // namespace

CMap::CMap()
    : m_declaredLayerCount(0),
      m_layersRead(0),
      m_currentCameraLayer(0),
      m_canvasWidth(0),
      m_canvasHeight(0) {}

bool CMap::Init(CArrayInputStream &stream) {
    m_tileLayers.clear();
    m_objectLayers.clear();
    m_cameraLayers.clear();
    m_currentCameraLayer = 0;
    m_canvasWidth = 0;
    m_canvasHeight = 0;

    m_tileSetRef.Init(stream);
    m_requirements.Init(stream);
    m_declaredLayerCount = stream.ReadUInt8();

    if (stream.Overran()) {
        std::printf("[map] header truncated\n");
        return false;
    }

    m_layersRead = 0;
    for (std::uint8_t i = 0; i < m_declaredLayerCount; ++i) {
        const std::uint8_t layerType = stream.ReadUInt8();

        if (layerType == static_cast<std::uint8_t>(MapLayerType::Tile)) {
            CLayerTile layer;
            if (!layer.Init(stream)) {
                return false;
            }

            if (layer.GetWidth() > m_canvasWidth) {
                m_canvasWidth = layer.GetWidth();
            }
            if (layer.GetHeight() > m_canvasHeight) {
                m_canvasHeight = layer.GetHeight();
            }
            m_tileLayers.push_back(layer);

        } else if (layerType == static_cast<std::uint8_t>(MapLayerType::Object)) {
            CLayerObject layer;
            if (!layer.Init(stream)) {
                return false;
            }
            m_objectLayers.push_back(layer);

        } else if (layerType == static_cast<std::uint8_t>(MapLayerType::Collision)) {
            SkipCollisionLayer(stream);
        } else if (layerType == static_cast<std::uint8_t>(MapLayerType::Movie)) {
            SkipMovieLayer(stream);
        } else if (layerType == static_cast<std::uint8_t>(MapLayerType::Camera)) {
            CLayerCamera layer;
            if (!layer.Init(stream)) {
                return false;
            }
            layer.SetLayerIndex(i);
            m_cameraLayers.push_back(layer);
        } else if (layerType == static_cast<std::uint8_t>(MapLayerType::PathLink)) {
            SkipPathLinkLayer(stream);
        } else if (layerType == static_cast<std::uint8_t>(MapLayerType::PathMesh)) {
            SkipPathMeshLayer(stream);
        } else {
            // An unknown type has an unknown size, so the stream cannot be
            // resynchronised -- stop here rather than read garbage.
            std::printf("[map] layer %u is type %u, which has no parser; "
                        "%u layers left unread\n",
                        i, layerType, m_declaredLayerCount - i);
            break;
        }

        m_layersRead++;

        if (stream.Overran()) {
            std::printf("[map] stream overran while reading layer %u (type %u)\n",
                        i, layerType);
            return false;
        }
    }

    if (m_tileLayers.empty() && m_objectLayers.empty()) {
        std::printf("[map] no tile or object layers\n");
        return false;
    }

    // Every layer type now has a parser, so a fully read map should land on
    // the last byte of its resource. Anything left over means one of the
    // layouts is wrong, and that is worth hearing about immediately.
    if (m_layersRead == m_declaredLayerCount && stream.Available() != 0) {
        std::printf("[map] %zu bytes left after %u layers\n",
                    stream.Available(), m_declaredLayerCount);
    }

    return true;
}

bool CMap::SetCameraLayer(std::uint32_t layerIndex) {
    for (std::size_t i = 0; i < m_cameraLayers.size(); ++i) {
        if (m_cameraLayers[i].GetLayerIndex() == layerIndex) {
            m_currentCameraLayer = static_cast<std::uint32_t>(i);
            return true;
        }
    }

    return false;
}

MapRectangle CMap::GetCameraExtent() const {
    if (m_cameraLayers.empty()) {
        return MapRectangle();
    }

    const MapRectangle &first = m_cameraLayers[0].GetPrimaryBounds();
    int left = first.x;
    int top = first.y;
    int right = first.x + first.width;
    int bottom = first.y + first.height;

    for (std::size_t i = 1; i < m_cameraLayers.size(); ++i) {
        const MapRectangle &bounds = m_cameraLayers[i].GetPrimaryBounds();
        if (bounds.IsEmpty()) {
            continue;
        }

        if (bounds.x < left) {
            left = bounds.x;
        }
        if (bounds.y < top) {
            top = bounds.y;
        }
        if (bounds.x + bounds.width > right) {
            right = bounds.x + bounds.width;
        }
        if (bounds.y + bounds.height > bottom) {
            bottom = bounds.y + bounds.height;
        }
    }

    MapRectangle extent;
    extent.x = static_cast<std::int16_t>(left);
    extent.y = static_cast<std::int16_t>(top);
    extent.width = static_cast<std::int16_t>(right - left);
    extent.height = static_cast<std::int16_t>(bottom - top);
    return extent;
}

MapRectangle CMap::GetVisibleBounds() const {
    if (m_currentCameraLayer >= m_cameraLayers.size()) {
        return MapRectangle();
    }

    return m_cameraLayers[m_currentCameraLayer].GetPrimaryBounds();
}

std::uint32_t CMap::GetUnparsedLayerCount() const {
    return m_declaredLayerCount - m_layersRead;
}
