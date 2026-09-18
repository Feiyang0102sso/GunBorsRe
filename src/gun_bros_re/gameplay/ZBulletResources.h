#pragma once
/** BIG template and Windows mesh cache; it does not own live bullets. */
#include "gun_bros_re/gameplay/CBullet.h"
#include "gun_bros_re/data/ZMeshAssets.h"
#include "engine/graphics/ZMeshBuffer.h"
#include <map>
struct ZBulletVisual {
    struct Mesh {
        CMesh mesh;
        ZTexture texture;
        ZMeshBuffer buffer;
    };
    CBullet::Template data;
    std::unique_ptr<Mesh> mesh;
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
