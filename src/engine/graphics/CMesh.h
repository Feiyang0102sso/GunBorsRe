/**
 * @file CMesh.h
 * @brief One 3D model: index buffer, static UVs, and per-frame vertex data.
 *
 * Port of CMesh (src/gunbros/mesh.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:97705 (Init), :98184 (ComputeBounds)
 *
 * Wire format (Section 31):
 *   uint8  magic                                    -- always 3, discarded
 *   uint32 indexCount
 *   uint8  boneCount
 *   uint16 frameCount
 *   uint16 vertexCount
 *   { uint8 length, char name[length] } [boneCount] -- not null terminated
 *   uint16 index[indexCount]
 *   float  uv[vertexCount][2]
 *   { int32 timeMs,
 *     float bone[boneCount][7],                     -- pos xyz + quat xyzw
 *     float vertex[vertexCount][3] } [frameCount]
 *
 * One frame is 4 + 28 * boneCount + 12 * vertexCount bytes.
 *
 * The UVs are kept exactly as they arrive, and they measure v from the BOTTOM
 * of the texture -- the opposite of the sprite line, whose UVs are pixel rows
 * counted from the top. CMeshBuffer is where that gets reconciled; see its
 * header for why the flip lives there and not here.
 *
 * Three things differ from the sprite line:
 *
 * - The animation is per-frame vertex data, not skinning. Every frame carries
 *   a full copy of the vertex array and BuildTweenFrame (:98051) lerps between
 *   two of them. The bones ride along as attachment points -- muzzle, hand --
 *   and never deform anything.
 * - The index buffer is one triangle strip. Repeated indices build zero-area
 *   triangles that stitch separate strips together.
 * - The texture is not in here. CMesh::Init takes a CMoveSetMesh as its third
 *   argument and that is where the atlas comes from; M4b adds it.
 *
 * That third argument also decides which frames get loaded: the original skips
 * any frame no move uses, and loads all of them when it is null. Nothing calls
 * this with a move set yet, so this port is the null path.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CMESH_H
#define GUN_BROS_RE_GUN_BROS_CMESH_H

#include "engine/resources/CArrayInputStream.h"

#include <cstdint>
#include <string>
#include <vector>

// First byte of every mesh resource. The original reads it and throws it away.
constexpr std::uint8_t kMeshMagic = 3;

// Floats per bone transform: 3 for the position, 4 for the quaternion.
constexpr std::uint32_t kFloatsPerBone = 7;

// Floats per vertex position.
constexpr std::uint32_t kFloatsPerVertex = 3;

/** Where one bone sits on one frame. An attachment point, not a joint. */
struct MeshBoneTransform {
    float posX, posY, posZ;
    float rotX, rotY, rotZ, rotW;
};

/** One key frame: when it happens, where the bones are, where every vertex is. */
struct MeshFrame {
    std::int32_t timeMs;
    std::vector<MeshBoneTransform> bones;
    std::vector<float> vertices;  // kFloatsPerVertex per vertex
};

/**
 * The box the model occupies, plus what the engine derives from it.
 *
 * ComputeBounds seeds the minimum and the maximum at zero rather than at the
 * first vertex, so the box always contains the origin. Copied as-is: it is
 * what the draw scale is built on.
 */
struct MeshBounds {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
    float centerX, centerY, centerZ;
    float inverseExtent;  // 1 / longest side, or 0 for a mesh with no vertices
};

/**
 * One model out of Section 31.
 *
 * Vertices are kept flat -- three floats per vertex, in the order they arrive
 * -- because that is the shape a vertex buffer wants.
 */
class CMoveSetMesh;
class CMesh {
public:
    CMesh();

    bool Init(CArrayInputStream &stream, const CMoveSetMesh *moveSet = nullptr);

    const std::vector<std::string> &GetBoneNames() const { return m_boneNames; }
    const std::vector<std::uint16_t> &GetIndices() const { return m_indices; }

    /** Two floats per vertex, static across every frame. */
    const std::vector<float> &GetTexCoords() const { return m_texCoords; }

    const std::vector<MeshFrame> &GetFrames() const { return m_frames; }

    std::uint32_t GetVertexCount() const { return m_vertexCount; }
    const MeshBounds &GetBounds() const { return m_bounds; }

    /** Timestamp of the last frame, which is how long the animation runs. */
    std::int32_t GetDurationMs() const;

    // -----------------------------------------------------------------------
    // The time evaluator
    //
    // Two outlets on one mechanism: both find the pair of key frames that
    // bracket a moment and interpolate between them. Vertices lerp; bone
    // rotations nlerp. The whole of assembly hangs off GetNodeAt -- a gun goes
    // in a hand by asking where the "hand" bone is right now.
    // -----------------------------------------------------------------------

    /**
     * The model's vertices at a moment, in the same flat layout as a frame's.
     *
     * Port of GetVerticesAt (:98486). Time past the end wraps, so a caller can
     * hand this a clock that only ever grows.
     *
     * Returns false when the bracketing frame carries no vertices, which is
     * what a frame skipped at load time looks like; `out` is left alone, so a
     * caller that keeps its buffer between calls simply holds the last pose.
     */
    bool GetVerticesAt(std::int32_t timeMs, std::vector<float> &out) const;

    /**
     * Where one bone sits at a moment.
     *
     * Port of GetNodeAt (:98298). Returns false for a bone index this mesh
     * does not have.
     */
    bool GetNodeAt(std::int32_t timeMs, std::size_t boneIndex,
                   MeshBoneTransform &out) const;

    /**
     * Blend two frames' vertices into `out`, at `t` from the first to the
     * second. Port of BuildTweenFrame (:98051).
     *
     * `t` outside [0, 1] collapses to whichever end it passed, exactly as the
     * original does -- it is the one place the two frames need not bracket
     * anything.
     */
    bool BuildTweenFrame(std::size_t firstFrame, std::size_t secondFrame,
                         float t, std::vector<float> &out) const;

private:
    /**
     * The two frames bracketing a moment, and how far between them it falls.
     *
     * Both outlets of the evaluator open with this same search; the original
     * writes it out twice, once in each.
     */
    void FindFramesAt(std::int32_t timeMs, std::size_t &firstFrame,
                      std::size_t &secondFrame, float &t) const;

    /** Min, max, centre and draw scale, off the first frame that has vertices. */
    void ComputeBounds();

    std::vector<std::string> m_boneNames;
    std::vector<std::uint16_t> m_indices;
    std::vector<float> m_texCoords;
    std::vector<MeshFrame> m_frames;
    std::uint32_t m_vertexCount;
    MeshBounds m_bounds;
};

#endif  // GUN_BROS_RE_GUN_BROS_CMESH_H
