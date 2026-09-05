/**
 * @file CLayerTile.cpp
 * @brief One grid of tiles.
 */

#include "gun_bros/CLayerTile.h"

#include "gun_bros/TileSet.h"

#include <cstdio>

const TileCell CLayerTile::s_emptyCell = {kEmptyTileId, 0};

CLayerTile::CLayerTile() : m_width(0), m_height(0) {}

bool CLayerTile::Init(CArrayInputStream &stream) {
    m_cells.clear();

    // The original reads this byte and throws it away.
    stream.ReadUInt8();

    m_width = stream.ReadUInt16();
    m_height = stream.ReadUInt16();

    const std::uint32_t cellCount = static_cast<std::uint32_t>(m_width) * m_height;
    m_cells.resize(cellCount);
    for (std::uint32_t i = 0; i < cellCount; ++i) {
        m_cells[i].tileId = stream.ReadUInt8();
        m_cells[i].flags = stream.ReadUInt8();
    }

    if (stream.Overran()) {
        std::printf("[layertile] truncated at %ux%u\n", m_width, m_height);
        m_width = 0;
        m_height = 0;
        m_cells.clear();
        return false;
    }

    return true;
}

const TileCell &CLayerTile::GetCell(std::uint32_t column, std::uint32_t row) const {
    if (m_width == 0 || m_height == 0) {
        return s_emptyCell;
    }

    // Wrap, so a layer smaller than the canvas repeats instead of running out.
    const std::uint32_t wrappedColumn = column % m_width;
    const std::uint32_t wrappedRow = row % m_height;

    return m_cells[wrappedRow * m_width + wrappedColumn];
}
