#include "gun_bros_viewer/scenes/BrotherPreview.h"
#pragma once
#include "engine/core/ZPaths.h"
/**
 * @file MeshPreviewInternal.h
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

#include "gun_bros_re/data/ZPackTables.h"
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "gun_bros_re/data/ZArmorCatalog.h"
#include "gun_bros_re/data/ZWeaponCatalog.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/gameplay/collision/Collision.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/effects/CParticleEffect.h"

#include "engine/resources/CArrayInputStream.h"
#include "engine/core/ZMatrix4d.h"
#include "engine/graphics/ZMeshBuffer.h"
#include "engine/graphics/ZPNG.h"
#include "engine/graphics/ZShaderProgram.h"
#include "engine/graphics/ZTexture.h"
#include "engine/platform/ZWindow.h"
#include "engine/platform/ZGLLoader.h"
#include "engine/glu/script/CScript.h"
#include "gun_bros_re/gameplay/armor/CArmor.h"
#include "gun_bros_re/gameplay/weapon/CBullet.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "gun_bros_re/data/CGameObjectPack.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
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

namespace MeshPreviewDetail {

// Where the ported shaders live; the same pair M2 and M3 use.
const char *const kShaderDirectory = Paths::Shaders().c_str();

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
std::string JoinBoneNames(const CMesh &mesh);

/**
 * Report texture coordinates that leave the unit square.
 *
 * M2 had to copy a 1/4096 scale because sprite UVs are stored as integers.
 * Mesh UVs are floats on the wire, so they should already be normalised --
 * this is what proves it, and it stays quiet when nothing is wrong. The ones
 * that do go outside are tiling, which is why model textures wrap.
 */
void ReportTexCoordsOutsideUnitSquare(const CMesh &mesh);

/**
 * Report anything odd about the index buffer.
 *
 * An index at or past the vertex count cannot address a vertex, so it would
 * have to be a primitive-restart marker -- and a strip drawn without honouring
 * one grows a huge triangle joining the end of one piece to the start of the
 * next. Stitches are the other way of joining strips: a repeated index makes a
 * zero-area triangle that the rasteriser drops on its own.
 */
void ReportIndexBuffer(const CMesh &mesh);

/**
 * Report triangles that span a large part of the model.
 *
 * A triangle strip joined wrongly shows up as a long thin triangle reaching
 * from one piece of the model to another. Real geometry at this polygon count
 * has edges a few per cent of the model across, so anything over a third of it
 * is worth naming.
 */
void ReportLongTriangles(const CMesh &mesh);

/** What the mesh survey adds up to. */
struct MeshSurveyTotals {
    unsigned meshes = 0;
    unsigned placeholders = 0;
    unsigned unreadable = 0;
    unsigned withLeftovers = 0;
};

/** Parse every Section 31 resource of one pack and print a line each. */
void SurveyPack(CResPackTOC &pack, MeshSurveyTotals &totals);

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
                       std::uint32_t ordinal);

/** Hand every config of a move set to the sink. */
void EmitMoveSet(const CMoveSetMesh &moveSet, const std::string &owner,
                 IMeshPairSink &sink);

/** Hand a pair of asset refs to the sink. Two refs, so two packs. */
void EmitAssetRefs(const CGameAssetRef &meshRef, const CGameAssetRef &imageRef,
                   const std::string &owner, IMeshPairSink &sink);

/** Fetch one template resource. Returns false when the section is empty. */
bool ReadTemplate(CResPackTOC &pack, CGameObjectPack &objectPack,
                  ZGameSection section, std::uint32_t ordinal,
                  std::vector<std::uint8_t> &payload);

/** Player templates: parsed whole, so their leftover count means something. */
void WalkPlayers(CResPackTOC &pack, ZPackTables &tables, int packIndex,
                 IMeshPairSink &sink);

/**
 * Enemy templates, read only as far as their move set.
 *
 * CEnemy::Template::Init (:67174) opens with a flag byte, a CGameAssetRef and
 * a CScript, and the move set is the fourth thing it reads. Everything after
 * it needs CCollisionData, which belongs to M4a, so this stops there -- and
 * therefore cannot check for leftover bytes. A survey shortcut, not a CEnemy
 * port: that class stays unwritten until its own milestone.
 */
void WalkEnemies(CResPackTOC &pack, ZPackTables &tables, int packIndex,
                 IMeshPairSink &sink);

/** Gun templates: a move set at the end, plus the weapon's own model. */
void WalkGuns(CResPackTOC &pack, ZPackTables &tables, int packIndex,
              IMeshPairSink &sink);

/** Bullet templates: a pair of asset refs, and most name neither. */
void WalkBullets(CResPackTOC &pack, ZPackTables &tables, int packIndex,
                 IMeshPairSink &sink);

/** Armour templates: one model per brother, either of which may be absent. */
void WalkArmor(CResPackTOC &pack, ZPackTables &tables, int packIndex,
               IMeshPairSink &sink);

/**
 * Every model/atlas pair in the archives.
 *
 * Five template types own the 334 models between them. Players, enemies and
 * guns go through a move set; guns, bullets and armour also name models with
 * plain asset refs. Nothing else in the engine addresses section 31 --
 * CMoveSetMesh::LoadMesh (:123157) and three GetResId(0x1E, ...) call sites
 * are the whole list.
 */
void WalkMeshPairs(CResTOCManager &tocManager, ZPackTables &tables,
                   IMeshPairSink &sink);

// ---------------------------------------------------------------------------
// --movesets: follow every pair to a real resource
// ---------------------------------------------------------------------------

/** Pixel size out of a PNG's IHDR, which starts at a fixed offset. */
void ReadPngSize(const std::vector<std::uint8_t> &payload, std::uint32_t &width,
                 std::uint32_t &height);

/** Resolves each pair and prints what it landed on. */
class ReportingSink : public IMeshPairSink {
public:
    explicit ReportingSink(ZPackTables &tables) : m_tables(tables) {}

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
            pair.meshPackHash, ZGameSection::Mesh, pair.meshOrdinal, meshPayload);

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
            pair.imagePackHash, ZGameSection::Png, pair.imageOrdinal, imagePayload);

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
    ZPackTables &m_tables;
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
void PrintTemplateCounts(CResTOCManager &tocManager, ZPackTables &tables);

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

    /** Add unreferenced raw resources too; no invented atlas relationship. */
    void AddRawMeshes(CResTOCManager &toc, ZPackTables &tables) {
        for (std::uint32_t packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
            CResPackTOC *pack = toc.GetPack(static_cast<int>(packIndex));
            const auto count = tables.GetObjectPack(packIndex).GetSectionSpan(ZGameSection::Mesh);
            for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
                bool found = false;
                for (const CatalogEntry &entry : m_entries) {
                    if (entry.meshPackHash == pack->GetPackHash() && entry.meshOrdinal == ordinal) { found = true; break; }
                }
                if (found) { continue; }
                CatalogEntry entry{};
                entry.meshPackHash = pack->GetPackHash();
                entry.meshOrdinal = ordinal;
                entry.imageOrdinal = UINT32_MAX;
                entry.owner = pack->GetShortName() + " mesh " + std::to_string(ordinal) + " [untextured: no authored reference]";
                m_entries.push_back(entry);
            }
        }
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
    ZTexture texture;

    // The playback machinery, used only when the catalogue entry brought a
    // move set. A model named by a plain asset ref shows frame 0 and stops.
    CMoveSetMeshController controller;
    CMeshAnimationController rawAnimation;
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
void BindMoveSet(const CatalogEntry &entry, LoadedModel &model);

/**
 * Put the current pose in the vertex buffer.
 *
 * A model with a move set is asked where it is right now; one without shows
 * the still frame the command line picked. Either way the buffer keeps its
 * last contents if the evaluator has nothing -- better a held pose than a
 * model that blinks out.
 */
void UploadPose(LoadedModel &model, ZMeshBuffer &buffer,
                std::uint32_t stillFrameIndex);

/**
 * Run the animation on before the first frame is drawn.
 *
 * Stepped rather than handed the whole span at once, because the move's speed
 * is rounded to whole milliseconds every update -- one big step and one
 * hundred small ones do not land in the same place, and it is the small ones
 * that match what a running viewer does.
 */
void WarmUp(LoadedModel &model, std::uint32_t advanceMs);

/** "move 3 of 18 -- frames 40..79, speed 1.00" */
void ReportMove(const CatalogEntry &entry, const LoadedModel &model);

/** Fetch and decode one catalogue entry. */
bool LoadModel(ZPackTables &tables, const CatalogEntry &entry, LoadedModel &out);

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
void BuildModelViewProjection(const ZMeshBounds &bounds, const Turntable &view,
                              int drawableWidth, int drawableHeight, float *out);

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
bool BuildViewerCharacter(ZPackTables &tables, const CharacterSink &catalog,
                          std::size_t gunSlot, ZBrotherPreview &out);

}
