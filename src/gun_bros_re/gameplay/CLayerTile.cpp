/**
 * @file CLayerTile.cpp
 * @brief One grid of tiles.
 */

#include "gun_bros_re/gameplay/CLayerTile.h"

#include "gun_bros_re/gameplay/TileSet.h"

#include <cstdio>

const TileCell CLayerTile::s_emptyCell = {kEmptyTileId, 0};

CLayerTile::CLayerTile()
    : m_width(0),
      m_height(0),
      m_speedX(0.0f),
      m_speedY(0.0f),
      m_offsetX(0.0f),
      m_offsetY(0.0f) {}

void CLayerTile::SetSpeed(float speedX, float speedY) {
    m_speedX = speedX * kLayerSpeedScale;
    m_speedY = speedY * kLayerSpeedScale;
}

void CLayerTile::Update(std::uint16_t deltaMs) {
    const float seconds = deltaMs * 0.001f;

    // Truncation towards zero, which is what the original's float-to-int-to-
    // float round trip does -- so a negative offset keeps its negative
    // fraction rather than stepping down a whole tile.
    const float movedX = m_offsetX + m_speedX * seconds;
    const float movedY = m_offsetY + m_speedY * seconds;
    m_offsetX = movedX - static_cast<float>(static_cast<int>(movedX));
    m_offsetY = movedY - static_cast<float>(static_cast<int>(movedY));
}

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
