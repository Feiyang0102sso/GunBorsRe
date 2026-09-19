#include "gun_bros_re/gameplay/weapon/CGunDrawing.h"
#include "gun_bros_re/data/ZMeshAssets.h"
#include "gun_bros_re/gameplay/weapon/CBullet.h"

/** BIG resources belong to the gun; only CBrother binds the PLAYER script. */
bool CGun::Load(ZPackTables &tables, const Template &data, const std::string &owner) {
    m_drawing = std::make_unique<CGun::Drawing>();
    const CMoveSetMesh &moves = data.GetMoveSet();
    for (const ZMeshConfig &config : moves.GetMeshConfigs()) {
        std::unique_ptr<Drawing::Mesh> part(new Drawing::Mesh());
        if (!LoadMeshAndAtlas(tables, "weapon torso", moves.GetPackHash(), config.meshOrdinal,
            moves.GetPackHash(), config.imageOrdinal, part->mesh, part->texture, &moves)) { return false; }
        m_drawing->configs.push_back(std::move(part));
    }
    const CGameAssetRef &mesh = data.GetMeshRef();
    const CGameAssetRef &atlas = data.GetImageRef();
    if (!LoadMeshAndAtlas(tables, owner.c_str(), mesh.packHash, mesh.assetId,
        atlas.packHash, atlas.assetId, m_drawing->gunPart.mesh, m_drawing->gunPart.texture)) { return false; }
    // CBrother::UpdateNormal treats continuous beams specially when the gun
    // script clears its ready flag. Resolve that property from the real bullet.
    bool beam = false;
    const GameObjectRef &bulletRef = data.GetBulletRef();
    if (bulletRef.localIndex != 255) {
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(bulletRef.packHash, ZGameSection::Bullet, bulletRef.localIndex, payload)) { return false; }
        CArrayInputStream stream(payload);
        CBullet::Template bullet;
        if (!bullet.Init(stream)) { return false; }
        beam = (bullet.GetFlags() & 0x100) != 0;
    }
    Bind(data, &m_drawing->gunPart.mesh, beam);
    return true;
}


bool CGun::CreateBuffers(const ZShaderProgram &program) {
    for (auto &config : m_drawing->configs) {
        if (!config->buffer.Create(program) || !config->buffer.SetMesh(config->mesh)) { return false; }
    }
    auto &part = m_drawing->gunPart;
    return part.buffer.Create(program) && part.buffer.SetMesh(part.mesh);
}

std::vector<const CMesh *> CGun::GetBodyMeshes() const {
    std::vector<const CMesh *> meshes;
    for (const auto &part : m_drawing->configs) { meshes.push_back(&part->mesh); }
    return meshes;
}
