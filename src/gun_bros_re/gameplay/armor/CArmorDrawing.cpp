#include "gun_bros_re/gameplay/armor/CArmorDrawing.h"

bool CArmor::Load(CGunBros &tables, const Template &data, const ZShaderProgram &program) {
    if (data.GetSlot() >= kArmorSlotCount) {
        return false;
    }
    m_drawing = std::make_unique<CArmor::Drawing>();
    Bind(data);
    Equip();
    CResourceLoader &loader = tables.GetResourceLoader();
    data.Load(loader, m_drawing->images);
    if (!loader.LoadImmediate()) { return false; }
    for (std::uint32_t index = 0; index < kArmorVariantCount; ++index) {
        if (!data.HasMesh(index)) {
            continue;
        }
        std::unique_ptr<Drawing::Attachment> part(new Drawing::Attachment());
        part->mesh = data.GetMesh(index);
        if (!part->buffer.Create(program) || !part->buffer.SetMesh(*part->mesh)) { return false; }
        part->boneIndex = data.GetAttachmentNode(index);
        // CArmor::Bind holds each attachment at time zero; the torso node
        // supplies its animated placement, independently of the gun's pose.
        std::vector<float> pose;
        if (!part->mesh->GetVerticesAt(0, pose)) {
            return false;
        }
        part->buffer.SetVertices(pose);
        m_drawing->parts[index] = std::move(part);
    }
    return true;
}
