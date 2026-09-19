/** @file CMenuMeshEnemy.cpp
 * @brief Original menuMeshEnemy.cpp Bind/Update/Draw with Windows rendering.
 * Reference: iOS :169038, :169098, :169128; enemy.cpp UpdateUI :68332.
 */
#include "gun_bros_re/gameplay/enemy/CMenuMeshEnemy.h"
#include "gun_bros_re/data/store/CStoreItem.h"

bool CMenuMeshEnemy::Bind(CGunBros &tables, CResTOCManager &toc,
    const GameObjectRef &resource, const ZShaderProgram &program) {
    const std::string owner = tables.GetPackName(resource.packHash) + " enemy " + std::to_string(resource.localIndex);
    if (!m_template.Load(tables, resource.packHash, resource.localIndex, owner)) { return false; }
    if (!m_enemy.Bind(tables, m_template, true, &program)) { return false; }
    m_enemy.SpawnForUI();
    m_name = tables.ReadString(m_template.name);
    m_hasTick = false;
    return true;
}

void CMenuMeshEnemy::Update(std::uint64_t clock) {
    if (m_hasTick && clock >= m_lastTick) {
        m_enemy.UpdateUI(static_cast<int>(clock - m_lastTick));
    }
    m_lastTick = clock;
    m_hasTick = true;
}

bool CMenuMeshEnemy::Draw(const ZShaderProgram &program, float x, float y,
    float width, float height, float canvasWidth, float canvasHeight) {
    return m_enemy.DrawUI(program, x, y, width, height, canvasWidth, canvasHeight);
}
