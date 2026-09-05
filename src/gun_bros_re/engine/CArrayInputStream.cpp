/**
 * @file CArrayInputStream.cpp
 * @brief Sequential reader over a decompressed resource payload.
 */

#include "engine/CArrayInputStream.h"

CArrayInputStream::CArrayInputStream(const std::uint8_t *data, std::size_t size)
    : m_data(data), m_size(size), m_position(0), m_overran(false) {}

CArrayInputStream::CArrayInputStream(const std::vector<std::uint8_t> &data)
    : m_data(data.data()), m_size(data.size()), m_position(0), m_overran(false) {}

bool CArrayInputStream::CanRead(std::size_t count) {
    if (m_position + count > m_size) {
        m_overran = true;
        return false;
    }
    return true;
}

std::uint8_t CArrayInputStream::ReadUInt8() {
    if (!CanRead(1)) {
        return 0;
    }
    return m_data[m_position++];
}

std::uint16_t CArrayInputStream::ReadUInt16() {
    if (!CanRead(2)) {
        return 0;
    }
    const std::uint16_t value =
        static_cast<std::uint16_t>(m_data[m_position] | (m_data[m_position + 1] << 8));
    m_position += 2;
    return value;
}

std::uint32_t CArrayInputStream::ReadUInt32() {
    if (!CanRead(4)) {
        return 0;
    }
    const std::uint32_t value = static_cast<std::uint32_t>(m_data[m_position]) |
                                (static_cast<std::uint32_t>(m_data[m_position + 1]) << 8) |
                                (static_cast<std::uint32_t>(m_data[m_position + 2]) << 16) |
                                (static_cast<std::uint32_t>(m_data[m_position + 3]) << 24);
    m_position += 4;
    return value;
}

std::int16_t CArrayInputStream::ReadInt16() {
    return static_cast<std::int16_t>(ReadUInt16());
}

std::int32_t CArrayInputStream::ReadInt32() {
    return static_cast<std::int32_t>(ReadUInt32());
}

void CArrayInputStream::Skip(std::size_t count) {
    if (!CanRead(count)) {
        m_position = m_size;
        return;
    }
    m_position += count;
}

std::size_t CArrayInputStream::Available() const {
    return m_size - m_position;
}
