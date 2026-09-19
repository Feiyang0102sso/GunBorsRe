#include "gun_bros_re/gameplay/weapon/CGunDrawing.h"
#include "gun_bros_re/gameplay/weapon/CBullet.h"

/** BIG resources belong to the gun; only CBrother binds the PLAYER script. */
bool CGun::Load(CGunBros &tables, const Template &data, const std::string &owner) {
    m_drawing = std::make_unique<CGun::Drawing>();
    CResourceLoader &loader = tables.GetResourceLoader();
    const CMoveSetMesh &moves = data.GetMoveSet();
    std::vector<std::shared_ptr<ZTexture>> images;
    data.Load(loader, m_drawing->gunPart.texture, &images);
    if (!loader.LoadImmediate()) { return false; }
    for (unsigned index = 0; index < moves.GetMeshConfigs().size(); ++index) {
        auto part = std::make_unique<Drawing::Mesh>();
        part->mesh = moves.GetMesh(index);
        part->texture = images[index];
        m_drawing->configs.push_back(std::move(part));
    }
    m_drawing->gunPart.mesh = data.GetMesh();
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
    Bind(data, m_drawing->gunPart.mesh.get(), beam);
    return true;
}


bool CGun::CreateBuffers(const ZShaderProgram &program) {
    for (auto &config : m_drawing->configs) {
        if (!config->buffer.Create(program) || !config->buffer.SetMesh(*config->mesh)) { return false; }
    }
    auto &part = m_drawing->gunPart;
    return part.buffer.Create(program) && part.buffer.SetMesh(*part.mesh);
}

std::vector<const CMesh *> CGun::GetBodyMeshes() const {
    std::vector<const CMesh *> meshes;
    for (const auto &part : m_drawing->configs) { meshes.push_back(part->mesh.get()); }
    return meshes;
}
