/**
 * @file CMoveSetMeshController.cpp
 * @brief Plays one move out of a move set: picks the mesh, drives the clock.
 */

#include "gun_bros/CMoveSetMeshController.h"

CMoveSetMeshController::CMoveSetMeshController()
    : m_moveSet(nullptr), m_moveIndex(kNoMoveIndex) {}

void CMoveSetMeshController::SetMoveSet(const CMoveSetMesh *moveSet,
                                        const std::vector<const CMesh *> &meshes) {
    m_moveSet = moveSet;
    m_meshes = meshes;
    m_moveIndex = kNoMoveIndex;
    m_animation.SetMesh(nullptr);
}

std::int32_t CMoveSetMeshController::GetMeshConfigIndex() const {
    if (m_moveSet == nullptr || m_moveIndex == kNoMoveIndex) {
        return -1;
    }
    return m_moveSet->GetMoves()[m_moveIndex].meshConfigIndex;
}

bool CMoveSetMeshController::SetMove(std::int32_t moveIndex) {
    if (m_moveSet == nullptr || moveIndex < 0 ||
        static_cast<std::size_t>(moveIndex) >= m_moveSet->GetMoves().size()) {
        return false;
    }

    const MeshMove &move = m_moveSet->GetMoves()[moveIndex];

    // Asking again for the move already playing is normally a no-op, which is
    // what keeps a held key from restarting the walk cycle every frame. Two
    // things override that: a move flagged as restarting, and a move that has
    // already run to the end of its range.
    if (moveIndex == m_moveIndex && move.restartsWhenRepeated == 0) {
        // The range's own end, which SetRange already read off the mesh and
        // vetted -- the same timestamp the original re-indexes for here.
        const std::int32_t endMs =
            m_animation.GetRangeStartMs() + m_animation.GetRangeDurationMs();
        if (m_animation.GetTimeMs() < endMs) {
            return false;
        }
    }

    const std::uint8_t configIndex = move.meshConfigIndex;
    if (configIndex >= m_meshes.size() || m_meshes[configIndex] == nullptr) {
        return false;
    }

    m_moveIndex = moveIndex;
    m_animation.SetMesh(m_meshes[configIndex]);
    m_animation.SetRange(move.firstFrame, move.lastFrame);
    m_animation.SetFrame(move.firstFrame);
    return true;
}

void CMoveSetMeshController::Update(std::int32_t deltaMs) {
    m_sounds.clear();
    if (m_moveSet == nullptr || m_moveIndex == kNoMoveIndex) {
        return;
    }

    const MeshMove &move = m_moveSet->GetMoves()[m_moveIndex];

    std::int32_t step = deltaMs;
    if (move.speed != 1.0f) {
        // Rounded to the nearest millisecond, away from zero -- the original
        // adds or subtracts a half before truncating.
        const float scaled = move.speed * static_cast<float>(deltaMs);
        float rounded = 0.0f;
        if (scaled >= 0.0f) {
            rounded = scaled + 0.5f;
        } else {
            rounded = scaled - 0.5f;
        }
        step = static_cast<std::int32_t>(rounded);

        // The floor is one millisecond, not zero: a move with a speed small
        // enough to round away still crawls forward rather than freezing.
        if (step <= 0) {
            step = 1;
        }
    }

    const std::int32_t previousMs = m_animation.GetTimeMs();
    m_animation.Update(step);

    // TODO: the original also asks CMoveSetMesh::GetSound whether a sound
    // frame falls in the window this update just crossed, and queues it.
    // Audio is not in the project yet; the move's sound list is parsed.
    // Implemented below: CMoveSetMesh::GetSound (:123129) returns the first
    // cue in [previous, current), and the owning viewer plays its direct WAV.
    const CMesh *mesh = m_animation.GetMesh();
    if (mesh == nullptr) { return; }
    for (const MoveSound &sound : move.sounds) {
        if (sound.frame >= mesh->GetFrames().size() || sound.soundId == 255) { continue; }
        const std::int32_t time = mesh->GetFrames()[sound.frame].timeMs;
        if (time >= previousMs && time < m_animation.GetTimeMs()) {
            GameObjectRef cue;
            cue.packHash = m_moveSet->GetPackHash();
            cue.localIndex = sound.soundId;
            m_sounds.push_back(cue);
            break;
        }
    }
}

std::vector<GameObjectRef> CMoveSetMeshController::TakeSounds() {
    std::vector<GameObjectRef> sounds;
    sounds.swap(m_sounds);
    return sounds;
}
