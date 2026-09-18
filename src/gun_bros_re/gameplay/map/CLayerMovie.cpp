/**
 * CLayerMovie: a CGameAssetRef and a position.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:126049
 */
#include "gun_bros_re/gameplay/map/CLayerMovie.h"
bool CLayerMovie::Init(CArrayInputStream &stream) {
    m_movie.Init(stream); // pack hash and asset id
    m_x = stream.ReadInt16(); // x
    m_y = stream.ReadInt16(); // y
    return !stream.Overran();
}
