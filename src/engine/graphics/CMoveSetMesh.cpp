/**
 * @file CMoveSetMesh.cpp
 * @brief A character's model set: the meshes it can wear, and its moves.
 */

#include "engine/graphics/CMoveSetMesh.h"

#include <cstdio>
#include "engine/graphics/CMesh.h"
#include "engine/resources/CResourceLoader.h"

CMoveSetMesh::CMoveSetMesh() : m_packHash(0) {}

bool CMoveSetMesh::Init(CArrayInputStream &stream) {
    m_packHash = 0;
    m_meshConfigs.clear();
    m_moves.clear();
    m_meshes.clear();

    // The original turns the hash into a pack index here and keeps only that.
    // We keep the hash: resolving it needs the TOC manager, which a template
    // parser has no business holding.
    m_packHash = stream.ReadUInt32();

    const std::uint8_t meshConfigCount = stream.ReadUInt8();
    m_meshConfigs.resize(meshConfigCount);
    for (std::uint8_t i = 0; i < meshConfigCount; ++i) {
        m_meshConfigs[i].meshOrdinal = stream.ReadUInt8();
        m_meshConfigs[i].imageOrdinal = stream.ReadUInt8();
        m_meshes.push_back(std::make_shared<CMesh>());
    }

    const std::uint8_t moveCount = stream.ReadUInt8();
    m_moves.resize(moveCount);
    for (std::uint8_t i = 0; i < moveCount; ++i) {
        ZMeshMove &move = m_moves[i];
        move.meshConfigIndex = stream.ReadUInt8();
        move.firstFrame = stream.ReadUInt16();
        move.lastFrame = stream.ReadUInt16();
        move.restartsWhenRepeated = stream.ReadUInt8();
        move.speed = static_cast<float>(stream.ReadInt32()) * kMoveSpeedScale;
        move.unknown = stream.ReadUInt32();

        const std::uint8_t soundCount = stream.ReadUInt8();
        move.sounds.resize(soundCount);
        for (std::uint8_t soundIndex = 0; soundIndex < soundCount; ++soundIndex) {
            move.sounds[soundIndex].frame = stream.ReadUInt16();
            move.sounds[soundIndex].soundId = stream.ReadUInt8();
        }
    }

    if (stream.Overran()) {
        std::printf("[moveset] truncated\n");
        m_meshConfigs.clear();
        m_moves.clear();
        return false;
    }

    return true;
}

bool CMoveSetMesh::IsFrameUsedInMoves(std::uint16_t frameIndex) const {
    const int frame = static_cast<int>(frameIndex);
    for (std::size_t i = 0; i < m_moves.size(); ++i) {
        // One frame of lead-in: the original compares against firstFrame - 1,
        // which underflows to -1 for a move starting at frame 0 and so lets
        // frame 0 through.
        const int first = static_cast<int>(m_moves[i].firstFrame) - 1;
        const int last = static_cast<int>(m_moves[i].lastFrame);
        if (first <= frame && frame <= last) {
            return true;
        }
    }
    return false;
}
