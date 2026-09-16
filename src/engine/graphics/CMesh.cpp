/**
 * @file CMesh.cpp
 * @brief One 3D model: index buffer, static UVs, and per-frame vertex data.
 */

#include "engine/graphics/CMesh.h"
#include "engine/graphics/CMoveSetMesh.h"

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

bool CMesh::Init(CArrayInputStream &stream, const CMoveSetMesh *moveSet) {
    m_boneNames.clear();
    m_indices.clear();
    m_texCoords.clear();
    m_frames.clear();
    m_vertexCount = 0;
    m_bounds = ZMeshBounds();

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
        ZMeshFrame &frame = m_frames[frameIndex];
        frame.timeMs = stream.ReadInt32();

        // CMesh::Init :97965 keeps timestamps but skips unused frame data.
        // ComputeBounds must therefore use the first retained frame, not an
        // unrelated pose at file frame zero. The range comes from the BIG move set.
        if (moveSet != nullptr && !moveSet->IsFrameUsedInMoves(frameIndex)) {
            stream.Skip(frameSize - 4);
            continue;
        }

        frame.bones.resize(boneCount);
        for (std::uint8_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
            ZMeshBoneTransform &bone = frame.bones[boneIndex];
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

void CMesh::FindFramesAt(std::int32_t timeMs, std::size_t &firstFrame,
                         std::size_t &secondFrame, float &t) const {
    // A model with one frame is not animated; so is one whose frames all sit
    // at the same timestamp, and that case has to be caught before the modulo
    // below divides by its length.
    const std::int32_t duration = GetDurationMs();
    if (m_frames.size() <= 1 || duration <= 0) {
        firstFrame = 0;
        secondFrame = 0;
        t = 0.0f;
        return;
    }

    // Time past the last frame wraps round, which is what lets a caller feed
    // this a clock that only grows.
    std::int32_t wrapped = timeMs;
    if (duration < timeMs) {
        wrapped = timeMs % duration;
    }

    for (std::size_t i = 0; i < m_frames.size(); ++i) {
        if (m_frames[i].timeMs < wrapped) {
            continue;
        }

        // The first frame cannot be the second half of a pair.
        if (i == 0) {
            firstFrame = 0;
            secondFrame = 0;
            t = 0.0f;
            return;
        }

        const std::int32_t startMs = m_frames[i - 1].timeMs;
        const std::int32_t endMs = m_frames[i].timeMs;
        firstFrame = i - 1;
        secondFrame = i;
        t = static_cast<float>(wrapped - startMs) / static_cast<float>(endMs - startMs);
        return;
    }

    // Unreachable after the wrap above, because the last frame's timestamp is
    // the duration the wrap is taken against. The original falls through here
    // with an index of -1 and reads off the end of the array; we hold on the
    // last frame instead.
    firstFrame = m_frames.size() - 1;
    secondFrame = m_frames.size() - 1;
    t = 0.0f;
}

bool CMesh::BuildTweenFrame(std::size_t firstFrame, std::size_t secondFrame,
                            float t, std::vector<float> &out) const {
    if (firstFrame >= m_frames.size() || secondFrame >= m_frames.size()) {
        return false;
    }

    // Which two frames actually get blended: t at or outside either end
    // collapses the pair onto one frame, and the copy below runs instead.
    std::size_t startFrame = firstFrame;
    std::size_t endFrame = firstFrame;
    if (t > 0.0f) {
        endFrame = secondFrame;
        if (t < 1.0f) {
            startFrame = firstFrame;
        } else {
            startFrame = secondFrame;
        }
    }

    // The original sizes the write off the start frame, so a start frame with
    // no vertices writes nothing at all -- that is what a frame skipped at
    // load time does. Say so rather than leaving the caller a stale buffer it
    // cannot tell apart from a fresh one.
    const std::vector<float> &startVertices = m_frames[startFrame].vertices;
    if (startVertices.empty()) {
        return false;
    }

    out.resize(startVertices.size());

    if (startFrame == endFrame) {
        for (std::size_t i = 0; i < startVertices.size(); ++i) {
            out[i] = startVertices[i];
        }
        return true;
    }

    const std::vector<float> &endVertices = m_frames[endFrame].vertices;
    if (endVertices.size() < startVertices.size()) {
        return false;
    }

    const float startWeight = 1.0f - t;
    for (std::size_t i = 0; i < startVertices.size(); ++i) {
        out[i] = startVertices[i] * startWeight + endVertices[i] * t;
    }
    return true;
}

bool CMesh::GetVerticesAt(std::int32_t timeMs, std::vector<float> &out) const {
    if (m_frames.empty()) {
        return false;
    }

    std::size_t firstFrame = 0;
    std::size_t secondFrame = 0;
    float t = 0.0f;
    FindFramesAt(timeMs, firstFrame, secondFrame, t);
    return BuildTweenFrame(firstFrame, secondFrame, t, out);
}

bool CMesh::GetNodeAt(std::int32_t timeMs, std::size_t boneIndex,
                      ZMeshBoneTransform &out) const {
    // Against the name list, not against a frame's bone array: a frame skipped
    // at load time has no bones, and the names are always all there.
    if (m_frames.empty() || boneIndex >= m_boneNames.size()) {
        return false;
    }

    std::size_t firstFrame = 0;
    std::size_t secondFrame = 0;
    float t = 0.0f;
    FindFramesAt(timeMs, firstFrame, secondFrame, t);

    if (boneIndex >= m_frames[firstFrame].bones.size() ||
        boneIndex >= m_frames[secondFrame].bones.size()) {
        return false;
    }

    if (firstFrame == secondFrame) {
        out = m_frames[firstFrame].bones[boneIndex];
        return true;
    }

    const ZMeshBoneTransform &start = m_frames[firstFrame].bones[boneIndex];
    ZMeshBoneTransform end = m_frames[secondFrame].bones[boneIndex];

    // Two unit quaternions describe the same rotation when one is the other
    // negated, and the straight-line blend below takes the short way round
    // only if they point the same way to begin with. A negative dot product
    // means they do not, so flip one.
    const float dot = start.rotX * end.rotX + start.rotY * end.rotY +
                      start.rotZ * end.rotZ + start.rotW * end.rotW;
    if (dot < 0.0f) {
        end.rotX = -end.rotX;
        end.rotY = -end.rotY;
        end.rotZ = -end.rotZ;
        end.rotW = -end.rotW;
    }

    const float startWeight = 1.0f - t;
    out.posX = start.posX * startWeight + end.posX * t;
    out.posY = start.posY * startWeight + end.posY * t;
    out.posZ = start.posZ * startWeight + end.posZ * t;

    // Straight lerp, and deliberately not normalised afterwards: the original
    // does not either. Between neighbouring key frames the two quaternions are
    // close enough that the length error is a fraction of a per cent, and it
    // shows up as a scale on the attached part rather than a wrong angle.
    out.rotX = start.rotX * startWeight + end.rotX * t;
    out.rotY = start.rotY * startWeight + end.rotY * t;
    out.rotZ = start.rotZ * startWeight + end.rotZ * t;
    out.rotW = start.rotW * startWeight + end.rotW * t;
    return true;
}

void CMesh::ComputeBounds() {
    m_bounds = ZMeshBounds();

    // The first frame that has vertices, not necessarily frame 0: with a move
    // set bound, the frames no move uses are left empty.
    const ZMeshFrame *frame = nullptr;
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
