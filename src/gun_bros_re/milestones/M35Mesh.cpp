/**
 * @file M35Mesh.cpp
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
#include "milestones/M35Mesh.h"

#include "runtime/PackTables.h"
#include "runtime/PlayerModel.h"
#include "runtime/ArmorCatalog.h"
#include "runtime/WeaponCatalog.h"
#include "runtime/StoreCatalog.h"
#include "runtime/CombatGeometry.h"
#include "gun_bros/WeaponEffects.h"
#include "gun_bros/CParticleEffect.h"

#include "engine/CArrayInputStream.h"
#include "engine/CMatrix4d.h"
#include "engine/CMeshBuffer.h"
#include "engine/CPNG.h"
#include "engine/CShaderProgram.h"
#include "engine/CTexture.h"
#include "engine/platform/CWindow.h"
#include "engine/platform/GLLoader.h"
#include "glu_script/CScript.h"
#include "gun_bros/CArmor.h"
#include "gun_bros/CBrother.h"
#include "gun_bros/CBullet.h"
#include "gun_bros/CGameAssetRef.h"
#include "gun_bros/CGameObjectPack.h"
#include "gun_bros/CGun.h"
#include "gun_bros/CMesh.h"
#include "gun_bros/CMeshAnimationController.h"
#include "gun_bros/CMeshCamera.h"
#include "gun_bros/CMoveSetMesh.h"
#include "gun_bros/CMoveSetMeshController.h"
#include "gun_bros/CResTOCManager.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

// Where the ported shaders live; the same pair M2 and M3 use.
const char *const kShaderDirectory = ASSET_ROOT "/src/gun_bros_re/shaders";

// The eight bytes every PNG starts with.
const std::uint8_t kPngSignature[8] = {0x89, 0x50, 0x4E, 0x47,
                                       0x0D, 0x0A, 0x1A, 0x0A};

// Viewer: how much of the window's short side the model fills at zoom 1.
constexpr float kModelScreenFraction = 0.7f;

// Viewer: mouse pixels to degrees, and notches to zoom.
constexpr float kDragToDegrees = 0.5f;
constexpr float kZoomPerNotch = 1.15f;
constexpr float kMinZoom = 0.1f;
constexpr float kMaxZoom = 20.0f;

// Viewer: the ortho box is this many model widths deep, so nothing clips.
constexpr float kDepthMargin = 4.0f;

// Viewer: the longest step the animation clock will take in one frame. A
// window dragged around or a debugger break otherwise hands the move set a
// gap of seconds, which reads as a skip rather than as slow motion.
constexpr std::uint64_t kMaxFrameMs = 100;

// Viewer: how far the period key nudges the clock while paused.
constexpr std::int32_t kSingleStepMs = 33;

// Viewer: the step --advance runs the animation on in. Same bite size as M3's
// warm-up, so an animation lands in the same place either way.
constexpr std::int32_t kWarmUpFrameMs = 16;

/**
 * The two tilts the engine uses, copied from CMeshCamera.
 *
 * Models are authored z-up. OrientForUI (:98863) stands one up for the menus
 * with Rotate(90, X) and spins it about Z; OrientForGame (:98959) leans one
 * only 30 degrees, because the game is played from above and that lean is all
 * you get to see of a character's front. The viewer offers both: the UI tilt
 * to look at a model, the game tilt to see what the player sees.
 */
constexpr float kDegreesToRadians = 3.14159265f / 180.0f;
constexpr float kUiTiltDegrees = 90.0f;
constexpr float kGameTiltDegrees = 30.0f;

// OrientForUI follows its 90-degree tilt with Rotate(180, Z), which turns the
// model to face the viewer. Copied as the spin's starting point.
constexpr float kUiFacingDegrees = 180.0f;

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

/** What the mesh survey adds up to. */
struct MeshSurveyTotals {
    unsigned meshes = 0;
    unsigned placeholders = 0;
    unsigned unreadable = 0;
    unsigned withLeftovers = 0;
};

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

// ---------------------------------------------------------------------------
// Who owns the models: one walk, two consumers
// ---------------------------------------------------------------------------

/** One model and the atlas it wears, as some template names them. */
struct MeshPair {
    std::uint32_t meshPackHash;
    std::uint32_t meshOrdinal;
    std::uint32_t imagePackHash;
    std::uint32_t imageOrdinal;

    // The move set that decides which frames get loaded, or null for the
    // owners that have none. CMesh::Init takes the same argument.
    const CMoveSetMesh *moveSet;

    // Which of that set's mesh configs this pair is. Moves name a config, so
    // this is what says which moves drive this particular model.
    std::uint8_t meshConfigIndex;

    // "pack1 enemy 3", for the log and the viewer's title line.
    std::string owner;
};

/** What to do with each pair the walk turns up. */
class IMeshPairSink {
public:
    virtual ~IMeshPairSink() {}
    virtual void OnPair(const MeshPair &pair) = 0;

    /** Called once per template that failed to parse. */
    virtual void OnTemplateFailed(const std::string &owner) = 0;

    /** Called once per template that parsed but had bytes left over. */
    virtual void OnLeftoverBytes(const std::string &owner, std::size_t bytes) = 0;
};

/** "pack1 enemy 3" */
std::string OwnerLabel(const CResPackTOC &pack, const char *kind,
                       std::uint32_t ordinal) {
    char text[128];
    std::snprintf(text, sizeof(text), "%s %s %u", pack.GetShortName().c_str(), kind,
                  ordinal);
    return std::string(text);
}

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

/** Fetch one template resource. Returns false when the section is empty. */
bool ReadTemplate(CResPackTOC &pack, CGameObjectPack &objectPack,
                  GameSection section, std::uint32_t ordinal,
                  std::vector<std::uint8_t> &payload) {
    return pack.GetResource(objectPack.GetHandle(section, ordinal), payload);
}

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

/**
 * Enemy templates, read only as far as their move set.
 *
 * CEnemy::Template::Init (:67174) opens with a flag byte, a CGameAssetRef and
 * a CScript, and the move set is the fourth thing it reads. Everything after
 * it needs CCollisionData, which belongs to M4a, so this stops there -- and
 * therefore cannot check for leftover bytes. A survey shortcut, not a CEnemy
 * port: that class stays unwritten until its own milestone.
 */
void WalkEnemies(CResPackTOC &pack, PackTables &tables, int packIndex,
                 IMeshPairSink &sink) {
    CGameObjectPack &objectPack = tables.GetObjectPack(packIndex);
    const std::uint32_t count = objectPack.GetObjectCount(GameSection::Enemy);

    for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
        std::vector<std::uint8_t> payload;
        if (!ReadTemplate(pack, objectPack, GameSection::Enemy, ordinal, payload)) {
            continue;
        }

        const std::string owner = OwnerLabel(pack, "enemy", ordinal);
        CArrayInputStream stream(payload);
        stream.ReadUInt8();

        CGameAssetRef assetRef;
        assetRef.Init(stream);

        CScript script;
        script.Load(stream);

        CMoveSetMesh moveSet;
        if (!moveSet.Init(stream)) {
            sink.OnTemplateFailed(owner);
            continue;
        }

        EmitMoveSet(moveSet, owner, sink);
    }
}

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

/** Resolves each pair and prints what it landed on. */
class ReportingSink : public IMeshPairSink {
public:
    explicit ReportingSink(PackTables &tables) : m_tables(tables) {}

    void OnPair(const MeshPair &pair) override {
        m_pairs++;
        if (pair.owner != m_lastOwner) {
            std::printf("\n  %s\n", pair.owner.c_str());
            m_lastOwner = pair.owner;
        }

        std::printf("      %s mesh %3u -> %s png %3u",
                    m_tables.GetPackName(pair.meshPackHash).c_str(), pair.meshOrdinal,
                    m_tables.GetPackName(pair.imagePackHash).c_str(),
                    pair.imageOrdinal);

        std::vector<std::uint8_t> meshPayload;
        const bool meshRead = m_tables.ReadSectionResource(
            pair.meshPackHash, GameSection::Mesh, pair.meshOrdinal, meshPayload);

        CMesh mesh;
        bool meshParsed = false;
        if (meshRead && !meshPayload.empty()) {
            CArrayInputStream meshStream(meshPayload);
            meshParsed = mesh.Init(meshStream);
        }

        if (!meshParsed) {
            std::printf("   MESH UNRESOLVED\n");
            m_meshFailures++;
            return;
        }

        std::printf("  %5u verts %4zu frames", mesh.GetVertexCount(),
                    mesh.GetFrames().size());

        // The original asks the move set which frames to keep, and asks it per
        // frame index without caring which config the move belongs to. Copied
        // as-is; the count is what M4b will actually allocate.
        if (pair.moveSet != nullptr) {
            unsigned used = 0;
            for (std::size_t frame = 0; frame < mesh.GetFrames().size(); ++frame) {
                if (pair.moveSet->IsFrameUsedInMoves(static_cast<std::uint16_t>(frame))) {
                    used++;
                }
            }
            std::printf(" (%u used)", used);
        }

        std::vector<std::uint8_t> imagePayload;
        const bool imageRead = m_tables.ReadSectionResource(
            pair.imagePackHash, GameSection::Png, pair.imageOrdinal, imagePayload);

        bool isPng = false;
        if (imageRead && imagePayload.size() >= sizeof(kPngSignature)) {
            isPng = std::memcmp(imagePayload.data(), kPngSignature,
                                sizeof(kPngSignature)) == 0;
        }

        if (!isPng) {
            std::printf("   ATLAS UNRESOLVED\n");
            m_imageFailures++;
            return;
        }

        std::uint32_t width = 0;
        std::uint32_t height = 0;
        ReadPngSize(imagePayload, width, height);
        std::printf("   atlas %ux%u\n", width, height);
    }

    void OnTemplateFailed(const std::string &owner) override {
        std::printf("  %s: parse failed\n", owner.c_str());
        m_templateFailures++;
    }

    void OnLeftoverBytes(const std::string &owner, std::size_t bytes) override {
        std::printf("  %s: %zu bytes left over\n", owner.c_str(), bytes);
        m_withLeftovers++;
    }

    void PrintTotals() const {
        std::printf("\n%u mesh/atlas pairs, %u meshes unresolved, "
                    "%u atlases unresolved, %u templates unparsed, "
                    "%u with bytes left over\n",
                    m_pairs, m_meshFailures, m_imageFailures, m_templateFailures,
                    m_withLeftovers);
    }

    bool Passed() const {
        return m_meshFailures == 0 && m_imageFailures == 0 &&
               m_templateFailures == 0 && m_withLeftovers == 0;
    }

private:
    PackTables &m_tables;
    std::string m_lastOwner;
    unsigned m_pairs = 0;
    unsigned m_meshFailures = 0;
    unsigned m_imageFailures = 0;
    unsigned m_templateFailures = 0;
    unsigned m_withLeftovers = 0;
};

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

// ---------------------------------------------------------------------------
// The viewer
// ---------------------------------------------------------------------------

/** One entry of the catalogue the viewer walks. */
struct CatalogEntry {
    std::uint32_t meshPackHash;
    std::uint32_t meshOrdinal;
    std::uint32_t imagePackHash;
    std::uint32_t imageOrdinal;
    std::string owner;

    // The move set is COPIED, not pointed at: the one the walk turned up is a
    // local of whichever Walk function parsed the template and is gone by the
    // time the viewer opens a window. Empty for the models named by a plain
    // asset ref -- those have no moves and show a single still frame.
    CMoveSetMesh moveSet;
    bool hasMoveSet;
    std::uint8_t meshConfigIndex;
};

/**
 * Collects one entry per distinct model.
 *
 * A model can be named by several templates -- the same enemy body turns up in
 * six variants -- and they all name the same atlas, so the first naming wins
 * and the rest are dropped.
 */
class CatalogSink : public IMeshPairSink {
public:
    void OnPair(const MeshPair &pair) override {
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].meshPackHash == pair.meshPackHash &&
                m_entries[i].meshOrdinal == pair.meshOrdinal) {
                return;
            }
        }

        CatalogEntry entry;
        entry.meshPackHash = pair.meshPackHash;
        entry.meshOrdinal = pair.meshOrdinal;
        entry.imagePackHash = pair.imagePackHash;
        entry.imageOrdinal = pair.imageOrdinal;
        entry.owner = pair.owner;
        entry.hasMoveSet = pair.moveSet != nullptr;
        entry.meshConfigIndex = pair.meshConfigIndex;
        if (entry.hasMoveSet) {
            entry.moveSet = *pair.moveSet;
        }
        m_entries.push_back(entry);
    }

    void OnTemplateFailed(const std::string &owner) override { (void)owner; }
    void OnLeftoverBytes(const std::string &owner, std::size_t bytes) override {
        (void)owner;
        (void)bytes;
    }

    const std::vector<CatalogEntry> &GetEntries() const { return m_entries; }

private:
    std::vector<CatalogEntry> m_entries;
};

/**
 * The model currently on screen: its data, its atlas, and its clock.
 *
 * Never copied or moved once built -- `configMeshes` points back at `mesh`,
 * which is why the viewer holds one of these by pointer and swaps the pointer
 * rather than the object.
 */
struct LoadedModel {
    CMesh mesh;
    CTexture texture;

    // The playback machinery, used only when the catalogue entry brought a
    // move set. A model named by a plain asset ref shows frame 0 and stops.
    CMoveSetMeshController controller;
    std::vector<const CMesh *> configMeshes;

    // Which moves of the set drive THIS mesh, by index into the set. A player
    // move set has moves for the torso and moves for the legs; only one half
    // of them belongs to the model on screen.
    std::vector<std::int32_t> moves;
    std::size_t moveSlot;

    // The evaluator's output, kept between frames so it is not reallocated
    // sixty times a second.
    std::vector<float> pose;

    LoadedModel() : moveSlot(0) {}
};

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
    if (model.moves.empty()) {
        buffer.SetFrame(model.mesh, stillFrameIndex);
        return;
    }

    if (model.controller.GetAnimation().Evaluate(model.pose)) {
        buffer.SetVertices(model.pose);
    }
}

/**
 * Run the animation on before the first frame is drawn.
 *
 * Stepped rather than handed the whole span at once, because the move's speed
 * is rounded to whole milliseconds every update -- one big step and one
 * hundred small ones do not land in the same place, and it is the small ones
 * that match what a running viewer does.
 */
void WarmUp(LoadedModel &model, std::uint32_t advanceMs) {
    if (model.moves.empty()) {
        return;
    }
    for (std::uint32_t elapsed = 0; elapsed < advanceMs; elapsed += kWarmUpFrameMs) {
        model.controller.Update(kWarmUpFrameMs);
    }
}

/** "move 3 of 18 -- frames 40..79, speed 1.00" */
void ReportMove(const CatalogEntry &entry, const LoadedModel &model) {
    if (model.moves.empty()) {
        std::printf("[m35] no move drives this model; showing frame 0\n");
        return;
    }

    const std::int32_t moveIndex = model.moves[model.moveSlot];
    const MeshMove &move = entry.moveSet.GetMoves()[moveIndex];
    std::printf("[m35] move %d (%zu of %zu for this mesh) -- frames %u..%u, "
                "%d ms, speed %.2f%s\n",
                moveIndex, model.moveSlot + 1, model.moves.size(),
                move.firstFrame, move.lastFrame,
                model.controller.GetAnimation().GetRangeDurationMs(), move.speed,
                move.restartsWhenRepeated != 0 ? ", restarts" : "");
}

/** Fetch and decode one catalogue entry. */
bool LoadModel(PackTables &tables, const CatalogEntry &entry, LoadedModel &out) {
    if (!LoadMeshAndAtlas(tables, entry.owner.c_str(), entry.meshPackHash,
                          entry.meshOrdinal, entry.imagePackHash,
                          entry.imageOrdinal, out.mesh, out.texture)) {
        return false;
    }

    if (entry.hasMoveSet) {
        BindMoveSet(entry, out);
        ReportMove(entry, out);
    }
    return true;
}

/** How the model is standing: the engine's own two orientations, plus drag. */
struct Turntable {
    float spinDegrees;   // about the model's own up axis, as the engine spins it
    float tiltDegrees;   // about X: 90 for the menus, 30 for the game
    float extraTilt;     // viewer-only, added by dragging up and down
    float zoom;
};

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

// ---------------------------------------------------------------------------
// M3.7: a whole character, assembled
//
// The viewer above shows one model at a time. This one stands a player up out
// of the three models a player actually is -- torso, legs, and whatever gun is
// in his hand -- following the part table CBrother::Draw (:134780) builds.
// ---------------------------------------------------------------------------

// The assembly itself lives in PlayerModel.h, which the map viewer shares.
// What stays here is the catalogue of weapons to choose between, which is a
// viewer feature: the game hands a player one gun and never a list.

/** One weapon the viewer can put in the character's hand. */
struct GunEntry {
    std::uint32_t meshPackHash;
    std::uint32_t meshOrdinal;
    std::uint32_t imagePackHash;
    std::uint32_t imageOrdinal;
    std::string owner;
};

/** Collects the one player template, and every gun's own weapon model. */
class CharacterSink : public IMeshPairSink {
public:
    CharacterSink() : m_havePlayer(false) {}

    void OnPair(const MeshPair &pair) override {
        if (pair.moveSet != nullptr) {
            // Every config of the player's set carries the same move set, so
            // the first one is all it takes.
            if (!m_havePlayer && pair.owner.find(" player ") != std::string::npos) {
                m_playerMoveSet = *pair.moveSet;
                m_playerPackHash = pair.meshPackHash;
                m_playerOwner = pair.owner;
                m_havePlayer = true;
            }
            return;
        }

        // A gun emits its weapon model as a plain asset ref, which is the pair
        // with no move set behind it.
        if (pair.owner.find(" gun ") == std::string::npos) {
            return;
        }

        GunEntry gun;
        gun.meshPackHash = pair.meshPackHash;
        gun.meshOrdinal = pair.meshOrdinal;
        gun.imagePackHash = pair.imagePackHash;
        gun.imageOrdinal = pair.imageOrdinal;
        gun.owner = pair.owner;
        m_guns.push_back(gun);
    }

    void OnTemplateFailed(const std::string &owner) override { (void)owner; }
    void OnLeftoverBytes(const std::string &owner, std::size_t bytes) override {
        (void)owner;
        (void)bytes;
    }

    bool HavePlayer() const { return m_havePlayer; }
    const CMoveSetMesh &GetPlayerMoveSet() const { return m_playerMoveSet; }
    std::uint32_t GetPlayerPackHash() const { return m_playerPackHash; }
    const std::string &GetPlayerOwner() const { return m_playerOwner; }
    const std::vector<GunEntry> &GetGuns() const { return m_guns; }

private:
    bool m_havePlayer;
    CMoveSetMesh m_playerMoveSet;
    std::uint32_t m_playerPackHash;
    std::string m_playerOwner;
    std::vector<GunEntry> m_guns;
};

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

}  // namespace

int RunMeshSurvey(const std::string &bigDirectory) {
    CResTOCManager tocManager;
    if (!tocManager.Init(bigDirectory, kArtSetXga) || !tocManager.Bind()) {
        return 1;
    }

    std::printf("\n=== M3.5: Section 31 meshes ===\n");

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
    if (!tocManager.Init(bigDirectory, kArtSetXga) || !tocManager.Bind()) {
        return 1;
    }

    std::printf("\n=== M3.5: move sets, mesh to atlas ===\n");

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

int RunM35Mesh(const std::string &bigDirectory, std::uint32_t startIndex,
               float spinDegrees, std::uint32_t frameIndex,
               const std::string &screenshotPath, std::uint32_t advanceMs) {
    std::printf("=== M3.5: a model on screen ===\n\n");

    CResTOCManager tocManager;
    if (!tocManager.Init(bigDirectory, kArtSetXga) || !tocManager.Bind()) {
        return 1;
    }

    PackTables tables(tocManager);
    CatalogSink catalog;
    WalkMeshPairs(tocManager, tables, catalog);
    if (catalog.GetEntries().empty()) {
        std::printf("[m35] no template names a model\n");
        return 1;
    }
    std::printf("\n[m35] %zu models in the catalogue\n",
                catalog.GetEntries().size());

    std::size_t slot = startIndex;
    if (slot >= catalog.GetEntries().size()) {
        slot = 0;
    }

    CWindow window;
    if (!window.Open("gun_bros_re -- M3.5", kDefaultWindowWidth,
                     kDefaultWindowHeight)) {
        return 1;
    }

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

    std::printf("\n[m35] left/right: model, up/down: ten at a time, "
                "M/N: move, space: pause, period: step, "
                "drag: turn, wheel: zoom, G: game tilt, Home: reset view, "
                "Esc: quit\n");

    std::uint64_t previousTicks = window.GetTicksMs();
    bool paused = false;
    bool singleStep = false;

    bool reportedFirstFrame = false;
    while (window.PumpEvents()) {
        int drawableWidth = 0;
        int drawableHeight = 0;
        window.GetDrawableSize(drawableWidth, drawableHeight);

        // --- walking the catalogue ---
        const std::size_t previousSlot = slot;
        bool moveChanged = false;
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None;
             key = window.TakeKeyPress()) {
            const std::size_t count = catalog.GetEntries().size();
            if (key == KeyCode::Right) {
                slot = (slot + 1) % count;
            } else if (key == KeyCode::Left) {
                slot = (slot + count - 1) % count;
            } else if (key == KeyCode::Down) {
                slot = (slot + 10) % count;
            } else if (key == KeyCode::Up) {
                slot = (slot + count - 10) % count;
            } else if (key == KeyCode::M && !model->moves.empty()) {
                model->moveSlot = (model->moveSlot + 1) % model->moves.size();
                moveChanged = true;
            } else if (key == KeyCode::N && !model->moves.empty()) {
                const std::size_t moveCount = model->moves.size();
                model->moveSlot = (model->moveSlot + moveCount - 1) % moveCount;
                moveChanged = true;
            } else if (key == KeyCode::Space) {
                paused = !paused;
                std::printf("[m35] %s\n", paused ? "paused" : "playing");
            } else if (key == KeyCode::Period) {
                singleStep = true;
            } else if (key == KeyCode::G) {
                // The 30-degree lean the game plays at, against the 90 the
                // menus stand a model up with.
                if (view.tiltDegrees == kUiTiltDegrees) {
                    view.tiltDegrees = kGameTiltDegrees;
                } else {
                    view.tiltDegrees = kUiTiltDegrees;
                }
                std::printf("[m35] tilt %.0f degrees\n", view.tiltDegrees);
            } else if (key == KeyCode::Home) {
                view.spinDegrees = kUiFacingDegrees + spinDegrees;
                view.extraTilt = 0.0f;
                view.zoom = 1.0f;
            }
        }

        if (slot != previousSlot) {
            std::printf("\n[m35] --- model %zu of %zu ---\n", slot + 1,
                        catalog.GetEntries().size());

            std::unique_ptr<LoadedModel> replacement(new LoadedModel());
            if (LoadModel(tables, catalog.GetEntries()[slot], *replacement)) {
                model = std::move(replacement);
                buffer.SetMesh(model->mesh);
                WarmUp(*model, advanceMs);
                UploadPose(*model, buffer, frameIndex);
            } else {
                // A model that will not load leaves the previous one on screen
                // rather than a blank window.
                std::printf("[m35] staying on the previous model\n");
                slot = previousSlot;
            }
        } else if (moveChanged) {
            if (!model->controller.SetMove(model->moves[model->moveSlot])) {
                // Refused, so this is the move already playing and it is not
                // flagged to restart. Cycling round a mesh with one move would
                // otherwise look like a dead key; rewind it by hand.
                CMeshAnimationController &animation = model->controller.GetAnimation();
                animation.SetTimeMs(animation.GetRangeStartMs());
            }
            UploadPose(*model, buffer, frameIndex);
            ReportMove(catalog.GetEntries()[slot], *model);
        }

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

        if (elapsedMs > 0 && !model->moves.empty()) {
            model->controller.Update(static_cast<std::int32_t>(elapsedMs));
            UploadPose(*model, buffer, frameIndex);
        }

        // --- turntable ---
        int dragX = 0;
        int dragY = 0;
        window.TakeDragDelta(dragX, dragY);
        view.spinDegrees += static_cast<float>(dragX) * kDragToDegrees;
        view.extraTilt += static_cast<float>(dragY) * kDragToDegrees;

        const float wheel = window.TakeWheelDelta();
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

        if (!reportedFirstFrame) {
            GLCheckErrors("first frame");
            reportedFirstFrame = true;

            if (!screenshotPath.empty()) {
                if (!window.SaveFrame(screenshotPath)) {
                    return 1;
                }
                window.Present();
                break;
            }
        }

        window.Present();
    }

    std::printf("[m35] done\n");
    return 0;
}

int RunWeaponCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    std::vector<WeaponEntry> weapons;
    PlayerTemplateData playerTemplate;
    if (!LoadWeaponCatalog(toc, tables, weapons) ||
        !FindPlayerTemplate(toc, tables, playerTemplate)) { return 1; }

    // Test the input mapping independently of each category's catalogue size.
    for (std::size_t i = 0; i < weapons.size(); ++i) {
        const std::size_t next = SelectWeaponKey(weapons, i, KeyCode::M);
        if (weapons[next].category != weapons[i].category ||
            SelectWeaponKey(weapons, next, KeyCode::N) != i ||
            SelectWeaponKey(weapons, i, KeyCode::E) != i ||
            SelectWeaponKey(weapons, i, KeyCode::Digit8) != i ||
            SelectWeaponKey(weapons, i, KeyCode::Digit9) != i) { return 1; }
        for (int category = 0; category < kWeaponCategoryCount; ++category) {
            const KeyCode key = static_cast<KeyCode>(static_cast<int>(KeyCode::Digit1) + category);
            if (weapons[SelectWeaponKey(weapons, i, key)].category != category) { return 1; }
        }
    }
    CWindow window;
    if (!window.Open("Weapon verification", 800, 600)) { return 1; }
    CShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    WeaponEffects effects(toc, tables, program);
    // The observed Kraken failure: particle templates 0x10/0x20 must select
    // explosion animations 4/5, never laser animations 0/1 from the same atlas.
    ParticleEmitterTemplate emitter;
    emitter.animationMask = 0x10;
    if (emitter.SelectAnimation(0) != 4 || emitter.SelectAnimation(1) != 4) { return 1; }
    emitter.animationMask = 0x20;
    if (emitter.SelectAnimation(0.5f) != 5) { return 1; }
    // Validate the actual STORE join, not hand-selected resource ordinals.
    std::vector<StoreEntry> storeEntries;
    if (!LoadStoreCatalog(toc, tables, storeEntries)) { return 1; }
    for (const auto &store : storeEntries) {
        if (store.data.objects.size() != 1 || store.data.objects.front().type != 6) { continue; }
        const auto &reference = store.data.objects.front().object;
        bool matched = false;
        for (const auto &weapon : weapons) {
            if (weapon.packHash != reference.packHash || weapon.ordinal != reference.localIndex) { continue; }
            matched = weapon.hasStoreEntry && weapon.category == store.data.type;
        }
        if (!matched) { std::printf("[weapon-check] original store category join failed\n"); return 1; }
    }
    float identity[kMatrix4dElements];
    float modelToScene[kMatrix4dElements];
    float sceneMvp[kMatrix4dElements];
    Matrix4dIdentity(identity);
    Matrix4dOrthoTopLeft(800, 600, 1000, sceneMvp);
    std::size_t placeholders = 0;
    // A wall ahead of the muzzle exercises impact callbacks before the fuse.
    const std::vector<std::uint8_t> wallBytes = {
        2, 0, 0, 0, 0, 0, 100, 0, 0, 0,
        32, 3, 0, 0, 100, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0
    };
    WeaponCollision impactScene;
    CArrayInputStream wallStream(wallBytes);
    if (!impactScene.walls.Load(wallStream)) { return 1; }
    impactScene.terrain = impactScene.walls;
    for (std::size_t i = 0; i < weapons.size(); ++i) {
        if (!window.PumpEvents()) { return 1; }
        effects.Clear();
        PlayerModel player;
        const WeaponEntry &entry = weapons[i];
        if (!BuildPlayerBody(tables, playerTemplate.moveSet, player) ||
            !EquipPlayerWeapon(tables, playerTemplate.script, entry.data, entry.owner, player) ||
            !CreatePlayerBuffers(player, program)) { return 1; }
        // Real thresholds exercise each boundary and both critical outcomes;
        // restoring zero keeps the existing firing regression at base mastery.
        if (entry.hasStoreEntry && entry.data.GetMasteryLimit() > 0) {
            for (unsigned tier = 0; tier < 3; ++tier) {
                const unsigned threshold = entry.data.GetMasteryThreshold(tier);
                player.weapon->gun.SetMasteryExperience(threshold - 1);
                if (player.weapon->gun.GetMasteryLevel() != tier) { return 1; }
                player.weapon->gun.SetMasteryExperience(threshold);
                // Some scripted launchers intentionally have a zero fire-rate
                // field; their attack state sets the timer itself.
                if (player.weapon->gun.GetMasteryLevel() != tier + 1 || player.weapon->gun.GetMasterySpeedMod() < 100 ||
                    player.weapon->gun.GetMasteryDamageMultiplier(0) <
                    player.weapon->gun.GetMasteryDamageMultiplier(1)) {
                    std::printf("[mastery-check] FAIL %s tier=%u threshold=%u level=%u speed=%u interval=%u normal=%.2f critical=%.2f\n",
                        entry.owner.c_str(), tier, threshold, player.weapon->gun.GetMasteryLevel(), player.weapon->gun.GetMasterySpeedMod(),
                        player.weapon->gun.GetFireRateMs(), player.weapon->gun.GetMasteryDamageMultiplier(1), player.weapon->gun.GetMasteryDamageMultiplier(0));
                    return 1;
                }
            }
            player.weapon->gun.SetMasteryExperience(0);
        }
        BuildPlayerGameMatrix(identity, 400, 450,
            PlayerModelWorldScale(player, playerTemplate.gameScale, 1), 0, modelToScene);
        if (i == 0) {
            // Regression: the default pistols must use the gun's player moves
            // and two distinct hands, even though the mesh is shared with rifles.
            MeshBoneTransform right{}, left{};
            if (!player.weapon->brother.TorsoUsesWeapon() || entry.data.GetHandedness() != 2 ||
                !GetPlayerMuzzle(player, 0, 0, right) || !GetPlayerMuzzle(player, 1, 0, left) ||
                (right.posX == left.posX && right.posY == left.posY && right.posZ == left.posZ)) {
                std::printf("[weapon-check] pistol holding regression\n");
                return 1;
            }
        }
        // Stationary, collision-free launch probes cannot pass on footsteps
        // or distant impact sounds. Cover the reported silent weapon groups.
        const bool checkLaunch = (entry.category == 0 && entry.hasStoreEntry) ||
            (i >= 12 && i <= 15) || (i >= 24 && i <= 26) ||
            i == 33 || i == 34 || i == 36 || i == 44 || i == 46 || i == 68 || i == 69;
        if (checkLaunch) {
            const std::size_t launchSounds = effects.GetSoundCueCount();
            SetPlayerInput(player, false, true);
            for (int elapsed = 0; elapsed < 160; elapsed += 16) {
                AdvancePlayer(player, 16);
                effects.Update(player, modelToScene, 0, 16);
            }
            if (effects.GetSoundCueCount() == launchSounds) {
                std::printf("[weapon-check] FAIL %zu launch has no audio: %s\n", i, entry.name.c_str());
                return 1;
            }
            std::printf("[weapon-check] %zu launch audio queued before impact\n", i);
            effects.Clear();
            if (!EquipPlayerWeapon(tables, playerTemplate.script, entry.data, entry.owner, player) ||
                !CreatePlayerBuffers(player, program)) { return 1; }
        }
        // Reloading weapons must not loop attack sounds ahead of their next
        // shot. Use real scripts, no movement/impacts, and a held trigger.
        if ((i >= 12 && i <= 15) || (i >= 24 && i <= 26)) {
            const std::size_t startShots = effects.GetShotCount();
            const std::size_t startSounds = effects.GetSoundCueCount();
            SetPlayerInput(player, false, true);
            for (int elapsed = 0; elapsed < 3200; elapsed += 16) {
                AdvancePlayer(player, 16);
                effects.Update(player, modelToScene, 0, 16);
                if (effects.GetSoundCueCount() - startSounds > effects.GetShotCount() - startShots) {
                    std::printf("[weapon-check] FAIL %zu held attack precedes its projectile at %d ms\n", i, elapsed + 16);
                    return 1;
                }
            }
            if (effects.GetShotCount() - startShots < 2) {
                std::printf("[weapon-check] FAIL %zu held trigger never resumes after reload\n", i);
                return 1;
            }
            const std::size_t releaseShots = effects.GetShotCount();
            SetPlayerInput(player, false, false);
            for (int elapsed = 0; elapsed < 1600; elapsed += 16) {
                AdvancePlayer(player, 16);
                effects.Update(player, modelToScene, 0, 16);
            }
            if (effects.GetShotCount() != releaseShots) {
                std::printf("[weapon-check] FAIL %zu released trigger resumes after reload\n", i);
                return 1;
            }
            std::printf("[weapon-check] %zu reload keeps attack cues aligned with shots; release stops\n", i);
            effects.Clear();
            if (!EquipPlayerWeapon(tables, playerTemplate.script, entry.data, entry.owner, player) ||
                !CreatePlayerBuffers(player, program)) { return 1; }
        }
        const std::size_t before = effects.GetShotCount();
        const std::size_t soundBefore = effects.GetSoundCueCount();
        if (i == 21) {
            SetPlayerInput(player, false, true);
            int elapsed = 0;
            while (elapsed < 12000 && player.weapon->gun.CanFire()) {
                AdvancePlayer(player, 16);
                effects.Update(player, modelToScene, 0, 16, &impactScene);
                elapsed += 16;
            }
            if (elapsed >= 12000) { std::printf("[weapon-check] Gatling never overheated\n"); return 1; }
            const std::size_t coolingShots = effects.GetShotCount();
            SetPlayerInput(player, false, false);
            AdvancePlayer(player, 16);
            effects.Update(player, modelToScene, 0, 16);
            SetPlayerInput(player, false, true);
            for (int cooling = 0; cooling < 960; cooling += 16) {
                AdvancePlayer(player, 16);
                effects.Update(player, modelToScene, 0, 16);
            }
            if (effects.GetShotCount() != coolingShots) {
                std::printf("[weapon-check] release/repress bypassed Gatling cooling\n"); return 1;
            }
            for (int cooling = 0; cooling < 1000; cooling += 16) {
                AdvancePlayer(player, 16);
                effects.Update(player, modelToScene, 0, 16);
            }
            if (effects.GetShotCount() == coolingShots) { return 1; }
            SetPlayerInput(player, false, false);
            std::printf("[weapon-check] Gatling cooldown blocks repress and resumes automatically\n");
        }
        // Idle -> walk -> fire while walking -> stationary fire -> release -> fire again.
        const bool moving[] = {false, true, true, false, false, false, false};
        const bool shooting[] = {false, false, true, true, false, true, false};
        const int durations[] = {400, 400, 1600, 1600, 800, 800, 4000};
        for (int phase = 0; phase < 7; ++phase) {
            SetPlayerInput(player, moving[phase], shooting[phase]);
            for (int elapsed = 0; elapsed < durations[phase]; elapsed += 16) {
                AdvancePlayer(player, 16);
                effects.Update(player, modelToScene, 0, 16, &impactScene);
                if (i == 12 && phase == 2 && elapsed == 144 && effects.GetParticleCount() > 40) {
                    std::printf("[weapon-check] zero-interval shotgun emitter overproduced particles\n");
                    return 1;
                }
                if ((i == 29 || i == 32) && phase == 2 && elapsed == 16 && effects.GetTrailCount() == 0) {
                    std::printf("[weapon-check] FAIL flame trail stopped before its next emission\n");
                    return 1;
                }
            }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            float modelMvp[kMatrix4dElements];
            Matrix4dMultiply(sceneMvp, modelToScene, modelMvp);
            effects.Draw(sceneMvp, nullptr, 1.0f, WeaponDrawPass::BehindPlayer);
            glEnable(GL_DEPTH_TEST);
            DrawPlayer(player, program, modelMvp);
            effects.Draw(sceneMvp, nullptr, 1.0f, WeaponDrawPass::InFrontOfPlayer);
            if (glGetError() != GL_NO_ERROR) { return 1; }
        }
        const std::size_t emitted = effects.GetShotCount() - before;
        if (i >= 24 && i <= 26 && effects.GetSoundCueCount() == soundBefore) {
            std::printf("[weapon-check] shoulder launcher explosion sound missing\n"); return 1;
        }
        if (entry.visualOnly) {
            ++placeholders;
            std::printf("[weapon-check] %zu visual-only: archive has no firing script or projectile reference\n", i);
        }
        else if (emitted == 0) {
            std::printf("[weapon-check] %zu failed to fire: %s\n", i, entry.name.c_str());
            std::printf("  mode=%d ammo=%d defaultBullet=%08x:%u exports:", player.weapon->gun.GetFireMode(),
                *player.weapon->gun.VariableResolver(0), entry.data.GetBulletRef().packHash,
                entry.data.GetBulletRef().localIndex);
            for (int value : entry.data.GetScript().GetExportFunctions()) { std::printf(" %d", value); }
            std::printf("\n");
            for (const CScriptCode &code : entry.data.GetScript().GetFunctions()) {
                std::printf("  code:");
                for (int byte = 0; byte <= code.GetByteLength(); ++byte) {
                    std::printf(" %02x", code.Begin()[byte]);
                }
                std::printf("\n");
            }
            return 1;
        }
        if (player.weapon->gun.IsShooting() || effects.GetBulletCount() != 0) {
            std::printf("[weapon-check] %zu release left live bullets=%zu\n", i, effects.GetBulletCount());
            return 1;
        }
        std::printf("[weapon-check] %zu PASS shots=%zu %s\n", i, emitted, entry.name.c_str());
    }
    std::printf("[weapon-check] PASS %zu templates, %zu visual-only entries; keys and transitions verified\n",
        weapons.size(), placeholders);
    return 0;
}

int RunM37Character(const std::string &bigDirectory, std::uint32_t gunIndex,
                    float spinDegrees, const std::string &screenshotPath,
                    std::uint32_t advanceMs, bool firePreview, int armorIndex) {
    std::printf("=== M3.7: a whole character ===\n\n");

    CResTOCManager tocManager;
    if (!tocManager.Init(bigDirectory, kArtSetXga) || !tocManager.Bind()) {
        return 1;
    }

    PackTables tables(tocManager);
    std::vector<WeaponEntry> weapons;
    PlayerTemplateData playerTemplate;
    if (!LoadWeaponCatalog(tocManager, tables, weapons)) { return 1; }

    std::vector<ArmorEntry> armors;
    if (armorIndex >= 0 && !LoadArmorCatalog(tocManager, tables, armors)) {
        return 1;
    }
    if (armorIndex >= static_cast<int>(armors.size()) && armorIndex >= 0) {
        std::printf("[armor] index %d out of range\n", armorIndex);
        return 1;
    }

    if (!FindPlayerTemplate(tocManager, tables, playerTemplate)) {
        std::printf("[m37] no player template found\n");
        return 1;
    }
    if (weapons.empty()) {
        std::printf("[m37] no gun names a weapon model\n");
        return 1;
    }
    std::printf("\n[m37] %s, %zu weapon models\n", playerTemplate.owner.c_str(),
                weapons.size());

    std::size_t gunSlot = gunIndex;
    if (gunSlot >= weapons.size()) {
        gunSlot = 0;
    }

    CWindow window;
    if (!window.Open("gun_bros_re -- M3.7", kDefaultWindowWidth,
                     kDefaultWindowHeight)) {
        return 1;
    }

    CShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) {
        return 1;
    }

    // Held by pointer for the same reason one part is: swapping the gun
    // rebuilds the whole thing, and nothing in it can be moved.
    std::unique_ptr<PlayerModel> character(new PlayerModel());
    if (!BuildPlayerBody(tables, playerTemplate.moveSet, *character) ||
        !EquipPlayerWeapon(tables, playerTemplate.script, weapons[gunSlot].data,
                           weapons[gunSlot].owner, *character) ||
        !CreatePlayerBuffers(*character, program)) {
        return 1;
    }

    std::size_t moveSlot = 0;
    SelectPlayerMoveSlot(*character, moveSlot, true);
    SetPlayerInput(*character, false, firePreview);
    window.SetRightDrag(false);
    window.SetTitle("player weapon | " + WeaponSelectionLabel(weapons, gunSlot));
    if (armorIndex >= 0) {
        if (!EquipPlayerArmor(tables, armors[armorIndex].data, program, *character)) {
            return 1;
        }
        window.SetTitle("armor " + std::to_string(armorIndex) + "/" +
            std::to_string(armors.size() - 1) + " | " + armors[armorIndex].owner);
        std::printf("[armor] Left/Right: armor; B: remove all; 1-7,N/M: weapon; F: fire; WASD: walk\n");
    }
    PosePlayer(*character);
    WeaponEffects effects(tocManager, tables, program);

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

    std::printf("\n[m37] 1-7: category, N/M: weapon, F: fire, WASD: walk, space: pause, "
                "period: step, left drag: turn, wheel: zoom, G: game tilt, "
                "Home: reset view, Esc: quit\n");

    std::uint64_t previousTicks = window.GetTicksMs();
    bool paused = false;
    bool singleStep = false;
    bool reportedFirstFrame = false;
    std::uint32_t warmUpRemaining = advanceMs;

    while (window.PumpEvents()) {
        int drawableWidth = 0;
        int drawableHeight = 0;
        window.GetDrawableSize(drawableWidth, drawableHeight);

        const std::size_t previousGunSlot = gunSlot;
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None;
             key = window.TakeKeyPress()) {
            if (armorIndex >= 0 && (key == KeyCode::Left || key == KeyCode::Right)) {
                int next = armorIndex + 1;
                if (key == KeyCode::Left) {
                    next = armorIndex + static_cast<int>(armors.size()) - 1;
                }
                next %= static_cast<int>(armors.size());
                if (!EquipPlayerArmor(tables, armors[next].data, program, *character)) {
                    return 1;
                }
                armorIndex = next;
                window.SetTitle("armor " + std::to_string(armorIndex) + "/" +
                    std::to_string(armors.size() - 1) + " | " + armors[armorIndex].owner);
                continue;
            }
            if (armorIndex >= 0 && key == KeyCode::B) {
                ClearPlayerArmor(*character);
                continue;
            }
            const std::size_t gunCount = weapons.size();
            gunSlot = SelectWeaponKey(weapons, gunSlot, key);
            if (key == KeyCode::Right) {
                gunSlot = (gunSlot + 1) % gunCount;
            } else if (key == KeyCode::Left) {
                gunSlot = (gunSlot + gunCount - 1) % gunCount;
            } else if (key == KeyCode::Down) {
                gunSlot = (gunSlot + 10) % gunCount;
            } else if (key == KeyCode::Up) {
                gunSlot = (gunSlot + gunCount - 10) % gunCount;
            } else if (key == KeyCode::M) {
                // N/M now select equipment through the shared catalogue above.
            } else if (key == KeyCode::N) {
                // The torso has the longest move list, so step by it and let
                // the shorter ones wrap inside SelectMoveSlot.
                // The old synchronized slot convention is superseded by scripts.
            } else if (key == KeyCode::Space) {
                paused = !paused;
                std::printf("[m37] %s\n", paused ? "paused" : "playing");
            } else if (key == KeyCode::Period) {
                singleStep = true;
            } else if (key == KeyCode::G) {
                if (view.tiltDegrees == kUiTiltDegrees) {
                    view.tiltDegrees = kGameTiltDegrees;
                } else {
                    view.tiltDegrees = kUiTiltDegrees;
                }
                std::printf("[m37] tilt %.0f degrees\n", view.tiltDegrees);
            } else if (key == KeyCode::Home) {
                view.spinDegrees = kUiFacingDegrees + spinDegrees;
                view.extraTilt = 0.0f;
                view.zoom = 1.0f;
            }
        }

        if (gunSlot != previousGunSlot) {
            std::printf("\n[m37] --- weapon %zu of %zu ---\n", gunSlot + 1,
                        weapons.size());

            std::unique_ptr<PlayerModel> replacement(new PlayerModel());
            if (BuildPlayerBody(tables, playerTemplate.moveSet, *replacement) &&
                EquipPlayerWeapon(tables, playerTemplate.script, weapons[gunSlot].data,
                                  weapons[gunSlot].owner, *replacement) &&
                CreatePlayerBuffers(*replacement, program)) {
                // Weapon changes preserve all independently equipped armour slots.
                for (std::uint32_t slot = 0; slot < kArmorSlotCount; ++slot) {
                    replacement->armor[slot] = std::move(character->armor[slot]);
                }
                character = std::move(replacement);
                effects.Clear();
                SelectPlayerMoveSlot(*character, moveSlot, true);
                PosePlayer(*character);
                window.SetTitle("player weapon | " + WeaponSelectionLabel(weapons, gunSlot));
            } else {
                std::printf("[m37] staying on the previous weapon\n");
                gunSlot = previousGunSlot;
            }
        }

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
        // Screenshots advance by the requested fixed steps, independent of loading time.
        if (!screenshotPath.empty()) { elapsedMs = 0; }
        if (elapsedMs > 0) {
            const bool moving = window.IsKeyDown(KeyCode::W) || window.IsKeyDown(KeyCode::A) ||
                window.IsKeyDown(KeyCode::S) || window.IsKeyDown(KeyCode::D);
            SetPlayerInput(*character, moving, firePreview || window.IsKeyDown(KeyCode::F));
            AdvancePlayer(*character, static_cast<std::int32_t>(elapsedMs));
        }

        int dragX = 0;
        int dragY = 0;
        window.TakeDragDelta(dragX, dragY);
        view.spinDegrees += static_cast<float>(dragX) * kDragToDegrees;
        view.extraTilt += static_cast<float>(dragY) * kDragToDegrees;

        const float wheel = window.TakeWheelDelta();
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

        float base[kMatrix4dElements];
        BuildModelViewProjection(PlayerBounds(*character), view, drawableWidth,
                                 drawableHeight, base);
        float viewport[kMatrix4dElements];
        Matrix4dIdentity(viewport);
        viewport[0] = drawableWidth * 0.5f;
        viewport[3] = drawableWidth * 0.5f;
        viewport[5] = -drawableHeight * 0.5f;
        viewport[7] = drawableHeight * 0.5f;
        float modelToScreen[kMatrix4dElements];
        Matrix4dMultiply(viewport, base, modelToScreen);
        // Simulate both viewers in the same world units. The turntable only
        // projects the result; spread is applied before the camera rotation.
        const float worldScale = PlayerModelWorldScale(*character, playerTemplate.gameScale, 1);
        float modelToWorld[kMatrix4dElements];
        Matrix4dScale(worldScale, modelToWorld);
        float inverseScale[kMatrix4dElements];
        Matrix4dScale(1.0f / worldScale, inverseScale);
        float worldToScreen[kMatrix4dElements];
        Matrix4dMultiply(modelToScreen, inverseScale, worldToScreen);
        // Looking straight into a barrel projects its travel to a point.
        // Do not turn floating-point noise at 90 degrees into diagonal shots.
        while (warmUpRemaining > 0) {
            const int step = static_cast<int>(std::min<std::uint32_t>(warmUpRemaining, kWarmUpFrameMs));
            AdvancePlayer(*character, step);
            effects.Update(*character, modelToWorld, 0, step);
            warmUpRemaining -= step;
        }
        effects.SetPaused(paused);
        effects.Update(*character, modelToWorld, 0, static_cast<int>(elapsedMs));
        float screenMvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(static_cast<float>(drawableWidth), static_cast<float>(drawableHeight), 1000.0f, screenMvp);
        effects.Draw(screenMvp, worldToScreen, 1.0f, WeaponDrawPass::BehindPlayer);
        glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        DrawPlayer(*character, program, base);
        effects.Draw(screenMvp, worldToScreen, 1.0f, WeaponDrawPass::InFrontOfPlayer);

        if (!reportedFirstFrame) {
            GLCheckErrors("first frame");
            reportedFirstFrame = true;

            if (!screenshotPath.empty()) {
                std::printf("[weapon-render] shots=%zu live=%zu torsoMove=%d legsMove=%d\n",
                    effects.GetShotCount(), effects.GetBulletCount(),
                    character->weapon->brother.GetTorso().GetMoveIndex(),
                    character->weapon->brother.GetLegs().GetMoveIndex());
                if (!window.SaveFrame(screenshotPath)) {
                    return 1;
                }
                window.Present();
                break;
            }
        }

        window.Present();
    }

    std::printf("[m37] done\n");
    return 0;
}

int RunWeaponSurvey(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
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

/** A target beside the muzzle ray reproduces invisible wide-beam obstruction. */
class WeaponRayCheckWorld : public IProjectileWorld {
public:
    unsigned beamContacts = 0;
    float targetOffset = 50;
    float targetDistance = 100;
    CombatTrace Trace(const CombatHit &hit, float x, float y, float dx, float dy,
        float radius, const std::vector<CombatId> &skip) override {
        if ((hit.flags & 0x100) == 0 || !skip.empty()) { return {}; }
        const float fraction = CombatGeometry::CircleFraction(x, y, dx, dy, x + targetOffset, y - targetDistance, 10 + radius);
        if (fraction > 1) { return {}; }
        ++beamContacts;
        return {99, fraction};
    }
    HitResult ApplyHit(CombatId, const CombatHit &) override { return HitResult::Hit; }
    void Splash(const CombatHit &, float, float, float, int) override {}
    void SpawnFromProjectile(const GameObjectRef &, const CombatHit &) override {}
    bool FindTarget(const CombatHit &, float, float &, float &) override { return false; }
    bool Anchor(CombatId, int, int, float &, float &, float &, float &) override { return false; }
};

/** BIG scripts and the production projectile update, with no map or input noise. */
int RunWeaponEffectsCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    std::vector<WeaponEntry> weapons;
    PlayerTemplateData playerTemplate;
    if (!LoadWeaponCatalog(toc, tables, weapons) || !FindPlayerTemplate(toc, tables, playerTemplate)) { return 1; }
    CWindow window;
    if (!window.Open("Weapon effect verification", 800, 600)) { return 1; }
    glViewport(0, 0, 800, 600);
    glEnable(GL_BLEND);
    CShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    WeaponEffects effects(toc, tables, program);
    WeaponRayCheckWorld rayWorld;
    effects.SetCombatWorld(&rayWorld);
    float identity[kMatrix4dElements], modelToScene[kMatrix4dElements], mvp[kMatrix4dElements];
    Matrix4dIdentity(identity);
    Matrix4dOrthoTopLeft(800, 600, 1000, mvp);
    unsigned failures = 0;
    for (std::size_t index = 0; index < weapons.size(); ++index) {
        const auto &entry = weapons[index];
        const bool kraken = entry.name.find("Kraken") != std::string::npos;
        const bool rifle = entry.name == "ER97E Elite";
        if (entry.category != 6 && !kraken && !rifle) { continue; }
        effects.Clear();
        rayWorld.beamContacts = 0;
        rayWorld.targetOffset = 50;
        rayWorld.targetDistance = 100;
        PlayerModel player;
        if (!BuildPlayerBody(tables, playerTemplate.moveSet, player) ||
            !EquipPlayerWeapon(tables, playerTemplate.script, entry.data, entry.owner, player) ||
            !CreatePlayerBuffers(player, program)) { return 1; }
        BuildPlayerGameMatrix(identity, 400, 540,
            PlayerModelWorldScale(player, playerTemplate.gameScale, 1), 0, modelToScene);
        SetPlayerInput(player, false, true);
        unsigned beamFrames = 0, missingFrames = 0;
        unsigned ribbonFrames = 0;
        bool sawBeam = false;
        std::vector<GameObjectRef> seen;
        for (int elapsed = 0; elapsed < 6000; elapsed += 16) {
            AdvancePlayer(player, 16);
            effects.Update(player, modelToScene, 0, 16);
            if (effects.GetRibbonCount() > 0) { ++ribbonFrames; }
            bool beam = false;
            for (const auto &shot : effects.GetProjectileStates()) {
                beam = beam || shot.beam;
                bool known = false;
                for (const auto &ref : seen) {
                    if (ref.packHash == shot.resource.packHash && ref.localIndex == shot.resource.localIndex) { known = true; }
                }
                if (known) { continue; }
                seen.push_back(shot.resource);
                std::vector<std::uint8_t> payload;
                if (!tables.ReadSectionResource(shot.resource.packHash, GameSection::Bullet, shot.resource.localIndex, payload)) { return 1; }
                CBullet::Template data;
                CArrayInputStream stream(payload);
                if (!data.Init(stream)) { return 1; }
                CBullet script;
                script.Bind(data, false);
                std::printf("[weapon-effects-check] %s bullet=%08x:%u flags=%x radius=%.2f template-animation=%u active-animation=%d lifetime=%d\n",
                    entry.name.c_str(), shot.resource.packHash, shot.resource.localIndex, data.GetFlags(), data.GetRadius(),
                    data.GetSpriteRef().animation, shot.animation, script.lifetimeMs);
                if (rifle && shot.animation != data.GetSpriteRef().animation) {
                    ++failures;
                    std::printf("[weapon-effects-check] FAIL rifle lost authored sprite animation\n");
                }
                // Native 18 sets a beam's range; it must never become an expiry timer.
                if (shot.beam && script.lifetimeMs < 1000000) {
                    ++failures;
                    std::printf("[weapon-effects-check] FAIL beam range became lifetime\n");
                }
                if (shot.beam) {
                    const int lifetime = script.lifetimeMs;
                    const std::int16_t range[] = {123};
                    script.FunctionResolver(18, range, 1);
                    if (script.maximumBeamLength != 123 || script.lifetimeMs != lifetime) { ++failures; }
                }
                if (script.ribbon.capacity != 0) {
                    std::printf("[weapon-effects-check] ribbon points=%u width=%.1f interval=%u color=%u,%u,%u,%u\n",
                        script.ribbon.capacity, script.ribbon.width, script.ribbon.intervalMs,
                        script.ribbon.color[0], script.ribbon.color[1], script.ribbon.color[2], script.ribbon.color[3]);
                }
            }
            if (beam) { sawBeam = true; ++beamFrames; }
            else if (sawBeam && entry.name == "Infinity Laser") { ++missingFrames; }
            if (elapsed == 992 || elapsed == 2992) {
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                effects.Draw(mvp);
                const std::string path = "out/weapon-effects-" + std::to_string(index) + "-" + std::to_string(elapsed + 16) + ".png";
                if (!window.SaveFrame(path)) { return 1; }
                if (rifle && elapsed == 992) {
                    std::vector<unsigned char> pixels(800 * 350 * 4);
                    glReadPixels(0, 250, 800, 350, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                    unsigned yellow = 0, blue = 0;
                    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
                        const int red = pixels[pixel], green = pixels[pixel + 1], cyan = pixels[pixel + 2];
                        if (red > cyan + 30 && green > cyan + 20) { ++yellow; }
                        if (cyan > red + 30) { ++blue; }
                    }
                    if (yellow <= blue) { ++failures; }
                    std::printf("[weapon-effects-check] rifle pixels yellow=%u blue=%u\n", yellow, blue);
                }
            }
        }
        if (!rifle && !sawBeam) { ++failures; }
        if (missingFrames != 0) { ++failures; }
        if (rayWorld.beamContacts != 0) { ++failures; }
        if ((rifle || kraken) && ribbonFrames == 0) { ++failures; }
        std::printf("[weapon-effects-check] off-axis-beam-contacts=%u\n", rayWorld.beamContacts);
        std::printf("[weapon-effects-check] weapon=%s beam-frames=%u missing-held-frames=%u\n", entry.name.c_str(), beamFrames, missingFrames);
        // A real point on the ray must still stop the beam at the target's edge.
        rayWorld.targetOffset = 0;
        unsigned clipped = 0;
        for (unsigned tick = 0; tick < 250; ++tick) {
            AdvancePlayer(player, 16);
            effects.Update(player, modelToScene, 0, 16);
            for (const auto &shot : effects.GetProjectileStates()) {
                if (!shot.beam) { continue; }
                if (std::abs(shot.length - 90) > 0.1f) { ++failures; }
                ++clipped;
            }
        }
        if (!rifle && clipped == 0) { ++failures; }
        rayWorld.targetDistance = 11;
        unsigned closeFrames = 0, invisibleFrames = 0;
        for (unsigned tick = 0; tick < 250; ++tick) {
            AdvancePlayer(player, 16);
            effects.Update(player, modelToScene, 0, 16);
            bool hasBeam = false;
            for (const auto &shot : effects.GetProjectileStates()) { hasBeam = hasBeam || shot.beam; }
            if (!hasBeam) { continue; }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            effects.Draw(mvp);
            ++closeFrames;
            if (effects.GetDrawnBeamQuadCount() == 0) { ++invisibleFrames; }
            if (closeFrames == 1 && !window.SaveFrame("out/weapon-effects-close-" + std::to_string(index) + ".png")) { return 1; }
        }
        if (invisibleFrames != 0) { ++failures; }
        std::printf("[weapon-effects-check] close-beam-frames=%u invisible=%u\n", closeFrames, invisibleFrames);
        SetPlayerInput(player, false, false);
        for (unsigned tick = 0; tick < 625; ++tick) {
            AdvancePlayer(player, 16);
            effects.Update(player, modelToScene, 0, 16);
        }
        if (effects.GetBulletCount() != 0 || effects.GetRibbonCount() != 0) { ++failures; }
        if (!GLCheckErrors("weapon effect regression")) { ++failures; }
        std::printf("[weapon-effects-check] on-axis-clipped=%u ribbon-frames=%u release-bullets=%zu release-ribbons=%zu\n",
            clipped, ribbonFrames, effects.GetBulletCount(), effects.GetRibbonCount());
    }
    std::printf("[weapon-effects-check] failures=%u\n", failures);
    return failures == 0 ? 0 : 1;
}
