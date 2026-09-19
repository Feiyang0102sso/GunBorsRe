#include "engine/graphics/CMoveSetMesh.h"
#include "engine/graphics/CMesh.h"
#include "engine/resources/CResourceLoader.h"
#include <cstdio>
/** moveSetMesh.cpp :123157, common.bt MeshConfig; retain authored frame filtering. */
bool CMoveSetMesh::LoadMesh(CResourceLoader &loader, unsigned index) const {
    if (index >= m_meshes.size()) { return false; }
    CMesh &mesh = *m_meshes[index];
    if (mesh.GetVertexCount() != 0) { return true; }
    std::vector<std::uint8_t> bytes;
    if (!loader.ReadMesh(m_packHash, m_meshConfigs[index].meshOrdinal, bytes)) { return false; }
    CArrayInputStream input(bytes);
    CMesh candidate;
    if (!candidate.Init(input, this) || input.Available() != 0) {
        std::printf("[moveset] invalid model pack=%u config=%u\n", m_packHash, index);
        return false;
    }
    mesh = std::move(candidate);
    return true;
}
/** :123213 optionally requests images independently of model callbacks. */
void CMoveSetMesh::Load(CResourceLoader &loader, std::vector<std::shared_ptr<ZTexture>> *images) const {
    if (images != nullptr) { images->resize(m_meshConfigs.size()); }
    for (unsigned index = 0; index < m_meshConfigs.size(); ++index) {
        loader.AddFunction([this, &loader, index]() { return LoadMesh(loader, index); });
        if (images != nullptr) { loader.AddImage(m_packHash, m_meshConfigs[index].imageOrdinal, (*images)[index]); }
    }
}
