#pragma once
#include "gun_bros_re/application/CGunBros.h"
#include "engine/graphics/CMesh.h"
#include "engine/graphics/ZTexture.h"
#include "engine/graphics/CMoveSetMesh.h"

/**
 * Fetch one model and the atlas it wears, and report what came out.
 *
 * The one place a mesh ordinal and an image ordinal turn into something
 * drawable.
 *
 * @param label Printed with the result, so a failure names its owner.
 */
bool LoadMeshAndAtlas(CGunBros &tables, const char *label,
                      std::uint32_t meshPackHash, std::uint32_t meshOrdinal,
                      std::uint32_t imagePackHash, std::uint32_t imageOrdinal,
                      CMesh &mesh, ZTexture &texture, const CMoveSetMesh *moveSet = nullptr);

