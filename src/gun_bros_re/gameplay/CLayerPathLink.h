/** @file CLayerPathLink.h
 * @brief Original node graph retained for spawn placement and navigation.
 * Reference: CLayerPathLink::Init, iOS :166451. Coordinates are world pixels.
 */
#ifndef GUN_BROS_RE_CLAYERPATHLINK_H
#define GUN_BROS_RE_CLAYERPATHLINK_H
#include "gun_bros_re/gameplay/ILayerPath.h"

class CLayerPathLink : public ILayerPath {
public:
    struct Region {
        std::vector<std::uint8_t> nodes;
        MapRectangle bounds;
    };
    bool Init(CArrayInputStream &stream);
private:
    std::vector<Region> m_regions;
};
#endif