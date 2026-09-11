#include "gun_bros_viewer/ViewerControls.h"
#include "gun_bros_viewer/ViewerSettings.h"
#include "engine/core/Paths.h"
/**
 * @file MeshPreview.cpp
 * @brief M3.5 and M3.7 harnesses: the 3D models in Section 31.
 *
 * Four entry points over one body of knowledge:
 *
 * - `--meshes` parses every model and reports what is in it,
 * - `--movesets` follows every model to the atlas it wears,
 * - `--mesh` puts one on screen and plays its moves,
 * - `--character` stands a whole player up out of three of them.
 *
 * All four share a walk over the five template types that own models, because
 * "which atlas does this model wear" has exactly one answer and it should be
 * computed in exactly one place. That sharing is why the M3.7 viewer is here
 * rather than in a file of its own.
 *
 * What is NOT here is the character assembly itself: torso, legs and the bone
 * a gun hangs off now live in PlayerModel.h, which the map viewer shares. What
 * stays is the weapon CATALOGUE, which is a viewer feature -- the game hands a
 * player one gun and never a list to page through.
 * The new WeaponCatalog now shares that list with GameView's equipment keys;
 * the original catalogue-only helpers below remain as historical reference.
 *
 * PackTables has already moved out to its own header, which M3.8 shares. The
 * walk itself should follow the next time something outside this file needs
 * it; neither M3.8 nor the map viewer does, because an enemy is reached by its
 * own template and the player by a direct look-up.
 */

#define NOMINMAX
#include "gun_bros_viewer/scenes/MeshPreview.h"

#include "gun_bros_re/data/PackTables.h"
#include "gun_bros_re/gameplay/PlayerModel.h"
#include "gun_bros_re/gameplay/EnemyModel.h"
#include "gun_bros_re/data/ArmorCatalog.h"
#include "gun_bros_re/data/WeaponCatalog.h"
#include "gun_bros_re/data/StoreCatalog.h"
#include "gun_bros_re/gameplay/CombatGeometry.h"
#include "gun_bros_re/gameplay/WeaponEffects.h"
#include "gun_bros_re/gameplay/CParticleEffect.h"

#include "engine/resources/CArrayInputStream.h"
#include "engine/core/CMatrix4d.h"
#include "engine/graphics/CMeshBuffer.h"
#include "engine/graphics/CPNG.h"
#include "engine/graphics/CShaderProgram.h"
#include "engine/graphics/CTexture.h"
#include "engine/platform/CWindow.h"
#include "engine/platform/GLLoader.h"
#include "engine/glu/script/CScript.h"
#include "gun_bros_re/gameplay/CArmor.h"
#include "gun_bros_re/gameplay/CBrother.h"
#include "gun_bros_re/gameplay/CBullet.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "gun_bros_re/data/CGameObjectPack.h"
#include "gun_bros_re/gameplay/CGun.h"
#include "engine/graphics/CMesh.h"
#include "engine/graphics/CMeshAnimationController.h"
#include "engine/graphics/CMeshCamera.h"
#include "engine/graphics/CMoveSetMesh.h"
#include "engine/graphics/CMoveSetMeshController.h"
#include "engine/resources/CResTOCManager.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "gun_bros_viewer/scenes/MeshPreviewInternal.h"
using namespace MeshPreviewDetail;

namespace MeshPreviewDetail {

// ---------------------------------------------------------------------------
// --meshes: every model, parsed
// ---------------------------------------------------------------------------

/** Bone names on one line, in the order the mesh stores them. */
std::string JoinBoneNames(const CMesh &mesh) {
    std::string joined;
    for (std::size_t i = 0; i < mesh.GetBoneNames().size(); ++i) {
        if (i > 0) {
            joined += ", ";
        }
        joined += mesh.GetBoneNames()[i];
    }
    return joined;
}
}

namespace MeshPreviewDetail {

/**
 * Report texture coordinates that leave the unit square.
 *
 * M2 had to copy a 1/4096 scale because sprite UVs are stored as integers.
 * Mesh UVs are floats on the wire, so they should already be normalised --
 * this is what proves it, and it stays quiet when nothing is wrong. The ones
 * that do go outside are tiling, which is why model textures wrap.
 */
void ReportTexCoordsOutsideUnitSquare(const CMesh &mesh) {
    const std::vector<float> &texCoords = mesh.GetTexCoords();
    if (texCoords.empty()) {
        return;
    }

    float lowest = texCoords[0];
    float highest = texCoords[0];
    for (std::size_t i = 1; i < texCoords.size(); ++i) {
        if (texCoords[i] < lowest) {
            lowest = texCoords[i];
        }
        if (texCoords[i] > highest) {
            highest = texCoords[i];
        }
    }

    if (lowest < 0.0f || highest > 1.0f) {
        std::printf("       uv spans %.4f .. %.4f\n", lowest, highest);
    }
}
}

namespace MeshPreviewDetail {

/**
 * Report anything odd about the index buffer.
 *
 * An index at or past the vertex count cannot address a vertex, so it would
 * have to be a primitive-restart marker -- and a strip drawn without honouring
 * one grows a huge triangle joining the end of one piece to the start of the
 * next. Stitches are the other way of joining strips: a repeated index makes a
 * zero-area triangle that the rasteriser drops on its own.
 */
void ReportIndexBuffer(const CMesh &mesh) {
    const std::vector<std::uint16_t> &indices = mesh.GetIndices();
    unsigned outOfRange = 0;
    unsigned stitches = 0;

    for (std::size_t i = 0; i < indices.size(); ++i) {
        if (indices[i] >= mesh.GetVertexCount()) {
            outOfRange++;
        }
        if (i > 0 && indices[i] == indices[i - 1]) {
            stitches++;
        }
    }

    if (outOfRange > 0) {
        std::printf("       %u indices past the vertex count\n", outOfRange);
    }
    if (stitches > 0) {
        std::printf("       %u repeated indices\n", stitches);
    }
}
}

namespace MeshPreviewDetail {

/**
 * Report triangles that span a large part of the model.
 *
 * A triangle strip joined wrongly shows up as a long thin triangle reaching
 * from one piece of the model to another. Real geometry at this polygon count
 * has edges a few per cent of the model across, so anything over a third of it
 * is worth naming.
 */
void ReportLongTriangles(const CMesh &mesh) {
    if (mesh.GetFrames().empty()) {
        return;
    }

    const MeshBounds &bounds = mesh.GetBounds();
    float extent = bounds.maxX - bounds.minX;
    if (bounds.maxY - bounds.minY > extent) {
        extent = bounds.maxY - bounds.minY;
    }
    if (bounds.maxZ - bounds.minZ > extent) {
        extent = bounds.maxZ - bounds.minZ;
    }
    if (extent <= 0.0f) {
        return;
    }

    const std::vector<float> &vertices = mesh.GetFrames()[0].vertices;
    const std::vector<std::uint16_t> &indices = mesh.GetIndices();

    unsigned drawn = 0;
    unsigned oversized = 0;
    float longest = 0.0f;

    for (std::size_t i = 2; i < indices.size(); ++i) {
        const std::uint32_t a = indices[i - 2];
        const std::uint32_t b = indices[i - 1];
        const std::uint32_t c = indices[i];

        // A repeated index makes a zero-area triangle: that is the stitch, and
        // the rasteriser drops it. Only what actually gets drawn counts.
        if (a == b || b == c || a == c) {
            continue;
        }
        drawn++;

        const std::uint32_t corners[3] = {a, b, c};
        float widest = 0.0f;
        for (int edge = 0; edge < 3; ++edge) {
            const std::uint32_t from = corners[edge];
            const std::uint32_t to = corners[(edge + 1) % 3];
            const float dx = vertices[3 * from] - vertices[3 * to];
            const float dy = vertices[3 * from + 1] - vertices[3 * to + 1];
            const float dz = vertices[3 * from + 2] - vertices[3 * to + 2];
            const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (length > widest) {
                widest = length;
            }
        }

        if (widest > longest) {
            longest = widest;
        }
        if (widest > extent / 3.0f) {
            oversized++;
        }
    }

    if (oversized > 0) {
        std::printf("       %u of %u drawn triangles span over a third of the "
                    "model (widest %.1f of %.1f)\n",
                    oversized, drawn, longest, extent);
    }
}
}

namespace MeshPreviewDetail {

/** Parse every Section 31 resource of one pack and print a line each. */
void SurveyPack(CResPackTOC &pack, MeshSurveyTotals &totals) {
    CGameObjectPack objectPack;
    if (!objectPack.Init(pack)) {
        return;
    }

    // Section 31 is past the counts resource, so its size is the distance to
    // the next section base.
    const std::uint32_t span = objectPack.GetSectionSpan(GameSection::Mesh);
    if (span == 0) {
        return;
    }

    std::printf("\n%s -- %u handles from 0x%08X\n", pack.GetShortName().c_str(),
                span, objectPack.GetSectionBase(GameSection::Mesh));

    for (std::uint32_t ordinal = 0; ordinal < span; ++ordinal) {
        const std::uint32_t handle = objectPack.GetHandle(GameSection::Mesh, ordinal);

        std::vector<std::uint8_t> payload;
        if (!pack.GetResource(handle, payload)) {
            std::printf("  %3u  handle 0x%08X unreadable\n", ordinal, handle);
            totals.unreadable++;
            continue;
        }

        // A section that holds nothing still owns one empty resource.
        if (payload.empty()) {
            totals.placeholders++;
            continue;
        }

        CArrayInputStream stream(payload);
        CMesh mesh;
        if (!mesh.Init(stream)) {
            std::printf("  %3u  %zu bytes, parse failed\n", ordinal, payload.size());
            totals.unreadable++;
            continue;
        }

        totals.meshes++;
        const MeshBounds &bounds = mesh.GetBounds();
        std::printf("  %3u  %2zu bones %5u verts %6zu idx %4zu frames %6d ms"
                    "  %6.1f x %6.1f x %6.1f  %s\n",
                    ordinal, mesh.GetBoneNames().size(), mesh.GetVertexCount(),
                    mesh.GetIndices().size(), mesh.GetFrames().size(),
                    mesh.GetDurationMs(), bounds.maxX - bounds.minX,
                    bounds.maxY - bounds.minY, bounds.maxZ - bounds.minZ,
                    JoinBoneNames(mesh).c_str());

        ReportTexCoordsOutsideUnitSquare(mesh);
        ReportIndexBuffer(mesh);
        ReportLongTriangles(mesh);

        // The one number that decides whether the format is understood.
        if (stream.Available() != 0) {
            std::printf("       %zu bytes left over\n", stream.Available());
            totals.withLeftovers++;
        }
    }
}
}

namespace MeshPreviewDetail {

/** "pack1 enemy 3" */
std::string OwnerLabel(const CResPackTOC &pack, const char *kind,
                       std::uint32_t ordinal) {
    char text[128];
    std::snprintf(text, sizeof(text), "%s %s %u", pack.GetShortName().c_str(), kind,
                  ordinal);
    return std::string(text);
}
}

namespace MeshPreviewDetail {

/** Hand every config of a move set to the sink. */
void EmitMoveSet(const CMoveSetMesh &moveSet, const std::string &owner,
                 IMeshPairSink &sink) {
    for (std::size_t i = 0; i < moveSet.GetMeshConfigs().size(); ++i) {
        const MeshConfig &config = moveSet.GetMeshConfigs()[i];

        MeshPair pair;
        // A move set names one pack for both halves of every pair.
        pair.meshPackHash = moveSet.GetPackHash();
        pair.meshOrdinal = config.meshOrdinal;
        pair.imagePackHash = moveSet.GetPackHash();
        pair.imageOrdinal = config.imageOrdinal;
        pair.moveSet = &moveSet;
        pair.meshConfigIndex = static_cast<std::uint8_t>(i);
        pair.owner = owner;
        sink.OnPair(pair);
    }
}
}

namespace MeshPreviewDetail {

/** Hand a pair of asset refs to the sink. Two refs, so two packs. */
void EmitAssetRefs(const CGameAssetRef &meshRef, const CGameAssetRef &imageRef,
                   const std::string &owner, IMeshPairSink &sink) {
    MeshPair pair;
    pair.meshPackHash = meshRef.packHash;
    pair.meshOrdinal = static_cast<std::uint32_t>(meshRef.assetId);
    pair.imagePackHash = imageRef.packHash;
    pair.imageOrdinal = static_cast<std::uint32_t>(imageRef.assetId);
    pair.moveSet = nullptr;
    pair.meshConfigIndex = 0;
    pair.owner = owner;
    sink.OnPair(pair);
}
}

namespace MeshPreviewDetail {

/** Fetch one template resource. Returns false when the section is empty. */
bool ReadTemplate(CResPackTOC &pack, CGameObjectPack &objectPack,
                  GameSection section, std::uint32_t ordinal,
                  std::vector<std::uint8_t> &payload) {
    return pack.GetResource(objectPack.GetHandle(section, ordinal), payload);
}
}

namespace MeshPreviewDetail {

/** Player templates: parsed whole, so their leftover count means something. */
void WalkPlayers(CResPackTOC &pack, PackTables &tables, int packIndex,
                 IMeshPairSink &sink) {
    CGameObjectPack &objectPack = tables.GetObjectPack(packIndex);
    const std::uint32_t count = objectPack.GetObjectCount(GameSection::Player);

    for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
        std::vector<std::uint8_t> payload;
        if (!ReadTemplate(pack, objectPack, GameSection::Player, ordinal, payload)) {
            continue;
        }

        const std::string owner = OwnerLabel(pack, "player", ordinal);
        CArrayInputStream stream(payload);
        CBrother::Template brother;
        if (!brother.Init(stream)) {
            sink.OnTemplateFailed(owner);
            continue;
        }

        EmitMoveSet(brother.GetMoveSet(), owner, sink);
        if (stream.Available() != 0) {
            sink.OnLeftoverBytes(owner, stream.Available());
        }
    }
}
}

namespace MeshPreviewDetail {

/**
 * Enemy templates, read only as far as their move set.
 *
 * CEnemy::Template::Init (:67174) opens with a flag byte, a CGameAssetRef and
 * a CScript, and the move set is the fourth thing it reads. Everything after
 * it needs CCollisionData, which belongs to M4a, so this stops there -- and
 * therefore cannot check for leftover bytes. A survey shortcut, not a CEnemy
 * port: that class stays unwritten until its own milestone.
 */
// The historical partial-reader notes above are superseded by ReadEnemyTemplate.
void WalkEnemies(CResPackTOC &pack, PackTables &tables, int packIndex,
                 IMeshPairSink &sink) {
    CGameObjectPack &objectPack = tables.GetObjectPack(packIndex);
    const std::uint32_t count = objectPack.GetObjectCount(GameSection::Enemy);

    for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
        const std::string owner = OwnerLabel(pack, "enemy", ordinal);
        EnemyTemplateData entry;
        if (!ReadEnemyTemplate(tables, pack.GetPackHash(), ordinal, owner, entry)) {
            sink.OnTemplateFailed(owner);
            continue;
        }
        EmitMoveSet(entry.moveSet, owner, sink);
    }
}
}

namespace MeshPreviewDetail {

/** Gun templates: a move set at the end, plus the weapon's own model. */
void WalkGuns(CResPackTOC &pack, PackTables &tables, int packIndex,
              IMeshPairSink &sink) {
    CGameObjectPack &objectPack = tables.GetObjectPack(packIndex);
    const std::uint32_t count = objectPack.GetObjectCount(GameSection::Gun);

    for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
        std::vector<std::uint8_t> payload;
        if (!ReadTemplate(pack, objectPack, GameSection::Gun, ordinal, payload)) {
            continue;
        }

        const std::string owner = OwnerLabel(pack, "gun", ordinal);
        CArrayInputStream stream(payload);
        CGun::Template gun;
        if (!gun.Init(stream)) {
            sink.OnTemplateFailed(owner);
            continue;
        }

        EmitAssetRefs(gun.GetMeshRef(), gun.GetImageRef(), owner, sink);
        EmitMoveSet(gun.GetMoveSet(), owner, sink);
        if (stream.Available() != 0) {
            sink.OnLeftoverBytes(owner, stream.Available());
        }
    }
}
}

namespace MeshPreviewDetail {

/** Bullet templates: a pair of asset refs, and most name neither. */
void WalkBullets(CResPackTOC &pack, PackTables &tables, int packIndex,
                 IMeshPairSink &sink) {
    CGameObjectPack &objectPack = tables.GetObjectPack(packIndex);
    const std::uint32_t count = objectPack.GetObjectCount(GameSection::Bullet);

    for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
        std::vector<std::uint8_t> payload;
        if (!ReadTemplate(pack, objectPack, GameSection::Bullet, ordinal, payload)) {
            continue;
        }

        const std::string owner = OwnerLabel(pack, "bullet", ordinal);
        CArrayInputStream stream(payload);
        CBullet::Template bullet;
        if (!bullet.Init(stream)) {
            sink.OnTemplateFailed(owner);
            continue;
        }

        if (bullet.HasMesh() && bullet.HasImage()) {
            EmitAssetRefs(bullet.GetMeshRef(), bullet.GetImageRef(), owner, sink);
        }
        if (stream.Available() != 0) {
            sink.OnLeftoverBytes(owner, stream.Available());
        }
    }
}
}

namespace MeshPreviewDetail {

/** Armour templates: one model per brother, either of which may be absent. */
void WalkArmor(CResPackTOC &pack, PackTables &tables, int packIndex,
               IMeshPairSink &sink) {
    CGameObjectPack &objectPack = tables.GetObjectPack(packIndex);
    const std::uint32_t count = objectPack.GetObjectCount(GameSection::Armor);

    for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
        std::vector<std::uint8_t> payload;
        if (!ReadTemplate(pack, objectPack, GameSection::Armor, ordinal, payload)) {
            continue;
        }

        const std::string owner = OwnerLabel(pack, "armor", ordinal);
        CArrayInputStream stream(payload);
        CArmor::Template armor;
        if (!armor.Init(stream)) {
            sink.OnTemplateFailed(owner);
            continue;
        }

        for (std::uint32_t variant = 0; variant < kArmorVariantCount; ++variant) {
            if (!armor.HasMesh(variant)) {
                continue;
            }
            EmitAssetRefs(armor.GetMeshRef(variant), armor.GetImageRef(variant),
                          owner, sink);
        }

        if (stream.Available() != 0) {
            sink.OnLeftoverBytes(owner, stream.Available());
        }
    }
}
}

namespace MeshPreviewDetail {

/**
 * Every model/atlas pair in the archives.
 *
 * Five template types own the 334 models between them. Players, enemies and
 * guns go through a move set; guns, bullets and armour also name models with
 * plain asset refs. Nothing else in the engine addresses section 31 --
 * CMoveSetMesh::LoadMesh (:123157) and three GetResId(0x1E, ...) call sites
 * are the whole list.
 */
void WalkMeshPairs(CResTOCManager &tocManager, PackTables &tables,
                   IMeshPairSink &sink) {
    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        CResPackTOC *pack = tocManager.GetPack(static_cast<int>(i));
        const int packIndex = static_cast<int>(i);

        WalkPlayers(*pack, tables, packIndex, sink);
        WalkEnemies(*pack, tables, packIndex, sink);
        WalkGuns(*pack, tables, packIndex, sink);
        WalkBullets(*pack, tables, packIndex, sink);
        WalkArmor(*pack, tables, packIndex, sink);
    }
}
}

namespace MeshPreviewDetail {

// ---------------------------------------------------------------------------
// --movesets: follow every pair to a real resource
// ---------------------------------------------------------------------------

/** Pixel size out of a PNG's IHDR, which starts at a fixed offset. */
void ReadPngSize(const std::vector<std::uint8_t> &payload, std::uint32_t &width,
                 std::uint32_t &height) {
    width = 0;
    height = 0;
    if (payload.size() < 24) {
        return;
    }

    // Big-endian, like everything inside a PNG chunk header.
    width = (payload[16] << 24) | (payload[17] << 16) | (payload[18] << 8) | payload[19];
    height = (payload[20] << 24) | (payload[21] << 16) | (payload[22] << 8) | payload[23];
}
}

namespace MeshPreviewDetail {

/**
 * Who owns the models, per pack.
 *
 * The last column is section 31's size; the rest are the template types that
 * point into it.
 */
void PrintTemplateCounts(CResTOCManager &tocManager, PackTables &tables) {
    std::printf("\n%-12s %8s %8s %6s %8s %8s %8s\n", "pack", "players",
                "enemies", "guns", "bullets", "armour", "meshes");
    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        CResPackTOC *pack = tocManager.GetPack(static_cast<int>(i));
        CGameObjectPack &objectPack = tables.GetObjectPack(static_cast<int>(i));

        const std::uint32_t meshes = objectPack.GetSectionSpan(GameSection::Mesh);
        if (meshes == 0) {
            continue;
        }

        std::printf("%-12s %8u %8u %6u %8u %8u %8u\n",
                    pack->GetShortName().c_str(),
                    objectPack.GetObjectCount(GameSection::Player),
                    objectPack.GetObjectCount(GameSection::Enemy),
                    objectPack.GetObjectCount(GameSection::Gun),
                    objectPack.GetObjectCount(GameSection::Bullet),
                    objectPack.GetObjectCount(GameSection::Armor), meshes);
    }
}
}

namespace MeshPreviewDetail {

/**
 * Bind the move set and pick out the moves that drive this model.
 *
 * The controller wants the whole config array because a move can switch which
 * mesh is shown; here only one config is loaded, so the rest stay null and the
 * moves naming them are filtered out instead of failing at SetMove.
 */
void BindMoveSet(const CatalogEntry &entry, LoadedModel &model) {
    model.configMeshes.assign(entry.moveSet.GetMeshConfigs().size(), nullptr);
    if (entry.meshConfigIndex < model.configMeshes.size()) {
        model.configMeshes[entry.meshConfigIndex] = &model.mesh;
    }

    model.moves.clear();
    for (std::size_t i = 0; i < entry.moveSet.GetMoves().size(); ++i) {
        if (entry.moveSet.GetMoves()[i].meshConfigIndex == entry.meshConfigIndex) {
            model.moves.push_back(static_cast<std::int32_t>(i));
        }
    }

    model.moveSlot = 0;
    model.controller.SetMoveSet(&entry.moveSet, model.configMeshes);
    if (!model.moves.empty()) {
        model.controller.SetMove(model.moves[0]);
    }
}
}

namespace MeshPreviewDetail {

/**
 * Put the current pose in the vertex buffer.
 *
 * A model with a move set is asked where it is right now; one without shows
 * the still frame the command line picked. Either way the buffer keeps its
 * last contents if the evaluator has nothing -- better a held pose than a
 * model that blinks out.
 */
void UploadPose(LoadedModel &model, CMeshBuffer &buffer,
                std::uint32_t stillFrameIndex) {
    if (model.rawAnimation.Evaluate(model.pose)) {
        buffer.SetVertices(model.pose);
    }
    (void)stillFrameIndex;
}
}

namespace MeshPreviewDetail {

/**
 * Run the animation on before the first frame is drawn.
 *
 * Stepped rather than handed the whole span at once, because the move's speed
 * is rounded to whole milliseconds every update -- one big step and one
 * hundred small ones do not land in the same place, and it is the small ones
 * that match what a running viewer does.
 */
void WarmUp(LoadedModel &model, std::uint32_t advanceMs) {
    for (std::uint32_t elapsed = 0; elapsed < advanceMs; elapsed += kWarmUpFrameMs) {
        model.rawAnimation.Update(kWarmUpFrameMs);
    }
}
}

namespace MeshPreviewDetail {

/** "move 3 of 18 -- frames 40..79, speed 1.00" */
void ReportMove(const CatalogEntry &entry, const LoadedModel &model) {
    if (model.moves.empty()) {
        std::printf("[mesh] no move drives this model; showing frame 0\n");
        return;
    }

    const std::int32_t moveIndex = model.moves[model.moveSlot];
    const MeshMove &move = entry.moveSet.GetMoves()[moveIndex];
    std::printf("[mesh] move %d (%zu of %zu for this mesh) -- frames %u..%u, "
                "%d ms, speed %.2f%s\n",
                moveIndex, model.moveSlot + 1, model.moves.size(),
                move.firstFrame, move.lastFrame,
                model.controller.GetAnimation().GetRangeDurationMs(), move.speed,
                move.restartsWhenRepeated != 0 ? ", restarts" : "");
}
}

namespace MeshPreviewDetail {

/** Fetch and decode one catalogue entry. */
bool LoadModel(PackTables &tables, const CatalogEntry &entry, LoadedModel &out) {
    if (entry.imageOrdinal == UINT32_MAX) {
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(entry.meshPackHash, GameSection::Mesh, entry.meshOrdinal, payload)) { return false; }
        CArrayInputStream stream(payload);
        if (!out.mesh.Init(stream)) { return false; }
        // Neutral inspection material; explicitly labeled as untextured, not a game asset.
        PNGImage neutral;
        neutral.width = 1;
        neutral.height = 1;
        neutral.pixels.assign(4, 255);
        if (!out.texture.Create(neutral, GL_REPEAT)) { return false; }
    } else if (!LoadMeshAndAtlas(tables, entry.owner.c_str(), entry.meshPackHash,
                   entry.meshOrdinal, entry.imagePackHash, entry.imageOrdinal, out.mesh, out.texture)) {
        return false;
    }
    if (out.mesh.GetFrames().empty()) { return false; }
    out.rawAnimation.SetMesh(&out.mesh);
    out.rawAnimation.SetRange(0, static_cast<std::int32_t>(out.mesh.GetFrames().size() - 1));
    out.rawAnimation.SetLooped(true);
    std::printf("[mesh] raw frames 0..%zu; %s\n", out.mesh.GetFrames().size() - 1, entry.owner.c_str());
    return true;
}
}

namespace MeshPreviewDetail {

/**
 * Model to clip space, following CMeshCamera.
 *
 * The engine's order, read off OrientForUI (:98863), is Rotate(90, X) then
 * Rotate(180 + facing, Z). Its Rotate post-multiplies, so the vertex meets the
 * spin first and the tilt second: a model turns on its own feet and is then
 * stood up. Doing it the other way round tumbles it sideways.
 *
 * What is ours rather than the engine's: the model is pushed to the origin by
 * its own centre and scaled by its own inverse extent -- both worked out by
 * ComputeBounds -- so every model arrives the same size on screen. The engine
 * instead translates to a world position and scales by whatever the caller
 * passes.
 */
void BuildModelViewProjection(const MeshBounds &bounds, const Turntable &view,
                              int drawableWidth, int drawableHeight, float *out) {
    float centre[kMatrix4dElements];
    Matrix4dTranslation(-bounds.centerX, -bounds.centerY, -bounds.centerZ, centre);

    float normalise[kMatrix4dElements];
    Matrix4dScale(bounds.inverseExtent, normalise);

    float spin[kMatrix4dElements];
    Matrix4dRotationZ(view.spinDegrees * kDegreesToRadians, spin);

    float tilt[kMatrix4dElements];
    Matrix4dRotationX((view.tiltDegrees + view.extraTilt) * kDegreesToRadians, tilt);

    float normalised[kMatrix4dElements];
    Matrix4dMultiply(normalise, centre, normalised);

    float spun[kMatrix4dElements];
    Matrix4dMultiply(tilt, spin, spun);

    float model[kMatrix4dElements];
    Matrix4dMultiply(spun, normalised, model);

    const float zoom = view.zoom;

    // The normalised model is one unit across, so the view is sized in units.
    const float shortSide = static_cast<float>(
        drawableWidth < drawableHeight ? drawableWidth : drawableHeight);
    const float unitsPerScreen = 1.0f / (kModelScreenFraction * zoom);
    const float viewWidth = unitsPerScreen * static_cast<float>(drawableWidth) / shortSide;
    const float viewHeight = unitsPerScreen * static_cast<float>(drawableHeight) / shortSide;

    float projection[kMatrix4dElements];
    Matrix4dOrthoCentred(viewWidth, viewHeight, kDepthMargin, projection);

    Matrix4dMultiply(projection, model, out);
}
}

namespace MeshPreviewDetail {

/** Assemble the player, with one of the catalogue's guns in his hand. */
bool BuildViewerCharacter(PackTables &tables, const CharacterSink &catalog,
                          std::size_t gunSlot, PlayerModel &out) {
    if (!BuildPlayerBody(tables, catalog.GetPlayerMoveSet(), out)) {
        return false;
    }

    const GunEntry &entry = catalog.GetGuns()[gunSlot];
    return AttachPlayerGun(tables, entry.owner, entry.meshPackHash,
                           entry.meshOrdinal, entry.imagePackHash,
                           entry.imageOrdinal, out);
}
}
  // namespace

int RunMeshSurvey(const std::string &bigDirectory) {
    CResTOCManager tocManager;
    if (!tocManager.InitAuto(bigDirectory) || !tocManager.Bind()) {
        return 1;
    }

    std::printf("\n=== Mesh: Section 31 meshes ===\n");

    MeshSurveyTotals totals;
    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        SurveyPack(*tocManager.GetPack(static_cast<int>(i)), totals);
    }

    std::printf("\n%u meshes, %u empty placeholders, %u unreadable, "
                "%u with bytes left over\n",
                totals.meshes, totals.placeholders, totals.unreadable,
                totals.withLeftovers);

    if (totals.unreadable > 0 || totals.withLeftovers > 0) {
        return 1;
    }
    return 0;
}

int RunMoveSetSurvey(const std::string &bigDirectory) {
    CResTOCManager tocManager;
    if (!tocManager.InitAuto(bigDirectory) || !tocManager.Bind()) {
        return 1;
    }

    std::printf("\n=== Mesh: move sets, mesh to atlas ===\n");

    PackTables tables(tocManager);
    ReportingSink sink(tables);
    WalkMeshPairs(tocManager, tables, sink);

    PrintTemplateCounts(tocManager, tables);
    sink.PrintTotals();

    if (!sink.Passed()) {
        return 1;
    }
    return 0;
}

int RunMeshPreview(const std::string &bigDirectory, std::uint32_t startIndex,
               float spinDegrees, std::uint32_t frameIndex,
               const std::string &screenshotPath, std::uint32_t advanceMs) {
    std::printf("=== Mesh: a model on screen ===\n\n");

    CResTOCManager tocManager;
    if (!tocManager.InitAuto(bigDirectory) || !tocManager.Bind()) {
        return 1;
    }

    PackTables tables(tocManager);
    CatalogSink catalog;
    WalkMeshPairs(tocManager, tables, catalog);
    catalog.AddRawMeshes(tocManager, tables);
    if (catalog.GetEntries().empty()) {
        std::printf("[mesh] no template names a model\n");
        return 1;
    }
    std::printf("\n[mesh] %zu models in the catalogue\n",
                catalog.GetEntries().size());

    std::size_t slot = startIndex;
    if (slot >= catalog.GetEntries().size()) {
        std::printf("[mesh] index out of range\n"); return 1;
    }

    CWindow window;
    if (!OpenViewerWindow(window, "Mesh")) {
        return 1;
    }
    ViewerControls controls(window, meshview::Bindings);
    if (!controls.Init()) { return 1; }


    CShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) {
        return 1;
    }

    CMeshBuffer buffer;
    if (!buffer.Create(program)) {
        return 1;
    }

    // Held by pointer so a model can be swapped in wholesale: a CTexture
    // owns a GL name and is neither copyable nor movable.
    std::unique_ptr<LoadedModel> model(new LoadedModel());
    if (!LoadModel(tables, catalog.GetEntries()[slot], *model)) {
        return 1;
    }
    if (!buffer.SetMesh(model->mesh)) {
        return 1;
    }
    if (frameIndex >= model->mesh.GetFrames().size()) { std::printf("[mesh] frame index out of range\n"); return 1; }
    model->rawAnimation.SetFrame(frameIndex);
    WarmUp(*model, advanceMs);
    UploadPose(*model, buffer, frameIndex);

    // Depth, because a model is solid: without this the far side of it draws
    // over the near side wherever the strip happens to arrive later.
    glEnable(GL_DEPTH_TEST);

    // Meshes carry alpha: the turret's ground shadow is a faded disc, and
    // without this it draws as a white plate.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Turntable view;
    view.spinDegrees = kUiFacingDegrees + spinDegrees;
    view.tiltDegrees = kUiTiltDegrees;
    view.extraTilt = 0.0f;
    view.zoom = 1.0f;

    // Input help is generated by ViewerControls from ViewerBindings.h.

    std::uint64_t previousTicks = window.GetTicksMs();
    bool paused = false;
    bool singleStep = false;

    bool reportedFirstFrame = false;
    while (controls.PumpEvents()) {
        int drawableWidth = 0;
        int drawableHeight = 0;
        controls.GetDrawableSize(drawableWidth, drawableHeight);

        // --- walking the catalogue ---
        const std::size_t previousSlot = slot;
        bool frameChanged = false;
        for (KeyCode key = controls.TakeKeyPress(); key != KeyCode::None;
             key = controls.TakeKeyPress()) {
            const std::size_t count = catalog.GetEntries().size();
            if (controls.IsPressed(key, ViewerAction::Next)) {
                slot = (slot + 1) % count;
            } else if (controls.IsPressed(key, ViewerAction::Previous)) {
                slot = (slot + count - 1) % count;
            } else if (controls.IsPressed(key, ViewerAction::NextPage)) {
                slot = (slot + 10) % count;
            } else if (controls.IsPressed(key, ViewerAction::PreviousPage)) {
                slot = (slot + count - 10) % count;
            } else if (controls.IsPressed(key, ViewerAction::NextVariant) || controls.IsPressed(key, ViewerAction::PreviousVariant)) {
                const auto &frames = model->mesh.GetFrames();
                std::size_t currentFrame = 0;
                for (std::size_t index = 0; index < frames.size(); ++index) {
                    if (frames[index].timeMs > model->rawAnimation.GetTimeMs()) { break; }
                    currentFrame = index;
                }
                if (controls.IsPressed(key, ViewerAction::NextVariant)) { currentFrame = (currentFrame + 1) % frames.size(); }
                else { currentFrame = (currentFrame + frames.size() - 1) % frames.size(); }
                frameIndex = static_cast<std::uint32_t>(currentFrame);
                model->rawAnimation.SetFrame(frameIndex);
                paused = true;
                frameChanged = true;
            } else if (controls.IsPressed(key, ViewerAction::Pause)) {
                paused = !paused;
                std::printf("[mesh] %s\n", paused ? "paused" : "playing");
            } else if (controls.IsPressed(key, ViewerAction::Step)) {
                singleStep = true;
            } else if (controls.IsPressed(key, ViewerAction::Tilt)) {
                // The 30-degree lean the game plays at, against the 90 the
                // menus stand a model up with.
                if (view.tiltDegrees == kUiTiltDegrees) {
                    view.tiltDegrees = kGameTiltDegrees;
                } else {
                    view.tiltDegrees = kUiTiltDegrees;
                }
                std::printf("[mesh] tilt %.0f degrees\n", view.tiltDegrees);
            } else if (controls.IsPressed(key, ViewerAction::ResetView)) {
                view.spinDegrees = kUiFacingDegrees + spinDegrees;
                view.extraTilt = 0.0f;
                view.zoom = 1.0f;
            }
        }

        if (slot != previousSlot) {
            std::printf("\n[mesh] --- model %zu of %zu ---\n", slot + 1,
                        catalog.GetEntries().size());

            std::unique_ptr<LoadedModel> replacement(new LoadedModel());
            if (LoadModel(tables, catalog.GetEntries()[slot], *replacement)) {
                model = std::move(replacement);
                frameIndex = 0;
                buffer.SetMesh(model->mesh);
                WarmUp(*model, advanceMs);
                UploadPose(*model, buffer, frameIndex);
            } else {
                // A model that will not load leaves the previous one on screen
                // rather than a blank window.
                std::printf("[mesh] staying on the previous model\n");
                slot = previousSlot;
            }
        } else if (frameChanged) {
            // Explicit frame stepping pauses the shared animation clock.
            UploadPose(*model, buffer, frameIndex);
            std::printf("[mesh] frame %u at %d ms\n", frameIndex, model->rawAnimation.GetTimeMs());
        }

        const CatalogEntry &selected = catalog.GetEntries()[slot];
        window.SetTitle(ViewerWindowTitle("Mesh", std::to_string(slot) + " | " + tables.GetPackName(selected.meshPackHash) +
            " mesh " + std::to_string(selected.meshOrdinal) + " | " + selected.owner));

        // --- animation clock ---
        const std::uint64_t nowTicks = window.GetTicksMs();
        std::uint64_t elapsedMs = nowTicks - previousTicks;
        previousTicks = nowTicks;

        if (elapsedMs > kMaxFrameMs) {
            elapsedMs = kMaxFrameMs;
        }
        if (paused) {
            elapsedMs = 0;
        }
        if (singleStep) {
            elapsedMs = static_cast<std::uint64_t>(kSingleStepMs);
            singleStep = false;
        }

        if (!screenshotPath.empty()) { elapsedMs = 0; }
        if (elapsedMs > 0) {
            model->rawAnimation.Update(static_cast<std::int32_t>(elapsedMs));
            UploadPose(*model, buffer, frameIndex);
        }

        // --- turntable ---
        int dragX = 0;
        int dragY = 0;
        controls.TakeDragDelta(dragX, dragY);
        view.spinDegrees += static_cast<float>(dragX) * kDragToDegrees;
        view.extraTilt += static_cast<float>(dragY) * kDragToDegrees;

        const float wheel = controls.TakeWheelDelta();
        for (float notch = 0.0f; notch < wheel; notch += 1.0f) {
            view.zoom *= kZoomPerNotch;
        }
        for (float notch = 0.0f; notch > wheel; notch -= 1.0f) {
            view.zoom /= kZoomPerNotch;
        }
        if (view.zoom < kMinZoom) {
            view.zoom = kMinZoom;
        }
        if (view.zoom > kMaxZoom) {
            view.zoom = kMaxZoom;
        }

        glViewport(0, 0, drawableWidth, drawableHeight);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float mvp[kMatrix4dElements];
        BuildModelViewProjection(model->mesh.GetBounds(), view, drawableWidth,
                                 drawableHeight, mvp);
        buffer.Draw(program, mvp, model->texture);

        if (!controls.Draw()) { return 1; }

        if (!reportedFirstFrame) {
            GLCheckErrors("first frame");
            reportedFirstFrame = true;

            if (!screenshotPath.empty()) {
                if (!GB_SAVE_FRAME(window, screenshotPath)) {
                    return 1;
                }
                window.Present();
                break;
            }
        }

        window.Present();
    }

    std::printf("[mesh] done\n");
    return 0;
}

int RunWeaponSurvey(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.InitAuto(bigDirectory) || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    std::vector<WeaponEntry> weapons;
    if (!LoadWeaponCatalog(toc, tables, weapons)) { return 1; }
    for (std::size_t i = 0; i < weapons.size(); ++i) {
        const WeaponEntry &entry = weapons[i];
        CGun gun;
        gun.Bind(entry.data, nullptr);
        gun.OnEquip();
        std::printf("[weapon] %zu %s category=%d hand=%u interval=%u %s overrides:",
            i, entry.owner.c_str(), entry.category, entry.data.GetHandedness(),
            entry.data.GetFireIntervalMs(), entry.name.c_str());
        for (int move : gun.GetOverrides()) { std::printf(" %d", move); }
        std::printf("\n");
        if (entry.data.GetBulletRef().IsNull()) {
            std::printf("  default bullet: null; script resources:");
            for (const ScriptResourceRef &ref : entry.data.GetScript().GetResources()) {
                std::printf(" %08x:%u:%u", ref.packHash, ref.sectionOrType, ref.resourceId);
            }
            std::printf("\n");
        }
        for (const MeshConfig &config : entry.data.GetMoveSet().GetMeshConfigs()) {
            std::printf("  config mesh=%u atlas=%u pack=%08x\n", config.meshOrdinal,
                config.imageOrdinal, entry.data.GetMoveSet().GetPackHash());
        }
    }
    std::printf("[weapons] %zu templates parsed\n", weapons.size());
    return 0;
}

