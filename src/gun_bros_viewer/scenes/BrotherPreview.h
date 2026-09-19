class CMap;
#pragma once
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "engine/graphics/ZMeshBuffer.h"
#include "engine/graphics/ZTexture.h"


/** Catalogue-only assembly; no script, inventory or gameplay ownership. */
class ZBrotherPreview {
public:
    CBrother body;
    /**
     * Hang a weapon model off the torso's gun bone.
     *
     * The bone index is the one hardwired number in the whole assembly:
     * CBrother::Draw picks bone 4 off the torso mesh for a one-handed weapon, and
     * the torso's bone list names bone 4 "gun".
     */
    bool AttachGun(CGunBros &tables, const std::string &owner,
        std::uint32_t meshPackHash, std::uint32_t meshOrdinal,
        std::uint32_t imagePackHash, std::uint32_t imageOrdinal);
    bool CreateBuffers(const ZShaderProgram &program);
    void Update(std::int32_t deltaMs);
    void Draw(const ZShaderProgram &program, const float *base);
private:
    struct Gun {
        CMesh mesh;
        ZTexture texture;
        ZMeshBuffer buffer;
        // Empty moves for a gun model: its pose follows the torso's hand.
        bool attached = false;
        std::size_t boneIndex = 0;
    };
    std::unique_ptr<Gun> m_gun;
};

/**
 * Point every animated part at the same slot of its own move list.
 *
 * A viewer convention, not something the data says: the torso's moves and the
 * legs' moves are separate lists, and the game picks one of each independently
 * -- aim with the arms, walk with the feet. Stepping them together is just the
 * cheapest way to see a whole character move.
 *
 * @param report Print what each part landed on.
 */
void SelectPlayerMoveSlot(CBrother &model, std::size_t slot, bool report);


/** Advance bare body controllers without entering the player script. */
void AdvanceBrotherPreview(CBrother &model, std::int32_t deltaMs);

class ZWindow;
class CLevel;
namespace MapDetail {

/** Move every placed player's animation on. */
void AdvancePlayers(CMap &loaded, std::int32_t deltaMs);
/**
 * Run the animation clock forward, in the bites playback would use.
 *
 * What makes a still screenshot able to prove anything about animation: shoot
 * the same map at two different times and diff them. Deterministic, because
 * the bite size is fixed rather than taken from the wall clock.
 */
void WarmUp(CMap &loaded, std::uint32_t totalMs,
            CLevel *effects = nullptr, bool firing = false);
/**
 * Drive the first player with WASD and resolve the requested movement.
 *
 * Maps contain one real player spawn. A few abandoned campaign maps contain
 * none; those remain valid viewers and simply ignore movement input.
 */
bool UpdateControlledPlayer(CMap &loaded, const ZWindow &window,
                            std::uint64_t elapsedMs);
}
