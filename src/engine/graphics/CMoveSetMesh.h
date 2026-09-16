/**
 * @file CMoveSetMesh.h
 * @brief A character's model set: the meshes it can wear, and its moves.
 *
 * Port of CMoveSetMesh (src/gunbros/moveSetMesh.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:122930 (Init), :123157 (LoadMesh),
 *            :123213 (Load), :134082 (CMoveSetMeshController::SetMove),
 *            :123080 (IsFrameUsedInMoves), :123117 (GetMoveLengthMS)
 *
 * Wire format:
 *   uint32 packHash
 *   uint8  meshConfigCount
 *   { uint8 meshOrdinal, uint8 imageOrdinal } [meshConfigCount]
 *   uint8  moveCount
 *   { uint8  meshConfigIndex,
 *     uint16 firstFrame, uint16 lastFrame,
 *     uint8  restartsWhenRepeated,
 *     int32  speed,                                -- 16.16 fixed point
 *     uint32 unknown,
 *     uint8  soundCount,
 *     { uint16 frame, uint8 soundId } [soundCount] } [moveCount]
 *
 * **This is where a mesh gets its texture.** LoadMesh resolves meshOrdinal
 * against section 31 and Load resolves imageOrdinal against section 29, both
 * inside the pack the hash names, so the pair is "model plus atlas" and a
 * CMesh on its own can never be drawn.
 *
 * A move is a frame range of one of those meshes. Nothing here plays one --
 * that is CMoveSetMeshController, which M4b needs and this does not.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CMOVESETMESH_H
#define GUN_BROS_RE_GUN_BROS_CMOVESETMESH_H

#include "engine/resources/CArrayInputStream.h"

#include <cstdint>
#include <vector>

// Speed is stored as 16.16 fixed point; 1.0 means "play as authored".
constexpr float kMoveSpeedScale = 1.0f / 65536.0f;

/** One model the character can put on screen, and the atlas it wears. */
struct ZMeshConfig {
    std::uint8_t meshOrdinal;   // ordinal into section 31
    std::uint8_t imageOrdinal;  // ordinal into section 29
};

/** A sound the move fires when its animation reaches a frame. */
struct ZMoveSound {
    std::uint16_t frame;
    std::uint8_t soundId;
};

/** One animation: a frame range of one mesh, played at some speed. */
struct ZMeshMove {
    std::uint8_t meshConfigIndex;
    std::uint16_t firstFrame;
    std::uint16_t lastFrame;

    // Whether asking for this move again while it is already playing starts it
    // over. SetMove (:134082) otherwise waits for the animation to finish.
    std::uint8_t restartsWhenRepeated;

    float speed;

    // TODO: offset 12 of the move record -- nothing in the decompile reads it.
    std::uint32_t unknown;

    std::vector<ZMoveSound> sounds;
};

/**
 * The move set of one character template.
 */
class CMoveSetMesh {
public:
    CMoveSetMesh();

    bool Init(CArrayInputStream &stream);

    std::uint32_t GetPackHash() const { return m_packHash; }
    const std::vector<ZMeshConfig> &GetMeshConfigs() const { return m_meshConfigs; }
    const std::vector<ZMeshMove> &GetMoves() const { return m_moves; }

    /**
     * Whether any move covers this mesh frame.
     *
     * CMesh::Init asks this per frame and skips the ones nobody uses, which is
     * how a 554-frame mesh costs less than 554 frames of vertices. The window
     * starts one frame early, exactly as the original does (:123080).
     */
    bool IsFrameUsedInMoves(std::uint16_t frameIndex) const;

private:
    std::uint32_t m_packHash;
    std::vector<ZMeshConfig> m_meshConfigs;
    std::vector<ZMeshMove> m_moves;
};

#endif  // GUN_BROS_RE_GUN_BROS_CMOVESETMESH_H
