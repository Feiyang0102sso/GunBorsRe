#pragma once
/** Host diagnostics shared by survival, map preview and Arena. No simulation is advanced. */
class CMarkerBatch;
class CShaderProgram;
class CombatScene;
class CBrotherAI;
class WeaponEffects;
namespace MapDetail { struct LoadedMap; }

// pixelSize is one screen pixel expressed in the caller's world coordinates.
void DrawCollisionOverlay(CMarkerBatch &markers, const CShaderProgram &program,
    const float *projection, float pixelSize, const MapDetail::LoadedMap *map,
    const CombatScene *combat = nullptr, const CBrotherAI *brother = nullptr,
    const WeaponEffects *effects = nullptr);
