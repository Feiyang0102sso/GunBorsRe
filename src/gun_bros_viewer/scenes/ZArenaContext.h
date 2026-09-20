#pragma once
/** Viewer-only binding of an empty arena to original LEVEL resource tables. */
#include "gun_bros_re/gameplay/level/CLevel.h"

class ZArenaContext {
public:
    bool Load(CResTOCManager &toc, CGunBros &tables);
    /** Bind a fresh combat runtime; scene must not retain an earlier LEVEL. */
    bool Bind(CGunBros &tables, CLevel &scene, const CEnemy::Template &enemy);
    CEnemy *SpawnNearby(CLevel &scene, std::size_t index, const CEnemy::Template &enemy) const;
    const std::string &GetLabel() const { return m_label; }

private:
    struct Entry {
        GameObjectRef reference;
        CLevel::Template data;
    };
    std::vector<Entry> m_levels;
    CMap m_map;
    CCollisionData m_emptyCollision;
    CCollisionData::Scene m_emptyWeaponCollision;
    std::string m_label;
};
