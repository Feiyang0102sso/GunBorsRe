/**
 * @file CMeshAnimationController.h
 * @brief A clock pointed at one mesh: which frames, how far in, playing or not.
 *
 * Port of CMeshAnimationController (src/gunbros/mesh.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:98708 (SetMesh), :98741 (SetRange),
 *            :98756 (Update), :98806 (SetFrame), :98831 (Render)
 *
 * The whole class is a time cursor. It holds no geometry of its own: Evaluate
 * asks the mesh what its vertices look like at the current moment, and the
 * caller does what it likes with them.
 *
 * A range is a pair of FRAME INDICES, but the cursor runs in MILLISECONDS --
 * the two are related only through the frames' own timestamps, which are not
 * evenly spaced. That is why Update reads `frames[first].timeMs` rather than
 * counting frames.
 *
 * One difference from the original, and it is the reason this class does not
 * simply own its output: the original's Render evaluates the pose straight
 * into a GL vertex buffer that the controller owns. Keeping GL out of
 * gun_bros/ is the same call M3.5 made when it split CMeshBuffer off CMesh, so
 * Evaluate fills a plain vector and CMeshBuffer takes it from there.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CMESHANIMATIONCONTROLLER_H
#define GUN_BROS_RE_GUN_BROS_CMESHANIMATIONCONTROLLER_H

#include "gun_bros/CMesh.h"

#include <cstdint>
#include <vector>

// What SetRange stores for "no range set". The original seeds the pair with
// -1 and Update skips the whole wrap when either end still holds it.
constexpr std::int32_t kNoFrameIndex = -1;

class CMeshAnimationController {
public:
    CMeshAnimationController();

    /** Point the cursor at a mesh. Resets the range and the clock. */
    void SetMesh(const CMesh *mesh);
    const CMesh *GetMesh() const { return m_mesh; }

    /** The frame indices the cursor runs between, both ends inclusive. */
    void SetRange(std::int32_t firstFrame, std::int32_t lastFrame);

    /** Put the cursor on one frame, by its timestamp. */
    void SetFrame(std::int32_t frameIndex);

    void SetTimeMs(std::int32_t timeMs);
    std::int32_t GetTimeMs() const { return m_timeMs; }

    void SetLooped(bool looped) { m_looped = looped; }
    bool GetLooped() const { return m_looped; }

    /** Move the cursor on. `deltaMs` is already scaled by the move's speed. */
    void Update(std::int32_t deltaMs);

    /**
     * Whether the last Update ran the cursor off the end of its range.
     *
     * True for the one update that crossed the end, looped or not -- it is how
     * a caller notices a one-shot move has played out.
     */
    bool IsFinished() const { return m_finished; }

    /** The mesh's vertices at the current moment. See CMesh::GetVerticesAt. */
    bool Evaluate(std::vector<float> &outVertices) const;

    /** Where one bone sits at the current moment. See CMesh::GetNodeAt. */
    bool GetNodeAt(std::size_t boneIndex, MeshBoneTransform &out) const;

    /** Where the range starts, and how long it runs. Zero with no range set. */
    std::int32_t GetRangeStartMs() const { return m_rangeStartMs; }
    std::int32_t GetRangeDurationMs() const { return m_rangeDurationMs; }

private:
    const CMesh *m_mesh;
    std::int32_t m_timeMs;

    std::int32_t m_firstFrame;
    std::int32_t m_lastFrame;
    std::int32_t m_rangeStartMs;
    std::int32_t m_rangeDurationMs;

    bool m_looped;
    bool m_finished;
};

#endif  // GUN_BROS_RE_GUN_BROS_CMESHANIMATIONCONTROLLER_H
