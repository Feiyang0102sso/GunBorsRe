#pragma once
/** Host diagnostics shared by survival, map preview and Arena. No simulation is advanced. */
class ZMarkerBatch;
class ZShaderProgram;
class CLevel;
class CBrotherAI;
class CLevel;
class CMap;

// pixelSize is one screen pixel expressed in the caller's world coordinates.
void DrawCollisionOverlay(ZMarkerBatch &markers, const ZShaderProgram &program,
    const float *projection, float pixelSize, const CMap *map,
    const CLevel *combat = nullptr, const CBrotherAI *brother = nullptr,
    const CLevel *effects = nullptr);
