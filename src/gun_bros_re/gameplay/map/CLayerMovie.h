#pragma once
/** Original layerMovie.cpp :126049; maps/map.bt layer type 3.
 * The BIG samples contain no Movie layer. Keep its reference and signed
 * position intact; do not silently discard an unverified rendering branch.
 */
#include "gun_bros_re/data/objects/CGameAssetRef.h"
class CLayerMovie {
public:
    bool Init(CArrayInputStream &stream);
    const CGameAssetRef &GetMovieRef() const { return m_movie; }
    std::int16_t GetX() const { return m_x; }
    std::int16_t GetY() const { return m_y; }
    unsigned GetLayerIndex() const { return m_layerIndex; }
    void SetLayerIndex(unsigned index) { m_layerIndex = index; }
private:
    CGameAssetRef m_movie;
    std::int16_t m_x = 0, m_y = 0;
    unsigned m_layerIndex = 0;
};
