/** @file SurvivalPilot.h
 * @brief Test-only driver using the same collision sweeps as the player.
 * This is not original enemy AI; it supplies ordinary movement/fire inputs.
 */
#ifndef GUN_BROS_RE_SURVIVALPILOT_H
#define GUN_BROS_RE_SURVIVALPILOT_H
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/map/CLayerCamera.h"

class SurvivalPilot {
public:
    SurvivalPilot(CLevel &scene, const CLayerCamera::Rectangle &bounds);
    void Update(int deltaMs, float &moveX, float &moveY);
    void Report() const;
private:
    struct Node { float x = 0; float y = 0; std::vector<int> neighbors; };
    void Plan(const CEnemy &target);
    CLevel &m_scene;
    std::vector<Node> m_nodes;
    std::vector<int> m_route;
    unsigned m_step = 0;
    int m_planTimer = 0;
    int m_elapsed = 0;
    ZCombatId m_target = 0;
    float m_lastDamage = 0;
    int m_noDamageMs = 0;
    int m_retreatMs = 0;
    unsigned m_missingRoutes = 0;
};
#endif
