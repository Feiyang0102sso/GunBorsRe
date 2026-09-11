#pragma once
/** Viewer controls for the authored Haven turret and its separate indicator. */
#include <vector>
class CEnemy;
namespace MapDetail { struct LoadedMap; struct PlacedProp; }

class MapTurretPreview {
public:
    void Bind(MapDetail::LoadedMap &map);
    void Cycle();
    void Update(int deltaMs);
    const char *StateName() const;
    bool Empty() const { return m_enemies.empty(); }

private:
    void ApplyState();
    unsigned m_state = 0;
    std::vector<CEnemy *> m_enemies;
    std::vector<MapDetail::PlacedProp *> m_indicators;
};
