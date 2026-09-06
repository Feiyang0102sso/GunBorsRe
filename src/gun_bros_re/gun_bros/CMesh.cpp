/**
 * @file CMesh.cpp
 * @brief One 3D model: index buffer, static UVs, and per-frame vertex data.
 */

#include "gun_bros/CMesh.h"

#include <cstdio>
#include <cstring>

namespace {

/**
 * Reinterpret the next four bytes as a float.
 *
 * Floats travel as raw bit patterns: the original reads them with ReadInt32
 * and hands the words straight to the vertex buffer, so there is no conversion
 * to undo. Both sides are little-endian IEEE 754.
 */
float ReadFloat(CArrayInputStream &stream) {
    const std::int32_t bits = stream.ReadInt32();
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

/** Length-prefixed, not null terminated; the engine terminates it itself. */
std::string ReadBoneName(CArrayInputStream &stream) {
    const std::uint8_t length = stream.ReadUInt8();
    std::string name;
    name.reserve(length);
    for (std::uint8_t i = 0; i < length; ++i) {
        name.push_back(static_cast<char>(stream.ReadUInt8()));
    }
    return name;
}

}  // namespace

CMesh::CMesh() : m_vertexCount(0), m_bounds() {}

bool CMesh::Init(CArrayInputStream &stream) {
    m_boneNames.clear();
    m_indices.clear();
    m_texCoords.clear();
    m_frames.clear();
    m_vertexCount = 0;
    m_bounds = MeshBounds();

    const std::uint8_t magic = stream.ReadUInt8();
    const std::uint32_t indexCount = stream.ReadUInt32();
    const std::uint8_t boneCount = stream.ReadUInt8();
    const std::uint16_t frameCount = stream.ReadUInt16();
    const std::uint16_t vertexCount = stream.ReadUInt16();

    if (magic != kMeshMagic) {
        std::printf("[mesh] magic %u, expected %u\n", magic, kMeshMagic);
    }

    for (std::uint8_t i = 0; i < boneCount; ++i) {
        m_boneNames.push_back(ReadBoneName(stream));
    }

    // The header sizes the rest of the resource exactly, so check it before
    // allocating: a resource that is not a mesh would otherwise ask for a
    // gigabyte of index buffer on its way to failing.
    const std::size_t frameSize = 4 + 28 * static_cast<std::size_t>(boneCount) +
                                  12 * static_cast<std::size_t>(vertexCount);
    const std::size_t promised = 2 * static_cast<std::size_t>(indexCount) +
                                 8 * static_cast<std::size_t>(vertexCount) +
                                 frameSize * static_cast<std::size_t>(frameCount);
    if (promised > stream.Available()) {
        std::printf("[mesh] header wants %zu bytes, %zu left\n", promised,
                    stream.Available());
        m_boneNames.clear();
        return false;
    }

    m_vertexCount = vertexCount;

    m_indices.resize(indexCount);
    for (std::uint32_t i = 0; i < indexCount; ++i) {
        m_indices[i] = stream.ReadUInt16();
    }

    m_texCoords.resize(2 * static_cast<std::size_t>(vertexCount));
    for (std::size_t i = 0; i < m_texCoords.size(); ++i) {
        m_texCoords[i] = ReadFloat(stream);
    }

    m_frames.resize(frameCount);
    for (std::uint16_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        MeshFrame &frame = m_frames[frameIndex];
        frame.timeMs = stream.ReadInt32();

        frame.bones.resize(boneCount);
        for (std::uint8_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
            MeshBoneTransform &bone = frame.bones[boneIndex];
            bone.posX = ReadFloat(stream);
            bone.posY = ReadFloat(stream);
            bone.posZ = ReadFloat(stream);
            bone.rotX = ReadFloat(stream);
            bone.rotY = ReadFloat(stream);
            bone.rotZ = ReadFloat(stream);

            // The original negates the last component on the way in (:97705,
            // the vneg after the seventh read). The quaternions on the wire
            // are conjugates of the ones the engine works with.
            bone.rotW = -ReadFloat(stream);
        }

        frame.vertices.resize(kFloatsPerVertex * static_cast<std::size_t>(vertexCount));
        for (std::size_t i = 0; i < frame.vertices.size(); ++i) {
            frame.vertices[i] = ReadFloat(stream);
        }
    }

    if (stream.Overran()) {
        std::printf("[mesh] truncated\n");
        m_boneNames.clear();
        m_indices.clear();
        m_texCoords.clear();
        m_frames.clear();
        m_vertexCount = 0;
        return false;
    }

    ComputeBounds();
    return true;
}

std::int32_t CMesh::GetDurationMs() const {
    if (m_frames.empty()) {
        return 0;
    }
    return m_frames.back().timeMs;
}

void CMesh::ComputeBounds() {
    m_bounds = MeshBounds();

    // The first frame that has vertices, not necessarily frame 0: with a move
    // set bound, the frames no move uses are left empty.
    const MeshFrame *frame = nullptr;
    for (std::size_t i = 0; i < m_frames.size(); ++i) {
        if (!m_frames[i].vertices.empty()) {
            frame = &m_frames[i];
            break;
        }
    }
    if (frame == nullptr) {
        return;
    }

    // Seeded at zero, as the original does, so the box always contains the
    // origin even when the model sits entirely off to one side.
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    for (std::size_t i = 0; i + 2 < frame->vertices.size(); i += kFloatsPerVertex) {
        const float x = frame->vertices[i];
        const float y = frame->vertices[i + 1];
        const float z = frame->vertices[i + 2];

        if (x < minX) {
            minX = x;
        }
        if (y < minY) {
            minY = y;
        }
        if (z < minZ) {
            minZ = z;
        }
        if (x > maxX) {
            maxX = x;
        }
        if (y > maxY) {
            maxY = y;
        }
        if (z > maxZ) {
            maxZ = z;
        }
    }

    m_bounds.minX = minX;
    m_bounds.minY = minY;
    m_bounds.minZ = minZ;
    m_bounds.maxX = maxX;
    m_bounds.maxY = maxY;
    m_bounds.maxZ = maxZ;
    m_bounds.centerX = 0.5f * (minX + maxX);
    m_bounds.centerY = 0.5f * (minY + maxY);
    m_bounds.centerZ = 0.5f * (minZ + maxZ);

    // The draw scale is one over the longest side, which is how the engine
    // normalises models of wildly different sizes.
    float extent = maxX - minX;
    if (maxY - minY > extent) {
        extent = maxY - minY;
    }
    if (maxZ - minZ > extent) {
        extent = maxZ - minZ;
    }
    if (extent > 0.0f) {
        m_bounds.inverseExtent = 1.0f / extent;
    }
}
