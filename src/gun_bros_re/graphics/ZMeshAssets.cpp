#include "gun_bros_re/graphics/ZMeshAssets.h"
#include "engine/graphics/ZPNG.h"
#include "engine/platform/ZGLLoader.h"
#include <cstdio>

bool LoadMeshAndAtlas(CGunBros &tables, const char *label,
                      std::uint32_t meshPackHash, std::uint32_t meshOrdinal,
                      std::uint32_t imagePackHash, std::uint32_t imageOrdinal,
                      CMesh &mesh, ZTexture &texture, const CMoveSetMesh *moveSet) {
    std::vector<std::uint8_t> meshPayload;
    if (!tables.ReadSectionResource(meshPackHash, ZGameSection::Mesh, meshOrdinal,
                                    meshPayload)) {
        std::printf("[mesh] mesh %u unreadable\n", meshOrdinal);
        return false;
    }

    CArrayInputStream meshStream(meshPayload);
    if (!mesh.Init(meshStream, moveSet)) {
        return false;
    }

    std::vector<std::uint8_t> imagePayload;
    if (!tables.ReadSectionResource(imagePackHash, ZGameSection::Png, imageOrdinal,
                                    imagePayload)) {
        std::printf("[mesh] atlas %u unreadable\n", imageOrdinal);
        return false;
    }

    ZPNGImage decoded;
    if (!PNGDecode(imagePayload, decoded)) {
        return false;
    }

    // Models tile their textures, unlike sprite atlases.
    if (!texture.Create(decoded, GL_REPEAT)) {
        return false;
    }

    std::printf("[mesh] %s: %s mesh %u -- %u verts, %zu indices, %zu frames; "
                "atlas %s %u (%ux%u)\n",
                label, tables.GetPackName(meshPackHash).c_str(), meshOrdinal,
                mesh.GetVertexCount(), mesh.GetIndices().size(),
                mesh.GetFrames().size(), tables.GetPackName(imagePackHash).c_str(),
                imageOrdinal, decoded.width, decoded.height);
    return true;
}

