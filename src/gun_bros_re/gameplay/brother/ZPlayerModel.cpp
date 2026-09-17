/**
 * @file ZPlayerModel.cpp
 * @brief The player, assembled out of the models a player actually is.
 */

#include "gun_bros_re/gameplay/brother/ZPlayerModel.h"

#include "engine/resources/CArrayInputStream.h"
#include "engine/core/ZMatrix4d.h"
#include "engine/graphics/ZPNG.h"
#include "engine/platform/ZGLLoader.h"
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "gun_bros_re/gameplay/CBullet.h"
#include "engine/graphics/CMeshCamera.h"

#include <cstdio>
#include <cmath>

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
bool BuildAnimatedPart(ZPackTables &tables, const CMoveSetMesh &moveSet,
                       std::uint8_t configIndex, const char *name,
                       ZPlayerPart &part) {
    const ZMeshConfig &config = moveSet.GetMeshConfigs()[configIndex];
    part.name = name;

    if (!LoadMeshAndAtlas(tables, name, moveSet.GetPackHash(), config.meshOrdinal,
                          moveSet.GetPackHash(), config.imageOrdinal, part.mesh,
                          part.texture, &moveSet)) {
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

bool LoadMeshAndAtlas(ZPackTables &tables, const char *label,
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

bool EquipPlayerArmor(ZPackTables &tables, const CArmor::Template &data,
    const ZShaderProgram &program, ZPlayerModel &out) {
    if (data.GetSlot() >= kArmorSlotCount) {
        return false;
    }
    std::unique_ptr<ZPlayerArmorState> replacement(new ZPlayerArmorState());
    replacement->data = data;
    replacement->armor.Bind(replacement->data);
    replacement->armor.Equip();
    for (std::uint32_t index = 0; index < kArmorVariantCount; ++index) {
        const CGameAssetRef &image = data.GetLoadedImageRef(index);
        if (image.assetId >= 0 && !image.IsNull()) {
            std::vector<std::uint8_t> payload;
            ZPNGImage decoded;
            if (!tables.ReadSectionResource(image.packHash, ZGameSection::Png, image.assetId, payload) ||
                !PNGDecode(payload, decoded) || !replacement->images[index].Create(decoded, GL_REPEAT)) {
                return false;
            }
        }
        if (!data.HasMesh(index)) {
            continue;
        }
        std::unique_ptr<ZPlayerPart> part(new ZPlayerPart());
        const CGameAssetRef &mesh = data.GetMeshRef(index);
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(mesh.packHash, ZGameSection::Mesh, mesh.assetId, payload)) {
            return false;
        }
        CArrayInputStream stream(payload);
        if (!part->mesh.Init(stream) || !part->buffer.Create(program) || !part->buffer.SetMesh(part->mesh)) {
            return false;
        }
        part->attached = true;
        part->boneIndex = data.GetAttachmentNode(index);
        // CArmor::Bind holds each attachment at time zero; the torso node
        // supplies its animated placement, independently of the gun's pose.
        if (!part->mesh.GetVerticesAt(0, part->pose)) {
            return false;
        }
        part->buffer.SetVertices(part->pose);
        replacement->parts[index] = std::move(part);
    }
    out.armor[data.GetSlot()] = std::move(replacement);
    std::printf("[armor] equipped slot %u defense=%.0f%% damage=%.0f%% speed=%.0f%%\n",
        data.GetSlot(), (PlayerArmorMultiplier(out, 0) - 1) * 100,
        (PlayerArmorMultiplier(out, 1) - 1) * 100, (PlayerArmorMultiplier(out, 2) - 1) * 100);
    return true;
}

void ClearPlayerArmor(ZPlayerModel &model) {
    for (auto &armor : model.armor) {
        armor.reset();
    }
}

float PlayerArmorMultiplier(const ZPlayerModel &model, std::uint32_t attribute) {
    float result = 1.0f;
    for (const auto &armor : model.armor) {
        if (armor) {
            result += armor->armor.GetAttribute(attribute) / 100.0f;
        }
    }
    return result;
}

ZPlayerTemplateData::ZPlayerTemplateData()
    : packHash(0), ordinal(0), gameScale(0.0f) {}

bool FindPlayerTemplate(CResTOCManager &tocManager, ZPackTables &tables,
                        ZPlayerTemplateData &out) {
    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        CResPackTOC *pack = tocManager.GetPack(static_cast<int>(i));
        CGameObjectPack &objectPack = tables.GetObjectPack(static_cast<int>(i));
        const std::uint32_t count = objectPack.GetObjectCount(ZGameSection::Player);

        for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
            std::vector<std::uint8_t> payload;
            if (!pack->GetResource(objectPack.GetHandle(ZGameSection::Player, ordinal),
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
            out.script = brother.GetScript();
            out.gameScale = brother.GetGameScale();
            return true;
        }
    }
    return false;
}

bool BuildPlayerBody(ZPackTables &tables, const CMoveSetMesh &moveSet,
                     ZPlayerModel &out) {
    ClearPlayerArmor(out);
    out.weapon.reset();
    out.parts.clear();
    out.moveSet = moveSet;

    if (out.moveSet.GetMeshConfigs().size() <= kPlayerLegsConfigIndex) {
        std::printf("[player] the move set has %zu configs, expected two\n",
                    out.moveSet.GetMeshConfigs().size());
        return false;
    }

    // Part 0 is the torso, and it is the parent: every attachment is read off
    // ITS mesh at ITS animation time.
    std::unique_ptr<ZPlayerPart> torso(new ZPlayerPart());
    std::unique_ptr<ZPlayerPart> legs(new ZPlayerPart());
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

bool AttachPlayerGun(ZPackTables &tables, const std::string &owner,
                     std::uint32_t meshPackHash, std::uint32_t meshOrdinal,
                     std::uint32_t imagePackHash, std::uint32_t imageOrdinal,
                     ZPlayerModel &out) {
    if (out.parts.empty()) {
        std::printf("[player] no torso to hang %s off\n", owner.c_str());
        return false;
    }

    std::unique_ptr<ZPlayerPart> gun(new ZPlayerPart());
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

bool CreatePlayerBuffers(ZPlayerModel &model, const ZShaderProgram &program) {
    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        ZPlayerPart &part = *model.parts[i];
        if (!part.buffer.Create(program) || !part.buffer.SetMesh(part.mesh)) {
            return false;
        }
    }
    ZPlayerWeaponState *weaponBanks[] = {model.weapon.get(), model.uiOtherWeapon.get()};
    for (ZPlayerWeaponState *weapon : weaponBanks) {
        if (weapon == nullptr) { continue; }
        for (auto &config : weapon->configs) {
            if (!config->buffer.Create(program) || !config->buffer.SetMesh(config->mesh)) { return false; }
        }
        ZPlayerPart &gun = weapon->gunPart;
        if (!gun.buffer.Create(program) || !gun.buffer.SetMesh(gun.mesh)) { return false; }
    }
    // SetMesh uploads frame zero, which can differ from the script's idle
    // range. Pose now so load/restart/weapon changes never expose that frame.
    PosePlayer(model);
    return true;
}

void AdvancePlayer(ZPlayerModel &model, std::int32_t deltaMs) {
    if (model.weapon) {
        model.weapon->brother.Update(deltaMs);
        PosePlayer(model);
        return;
    }
    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        ZPlayerPart &part = *model.parts[i];
        if (part.moves.empty()) {
            continue;
        }

        part.controller.Update(deltaMs);
        if (part.controller.GetAnimation().Evaluate(part.pose)) {
            part.buffer.SetVertices(part.pose);
        }
    }
}

/** Resolve the actual controller mesh, including an outgoing gun's torso. */
ZPlayerPart *FindPlayerTorsoPart(ZPlayerModel &model) {
    const CMesh *mesh = model.weapon->brother.GetTorso().GetAnimation().GetMesh();
    for (auto &part : model.parts) {
        if (&part->mesh == mesh) { return part.get(); }
    }
    ZPlayerWeaponState *banks[] = {model.weapon.get(), model.uiOtherWeapon.get()};
    for (ZPlayerWeaponState *bank : banks) {
        if (bank == nullptr) { continue; }
        for (auto &part : bank->configs) {
            if (&part->mesh == mesh) { return part.get(); }
        }
    }
    for (auto &entry : model.matchWeapons) {
        // EquipMatchGun reserves the destination while uploading its buffers;
        // PosePlayer can still be evaluating the outgoing weapon at this point.
        if (entry.second == nullptr) { continue; }
        for (auto &part : entry.second->configs) {
            if (&part->mesh == mesh) { return part.get(); }
        }
    }
    return nullptr;
}

void PosePlayer(ZPlayerModel &model) {
    if (model.weapon) {
        ZPlayerWeaponState &weapon = *model.weapon;
        CMoveSetMeshController &torso = weapon.brother.GetTorso();
        ZPlayerPart *part = FindPlayerTorsoPart(model);
        if (part != nullptr) {
            if (torso.GetAnimation().Evaluate(part->pose)) { part->buffer.SetVertices(part->pose); }
        }
        CMoveSetMeshController &legs = weapon.brother.GetLegs();
        const int legsIndex = legs.GetMeshConfigIndex();
        if (legsIndex >= 0) {
            ZPlayerPart &part = *model.parts[legsIndex];
            if (legs.GetAnimation().Evaluate(part.pose)) { part.buffer.SetVertices(part.pose); }
        }
        ZPlayerWeaponState *active = &weapon;
        if (model.uiActiveWeapon != nullptr) { active = model.uiActiveWeapon; }
        ZPlayerPart &gun = active->gunPart;
        if (active->gun.GetAnimation().Evaluate(gun.pose)) { gun.buffer.SetVertices(gun.pose); }
        return;
    }
    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        ZPlayerPart &part = *model.parts[i];
        if (part.moves.empty()) {
            part.buffer.SetFrame(part.mesh, 0);
            continue;
        }
        if (part.controller.GetAnimation().Evaluate(part.pose)) {
            part.buffer.SetVertices(part.pose);
        }
    }
}

ZMeshBounds PlayerBounds(const ZPlayerModel &model) {
    ZMeshBounds combined = ZMeshBounds();
    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        const ZPlayerPart &part = *model.parts[i];
        if (part.attached) {
            continue;
        }

        const ZMeshBounds &bounds = part.mesh.GetBounds();
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

bool BuildPlayerUIMatrix(const ZPlayerModel &model, float centerX, float top, float height,
    float facingRadians, float screenWidth, float screenHeight, float *out) {
    if (!model.weapon) { return false; }
    const CMesh *torso = model.weapon->brother.GetTorso().GetAnimation().GetMesh();
    if (torso == nullptr) { return false; }
    const ZMeshBounds &bounds = torso->GetBounds();
    const float torsoHeight = std::abs(bounds.maxZ - bounds.minZ);
    if (torsoHeight <= 0 || height <= 0) { return false; }

    // CBrother::DrawUI :136337-136357 uses mesh mem+68/+80/+92, not
    // combined player bounds or the weapon's extent. These are derived from BIG.
    const float scale = height / torsoHeight;
    const float originY = static_cast<float>(static_cast<int>(top - bounds.centerZ * scale + height + height * 0.5f));
    float projection[16], translation[16], scaling[16], tilt[16], facing[16], first[16], second[16];
    // CGraphics2d_OGLES::SetWidthAndHeightMappedOrthoProjection :378895:
    // original near/far = 0/32767; OrientForUI :98920 places the mesh at -500.
    Matrix4dOrthoTopLeft(screenWidth, screenHeight, 32767, projection);
    projection[11] = -1;
    Matrix4dTranslation(static_cast<float>(static_cast<int>(centerX)), originY, -500, translation);
    Matrix4dScale(scale, scaling);
    Matrix4dRotationX(3.14159265f * 0.5f, tilt);
    Matrix4dRotationZ(3.14159265f + facingRadians, facing);
    Matrix4dMultiply(projection, translation, first);
    Matrix4dMultiply(first, scaling, second);
    Matrix4dMultiply(second, tilt, first);
    Matrix4dMultiply(first, facing, out);
    return true;
}

void DrawPlayer(ZPlayerModel &model, const ZShaderProgram &program,
                const float *base) {
    if (model.weapon && (!model.weapon->brother.IsVisible() || model.weapon->brother.IsImmunityHidden())) { return; }
    float flash = 0;
    if (model.vitals != nullptr) { flash = model.vitals->flash; }
    if (model.parts.empty()) {
        return;
    }
    if (model.weapon) {
        ZPlayerWeaponState &weapon = *model.weapon;
        const int legsIndex = weapon.brother.GetLegs().GetMeshConfigIndex();
        ZPlayerPart *part = FindPlayerTorsoPart(model);
        if (part != nullptr) {
            const ZTexture *texture = &part->texture;
            if (model.armor[1] && model.armor[1]->images[model.brotherIndex].IsValid()) {
                texture = &model.armor[1]->images[model.brotherIndex];
            }
            part->buffer.Draw(program, base, *texture, flash);
        }
        if (legsIndex >= 0) {
            ZPlayerPart &part = *model.parts[legsIndex];
            const ZTexture *texture = &part.texture;
            // CBrother::Draw :134795 always uses the first legs image.
            if (model.armor[0] && model.armor[0]->images[0].IsValid()) {
                texture = &model.armor[0]->images[0];
            }
            part.buffer.Draw(program, base, *texture, flash);
        }
        ZPlayerWeaponState *active = &weapon;
        if (model.uiActiveWeapon != nullptr) { active = model.uiActiveWeapon; }
        const int handedness = active->data.GetHandedness();
        int count = 1;
        if (handedness == 2) { count = 2; }
        for (int i = 0; i < count; ++i) {
            std::size_t bone = kGunBoneIndex;
            if (handedness == 1 || i == 1) { bone = kGunLeftBoneIndex; }
            ZMeshPart placement;
            if (!weapon.brother.GetTorso().GetAnimation().GetNodeAt(bone, placement.attachment)) { continue; }
            float mvp[kMatrix4dElements];
            MeshCameraBuildPartMatrix(placement, base, mvp);
            active->gunPart.buffer.Draw(program, mvp, active->gunPart.texture, active->gun.GetHeatIntensity());
        }
        // Original order after weapons: head, then both torso attachments.
        const std::uint32_t slots[] = {2, 1};
        for (std::uint32_t slot : slots) {
            if (!model.armor[slot]) {
                continue;
            }
            ZPlayerArmorState &armor = *model.armor[slot];
            std::uint32_t partCount = kArmorVariantCount;
            if (slot == 2) {
                partCount = 1;
            }
            for (std::uint32_t index = 0; index < partCount; ++index) {
                if (!armor.parts[index]) {
                    continue;
                }
                ZPlayerPart &part = *armor.parts[index];
                ZMeshPart placement;
                if (!weapon.brother.GetTorso().GetAnimation().GetNodeAt(part.boneIndex, placement.attachment)) {
                    continue;
                }
                float mvp[kMatrix4dElements];
                MeshCameraBuildPartMatrix(placement, base, mvp);
                unsigned imageIndex = model.brotherIndex;
                if (slot == 2) { imageIndex = 0; }
                part.buffer.Draw(program, mvp, armor.images[imageIndex], flash);
            }
        }
        return;
    }

    const CMeshAnimationController &parent =
        model.parts[0]->controller.GetAnimation();

    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        ZPlayerPart &part = *model.parts[i];

        ZMeshPart placement;
        if (part.attached) {
            parent.GetNodeAt(part.boneIndex, placement.attachment);
        }

        float mvp[kMatrix4dElements];
        MeshCameraBuildPartMatrix(placement, base, mvp);
        part.buffer.Draw(program, mvp, part.texture);
    }
}

void SelectPlayerMoveSlot(ZPlayerModel &model, std::size_t slot, bool report) {
    // Equipped actors are driven by the original player script, independently
    // for torso and legs. The old manual slot browser remains for bare bodies.
    if (model.weapon) { return; }
    for (std::size_t i = 0; i < model.parts.size(); ++i) {
        ZPlayerPart &part = *model.parts[i];
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

        const ZMeshMove &move = model.moveSet.GetMoves()[part.moves[part.moveSlot]];
        std::printf("[player] %s: move %d (%zu of %zu) -- frames %u..%u, %d ms\n",
                    part.name.c_str(), part.moves[part.moveSlot],
                    part.moveSlot + 1, part.moves.size(), move.firstFrame,
                    move.lastFrame,
                    part.controller.GetAnimation().GetRangeDurationMs());
    }
}

float PlayerModelWorldScale(const ZPlayerModel &model, float gameScale,
                            float cameraScale) {
    if (model.parts.empty()) {
        return 0.0f;
    }

    const float inverseExtent = model.parts[0]->mesh.GetBounds().inverseExtent;
    if (model.weapon) {
        const CMesh *torso = model.weapon->brother.GetTorso().GetAnimation().GetMesh();
        if (torso != nullptr) {
            return torso->GetBounds().inverseExtent * kPlayerRuntimeScale * gameScale * cameraScale;
        }
    }
    return inverseExtent * kPlayerRuntimeScale * gameScale * cameraScale;
}

/** Shared BIG asset loading; only the primary state binds a brother script. */
static bool LoadPlayerWeaponAssets(ZPackTables &tables, const CGun::Template &data,
    const std::string &owner, std::unique_ptr<ZPlayerWeaponState> &weapon) {
    weapon = std::make_unique<ZPlayerWeaponState>();
    weapon->data = data;
    const CMoveSetMesh &moves = weapon->data.GetMoveSet();
    for (const ZMeshConfig &config : moves.GetMeshConfigs()) {
        std::unique_ptr<ZPlayerPart> part(new ZPlayerPart());
        if (!LoadMeshAndAtlas(tables, "weapon torso", moves.GetPackHash(), config.meshOrdinal,
            moves.GetPackHash(), config.imageOrdinal, part->mesh, part->texture, &moves)) { return false; }
        weapon->configs.push_back(std::move(part));
    }
    const CGameAssetRef &mesh = data.GetMeshRef();
    const CGameAssetRef &atlas = data.GetImageRef();
    if (!LoadMeshAndAtlas(tables, owner.c_str(), mesh.packHash, mesh.assetId,
        atlas.packHash, atlas.assetId, weapon->gunPart.mesh, weapon->gunPart.texture)) { return false; }
    // CBrother::UpdateNormal treats continuous beams specially when the gun
    // script clears its ready flag. Resolve that property from the real bullet.
    bool beam = false;
    const GameObjectRef &bulletRef = data.GetBulletRef();
    if (bulletRef.localIndex != 255) {
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(bulletRef.packHash, ZGameSection::Bullet, bulletRef.localIndex, payload)) { return false; }
        CArrayInputStream stream(payload);
        CBullet::Template bullet;
        if (!bullet.Init(stream)) { return false; }
        beam = (bullet.GetFlags() & 0x100) != 0;
    }
    weapon->gun.Bind(weapon->data, &weapon->gunPart.mesh, beam);
    return true;
}

bool PreparePlayerUIWeapon(ZPackTables &tables, const CGun::Template &data,
    const std::string &owner, ZPlayerModel &out) {
    return LoadPlayerWeaponAssets(tables, data, owner, out.uiOtherWeapon);
}

void SelectPlayerUIWeapon(ZPlayerModel &model, bool primary) {
    ZPlayerWeaponState *active = model.uiOtherWeapon.get();
    if (primary) { active = model.weapon.get(); }
    std::vector<const CMesh *> meshes;
    for (const auto &part : active->configs) { meshes.push_back(&part->mesh); }
    model.weapon->brother.SetUIGun(active->gun, meshes);
    model.uiActiveWeapon = active;
}

bool EquipPlayerWeapon(ZPackTables &tables, const CScript &playerScript,
    const CGun::Template &data, const std::string &owner, ZPlayerModel &out) {
    std::unique_ptr<ZPlayerWeaponState> weapon;
    if (!LoadPlayerWeaponAssets(tables, data, owner, weapon)) { return false; }
    weapon->playerScript = playerScript;
    std::vector<const CMesh *> meshes;
    for (auto &part : weapon->configs) { meshes.push_back(&part->mesh); }
    std::vector<const CMesh *> bodyMeshes;
    for (auto &part : out.parts) { bodyMeshes.push_back(&part->mesh); }
    weapon->brother.SetHuman(out.human);
    weapon->brother.SetCooperative(out.cooperative);
    weapon->brother.SetDeathmatch(out.deathmatch);
    weapon->gun.SetDeathmatch(out.deathmatch);
    weapon->gun.SetMasteryExperience(out.masteryExperience);
    weapon->brother.Bind(weapon->playerScript, out.moveSet, bodyMeshes, weapon->gun, meshes);
    std::printf("[player] equipped %s: weaponTorso=%d move=%d legs=%d hand=%u state=%d\n",
        owner.c_str(), weapon->brother.TorsoUsesWeapon(), weapon->brother.GetTorso().GetMoveIndex(),
        weapon->brother.GetLegs().GetMoveIndex(), data.GetHandedness(), weapon->brother.GetStateId());
    out.weapon = std::move(weapon);
    out.uiActiveWeapon = nullptr;
    out.uiOtherWeapon.reset();
    out.weapon->brother.SetVitals(out.vitals);
    out.weapon->brother.SetPowerupState(&out.powerups);
    return true;
}

void SetPlayerInput(ZPlayerModel &model, bool moving, bool shooting) {
    if (model.weapon) { model.weapon->brother.SetInput(moving, shooting); }
}

bool GetPlayerMuzzle(ZPlayerModel &model, int hand, int node, ZMeshBoneTransform &out) {
    if (!model.weapon) { return false; }
    ZMeshPart placement;
    std::size_t bone = kGunBoneIndex;
    if (hand == 1) { bone = kGunLeftBoneIndex; }
    if (!model.weapon->brother.GetTorso().GetAnimation().GetNodeAt(bone, placement.attachment)) { return false; }
    ZMeshBoneTransform muzzle{};
    // CGun::FireBullet zero-initializes the node and retains that origin when
    // a model has no named muzzle (for example pack5 gun 60).
    model.ActiveWeapon().gun.GetAnimation().GetNodeAt(node, muzzle);
    float identity[kMatrix4dElements];
    Matrix4dIdentity(identity);
    float transform[kMatrix4dElements];
    MeshCameraBuildPartMatrix(placement, identity, transform);
    out = muzzle;
    out.posX = transform[0] * muzzle.posX + transform[1] * muzzle.posY + transform[2] * muzzle.posZ + transform[3];
    out.posY = transform[4] * muzzle.posX + transform[5] * muzzle.posY + transform[6] * muzzle.posZ + transform[7];
    out.posZ = transform[8] * muzzle.posX + transform[9] * muzzle.posY + transform[10] * muzzle.posZ + transform[11];
    return true;
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
