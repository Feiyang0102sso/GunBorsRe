#include "gun_bros_re/gameplay/weapon/CGun.h"
#include <cstdio>
/** gun.cpp :128918; gun_template.bt model reference, CMesh owned by Template. */
bool CGun::Template::LoadMesh(CResourceLoader &loader) const {
    if (m_model->GetVertexCount() != 0) { return true; }
    std::vector<std::uint8_t> bytes;
    if (!loader.ReadMesh(m_meshRef.packHash, m_meshRef.assetId, bytes)) { return false; }
    CArrayInputStream input(bytes);
    CMesh candidate;
    if (!candidate.Init(input) || input.Available() != 0) {
        std::printf("[gun] invalid model pack=%u ordinal=%d\n", m_meshRef.packHash, m_meshRef.assetId);
        return false;
    }
    *m_model = std::move(candidate);
    return true;
}
/** :127965 queues body models, gun model callback, then its atlas.
 * Original gun loading omits body images; the desktop outgoing-pose renderer
 * can request CMoveSetMesh's image branch explicitly with moveImages.
 */
void CGun::Template::Load(CResourceLoader &loader, std::shared_ptr<ZTexture> &image,
    std::vector<std::shared_ptr<ZTexture>> *moveImages) const {
    m_moveSet.Load(loader, moveImages);
    loader.AddFunction([this, &loader]() { return LoadMesh(loader); });
    loader.AddImage(m_imageRef.packHash, m_imageRef.assetId, image);
}
