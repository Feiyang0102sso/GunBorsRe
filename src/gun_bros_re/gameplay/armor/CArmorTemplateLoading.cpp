#include "gun_bros_re/gameplay/armor/CArmor.h"
#include <cstdio>
/** armor.cpp :176635 and armor_template.bt: both authored attachments are distinct. */
bool CArmor::Template::LoadMesh(CResourceLoader &loader, unsigned variant) const {
    if (variant >= kArmorVariantCount || !HasMesh(variant)) { return false; }
    CMesh &model = *m_models[variant];
    if (model.GetVertexCount() != 0) { return true; }
    const CGameAssetRef &ref = m_meshRef[variant];
    std::vector<std::uint8_t> bytes;
    if (!loader.ReadMesh(ref.packHash, ref.assetId, bytes)) { return false; }
    CArrayInputStream input(bytes);
    CMesh candidate;
    if (!candidate.Init(input) || input.Available() != 0) {
        std::printf("[armor] invalid model pack=%u ordinal=%d\n", ref.packHash, ref.assetId);
        return false;
    }
    model = std::move(candidate);
    return true;
}
/** :176493 selects model images or body-texture alternatives, then queues meshes. */
void CArmor::Template::Load(CResourceLoader &loader,
    std::shared_ptr<ZTexture> (&images)[kArmorVariantCount]) const {
    for (unsigned variant = 0; variant < kArmorVariantCount; ++variant) {
        const auto &ref = GetLoadedImageRef(variant);
        if (ref.assetId >= 0 && !ref.IsNull()) { loader.AddImage(ref.packHash, ref.assetId, images[variant]); }
    }
    for (unsigned variant = 0; variant < kArmorVariantCount; ++variant) {
        if (HasMesh(variant)) {
            loader.AddFunction([this, &loader, variant]() { return LoadMesh(loader, variant); });
        }
    }
}
