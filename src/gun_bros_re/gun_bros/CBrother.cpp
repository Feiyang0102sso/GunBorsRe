/**
 * @file CBrother.cpp
 * @brief The player template: a script, a model set, and a shadow sprite.
 */

#include "gun_bros/CBrother.h"

#include <cstdio>

CBrother::Template::Template() : m_unknown(0.0f) {}

bool CBrother::Template::Init(CArrayInputStream &stream) {
    m_script.Load(stream);

    if (!m_moveSet.Init(stream)) {
        return false;
    }

    m_objectRef.Init(stream);
    m_unknown = static_cast<float>(stream.ReadUInt16());

    // Two more object references that the original reads into one stack slot
    // and never reads back -- the second overwrites the first. They are read
    // here for the same reason: to keep the stream in step.
    GameObjectRef discarded;
    discarded.Init(stream);
    discarded.Init(stream);

    m_shadowSprite.Init(stream);

    if (stream.Overran()) {
        std::printf("[brother] template truncated\n");
        return false;
    }

    return true;
}
