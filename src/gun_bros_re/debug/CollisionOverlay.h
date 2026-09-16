#pragma once
/** Host diagnostics shared by survival, map preview and Arena. No simulation is advanced. */
class ZMarkerBatch;
class ZShaderProgram;
class ZCombatWorld;
class CBrotherAI;
class ZWeaponEffects;
namespace MapDetail { struct ZLoadedMap; }

// pixelSize is one screen pixel expressed in the caller's world coordinates.
void DrawCollisionOverlay(ZMarkerBatch &markers, const ZShaderProgram &program,
    const float *projection, float pixelSize, const MapDetail::ZLoadedMap *map,
    const ZCombatWorld *combat = nullptr, const CBrotherAI *brother = nullptr,
    const ZWeaponEffects *effects = nullptr);
