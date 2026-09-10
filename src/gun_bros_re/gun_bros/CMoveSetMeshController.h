/**
 * @file CMoveSetMeshController.h
 * @brief Plays one move out of a move set: picks the mesh, drives the clock.
 *
 * Port of CMoveSetMeshController (src/gunbros/moveSetMesh.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:134034 (SetMoveSet),
 *            :134044 (Update), :134082 (SetMove)
 *
 * One layer above CMeshAnimationController, and it adds exactly three things:
 *
 * - a move names WHICH mesh of the set to show, so switching move can switch
 *   model and atlas underneath, not just the frame range,
 * - the move's speed scales the clock,
 * - asking for the move that is already playing is normally ignored, so a key
 *   held down does not restart the walk cycle every frame.
 *
 * **The meshes come from outside.** In the original each mesh config record
 * embeds its CMesh and its texture handle, because the resource loader fills
 * them in place. Ours are loaded by whoever owns the resources and handed over
 * as a parallel array -- same shape, without dragging loading in here.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CMOVESETMESHCONTROLLER_H
#define GUN_BROS_RE_GUN_BROS_CMOVESETMESHCONTROLLER_H

#include "gun_bros/CMeshAnimationController.h"
#include "gun_bros/CMoveSetMesh.h"
#include "gun_bros/CGameAssetRef.h"

#include <cstdint>
#include <vector>

// What GetMoveIndex reports when no move has been set.
constexpr std::int32_t kNoMoveIndex = -1;

class CMoveSetMeshController {
public:
    CMoveSetMeshController();

    /**
     * Bind a move set and the meshes its configs name.
     *
     * `meshes` is indexed by mesh config, so it must be as long as the set's
     * config list; an entry may be null for a config that failed to load.
     * Neither the set nor the meshes are owned, and both must outlive this.
     */
    void SetMoveSet(const CMoveSetMesh *moveSet,
                    const std::vector<const CMesh *> &meshes);

    /**
     * Start a move, by its index in the set.
     *
     * Asking for the move already playing does nothing unless that move is
     * marked as restarting, or it has already played out. Returns whether the
     * move actually (re)started.
     */
    bool SetMove(std::int32_t moveIndex);

    /** Move the clock on. The move's own speed scales `deltaMs`. */
    void Update(std::int32_t deltaMs);
    /** Collect authored WAV cues after an externally advanced UI animation. */
    void CollectSounds(std::int32_t previousMs);
    /** Direct WAV references emitted by crossed move frames (not sound templates). */
    std::vector<GameObjectRef> TakeSounds();

    std::int32_t GetMoveIndex() const { return m_moveIndex; }
    const CMoveSetMesh *GetMoveSet() const { return m_moveSet; }

    /** Which mesh config the current move shows, or -1 with no move set. */
    std::int32_t GetMeshConfigIndex() const;

    CMeshAnimationController &GetAnimation() { return m_animation; }
    const CMeshAnimationController &GetAnimation() const { return m_animation; }

private:
    const CMoveSetMesh *m_moveSet;
    std::vector<const CMesh *> m_meshes;
    std::int32_t m_moveIndex;
    CMeshAnimationController m_animation;
    std::vector<GameObjectRef> m_sounds;
};

#endif  // GUN_BROS_RE_GUN_BROS_CMOVESETMESHCONTROLLER_H
