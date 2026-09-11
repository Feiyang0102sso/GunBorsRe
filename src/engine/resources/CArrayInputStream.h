/**
 * @file CArrayInputStream.h
 * @brief Sequential reader over a decompressed resource payload.
 *
 * Port of com::glu::platform::components::CArrayInputStream.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:355887 (Open)
 *
 * Every Init in the engine takes a stream and reads fields off it in order,
 * so the ports read the same way. Everything inside a .big is little-endian;
 * packTOC_*.dat is the one big-endian file and is parsed elsewhere.
 *
 * Reads past the end return zero and latch an overrun flag rather than
 * throwing, so a parser can run to completion and be checked once at the end.
 */

#ifndef GUN_BROS_RE_ENGINE_CARRAYINPUTSTREAM_H
#define GUN_BROS_RE_ENGINE_CARRAYINPUTSTREAM_H

#include <cstddef>
#include <cstdint>
#include <vector>

class CArrayInputStream {
public:
    CArrayInputStream(const std::uint8_t *data, std::size_t size);
    explicit CArrayInputStream(const std::vector<std::uint8_t> &data);

    std::uint8_t ReadUInt8();
    std::uint16_t ReadUInt16();
    std::uint32_t ReadUInt32();
    std::int16_t ReadInt16();
    std::int32_t ReadInt32();

    /** Advance without reading. */
    void Skip(std::size_t count);

    /** Bytes not yet consumed. */
    std::size_t Available() const;

    std::size_t Position() const { return m_position; }

    /** True once any read has run past the end. */
    bool Overran() const { return m_overran; }

private:
    /** Bounds-check a read of `count` bytes; latches the flag and returns false. */
    bool CanRead(std::size_t count);

    const std::uint8_t *m_data;
    std::size_t m_size;
    std::size_t m_position;
    bool m_overran;
};

#endif  // GUN_BROS_RE_ENGINE_CARRAYINPUTSTREAM_H
