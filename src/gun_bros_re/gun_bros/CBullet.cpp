/**
 * @file CBullet.cpp
 * @brief The projectile template: a sprite, a model, and a pile of scalars.
 */

#include "gun_bros/CBullet.h"

#include <cstdio>

namespace {

// The same 16.16 fixed point every template scalar uses.
constexpr float kFixedPointScale = 1.0f / 65536.0f;

float ReadFixedPoint(CArrayInputStream &stream) {
    return static_cast<float>(stream.ReadInt32()) * kFixedPointScale;
}

}  // namespace

CBullet::Template::Template()
    : m_value16(0),
      m_value18(0),
      m_value20(0),
      m_flag32(0),
      m_scalar28(0.0f),
      m_value128(0),
      m_scalar24(0.0f),
      m_scalar256(0.0f),
      m_scalar116(0.0f),
      m_scalar120(0.0f),
      m_value124(0),
      m_scalar260(0.0f),
      m_value264(0),
      m_flag266(0) {}

bool CBullet::Template::Init(CArrayInputStream &stream) {
    m_sprite.Init(stream);
    m_meshRef.Init(stream);
    m_imageRef.Init(stream);

    // One byte the original steps over without reading.
    stream.Skip(1);

    m_value16 = stream.ReadInt16();
    m_value18 = stream.ReadInt16();
    m_value20 = stream.ReadInt16();
    m_flag32 = stream.ReadUInt8();
    m_scalar28 = ReadFixedPoint(stream);

    m_script.Load(stream);

    m_value128 = stream.ReadUInt32();
    m_scalar24 = ReadFixedPoint(stream);
    m_scalar256 = ReadFixedPoint(stream);
    m_scalar116 = ReadFixedPoint(stream);
    m_scalar120 = ReadFixedPoint(stream);
    m_value124 = stream.ReadUInt16();
    m_scalar260 = ReadFixedPoint(stream);
    m_value264 = stream.ReadUInt16();
    m_flag266 = stream.ReadUInt8();

    if (stream.Overran()) {
        std::printf("[bullet] template truncated\n");
        return false;
    }

    return true;
}
