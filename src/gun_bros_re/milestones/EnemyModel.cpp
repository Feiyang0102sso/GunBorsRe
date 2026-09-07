/**
 * @file EnemyModel.cpp
 * @brief One enemy's models, assembled by its script and ready to draw.
 */

#include "milestones/EnemyModel.h"

#include "engine/CArrayInputStream.h"
#include "engine/CMatrix4d.h"
#include "engine/CPNG.h"
#include "gun_bros/CGameObjectPack.h"
#include "gun_bros/CMeshCamera.h"

#include <cstdio>

namespace {

// The lean CEnemy::Draw asks OrientForGame for. Reference: :98959.
constexpr float kGameTiltDegrees = 30.0f;
constexpr float kDegreesToRadians = 3.14159265f / 180.0f;

}  // namespace

EnemyTemplateData::EnemyTemplateData()
    : packHash(0),
      ordinal(0),
      gameScale(0.0f),
      uiScalePercent(0.0f),
      value112(0),
      value114(0),
      flag117(0),
      radius116(0) {}

bool ReadEnemyTemplate(PackTables &tables, std::uint32_t packHash,
                       std::uint32_t ordinal, const std::string &owner,
                       EnemyTemplateData &out) {
    std::vector<std::uint8_t> payload;
    if (!tables.ReadSectionResource(packHash, GameSection::Enemy, ordinal,
                                    payload)) {
        return false;
    }

    out.packHash = packHash;
    out.ordinal = ordinal;
    out.owner = owner;

    CArrayInputStream stream(payload);
    stream.ReadUInt8();

    CGameAssetRef assetRef;
    assetRef.Init(stream);

    out.script.Load(stream);
    if (!out.moveSet.Init(stream)) {
        std::printf("[enemy] %s: move set unreadable\n", owner.c_str());
        return false;
    }

    out.objectRef104.Init(stream);
    out.value112 = stream.ReadUInt16();
    out.value114 = stream.ReadUInt16();
    out.flag117 = stream.ReadUInt8();
    out.radius116 = stream.ReadUInt8();
    out.gameScale = static_cast<float>(stream.ReadUInt16());
    out.uiScalePercent = static_cast<float>(stream.ReadUInt16());
    if (stream.Overran()) {
        std::printf("[enemy] %s: template truncated before its scales\n",
                    owner.c_str());
        return false;
    }

    // A move set names one pack for every model in it, and that is the pack an
    // enemy's parts come out of -- not necessarily the one the template is in.
    out.packHash = out.moveSet.GetPackHash();
    return true;
}

bool LoadEnemyModel(PackTables &tables, const EnemyTemplateData &entry,
                    bool createBuffers, const CShaderProgram *program,
                    EnemySpawnMode spawnMode, EnemyModel &out) {
    out.configs.clear();

    for (std::size_t i = 0; i < entry.moveSet.GetMeshConfigs().size(); ++i) {
        const MeshConfig &config = entry.moveSet.GetMeshConfigs()[i];
        std::unique_ptr<EnemyModelConfig> loaded(new EnemyModelConfig());

        std::vector<std::uint8_t> meshPayload;
        if (!tables.ReadSectionResource(entry.packHash, GameSection::Mesh,
                                        config.meshOrdinal, meshPayload)) {
            std::printf("[enemy] %s: mesh %u unreadable\n", entry.owner.c_str(),
                        config.meshOrdinal);
            out.configs.push_back(std::move(loaded));
            continue;
        }

        CArrayInputStream meshStream(meshPayload);
        if (!loaded->mesh.Init(meshStream)) {
            out.configs.push_back(std::move(loaded));
            continue;
        }

        if (createBuffers) {
            std::vector<std::uint8_t> imagePayload;
            PNGImage decoded;
            if (!tables.ReadSectionResource(entry.packHash, GameSection::Png,
                                            config.imageOrdinal, imagePayload) ||
                !PNGDecode(imagePayload, decoded) ||
                !loaded->texture.Create(decoded, GL_REPEAT)) {
                std::printf("[enemy] %s: atlas %u unreadable\n",
                            entry.owner.c_str(), config.imageOrdinal);
                out.configs.push_back(std::move(loaded));
                continue;
            }

            if (!loaded->buffer.Create(*program) ||
                !loaded->buffer.SetMesh(loaded->mesh)) {
                std::printf("[enemy] %s: config %zu has no GL buffer\n",
                            entry.owner.c_str(), i);
                out.configs.push_back(std::move(loaded));
                continue;
            }
        }

        loaded->valid = true;
        out.configs.push_back(std::move(loaded));
    }

    if (out.configs.empty()) {
        return false;
    }

    out.configMeshes.assign(out.configs.size(), nullptr);
    for (std::size_t i = 0; i < out.configs.size(); ++i) {
        if (out.configs[i]->valid) {
            out.configMeshes[i] = &out.configs[i]->mesh;
        }
    }

    out.enemy.Bind(entry.script, entry.moveSet, out.configMeshes);
    if (spawnMode == EnemySpawnMode::Level) {
        out.enemy.Spawn();
    } else {
        out.enemy.SpawnForUI();
    }

    // A template whose export 3 does nothing leaves part 0 without a move.
    // Enter the first state that gives it something to play -- a state is what
    // the game would put it in, so this is closer than picking a move out of
    // the list would be.
    if (out.enemy.GetPart(0).controller.GetMoveIndex() == kNoMoveIndex) {
        for (std::size_t stateId = 0; stateId < entry.script.GetStates().size();
             ++stateId) {
            out.enemy.SetState(static_cast<std::uint8_t>(stateId));
            if (out.enemy.GetPart(0).controller.GetMoveIndex() != kNoMoveIndex) {
                break;
            }
        }
    }

    // Still nothing: no state animates part 0, so show its first move rather
    // than an empty buffer.
    if (out.enemy.GetPart(0).controller.GetMoveIndex() == kNoMoveIndex &&
        !entry.moveSet.GetMoves().empty()) {
        out.enemy.GetPart(0).controller.SetMove(0);
    }
    return true;
}

std::int32_t EnemyPartConfig(const EnemyModel &model, std::uint32_t partIndex) {
    const std::int32_t configIndex =
        model.enemy.GetPart(partIndex).controller.GetMeshConfigIndex();
    if (configIndex < 0 ||
        static_cast<std::size_t>(configIndex) >= model.configs.size() ||
        !model.configs[configIndex]->valid) {
        return -1;
    }
    return configIndex;
}

void DrawEnemyModel(EnemyModel &model, const CShaderProgram &program,
                    const float *base) {
    const CMeshAnimationController &parent =
        model.enemy.GetPart(0).controller.GetAnimation();

    for (std::uint32_t i = 0; i < model.enemy.GetPartCount(); ++i) {
        const std::int32_t configIndex = EnemyPartConfig(model, i);
        if (configIndex < 0) {
            continue;
        }

        // Uploaded immediately before the draw, not once per frame: two parts
        // can be showing the same config, and then they share one buffer.
        EnemyModelConfig &config = *model.configs[configIndex];
        const CMeshAnimationController &animation =
            model.enemy.GetPart(i).controller.GetAnimation();
        if (animation.Evaluate(model.pose)) {
            config.buffer.SetVertices(model.pose);
        } else {
            config.buffer.SetFrame(config.mesh, 0);
        }

        const EnemyPart &part = model.enemy.GetPart(i);
        MeshPart placement;
        placement.extraAngleDegrees = part.extraAngleDegrees;
        placement.extraAxisX = part.extraAxisX;
        placement.extraAxisY = part.extraAxisY;
        placement.extraAxisZ = part.extraAxisZ;
        if (part.boneIndex != kEnemyNoBoneIndex) {
            parent.GetNodeAt(static_cast<std::size_t>(part.boneIndex),
                             placement.attachment);
        }

        float mvp[kMatrix4dElements];
        MeshCameraBuildPartMatrix(placement, base, mvp);
        config.buffer.Draw(program, mvp, config.texture);
    }
}

float EnemyModelWorldScale(const EnemyModel &model, float gameScale,
                           float cameraScale) {
    const std::int32_t configIndex = EnemyPartConfig(model, 0);
    if (configIndex < 0) {
        return 0.0f;
    }

    // The runtime scale factor CEnemy::SetScaleFactor holds is 1 until
    // something changes it, and nothing in this port does yet.
    const float inverseExtent =
        model.configs[configIndex]->mesh.GetBounds().inverseExtent;
    return inverseExtent * gameScale * cameraScale;
}

void BuildEnemyGameMatrix(const EnemyModel &model, const float *base, float x,
                          float y, float scale, float facingDegrees,
                          float *out) {
    float pivotX = 0.0f;
    float pivotY = 0.0f;
    float pivotZ = 0.0f;

    const std::int32_t configIndex = EnemyPartConfig(model, 0);
    if (configIndex >= 0) {
        const MeshBounds &bounds = model.configs[configIndex]->mesh.GetBounds();
        pivotX = bounds.centerX;
        pivotY = bounds.centerY;
        pivotZ = bounds.centerZ;
    }

    float step[kMatrix4dElements];
    float accumulated[kMatrix4dElements];
    float next[kMatrix4dElements];

    // base * translate(x, y)
    Matrix4dTranslation(x, y, 0.0f, step);
    Matrix4dMultiply(base, step, accumulated);

    // * scale
    Matrix4dScale(scale, step);
    Matrix4dMultiply(accumulated, step, next);

    // * translate(-pivot)
    Matrix4dTranslation(-pivotX, -pivotY, -pivotZ, step);
    Matrix4dMultiply(next, step, accumulated);

    // * rotateX(30)
    Matrix4dRotationX(kGameTiltDegrees * kDegreesToRadians, step);
    Matrix4dMultiply(accumulated, step, next);

    // * translate(pivot)
    Matrix4dTranslation(pivotX, pivotY, pivotZ, step);
    Matrix4dMultiply(next, step, accumulated);

    // * rotateZ(facing)
    Matrix4dRotationZ(facingDegrees * kDegreesToRadians, step);
    Matrix4dMultiply(accumulated, step, next);

    // * translate(-pivot), which the original leaves unbalanced
    Matrix4dTranslation(-pivotX, -pivotY, -pivotZ, step);
    Matrix4dMultiply(next, step, out);
}
