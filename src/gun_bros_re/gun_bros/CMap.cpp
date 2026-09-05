/**
 * @file CMap.cpp
 * @brief One level's map: a tile set reference and a stack of layers.
 */

#include "gun_bros/CMap.h"

#include <cstdio>

namespace {

/**
 * The non-tile layers, stepped over rather than parsed.
 *
 * Terrain is the only thing M3 draws, but the tile layers are not always
 * first -- pack7's maps put theirs after an object layer -- so the stream has
 * to be walked past the others to reach them. These read exactly what the
 * originals read and keep nothing.
 *
 * Each will become a real class when something needs its contents:
 * CLayerObject for spawn points and props in M4, CLayerCollision in M5.
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
 * CLayerObject::InitializeObjects.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:126460
 *
 *   uint16 totalObjects
 *   uint8  groupCount
 *   groups: uint8 objectType, uint16 count, uint16 allocCount
 *           objects: uint32 packHash, uint8 localIndex, uint8 hasExtra,
 *                    int16 x, int16 y, uint8 flags,
 *                    plus extra bytes decided by objectType when hasExtra
 */
void SkipObjectLayer(CArrayInputStream &stream) {
    stream.ReadUInt16();  // total objects, only used to size an array
    const std::uint8_t groupCount = stream.ReadUInt8();

    for (std::uint8_t group = 0; group < groupCount; ++group) {
        const std::uint8_t objectType = stream.ReadUInt8();
        const std::uint16_t count = stream.ReadUInt16();
        stream.ReadUInt16();  // allocation count

        for (std::uint16_t object = 0; object < count; ++object) {
            stream.ReadUInt32();  // pack hash
            stream.ReadUInt8();   // local index
            const std::uint8_t hasExtra = stream.ReadUInt8();
            stream.ReadInt16();   // x
            stream.ReadInt16();   // y
            stream.ReadUInt8();   // flags

            if (hasExtra == 0) {
                continue;
            }
            // Only three object types carry anything extra.
            if (objectType == 15) {
                stream.ReadUInt16();
            } else if (objectType == 5) {
                stream.ReadUInt8();
                stream.ReadInt16();
            } else if (objectType == 14) {
                stream.ReadUInt8();
            }
        }
    }
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
 * CLayerCamera: two rectangles.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:127663
 */
void SkipCameraLayer(CArrayInputStream &stream) {
    for (int i = 0; i < 8; ++i) {
        stream.ReadInt16();
    }
}

}  // namespace

CMap::CMap()
    : m_declaredLayerCount(0), m_layersRead(0), m_canvasWidth(0), m_canvasHeight(0) {}

bool CMap::Init(CArrayInputStream &stream) {
    m_tileLayers.clear();
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

        } else if (layerType == static_cast<std::uint8_t>(MapLayerType::Collision)) {
            SkipCollisionLayer(stream);
        } else if (layerType == static_cast<std::uint8_t>(MapLayerType::Object)) {
            SkipObjectLayer(stream);
        } else if (layerType == static_cast<std::uint8_t>(MapLayerType::Movie)) {
            SkipMovieLayer(stream);
        } else if (layerType == static_cast<std::uint8_t>(MapLayerType::Camera)) {
            SkipCameraLayer(stream);
        } else {
            // Path layers, whose formats are not worked out yet. Their sizes
            // are unknown, so the stream cannot be resynchronised -- stop here
            // rather than read garbage as tile data.
            std::printf("[map] layer %u is type %u, which has no parser yet; "
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

    if (m_tileLayers.empty()) {
        std::printf("[map] no tile layers\n");
        return false;
    }

    return true;
}

std::uint32_t CMap::GetUnparsedLayerCount() const {
    return m_declaredLayerCount - m_layersRead;
}
