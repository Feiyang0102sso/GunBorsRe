/**
 * @file CGun.cpp
 * @brief The weapon template: stat tables, a script, and a model set.
 */

#include "gun_bros/CGun.h"

#include <cstdio>

namespace {

// Same 16.16 fixed point the move set speeds use.
constexpr float kFixedPointScale = 1.0f / 65536.0f;

/** One stat table: a uint16 count followed by that many uint32 values. */
void ReadStatTable(CArrayInputStream &stream, std::vector<std::uint32_t> &values) {
    const std::uint16_t count = stream.ReadUInt16();
    values.resize(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        values[i] = stream.ReadUInt32();
    }
}

}  // namespace

CGun::Template::Template()
    : m_flag104(0),
      m_value140(0),
      m_scalar144(0.0f),
      m_value148(0),
      m_scalar152(0.0f),
      m_flag256(0) {}

bool CGun::Template::Init(CArrayInputStream &stream) {
    m_flag104 = stream.ReadUInt8();
    m_meshRef.Init(stream);
    m_imageRef.Init(stream);
    m_objectRef132.Init(stream);
    m_value140 = stream.ReadUInt16();
    m_scalar144 = static_cast<float>(stream.ReadInt32()) * kFixedPointScale;
    m_value148 = stream.ReadUInt16();
    m_scalar152 = static_cast<float>(stream.ReadInt32()) * kFixedPointScale;
    m_flag256 = stream.ReadUInt8();

    m_script.Load(stream);

    for (std::uint32_t i = 0; i < kGunStatTableCount; ++i) {
        ReadStatTable(stream, m_statTables[i]);
    }

    if (!m_moveSet.Init(stream)) {
        return false;
    }

    if (stream.Overran()) {
        std::printf("[gun] template truncated\n");
        return false;
    }

    return true;
}
