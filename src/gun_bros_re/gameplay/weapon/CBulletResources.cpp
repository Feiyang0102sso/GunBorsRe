/** BIG template and Windows mesh cache; it does not own live bullets. */
/** CBullet::Template::Load :130637 and LoadMesh :60448.
 * The BIG references remain the only source of sprite/model identity.
 */
#include "gun_bros_re/gameplay/weapon/CBullet.h"
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
    m_mesh = std::make_shared<Mesh>();
    CResourceLoader &loader = tables.GetResourceLoader();
    loader.AddFunction([this, &loader]() { return LoadMesh(loader); });
    loader.AddImage(m_imageRef.packHash, m_imageRef.assetId, m_mesh->texture);
    if (!loader.LoadImmediate() || !m_mesh->buffer.Create(program) || !m_mesh->buffer.SetMesh(m_mesh->mesh)) {
        m_mesh.reset();
        return false;
    }
    return true;
}

/** Original CBullet::Template::LoadMesh :60448, distinct from atlas loading. */
bool CBullet::Template::LoadMesh(CResourceLoader &loader) {
    if (m_mesh->mesh.GetVertexCount() != 0) { return true; }
    std::vector<std::uint8_t> bytes;
    if (!loader.ReadMesh(m_meshRef.packHash, m_meshRef.assetId, bytes)) { return false; }
    CArrayInputStream input(bytes);
    CMesh candidate;
    if (!candidate.Init(input) || input.Available() != 0) { return false; }
    m_mesh->mesh = std::move(candidate);
    return true;
}
