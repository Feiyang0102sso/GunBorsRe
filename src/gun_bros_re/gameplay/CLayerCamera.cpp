/**
 * @file CLayerCamera.cpp
 * @brief The rectangles a map's camera is allowed to show.
 */

#include "gun_bros_re/gameplay/CLayerCamera.h"

namespace {

/** Read one rectangle: x, y, width, height, all int16. */
MapRectangle ReadRectangle(CArrayInputStream &stream) {
    MapRectangle rectangle;
    rectangle.x = stream.ReadInt16();
    rectangle.y = stream.ReadInt16();
    rectangle.width = stream.ReadInt16();
    rectangle.height = stream.ReadInt16();
    return rectangle;
}

}  // namespace

CLayerCamera::CLayerCamera() : m_layerIndex(0) {}

bool CLayerCamera::Init(CArrayInputStream &stream) {
    m_primaryBounds = ReadRectangle(stream);
    m_secondaryBounds = ReadRectangle(stream);
    return !stream.Overran();
}
