#pragma once
#include "gun_bros_re/gameplay/armor/CArmor.h"
#include "engine/graphics/ZMeshBuffer.h"
#include "engine/graphics/ZTexture.h"

/** Each armor owns its optional attachments and brother-specific images. */
struct CArmor::Drawing {
    struct Attachment {
        std::shared_ptr<const CMesh> mesh;
        ZMeshBuffer buffer;
        // Which bone of the TORSO mesh this hangs off.
        std::size_t boneIndex = 0;
    };
    std::shared_ptr<ZTexture> images[kArmorVariantCount];
    std::unique_ptr<Attachment> parts[kArmorVariantCount];
};
