/**
 * @file TileSet.cpp
 * @brief The tile table: which atlases a map uses, and the rect of each tile.
 */

#include "gun_bros/TileSet.h"

#include <cstdio>

TileSet::TileSet() = default;

bool TileSet::Init(CArrayInputStream &stream) {
    m_images.clear();
    m_tiles.clear();

    const std::uint8_t imageCount = stream.ReadUInt8();
    m_images.resize(imageCount);
    for (std::uint8_t i = 0; i < imageCount; ++i) {
        m_images[i].Init(stream);
    }

    const std::uint8_t tileCount = stream.ReadUInt8();
    m_tiles.resize(tileCount);
    for (std::uint8_t i = 0; i < tileCount; ++i) {
        m_tiles[i].imageIndex = stream.ReadUInt8();
        m_tiles[i].x = stream.ReadUInt16();
        m_tiles[i].y = stream.ReadUInt16();
        m_tiles[i].width = stream.ReadUInt16();
        m_tiles[i].height = stream.ReadUInt16();
    }

    if (stream.Overran()) {
        std::printf("[tileset] truncated\n");
        m_images.clear();
        m_tiles.clear();
        return false;
    }

    return true;
}

std::uint16_t TileSet::GetDrawSize() const {
    if (m_tiles.empty()) {
        return 0;
    }
    return m_tiles[0].width;
}
