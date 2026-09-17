#pragma once
/** BIG template and Windows mesh cache; it does not own live bullets. */
#include "gun_bros_re/gameplay/CBullet.h"
#include "gun_bros_re/gameplay/brother/ZPlayerModel.h"
#include <map>
struct ZBulletVisual {
    CBullet::Template data;
    std::unique_ptr<ZPlayerPart> mesh;
};

class ZBulletResources {
public:
    ZBulletResources(ZPackTables &tables, const ZShaderProgram &program) : m_tables(tables), m_program(program) {}
    ZBulletVisual *Get(const GameObjectRef &resource);
private:
    ZPackTables &m_tables;
    const ZShaderProgram &m_program;
    std::map<std::uint64_t, std::unique_ptr<ZBulletVisual>> m_bullets;
};
