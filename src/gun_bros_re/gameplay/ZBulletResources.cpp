#include "gun_bros_re/gameplay/ZBulletResources.h"
namespace {
std::uint64_t ResourceKey(const GameObjectRef &ref) {
    return (static_cast<std::uint64_t>(ref.packHash) << 32) | ref.localIndex;
}

}
ZBulletVisual *ZBulletResources::Get(const GameObjectRef &ref) {
    const std::uint64_t key = ResourceKey(ref);
    auto found = m_bullets.find(key);
    if (found != m_bullets.end()) { return found->second.get(); }
    std::vector<std::uint8_t> payload;
    if (!m_tables.ReadSectionResource(ref.packHash, ZGameSection::Bullet, ref.localIndex, payload)) { return nullptr; }
    std::unique_ptr<ZBulletVisual> visual(new ZBulletVisual());
    CArrayInputStream stream(payload);
    if (!visual->data.Init(stream)) { return nullptr; }
    if (visual->data.HasMesh() && visual->data.HasImage()) {
        visual->mesh.reset(new ZBulletVisual::Mesh());
        const CGameAssetRef &mesh = visual->data.GetMeshRef();
        const CGameAssetRef &atlas = visual->data.GetImageRef();
        if (!LoadMeshAndAtlas(m_tables, "projectile", mesh.packHash, mesh.assetId, atlas.packHash,
            atlas.assetId, visual->mesh->mesh, visual->mesh->texture)) { return nullptr; }
        if (!visual->mesh->buffer.Create(m_program) || !visual->mesh->buffer.SetMesh(visual->mesh->mesh)) { return nullptr; }
    }
    ZBulletVisual *result = visual.get();
    m_bullets[key] = std::move(visual);
    return result;
}
