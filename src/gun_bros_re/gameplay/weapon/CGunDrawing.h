#pragma once
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "engine/graphics/ZMeshBuffer.h"
#include "engine/graphics/ZTexture.h"

/** Each gun owns its drawable mesh bank, including the outgoing torso. */
struct CGun::Drawing {
    struct Mesh {
        std::shared_ptr<const CMesh> mesh;
        std::shared_ptr<ZTexture> texture;
        ZMeshBuffer buffer;
        std::vector<float> pose;
    };
    std::vector<std::unique_ptr<Mesh>> configs;
    Mesh gunPart;
};
