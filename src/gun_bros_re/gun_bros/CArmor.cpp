/**
 * @file CArmor.cpp
 * @brief The armour template: two model variants, one per brother.
 */

#include "gun_bros/CArmor.h"

#include <cstdio>

CArmor::Template::Template() : m_flag4(0), m_flag224(0), m_flag225(0) {}

bool CArmor::Template::Init(CArrayInputStream &stream) {
    // The wire order interleaves the variants with their flags, so this does
    // not loop: mesh and image of variant 0, a flag, then variant 1, a flag,
    // then both sprite fallbacks together.
    m_flag4 = stream.ReadUInt8();
    m_meshRef[0].Init(stream);
    m_imageRef[0].Init(stream);
    m_flag224 = stream.ReadUInt8();
    m_meshRef[1].Init(stream);
    m_imageRef[1].Init(stream);
    m_flag225 = stream.ReadUInt8();
    m_spriteImageRef[0].Init(stream);
    m_spriteImageRef[1].Init(stream);

    m_script.Load(stream);

    if (stream.Overran()) {
        std::printf("[armor] template truncated\n");
        return false;
    }

    return true;
}
