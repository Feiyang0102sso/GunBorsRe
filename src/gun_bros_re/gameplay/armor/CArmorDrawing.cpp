#include "gun_bros_re/gameplay/armor/CArmorDrawing.h"
#include "gun_bros_re/graphics/ZMeshAssets.h"
#include "engine/graphics/ZPNG.h"
#include "engine/platform/ZGLLoader.h"

bool CArmor::Load(CGunBros &tables, const Template &data, const ZShaderProgram &program) {
    if (data.GetSlot() >= kArmorSlotCount) {
        return false;
    }
    m_drawing = std::make_unique<CArmor::Drawing>();
    Bind(data);
    Equip();
    for (std::uint32_t index = 0; index < kArmorVariantCount; ++index) {
        const CGameAssetRef &image = data.GetLoadedImageRef(index);
        if (image.assetId >= 0 && !image.IsNull()) {
            std::vector<std::uint8_t> payload;
            ZPNGImage decoded;
            if (!tables.ReadSectionResource(image.packHash, ZGameSection::Png, image.assetId, payload) ||
                !PNGDecode(payload, decoded) || !m_drawing->images[index].Create(decoded, GL_REPEAT)) {
                return false;
            }
        }
        if (!data.HasMesh(index)) {
            continue;
        }
        std::unique_ptr<Drawing::Attachment> part(new Drawing::Attachment());
        const CGameAssetRef &mesh = data.GetMeshRef(index);
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(mesh.packHash, ZGameSection::Mesh, mesh.assetId, payload)) {
            return false;
        }
        CArrayInputStream stream(payload);
        if (!part->mesh.Init(stream) || !part->buffer.Create(program) || !part->buffer.SetMesh(part->mesh)) {
            return false;
        }
        part->boneIndex = data.GetAttachmentNode(index);
        // CArmor::Bind holds each attachment at time zero; the torso node
        // supplies its animated placement, independently of the gun's pose.
        std::vector<float> pose;
        if (!part->mesh.GetVerticesAt(0, pose)) {
            return false;
        }
        part->buffer.SetVertices(pose);
        m_drawing->parts[index] = std::move(part);
    }
    return true;
}
