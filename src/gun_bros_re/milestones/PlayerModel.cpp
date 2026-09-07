/**
 * @file PlayerModel.cpp
 * @brief The player, assembled out of the models a player actually is.
 */

#include "milestones/PlayerModel.h"

#include "engine/CArrayInputStream.h"
#include "engine/CMatrix4d.h"
#include "engine/CPNG.h"
#include "engine/platform/GLLoader.h"
#include "gun_bros/CBrother.h"
#include "gun_bros/CMeshCamera.h"

#include <cstdio>

namespace {

// The lean CBrother::Draw asks OrientForGame for, same as an enemy's.
// Reference: :98959.
constexpr float kGameTiltDegrees = 30.0f;
constexpr float kDegreesToRadians = 3.14159265f / 180.0f;

// CBrother::Bind writes 1.0 into this[494] and nothing ever writes it again,
// so the runtime factor in the draw scale is a constant until something in a
// later milestone moves it.
constexpr float kPlayerRuntimeScale = 1.0f;

/** Which moves of the set drive one config, by index into the set. */
std::vector<std::int32_t> MovesForConfig(const CMoveSetMesh &moveSet,
                                         std::uint8_t configIndex) {
    std::vector<std::int32_t> moves;
    for (std::size_t i = 0; i < moveSet.GetMoves().size(); ++i) {
        if (moveSet.GetMoves()[i].meshConfigIndex == configIndex) {
            moves.push_back(static_cast<std::int32_t>(i));
        }
    }
    return moves;
}

/**
 * Build one animated part out of a move set config.
 *
 * The controller gets the whole config array, as the original's does, but only
 * this config's mesh is filled in -- the moves naming the other configs are
 * filtered out rather than left to fail at SetMove.
 */
bool BuildAnimatedPart(PackTables &tables, const CMoveSetMesh &moveSet,
                       std::uint8_t configIndex, const char *name,
                       PlayerPart &part) {
    const MeshConfig &config = moveSet.GetMeshConfigs()[configIndex];
    part.name = name;

    if (!LoadMeshAndAtlas(tables, name, moveSet.GetPackHash(), config.meshOrdinal,
                          moveSet.GetPackHash(), config.imageOrdinal, part.mesh,
                          part.texture)) {
        return false;
    }

    part.configMeshes.assign(moveSet.GetMeshConfigs().size(), nullptr);
    part.configMeshes[configIndex] = &part.mesh;
    part.moves = MovesForConfig(moveSet, configIndex);
    part.moveSlot = 0;
    part.controller.SetMoveSet(&moveSet, part.configMeshes);
    if (!part.moves.empty()) {
        part.controller.SetMove(part.moves[0]);
    }
    return true;
}

}  // namespace

bool LoadMeshAndAtlas(PackTables &tables, const char *label,
                      std::uint32_t meshPackHash, std::uint32_t meshOrdinal,
                      std::uint32_t imagePackHash, std::uint32_t imageOrdinal,
                      CMesh &mesh, CTexture &texture) {
    std::vector<std::uint8_t> meshPayload;
    if (!tables.ReadSectionResource(meshPackHash, GameSection::Mesh, meshOrdinal,
                                    meshPayload)) {
        std::printf("[mesh] mesh %u unreadable\n", meshOrdinal);
        return false;
    }

    CArrayInputStream meshStream(meshPayload);
    if (!mesh.Init(meshStream)) {
        return false;
    }

    std::vector<std::uint8_t> imagePayload;
    if (!tables.ReadSectionResource(imagePackHash, GameSection::Png, imageOrdinal,
                                    imagePayload)) {
        std::printf("[mesh] atlas %u unreadable\n", imageOrdinal);
        return false;
    }

    PNGImage decoded;
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

PlayerTemplateData::PlayerTemplateData()
    : packHash(0), ordinal(0), gameScale(0.0f) {}

bool FindPlayerTemplate(CResTOCManager &tocManager, PackTables &tables,
                        PlayerTemplateData &out) {
    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        CResPackTOC *pack = tocManager.GetPack(static_cast<int>(i));
        CGameObjectPack &objectPack = tables.GetObjectPack(static_cast<int>(i));
        const std::uint32_t count = objectPack.GetObjectCount(GameSection::Player);

        for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
            std::vector<std::uint8_t> payload;
            if (!pack->GetResource(objectPack.GetHandle(GameSection::Player, ordinal),
                                   payload)) {
                continue;
            }

            CArrayInputStream stream(payload);
            CBrother::Template brother;
            if (!brother.Init(stream)) {
                continue;
            }

            char label[128];
            std::snprintf(label, sizeof(label), "%s player %u",
                          pack->GetShortName().c_str(), ordinal);

            out.packHash = brother.GetMoveSet().GetPackHash();
            out.ordinal = ordinal;
            out.owner = label;
            out.moveSet = brother.GetMoveSet();
            out.gameScale = brother.GetGameScale();
            return true;
        }
    }
    return false;
}

bool BuildPlayerBody(PackTables &tables, const CMoveSetMesh &moveSet,
                     PlayerModel &out) {
    out.parts.clear();
    out.moveSet = moveSet;

    if (out.moveSet.GetMeshConfigs().size() <= kPlayerLegsConfigIndex) {
        std::printf("[player] the move set has %zu configs, expected two\n",
                    out.moveSet.GetMeshConfigs().size());
        return false;
    }

    // Part 0 is the torso, and it is the parent: every attachment is read off
    // ITS mesh at ITS animation time.
    std::unique_ptr<PlayerPart> torso(new PlayerPart());
    std::unique_ptr<PlayerPart> legs(new PlayerPart());
    if (!BuildAnimatedPart(tables, out.moveSet, kPlayerTorsoConfigIndex, "torso",
                           *torso) ||
        !BuildAnimatedPart(tables, out.moveSet, kPlayerLegsConfigIndex, "legs",
                           *legs)) {
        return false;
    }

    out.parts.push_back(std::move(torso));
    out.parts.push_back(std::move(legs));
    return true;
}

bool AttachPlayerGun(PackTables &tables, const std::string &owner,
                     std::uint32_t meshPackHash, std::uint32_t meshOrdinal,
                     std::uint32_t imagePackHash, std::uint32_t imageOrdinal,
                     PlayerModel &out) {
    if (out.parts.empty()) {
        std::printf("[player] no torso to hang %s off\n", owner.c_str());
        return false;
    }

    std::unique_ptr<PlayerPart> gun(new PlayerPart());
    gun->name = owner;
    if (!LoadMeshAndAtlas(tables, owner.c_str(), meshPackHash, meshOrdinal,
                          imagePackHash, imageOrdinal, gun->mesh, gun->texture)) {
        return false;
    }

    const CMesh &torso = out.parts[0]->mesh;
    gun->attached = true;
    gun->boneIndex = kGunBoneIndex;
    if (kGunBoneIndex < torso.GetBoneNames().size()) {
        std::printf("[player] gun hangs off bone %zu, named \"%s\"\n",
                    kGunBoneIndex, torso.GetBoneNames()[kGunBoneIndex].c_str());
    } else {
        std::printf("[player] torso mesh has no bone %zu; the gun will sit at "
                    "the origin\n",
                    kGunBoneIndex);
        gun->attached = false;
    }

    out.parts.push_back(std::move(gun));
    return true;
}

bool CreatePlayerBuffers(PlayerModel &model, const CShaderProgram &program) {
    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        PlayerPart &part = *model.parts[i];
        if (!part.buffer.Create(program) || !part.buffer.SetMesh(part.mesh)) {
            return false;
        }
    }
    return true;
}

void AdvancePlayer(PlayerModel &model, std::int32_t deltaMs) {
    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        PlayerPart &part = *model.parts[i];
        if (part.moves.empty()) {
            continue;
        }

        part.controller.Update(deltaMs);
        if (part.controller.GetAnimation().Evaluate(part.pose)) {
            part.buffer.SetVertices(part.pose);
        }
    }
}

void PosePlayer(PlayerModel &model) {
    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        PlayerPart &part = *model.parts[i];
        if (part.moves.empty()) {
            part.buffer.SetFrame(part.mesh, 0);
            continue;
        }
        if (part.controller.GetAnimation().Evaluate(part.pose)) {
            part.buffer.SetVertices(part.pose);
        }
    }
}

MeshBounds PlayerBounds(const PlayerModel &model) {
    MeshBounds combined = MeshBounds();
    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        const PlayerPart &part = *model.parts[i];
        if (part.attached) {
            continue;
        }

        const MeshBounds &bounds = part.mesh.GetBounds();
        if (bounds.minX < combined.minX) {
            combined.minX = bounds.minX;
        }
        if (bounds.minY < combined.minY) {
            combined.minY = bounds.minY;
        }
        if (bounds.minZ < combined.minZ) {
            combined.minZ = bounds.minZ;
        }
        if (bounds.maxX > combined.maxX) {
            combined.maxX = bounds.maxX;
        }
        if (bounds.maxY > combined.maxY) {
            combined.maxY = bounds.maxY;
        }
        if (bounds.maxZ > combined.maxZ) {
            combined.maxZ = bounds.maxZ;
        }
    }

    combined.centerX = 0.5f * (combined.minX + combined.maxX);
    combined.centerY = 0.5f * (combined.minY + combined.maxY);
    combined.centerZ = 0.5f * (combined.minZ + combined.maxZ);

    float extent = combined.maxX - combined.minX;
    if (combined.maxY - combined.minY > extent) {
        extent = combined.maxY - combined.minY;
    }
    if (combined.maxZ - combined.minZ > extent) {
        extent = combined.maxZ - combined.minZ;
    }
    if (extent > 0.0f) {
        combined.inverseExtent = 1.0f / extent;
    }
    return combined;
}

void DrawPlayer(PlayerModel &model, const CShaderProgram &program,
                const float *base) {
    if (model.parts.empty()) {
        return;
    }

    const CMeshAnimationController &parent =
        model.parts[0]->controller.GetAnimation();

    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        PlayerPart &part = *model.parts[i];

        MeshPart placement;
        if (part.attached) {
            parent.GetNodeAt(part.boneIndex, placement.attachment);
        }

        float mvp[kMatrix4dElements];
        MeshCameraBuildPartMatrix(placement, base, mvp);
        part.buffer.Draw(program, mvp, part.texture);
    }
}

void SelectPlayerMoveSlot(PlayerModel &model, std::size_t slot, bool report) {
    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        PlayerPart &part = *model.parts[i];
        if (part.moves.empty()) {
            continue;
        }

        part.moveSlot = slot % part.moves.size();
        if (!part.controller.SetMove(part.moves[part.moveSlot])) {
            CMeshAnimationController &animation = part.controller.GetAnimation();
            animation.SetTimeMs(animation.GetRangeStartMs());
        }

        if (!report) {
            continue;
        }

        const MeshMove &move = model.moveSet.GetMoves()[part.moves[part.moveSlot]];
        std::printf("[player] %s: move %d (%zu of %zu) -- frames %u..%u, %d ms\n",
                    part.name.c_str(), part.moves[part.moveSlot],
                    part.moveSlot + 1, part.moves.size(), move.firstFrame,
                    move.lastFrame,
                    part.controller.GetAnimation().GetRangeDurationMs());
    }
}

float PlayerModelWorldScale(const PlayerModel &model, float gameScale,
                            float cameraScale) {
    if (model.parts.empty()) {
        return 0.0f;
    }

    const float inverseExtent = model.parts[0]->mesh.GetBounds().inverseExtent;
    return inverseExtent * kPlayerRuntimeScale * gameScale * cameraScale;
}

void BuildPlayerGameMatrix(const float *base, float x, float y, float scale,
                           float facingDegrees, float *out) {
    float step[kMatrix4dElements];
    float accumulated[kMatrix4dElements];
    float next[kMatrix4dElements];

    // base * translate(x, y)
    Matrix4dTranslation(x, y, 0.0f, step);
    Matrix4dMultiply(base, step, accumulated);

    // * scale
    Matrix4dScale(scale, step);
    Matrix4dMultiply(accumulated, step, next);

    // * rotateX(30), about the origin because CBrother::Draw passes no pivot
    Matrix4dRotationX(kGameTiltDegrees * kDegreesToRadians, step);
    Matrix4dMultiply(next, step, accumulated);

    // * rotateZ(facing)
    Matrix4dRotationZ(facingDegrees * kDegreesToRadians, step);
    Matrix4dMultiply(accumulated, step, out);
}
