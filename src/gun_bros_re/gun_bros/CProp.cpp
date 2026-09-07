/**
 * @file CProp.cpp
 * @brief The template behind a placed prop: which sprite it draws.
 */

#include "gun_bros/CProp.h"

#include "gun_bros/CGameAssetRef.h"

#include <cstdio>

CGameSpriteGluRef::CGameSpriteGluRef()
    : packHash(kNullPackHash), archetype(255), action(255), animation(255) {}

void CGameSpriteGluRef::Init(CArrayInputStream &stream) {
    // All four fields are always present, unlike GameObjectRef, whose index
    // byte disappears when its hash is zero.
    packHash = stream.ReadUInt32();
    archetype = stream.ReadUInt8();
    action = stream.ReadUInt8();
    animation = stream.ReadUInt8();
}

CProp::Template::Template()
    : m_foregroundAnimation(255), m_backgroundAnimation(255), m_persistent(0) {}

bool CProp::Template::Init(CArrayInputStream &stream) {
    m_sprite.Init(stream);

    // Foreground first on the wire, background second.
    m_foregroundAnimation = stream.ReadUInt8();
    m_backgroundAnimation = stream.ReadUInt8();

    if (!m_collision.Load(stream)) {
        return false;
    }
    if (!m_bulletCollision.Load(stream)) {
        return false;
    }

    m_persistent = stream.ReadUInt8();
    m_script.Load(stream);

    if (stream.Overran()) {
        std::printf("[prop] template truncated\n");
        return false;
    }

    return true;
}
