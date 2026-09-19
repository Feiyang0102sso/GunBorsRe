/** @file CMenuMeshEnemy.h
 * @brief Enemy preview ownership and UI-only animation lifecycle.
 * Original: src/gunbros/menuMeshEnemy.cpp, Bind :169128, Update :169098.
 * Windows adapter: BIG lookup, absolute host clock and shader/projection arguments.
 */
#pragma once
#include "gun_bros_re/gameplay/enemy/CEnemy.h"

class CMenuMeshEnemy {
public:
    bool Bind(CGunBros &tables, CResTOCManager &toc, const GameObjectRef &resource,
        const ZShaderProgram &program);
    void Update(std::uint64_t clock);
    bool Draw(const ZShaderProgram &program, float x, float y, float width,
        float height, float canvasWidth, float canvasHeight);
    const std::string &GetName() const { return m_name; }
private:
    CEnemy::Template m_template;
    CEnemy m_enemy;
    std::string m_name;
    std::uint64_t m_lastTick = 0;
    bool m_hasTick = false;
};
