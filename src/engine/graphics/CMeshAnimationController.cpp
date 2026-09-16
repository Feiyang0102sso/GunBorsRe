/**
 * @file CMeshAnimationController.cpp
 * @brief A clock pointed at one mesh: which frames, how far in, playing or not.
 */

#include "engine/graphics/CMeshAnimationController.h"

CMeshAnimationController::CMeshAnimationController()
    : m_mesh(nullptr),
      m_timeMs(0),
      m_firstFrame(kNoFrameIndex),
      m_lastFrame(kNoFrameIndex),
      m_rangeStartMs(0),
      m_rangeDurationMs(0),
      m_looped(true),
      m_finished(false) {}

void CMeshAnimationController::SetMesh(const CMesh *mesh) {
    m_mesh = mesh;
    m_timeMs = 0;
    m_firstFrame = kNoFrameIndex;
    m_lastFrame = kNoFrameIndex;
    m_rangeStartMs = 0;
    m_rangeDurationMs = 0;
    m_finished = false;
}

void CMeshAnimationController::SetRange(std::int32_t firstFrame,
                                        std::int32_t lastFrame) {
    m_firstFrame = firstFrame;
    m_lastFrame = lastFrame;
    m_rangeStartMs = 0;
    m_rangeDurationMs = 0;

    if (m_mesh == nullptr) {
        return;
    }

    const std::vector<ZMeshFrame> &frames = m_mesh->GetFrames();
    if (firstFrame < 0 || lastFrame < 0 ||
        static_cast<std::size_t>(firstFrame) >= frames.size() ||
        static_cast<std::size_t>(lastFrame) >= frames.size()) {
        m_firstFrame = kNoFrameIndex;
        m_lastFrame = kNoFrameIndex;
        return;
    }

    m_rangeStartMs = frames[firstFrame].timeMs;
    m_rangeDurationMs = frames[lastFrame].timeMs - m_rangeStartMs;
}

void CMeshAnimationController::SetFrame(std::int32_t frameIndex) {
    if (m_mesh == nullptr || frameIndex < 0) {
        return;
    }

    const std::vector<ZMeshFrame> &frames = m_mesh->GetFrames();
    if (static_cast<std::size_t>(frameIndex) >= frames.size()) {
        return;
    }

    m_timeMs = frames[frameIndex].timeMs;
    m_finished = false;
}

void CMeshAnimationController::SetTimeMs(std::int32_t timeMs) {
    m_timeMs = timeMs;
    m_finished = false;
}

void CMeshAnimationController::Update(std::int32_t deltaMs) {
    m_finished = false;
    if (m_mesh == nullptr) {
        return;
    }

    m_timeMs += deltaMs;

    // With no range set the cursor just keeps growing; CMesh::GetVerticesAt
    // wraps it against the whole mesh on its own.
    if (m_firstFrame == kNoFrameIndex || m_lastFrame == kNoFrameIndex) {
        return;
    }

    const std::int32_t startMs = m_rangeStartMs;
    const std::int32_t endMs = m_rangeStartMs + m_rangeDurationMs;

    // A range both ends of which land on the same timestamp is a still pose.
    if (m_rangeDurationMs == 0) {
        m_timeMs = startMs;
        return;
    }

    if (m_timeMs <= endMs) {
        return;
    }

    if (m_looped) {
        // The original wraps the OVERSHOOT, not the elapsed time: how far past
        // the end the cursor landed, measured from the start again.
        m_timeMs = (m_timeMs - endMs) % m_rangeDurationMs + startMs;
    } else {
        m_timeMs = endMs;
    }
    m_finished = true;
}

bool CMeshAnimationController::Evaluate(std::vector<float> &outVertices) const {
    if (m_mesh == nullptr) {
        return false;
    }
    return m_mesh->GetVerticesAt(m_timeMs, outVertices);
}

bool CMeshAnimationController::GetNodeAt(std::size_t boneIndex,
                                         ZMeshBoneTransform &out) const {
    if (m_mesh == nullptr) {
        return false;
    }
    return m_mesh->GetNodeAt(m_timeMs, boneIndex, out);
}
