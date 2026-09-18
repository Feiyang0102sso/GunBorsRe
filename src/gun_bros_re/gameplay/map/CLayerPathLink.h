/** @file CLayerPathLink.h
 * @brief Original node graph retained for spawn placement and navigation.
 * Reference: CLayerPathLink::Init, iOS :166451. Coordinates are world pixels.
 */
#ifndef GUN_BROS_RE_CLAYERPATHLINK_H
#define GUN_BROS_RE_CLAYERPATHLINK_H
#include "gun_bros_re/gameplay/map/ILayerPath.h"

class CLayerPathLink : public ILayerPath {
public:
    struct Region {
        std::vector<std::uint8_t> nodes;
        CLayerCamera::Rectangle bounds;
    };
    bool Init(CArrayInputStream &stream);
    int GetSpawnLocation(float sourceX, float sourceY, const COffscreenSpawnLocationFilter &filter, ZRandom &random) const override;
private:
    std::vector<Region> m_regions;
};
#endif
