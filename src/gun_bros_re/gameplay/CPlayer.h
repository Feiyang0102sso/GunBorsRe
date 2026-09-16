#pragma once
#include "gun_bros_re/gameplay/ZPlayerModel.h"
#include "gun_bros_re/gameplay/CTargetingController.h"
#include "gun_bros_re/data/CPlayerProgress.h"

class CCollisionData;
class CLevelObjectPool;
class CMap;

/** Player state and native input/progression responsibilities from player.cpp.
 * Equipment/render storage remains in the desktop model; the level owns peers,
 * collisions and match policy. This is not a claim of a complete CPlayer port.
 */
class CPlayer {
public:
    CPlayer() = default;
    CPlayer(ZPlayerModel &model, ZPlayerVitals &vitals);
    void BindActor(ZPlayerModel &model, ZPlayerVitals &vitals);
    void BindProgress(CPlayerProgress *progress);
    CPlayerProgress *GetProgress() const { return m_progress; }
    bool AddExperience(unsigned amount, bool updateHealth);
    std::uint64_t GetExperience() const;
    std::uint64_t AddXplodium(unsigned amount, unsigned percent);
    std::uint64_t GetXplodium() const { return m_xplodium; }
    void ResetXplodiumRemainder() { m_xplodiumRemainder = 0; }
    void AddHealth(unsigned amount);
    /** Bind the map and original level-object collection used by Move. */
    void BindLevel(CMap &map, const CCollisionData &collision,
        CLevelObjectPool &objects, float collisionRadius);
    void BeginMovement();
    // Original CPlayer::Move resolves bounds, enemy bodies, then map edges.
    void Move();
    bool UpdateMovement(int deltaMs, float moveX, float moveY);
    void UpdateShooting(int deltaMs, bool moving, bool shoot, ZBrotherAIWorld &world);
    /** Original per-frame player order: input, weapon, force, then collision. */
    void Update(int deltaMs, float moveX, float moveY, bool shoot,
        ZBrotherAIWorld &world, bool moveActor);
    void ApplyKnockback(int deltaMs, float seconds);
    CTargetingController &GetTargetingController() { return m_autoAim; }
    const CTargetingController &GetTargetingController() const { return m_autoAim; }

    float x = 600, y = 650, facing = 0;
    float previousX = 600, previousY = 650;
    float forceX = 0, forceY = 0;
    int forceMs = 0;
private:
    ZPlayerModel *m_model = nullptr;
    ZPlayerVitals *m_vitals = nullptr;
    CPlayerProgress *m_progress = nullptr;
    CTargetingController m_autoAim;
    std::uint64_t m_xplodium = 0;
    unsigned m_xplodiumRemainder = 0;
    CMap *m_map = nullptr;
    const CCollisionData *m_collision = nullptr;
    CLevelObjectPool *m_objects = nullptr;
    float m_collisionRadius = 0;
};
