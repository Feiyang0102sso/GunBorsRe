/** BIG template and Windows mesh cache; it does not own live bullets. */
/** CBullet::Template::Load :130637 and LoadMesh :60448.
 * The BIG references remain the only source of sprite/model identity.
 */
#include "gun_bros_re/gameplay/weapon/CBullet.h"
#include "gun_bros_re/graphics/ZMeshAssets.h"
#include <cstdio>

bool CBullet::Template::Load(CGunBros &tables, const ZShaderProgram &program, const GameObjectRef &ref) {
    std::vector<std::uint8_t> payload;
    if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Bullet, ref.localIndex, payload)) {
        std::printf("[bullet] missing template %08x:%u\n", ref.packHash, ref.localIndex);
        return false;
    }
    CArrayInputStream stream(payload);
    if (!Init(stream)) {
        std::printf("[bullet] invalid template %08x:%u\n", ref.packHash, ref.localIndex);
        return false;
    }
    if (!HasMesh() || !HasImage()) { return true; }
    auto mesh = std::make_shared<Mesh>();
    if (!LoadMeshAndAtlas(tables, "projectile", m_meshRef.packHash, m_meshRef.assetId,
        m_imageRef.packHash, m_imageRef.assetId, mesh->mesh, mesh->texture)) { return false; }
    if (!mesh->buffer.Create(program) || !mesh->buffer.SetMesh(mesh->mesh)) { return false; }
    m_mesh = std::move(mesh);
    return true;
}
