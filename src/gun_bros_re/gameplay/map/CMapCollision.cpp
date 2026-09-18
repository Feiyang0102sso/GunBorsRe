#include "gun_bros_re/gameplay/map/CMapResources.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace MapDetail;

/**
 * Assemble exactly the collision shapes CLayerCollision tests for a player.
 *
 * The level script selects one map collision layer. Static props then add
 * their template-local geometry at their placed position. Keeping this as one
 * scene lets the existing resolver choose the nearest edge across both kinds
 * instead of resolving each prop in an arbitrary order.
 */
void CMap::BuildCollisionScene() {
    CMap &loaded = *this;
    loaded.GetResources().collisionScene.Clear();
    loaded.GetResources().weaponCollision.walls.Clear();
    loaded.GetResources().weaponCollision.terrain.Clear();
    const CLayerCollision *bulletLayer = loaded.GetCurrentBulletCollisionLayer();
    if (bulletLayer != nullptr) {
        loaded.GetResources().weaponCollision.walls.AppendTranslated(bulletLayer->GetCollision(), 0, 0);
    }

    const CLayerCollision *mapLayer = loaded.GetCurrentCollisionLayer();
    if (mapLayer != nullptr) {
        loaded.GetResources().collisionScene.AppendTranslated(mapLayer->GetCollision(), 0.0f,
                                               0.0f);
        loaded.GetResources().weaponCollision.terrain.AppendTranslated(mapLayer->GetCollision(), 0, 0);
    }

    std::uint32_t propShapes = 0;
    for (std::size_t i = 0; i < loaded.GetResources().props.size(); ++i) {
        const CProp &prop = loaded.GetResources().props[i];
        if (!PropHasCollision(prop)) {
            continue;
        }
        const CCollisionData *body = &prop.GetCollision();
        const CCollisionData *bullets = &prop.GetCollision(true);
        loaded.GetResources().weaponCollision.walls.AppendTranslated(*bullets, prop.x, prop.y);
        loaded.GetResources().weaponCollision.terrain.AppendTranslated(*bullets, prop.x, prop.y);
        if (body->GetEdges().empty()) {
            continue;
        }
        if (!loaded.GetResources().collisionScene.AppendTranslated(*body,
                                                    prop.x, prop.y)) {
            break;
        }
        propShapes++;
    }

    int mapLayerIndex = -1;
    if (mapLayer != nullptr) {
        mapLayerIndex = static_cast<int>(mapLayer->GetLayerIndex());
    }
    std::printf("[m4] effective collision: map layer %d, %u prop shapes, "
                "%zu vertices, %zu edges\n",
                mapLayerIndex, propShapes,
                loaded.GetResources().collisionScene.GetVertices().size(),
                loaded.GetResources().collisionScene.GetEdges().size());
}

/** The original disables both collision shapes in the destroyed state. */
bool MapDetail::PropHasCollision(const CProp &prop) {
    return prop.active && !prop.IsRemoved();
}
