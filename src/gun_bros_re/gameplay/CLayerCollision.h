/**
 * @file CLayerCollision.h
 * @brief A map layer that owns collision geometry.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CLAYERCOLLISION_H
#define GUN_BROS_RE_GUN_BROS_CLAYERCOLLISION_H

#include "gun_bros_re/gameplay/CCollisionData.h"

/** Port of CLayerCollision (src/gunbros/layerCollision.cpp). */
class CLayerCollision {
public:
    CLayerCollision() : m_layerIndex(0) {}

    bool Init(CArrayInputStream &stream) { return m_collision.Load(stream); }

    const CCollisionData &GetCollision() const { return m_collision; }
    CCollisionData &GetCollision() { return m_collision; }

    /** Index in the map's complete layer stack, not the collision-only list. */
    void SetLayerIndex(std::uint32_t index) { m_layerIndex = index; }
    std::uint32_t GetLayerIndex() const { return m_layerIndex; }

private:
    CCollisionData m_collision;
    std::uint32_t m_layerIndex;
};

#endif  // GUN_BROS_RE_GUN_BROS_CLAYERCOLLISION_H
