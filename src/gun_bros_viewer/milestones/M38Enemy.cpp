#include "engine/core/Paths.h"
/**
 * @file M38Enemy.cpp
 * @brief M3.8 harness: enemies, assembled by their own scripts.
 *
 * Two entry points over one body of knowledge:
 *
 * - `--enemies` runs every enemy's script and prints the part table,
 * - the viewer draws one.
 *
 * The M3.7 player viewer could hardwire its three parts because CBrother::Draw
 * hardwires them too. Nothing here can: an enemy's part table is built at run
 * time by its own script, so this harness has to actually run one.
 */

#include "gun_bros_viewer/milestones/M38Enemy.h"

#include "engine/resources/CArrayInputStream.h"
#include "engine/core/CMatrix4d.h"
#include "engine/graphics/CMeshBuffer.h"
#include "engine/graphics/CPNG.h"
#include "engine/graphics/CShaderProgram.h"
#include "engine/graphics/CTexture.h"
#include "engine/platform/CWindow.h"
#include "engine/platform/GLLoader.h"
#include "engine/glu/script/CScript.h"
#include "engine/glu/script/CScriptState.h"
#include "gun_bros_re/gameplay/CEnemy.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "gun_bros_re/data/CGameObjectPack.h"
#include "engine/graphics/CMesh.h"
#include "engine/graphics/CMeshCamera.h"
#include "engine/graphics/CMoveSetMesh.h"
#include "engine/resources/CResTOCManager.h"
#include "gun_bros_re/data/PackTables.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

const char *const kShaderDirectory = Paths::Shaders().c_str();

// Viewer: the same framing and controls M3.5 and M3.7 use.
constexpr float kModelScreenFraction = 0.7f;
constexpr float kDragToDegrees = 0.5f;
constexpr float kZoomPerNotch = 1.15f;
constexpr float kMinZoom = 0.1f;
constexpr float kMaxZoom = 20.0f;
constexpr float kDepthMargin = 4.0f;
constexpr float kDegreesToRadians = 3.14159265f / 180.0f;
constexpr float kUiTiltDegrees = 90.0f;
constexpr float kGameTiltDegrees = 30.0f;
constexpr float kUiFacingDegrees = 180.0f;

// How many world units the game view spans across the window's short side.
// The engine gets this from CCamera::GetScale, which needs a level; this
// stands in for it, and only the ratio between enemies matters here. At 400 a
// grunt (game scale 74) is a fifth of the screen and a boss (500) overflows
// it, which is the point.
constexpr float kGameViewWorldUnits = 400.0f;

constexpr std::uint64_t kMaxFrameMs = 100;
constexpr std::int32_t kSingleStepMs = 33;
constexpr std::int32_t kWarmUpFrameMs = 16;

// ---------------------------------------------------------------------------
// Reaching an enemy template
// ---------------------------------------------------------------------------

/**
 * One enemy template, read as far as the two draw scales.
 *
 * CEnemy::Template::Init (:67174) reads, in order: a flag byte, a
 * CGameAssetRef, a CScript, the CMoveSetMesh, a GameObjectRef, four numbers,
 * the two scales, and finally a CCollisionData. Only that last one belongs to
 * M4a, so this stops one field short of it.
 *
 * **The two scales are the pair the same model is drawn at in the two places
 * it appears.** CEnemy::Draw (:67499) uses the first for the world -- the
 * model is normalised by its own longest side and then blown up to this many
 * units. CEnemy::DrawUI (:68451) uses the second as a PERCENTAGE on top of a
 * fit-to-box, for the menus and the results screen. The two also differ in
 * orientation, because DrawUI passes 1 for DrawHeirarchy's mode argument and
 * so gets OrientForUI's straight-on view instead of the game's 30-degree lean.
 */
struct EnemyTemplate {
    std::uint32_t packHash;
    std::uint32_t ordinal;
    std::string owner;

    CScript script;
    CMoveSetMesh moveSet;

    // Template word 66, CEnemy::Bind's this[213]. World units across the
    // model's longest side.
    float gameScale;

    // Template word 67, CEnemy::Bind's this[214]. A percentage of the box the
    // menu gives it; DrawUI divides it by 100.
    float uiScalePercent;

    // Named for their template offsets, which is the one thing certainly true
    // about them. Read so the two scales land at the right place.
    GameObjectRef objectRef104;
    std::uint16_t experienceReward;
    std::uint16_t xplodiumReward;
    std::uint8_t flag117;
    std::uint8_t radius116;

    EnemyTemplate()
        : packHash(0),
          ordinal(0),
          gameScale(0.0f),
          uiScalePercent(0.0f),
          experienceReward(0),
          xplodiumReward(0),
          flag117(0),
          radius116(0) {}
};

/** "pack1 enemy 18" */
std::string OwnerLabel(const std::string &packName, std::uint32_t ordinal) {
    char text[128];
    std::snprintf(text, sizeof(text), "%s enemy %u", packName.c_str(), ordinal);
    return std::string(text);
}

/** Every enemy template in every pack, in the order the archives hand them over. */
void CollectEnemies(CResTOCManager &tocManager, PackTables &tables,
                    std::vector<EnemyTemplate> &out) {
    for (std::uint32_t packIndex = 0; packIndex < tocManager.GetPackCount();
         ++packIndex) {
        CResPackTOC *pack = tocManager.GetPack(static_cast<int>(packIndex));
        CGameObjectPack &objectPack =
            tables.GetObjectPack(static_cast<int>(packIndex));
        const std::uint32_t count = objectPack.GetObjectCount(GameSection::Enemy);

        for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
            std::vector<std::uint8_t> payload;
            const std::uint32_t handle =
                objectPack.GetHandle(GameSection::Enemy, ordinal);
            if (handle == 0 || !pack->GetResource(handle, payload)) {
                continue;
            }

            EnemyTemplate entry;
            entry.ordinal = ordinal;
            entry.owner = OwnerLabel(pack->GetShortName(), ordinal);

            CArrayInputStream stream(payload);
            stream.ReadUInt8();

            CGameAssetRef assetRef;
            assetRef.Init(stream);

            entry.script.Load(stream);
            if (!entry.moveSet.Init(stream)) {
                std::printf("[m38] %s: move set unreadable\n", entry.owner.c_str());
                continue;
            }

            entry.objectRef104.Init(stream);
            entry.experienceReward = stream.ReadUInt16();
            entry.xplodiumReward = stream.ReadUInt16();
            entry.flag117 = stream.ReadUInt8();
            entry.radius116 = stream.ReadUInt8();
            entry.gameScale = static_cast<float>(stream.ReadUInt16());
            entry.uiScalePercent = static_cast<float>(stream.ReadUInt16());
            if (stream.Overran()) {
                std::printf("[m38] %s: template truncated before its scales\n",
                            entry.owner.c_str());
                continue;
            }

            // A move set names one pack for every model in it, and that is the
            // pack an enemy's parts come out of.
            entry.packHash = entry.moveSet.GetPackHash();
            out.push_back(entry);
        }
    }
}

// ---------------------------------------------------------------------------
// Loading what a move set names
// ---------------------------------------------------------------------------

/** One mesh config of a move set, decoded and uploaded. */
struct LoadedConfig {
    CMesh mesh;
    CTexture texture;
    CMeshBuffer buffer;
    bool valid;

    LoadedConfig() : valid(false) {}
};

/**
 * Every model a move set names, whether or not the script ends up using it.
 *
 * The original's resource loader does the same: CMoveSetMesh::Load (:123213)
 * queues every config before anything runs, because the script that picks
 * between them has not run yet.
 */
bool LoadConfigs(PackTables &tables, const EnemyTemplate &entry,
                 std::vector<std::unique_ptr<LoadedConfig>> &out,
                 bool createBuffers, const CShaderProgram *program) {
    out.clear();
    for (std::size_t i = 0; i < entry.moveSet.GetMeshConfigs().size(); ++i) {
        const MeshConfig &config = entry.moveSet.GetMeshConfigs()[i];
        std::unique_ptr<LoadedConfig> loaded(new LoadedConfig());

        std::vector<std::uint8_t> meshPayload;
        if (!tables.ReadSectionResource(entry.packHash, GameSection::Mesh,
                                        config.meshOrdinal, meshPayload)) {
            std::printf("[m38] %s: mesh %u unreadable\n", entry.owner.c_str(),
                        config.meshOrdinal);
            out.push_back(std::move(loaded));
            continue;
        }

        CArrayInputStream meshStream(meshPayload);
        if (!loaded->mesh.Init(meshStream)) {
            out.push_back(std::move(loaded));
            continue;
        }

        if (createBuffers) {
            std::vector<std::uint8_t> imagePayload;
            PNGImage decoded;
            if (!tables.ReadSectionResource(entry.packHash, GameSection::Png,
                                            config.imageOrdinal, imagePayload) ||
                !PNGDecode(imagePayload, decoded) ||
                !loaded->texture.Create(decoded, GL_REPEAT)) {
                std::printf("[m38] %s: atlas %u unreadable\n", entry.owner.c_str(),
                            config.imageOrdinal);
                out.push_back(std::move(loaded));
                continue;
            }

            if (!loaded->buffer.Create(*program) ||
                !loaded->buffer.SetMesh(loaded->mesh)) {
                out.push_back(std::move(loaded));
                continue;
            }
        }

        loaded->valid = true;
        out.push_back(std::move(loaded));
    }
    return !out.empty();
}

/** The mesh pointers CEnemy::Bind wants, indexed by config. */
std::vector<const CMesh *> ConfigMeshes(
    const std::vector<std::unique_ptr<LoadedConfig>> &configs) {
    std::vector<const CMesh *> meshes(configs.size(), nullptr);
    for (std::size_t i = 0; i < configs.size(); ++i) {
        if (configs[i]->valid) {
            meshes[i] = &configs[i]->mesh;
        }
    }
    return meshes;
}

// ---------------------------------------------------------------------------
// --enemies: what each script assembles
// ---------------------------------------------------------------------------

void ReportPartTable(std::size_t index, const EnemyTemplate &entry,
                     CEnemy &enemy) {
    std::printf("  %3zu  %-18s %zu configs, %u moves, %zu states, "
                "scale %.0f game / %.0f%% ui -> %u part%s\n",
                index, entry.owner.c_str(), entry.moveSet.GetMeshConfigs().size(),
                static_cast<unsigned>(entry.moveSet.GetMoves().size()),
                entry.script.GetStates().size(), entry.gameScale,
                entry.uiScalePercent, enemy.GetPartCount(),
                enemy.GetPartCount() == 1 ? "" : "s");

    for (std::uint32_t i = 0; i < enemy.GetPartCount(); ++i) {
        const EnemyPart &part = enemy.GetPart(i);
        const std::int32_t moveIndex = part.controller.GetMoveIndex();
        const std::int32_t configIndex = part.controller.GetMeshConfigIndex();

        std::printf("      part %u: move %d, config %d", i, moveIndex, configIndex);
        if (part.boneIndex == kEnemyNoBoneIndex) {
            std::printf(", free");
        } else {
            std::printf(", on bone %d", part.boneIndex);
        }
        if (part.extraAngleDegreesPerSecond != 0.0f) {
            std::printf(", spins %.0f deg/s about %.2f %.2f %.2f",
                        part.extraAngleDegreesPerSecond, part.extraAxisX,
                        part.extraAxisY, part.extraAxisZ);
        }
        std::printf("\n");
    }
}

// ---------------------------------------------------------------------------
// --enemyanims: the animations a script actually plays
//
// A move is not an animation. It is one window onto the mesh's frame bank, and
// what the game plays is a STATE: a chain of move indices that the interpreter
// walks as each one runs out. An enemy's idle, its attack and its death are
// states, so this is the list that maps onto those names.
// ---------------------------------------------------------------------------

/**
 * The sequence a state plays, following the parent chain.
 *
 * States inherit: one with no sequence of its own takes its parent's, which is
 * how a family of states shares one animation. Walked here rather than through
 * CScriptState::GetSequence so the survey needs no interpreter.
 */
const std::vector<std::uint8_t> *SequenceOfState(const CScript &script,
                                                 std::uint8_t stateId) {
    // At most one pass over the states: a parent chain that loops would
    // otherwise never end, and nothing guarantees the data is a tree.
    for (std::size_t hops = 0; hops < script.GetStates().size(); ++hops) {
        if (stateId >= script.GetStates().size()) {
            return nullptr;
        }

        const CScriptState &state = script.GetStates()[stateId];
        if (!state.GetOwnSequence().empty()) {
            return &state.GetOwnSequence();
        }
        if (state.GetParent() == kNoParentState) {
            return nullptr;
        }
        stateId = state.GetParent();
    }
    return nullptr;
}

/**
 * How long one move runs, in milliseconds.
 *
 * The move set only names a frame RANGE; the milliseconds live in the mesh,
 * because it is the frames that carry the timestamps. So this needs the loaded
 * models, which is why the listing loads them even though it draws nothing.
 */
std::int32_t MoveDurationMs(const EnemyTemplate &entry,
                            const std::vector<std::unique_ptr<LoadedConfig>> &configs,
                            std::uint8_t moveIndex) {
    if (moveIndex >= entry.moveSet.GetMoves().size()) {
        return 0;
    }

    const MeshMove &move = entry.moveSet.GetMoves()[moveIndex];
    if (move.meshConfigIndex >= configs.size() ||
        !configs[move.meshConfigIndex]->valid) {
        return 0;
    }

    const std::vector<MeshFrame> &frames =
        configs[move.meshConfigIndex]->mesh.GetFrames();
    if (move.firstFrame >= frames.size() || move.lastFrame >= frames.size()) {
        return 0;
    }
    return frames[move.lastFrame].timeMs - frames[move.firstFrame].timeMs;
}

/**
 * Spell out what a sequence plays: every move it chains, and how long each
 * runs. "moves 4" on its own says nothing; this says what move 4 IS.
 */
void PrintSequence(const EnemyTemplate &entry,
                   const std::vector<std::unique_ptr<LoadedConfig>> &configs,
                   const std::vector<std::uint8_t> &sequence) {
    std::int32_t total = 0;
    for (std::size_t i = 0; i < sequence.size(); ++i) {
        const std::uint8_t moveIndex = sequence[i];
        const std::int32_t durationMs = MoveDurationMs(entry, configs, moveIndex);
        total += durationMs;

        if (moveIndex < entry.moveSet.GetMoves().size()) {
            const MeshMove &move = entry.moveSet.GetMoves()[moveIndex];
            std::printf(" %s move %u [cfg %u, frames %u..%u, %d ms]",
                        i == 0 ? "--" : "then", moveIndex, move.meshConfigIndex,
                        move.firstFrame, move.lastFrame, durationMs);
        } else {
            std::printf(" %s move %u [past the set]", i == 0 ? "--" : "then",
                        moveIndex);
        }
    }
    std::printf("  = %d ms\n", total);
}

/** One enemy's states, with the moves each of them chains. */
void ReportStates(std::size_t index, const EnemyTemplate &entry,
                  const std::vector<std::unique_ptr<LoadedConfig>> &configs) {
    std::printf("  %3zu  %-18s %zu states, %u moves\n", index,
                entry.owner.c_str(), entry.script.GetStates().size(),
                static_cast<unsigned>(entry.moveSet.GetMoves().size()));

    for (std::size_t stateId = 0; stateId < entry.script.GetStates().size();
         ++stateId) {
        const CScriptState &state = entry.script.GetStates()[stateId];
        const std::vector<std::uint8_t> *sequence =
            SequenceOfState(entry.script, static_cast<std::uint8_t>(stateId));

        std::printf("      state %2zu", stateId);
        if (state.GetParent() != kNoParentState) {
            std::printf(" (parent %u)", state.GetParent());
        }
        if (sequence == nullptr || sequence->empty()) {
            std::printf(" -- no animation\n");
            continue;
        }
        if (state.GetOwnSequence().empty()) {
            std::printf(" -- inherited");
        }

        PrintSequence(entry, configs, *sequence);
    }
}

// ---------------------------------------------------------------------------
// The viewer
// ---------------------------------------------------------------------------

/**
 * Which of the two ways the game draws an enemy is on screen.
 *
 * They differ in more than the angle. `Game` is CEnemy::Draw (:67499): a
 * 30-degree lean, and the model blown up to `gameScale` world units across its
 * longest side, so a grunt at 74 really is a sixth the size of a boss at 500.
 * `Ui` is CEnemy::DrawUI (:68451): straight on, and fitted to the box it is
 * given before `uiScalePercent` trims it.
 */
enum class ViewMode { Game, Ui };

/**
 * What M and N step through.
 *
 * `Moves` walks the move set one raw move at a time, which is how you see
 * every window the artists cut. `States` walks the script's states, which is
 * what the game actually plays: a state chains several moves, and it is states
 * that correspond to idle, attack and death.
 */
enum class StepTarget { Moves, States };

struct Turntable {
    float spinDegrees;
    float extraTilt;
    float zoom;
    ViewMode mode;

    /** The lean each mode is drawn at. */
    float TiltDegrees() const {
        if (mode == ViewMode::Ui) {
            return kUiTiltDegrees;
        }
        return kGameTiltDegrees;
    }
};

/** One enemy on screen: its models, its script, and the parts it assembled. */
struct LoadedEnemy {
    std::vector<std::unique_ptr<LoadedConfig>> configs;
    std::vector<const CMesh *> configMeshes;
    CEnemy enemy;

    // Reused between frames so the evaluator does not reallocate.
    std::vector<float> pose;

    // Which move the viewer is holding on part 0 once M or N has taken it away
    // from the script. kNoMoveIndex until then.
    std::int32_t bodyMoveIndex;

    // Which state the viewer is holding, or -1 while the script runs free.
    // Both a cursor and a lock -- see StepState for why it has to be both.
    std::int32_t heldStateId;

    LoadedEnemy() : bodyMoveIndex(kNoMoveIndex), heldStateId(-1) {}
};

std::int32_t PartConfigIndex(const LoadedEnemy &loaded, std::uint32_t partIndex);

/**
 * Load an enemy's models, run its script, and take whatever it assembled.
 *
 * The order matters and it is the original's: every config is loaded first,
 * then the script runs. A script picks between models that are already there.
 */
bool BuildEnemy(PackTables &tables, const EnemyTemplate &entry,
                const CShaderProgram &program, LoadedEnemy &out) {
    if (!LoadConfigs(tables, entry, out.configs, true, &program)) {
        return false;
    }

    out.configMeshes = ConfigMeshes(out.configs);
    out.enemy.Bind(entry.script, entry.moveSet, out.configMeshes);
    out.enemy.SpawnForUI();

    // Export 3 is the menu's, and for eighteen of the seventy-eight templates
    // it gives part 0 nothing to play: a turret assembled this way is a bare
    // base with no barrel. Run the LEVEL export instead when that happens --
    // it is the one the game itself uses for these, and it is what puts the
    // second part on.
    if (out.enemy.GetPart(0).controller.GetMoveIndex() == kNoMoveIndex) {
        out.enemy.Spawn();
        std::printf("[m38] %s: export 3 assembled nothing; spawned as a level "
                    "would instead\n",
                    entry.owner.c_str());
    }

    // Still nothing: no export animates part 0. Show its first move rather
    // than a blank window, and say that is what happened -- this is the viewer
    // being helpful, not the engine doing it.
    if (out.enemy.GetPart(0).controller.GetMoveIndex() == kNoMoveIndex &&
        !entry.moveSet.GetMoves().empty()) {
        std::printf("[m38] %s: no spawn export set a move; falling back to "
                    "move 0\n",
                    entry.owner.c_str());
        out.enemy.GetPart(0).controller.SetMove(0);
    }

    std::printf("[m38] %s: %zu configs -> %u parts\n", entry.owner.c_str(),
                out.configs.size(), out.enemy.GetPartCount());

    return true;
}

/** Which config a part is currently showing, or -1 when it shows nothing. */
std::int32_t PartConfigIndex(const LoadedEnemy &loaded, std::uint32_t partIndex) {
    const std::int32_t configIndex =
        loaded.enemy.GetPart(partIndex).controller.GetMeshConfigIndex();
    if (configIndex < 0 ||
        static_cast<std::size_t>(configIndex) >= loaded.configs.size() ||
        !loaded.configs[configIndex]->valid) {
        return -1;
    }
    return configIndex;
}

/**
 * Rewrite every live part's vertices.
 *
 * Two parts can be showing the same config, and then they share one vertex
 * buffer -- so this cannot upload once and draw twice. It uploads immediately
 * before each part is drawn instead; see DrawEnemy.
 */
void UploadPart(LoadedEnemy &loaded, std::uint32_t partIndex,
                std::int32_t configIndex) {
    const CMeshAnimationController &animation =
        loaded.enemy.GetPart(partIndex).controller.GetAnimation();
    LoadedConfig &config = *loaded.configs[configIndex];

    if (animation.Evaluate(loaded.pose)) {
        config.buffer.SetVertices(loaded.pose);
    } else {
        config.buffer.SetFrame(config.mesh, 0);
    }
}

/**
 * Walk part 0's move forward or back through the set's move list.
 *
 * Part 0 is the body, and its move is the one the script chose on spawn. This
 * is the viewer's way of seeing the others -- the game reaches them through
 * state changes, which need a level. Moves whose mesh config did not load are
 * skipped rather than left to fail silently at SetMove.
 */
/**
 * Take part 0's move away from the script, once.
 *
 * Two things have to happen together. The lock stops Refresh putting the
 * state's own sequence back -- without it the body flips between the two moves
 * of the current state's sequence and a keypress looks like it did nothing.
 * And the loop flag has to go on, because part 0 is authored NOT to loop: in
 * the game the script picks the next move when one runs out, and with the
 * script no longer picking, an unlooped move would freeze on its last frame.
 */
void TakeOverBodyMove(LoadedEnemy &loaded) {
    if (loaded.enemy.IsBodyMoveLocked()) {
        return;
    }

    CMoveSetMeshController &controller = loaded.enemy.GetPart(0).controller;
    loaded.enemy.SetBodyMoveLocked(true);
    controller.GetAnimation().SetLooped(true);

    // Start from whatever the script had chosen, so the first press steps one
    // along from what is on screen rather than jumping to the top of the list.
    loaded.bodyMoveIndex = controller.GetMoveIndex();
    std::printf("[m38] body move taken over from the script; it now loops\n");
}

/** Hold one particular move on the body, by its index into the set. */
void SelectBodyMove(const EnemyTemplate &entry, LoadedEnemy &loaded,
                    std::int32_t moveIndex) {
    if (moveIndex < 0 ||
        static_cast<std::size_t>(moveIndex) >= entry.moveSet.GetMoves().size()) {
        return;
    }

    TakeOverBodyMove(loaded);
    loaded.bodyMoveIndex = moveIndex;

    CMoveSetMeshController &controller = loaded.enemy.GetPart(0).controller;
    CMeshAnimationController &animation = controller.GetAnimation();
    controller.SetMove(moveIndex);
    animation.SetTimeMs(animation.GetRangeStartMs());

    const MeshMove &move = entry.moveSet.GetMoves()[moveIndex];
    std::printf("[m38] body move %d of %zu -- config %u, frames %u..%u, %d ms\n",
                moveIndex, entry.moveSet.GetMoves().size(), move.meshConfigIndex,
                move.firstFrame, move.lastFrame, animation.GetRangeDurationMs());
}

/**
 * Walk the script's states forward or back, and play the one landed on.
 *
 * This is the animation the game plays, as against the raw move StepBodyMove
 * picks: entering a state starts its whole sequence, and the interpreter
 * chains the moves as each runs out. States with no sequence anywhere in their
 * parent chain are skipped -- they are pure logic and would leave the model
 * standing on whatever was last on screen.
 */
void StepState(const EnemyTemplate &entry, LoadedEnemy &loaded, int step) {
    const std::size_t stateCount = entry.script.GetStates().size();
    if (stateCount == 0) {
        return;
    }

    // The cursor is the viewer's own, NOT the interpreter's current state.
    // A script does not sit still in the state it is put in: enemy 2 entered
    // at state 3 has walked itself back to state 2 within a second. Stepping
    // from wherever the script happens to be would then bounce between two
    // states forever, which is what reading GetStateId() here used to do.
    int stateId = loaded.heldStateId;
    if (stateId < 0) {
        stateId = static_cast<int>(loaded.enemy.GetStateId());
    }

    for (std::size_t tried = 0; tried < stateCount; ++tried) {
        stateId = (stateId + step + static_cast<int>(stateCount)) %
                  static_cast<int>(stateCount);

        const std::vector<std::uint8_t> *sequence =
            SequenceOfState(entry.script, static_cast<std::uint8_t>(stateId));
        if (sequence == nullptr || sequence->empty()) {
            // Said out loud, because a silent skip looks like a lost state.
            // Such a state is pure logic: entering it would leave whatever was
            // last on screen exactly where it was.
            std::printf("[m38] state %d has no animation, skipped\n", stateId);
            continue;
        }

        loaded.heldStateId = stateId;
        loaded.enemy.SetState(static_cast<std::uint8_t>(stateId));

        // Ids are zero-based, so the last one is stateCount - 1. Spelt out
        // because "state 6 of 7" reads like there is a seventh still to come.
        std::printf("[m38] state %d of 0..%zu", stateId, stateCount - 1);
        PrintSequence(entry, loaded.configs, *sequence);
        return;
    }

    std::printf("[m38] no state of this enemy carries an animation\n");
}

/**
 * Put the held state back if the script has wandered off it.
 *
 * A state is not a loop: its sequence plays out and then the script's own
 * logic moves on, usually back to an idle. That is right in a level and wrong
 * in a previewer, where the point is to watch one animation repeat. Silent,
 * because it happens several times a second.
 */
void HoldState(LoadedEnemy &loaded) {
    if (loaded.heldStateId < 0) {
        return;
    }
    if (loaded.enemy.GetStateId() == loaded.heldStateId) {
        return;
    }
    loaded.enemy.SetState(static_cast<std::uint8_t>(loaded.heldStateId));
}

void StepBodyMove(const EnemyTemplate &entry, LoadedEnemy &loaded, int step) {
    const std::size_t moveCount = entry.moveSet.GetMoves().size();
    if (moveCount == 0) {
        return;
    }

    TakeOverBodyMove(loaded);
    if (loaded.bodyMoveIndex == kNoMoveIndex) {
        loaded.bodyMoveIndex = 0;
    }

    // At most one lap: a set every one of whose configs failed to load would
    // otherwise spin here forever.
    for (std::size_t tried = 0; tried < moveCount; ++tried) {
        loaded.bodyMoveIndex = static_cast<std::int32_t>(
            (loaded.bodyMoveIndex + step + static_cast<int>(moveCount)) %
            static_cast<int>(moveCount));

        const std::uint8_t configIndex =
            entry.moveSet.GetMoves()[loaded.bodyMoveIndex].meshConfigIndex;
        if (configIndex >= loaded.configs.size() ||
            !loaded.configs[configIndex]->valid) {
            continue;
        }

        SelectBodyMove(entry, loaded, loaded.bodyMoveIndex);
        return;
    }
}

/**
 * The box to frame and normalise against.
 *
 * The game path uses PART 0's mesh and nothing else -- CEnemy::Draw reads
 * `part0.mesh->inverseExtent` straight out of the mesh -- so a turret hanging
 * off a tank does not change how big the tank is drawn. The menu path measures
 * the assembled model instead, through GetBoundsInternal, so this widens to
 * every part that hangs off nothing.
 */
MeshBounds EnemyBounds(const LoadedEnemy &loaded, bool part0Only) {
    MeshBounds combined = MeshBounds();
    bool any = false;

    for (std::uint32_t i = 0; i < loaded.enemy.GetPartCount(); ++i) {
        if (part0Only && i > 0) {
            break;
        }
        if (loaded.enemy.GetPart(i).boneIndex != kEnemyNoBoneIndex) {
            continue;
        }
        const std::int32_t configIndex = PartConfigIndex(loaded, i);
        if (configIndex < 0) {
            continue;
        }

        const MeshBounds &bounds = loaded.configs[configIndex]->mesh.GetBounds();
        any = true;
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

    // An enemy every part of which hangs off a bone would otherwise frame to
    // nothing. Fall back to part 0, which is the one everything hangs from.
    if (!any && loaded.enemy.GetPartCount() > 0) {
        const std::int32_t configIndex = PartConfigIndex(loaded, 0);
        if (configIndex >= 0) {
            combined = loaded.configs[configIndex]->mesh.GetBounds();
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

/**
 * Model to clip space, at whichever of the two scales the mode calls for.
 *
 * Both modes start the same way -- push the model to the origin by its own
 * centre and divide by its longest side, which is the `inverseExtent`
 * ComputeBounds worked out and the same number the engine multiplies by. What
 * differs is what happens next:
 *
 * - **Game**: multiply by `gameScale` to get world units, and look at a fixed
 *   window of world units. Relative size between enemies is then the real
 *   thing -- the engine's own camera scale is the only factor missing, and it
 *   is common to everything on screen.
 * - **Ui**: fill a fixed fraction of the window, then apply `uiScalePercent`.
 *   That is DrawUI's fit-to-box followed by its percentage.
 *
 * @param bounds The box to frame and normalise against.
 * @param scale  `gameScale` in Game mode, `uiScalePercent` in Ui mode.
 */
void BuildBaseMatrix(const MeshBounds &bounds, const Turntable &view, float scale,
                     int drawableWidth, int drawableHeight, float *out) {
    float centre[kMatrix4dElements];
    Matrix4dTranslation(-bounds.centerX, -bounds.centerY, -bounds.centerZ, centre);

    float modelScale = bounds.inverseExtent;
    float screenFraction = kModelScreenFraction;
    if (view.mode == ViewMode::Game) {
        modelScale *= scale;
        screenFraction = 1.0f / kGameViewWorldUnits;
    } else {
        screenFraction *= scale * 0.01f;
    }

    float normalise[kMatrix4dElements];
    Matrix4dScale(modelScale, normalise);

    float spin[kMatrix4dElements];
    Matrix4dRotationZ(view.spinDegrees * kDegreesToRadians, spin);

    float tilt[kMatrix4dElements];
    Matrix4dRotationX((view.TiltDegrees() + view.extraTilt) * kDegreesToRadians,
                      tilt);

    float normalised[kMatrix4dElements];
    Matrix4dMultiply(normalise, centre, normalised);

    float spun[kMatrix4dElements];
    Matrix4dMultiply(tilt, spin, spun);

    float model[kMatrix4dElements];
    Matrix4dMultiply(spun, normalised, model);

    const float shortSide = static_cast<float>(
        drawableWidth < drawableHeight ? drawableWidth : drawableHeight);
    const float unitsPerScreen = 1.0f / (screenFraction * view.zoom);
    const float viewWidth = unitsPerScreen * static_cast<float>(drawableWidth) / shortSide;
    const float viewHeight = unitsPerScreen * static_cast<float>(drawableHeight) / shortSide;

    // The depth box is sized off the view, not off the model: in Game mode a
    // 500-unit boss is much deeper than the one-unit box the Ui mode needs.
    float projection[kMatrix4dElements];
    Matrix4dOrthoCentred(viewWidth, viewHeight, kDepthMargin * unitsPerScreen,
                         projection);

    Matrix4dMultiply(projection, model, out);
}

/**
 * Draw every part.
 *
 * The attachment comes from PART 0 -- its mesh, at its animation time --
 * whichever part is being drawn. CEnemy::Draw (:67499) reads both off part 0's
 * controller and passes the same pair to every GetNodeAt it makes.
 */
void DrawEnemy(LoadedEnemy &loaded, const CShaderProgram &program,
               const float *base) {
    const CMeshAnimationController &parent =
        loaded.enemy.GetPart(0).controller.GetAnimation();

    for (std::uint32_t i = 0; i < loaded.enemy.GetPartCount(); ++i) {
        const std::int32_t configIndex = PartConfigIndex(loaded, i);
        if (configIndex < 0) {
            continue;
        }

        UploadPart(loaded, i, configIndex);

        const EnemyPart &part = loaded.enemy.GetPart(i);
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

        LoadedConfig &config = *loaded.configs[configIndex];
        config.buffer.Draw(program, mvp, config.texture);
    }
}

}  // namespace

int RunEnemySurvey(const std::string &bigDirectory) {
    std::printf("=== M3.8: what each enemy script assembles ===\n\n");

    CResTOCManager tocManager;
    if (!tocManager.Init(bigDirectory, kArtSetXga) || !tocManager.Bind()) {
        return 1;
    }

    PackTables tables(tocManager);
    std::vector<EnemyTemplate> enemies;
    CollectEnemies(tocManager, tables, enemies);

    unsigned assembled = 0;
    unsigned attached = 0;
    for (std::size_t i = 0; i < enemies.size(); ++i) {
        std::vector<std::unique_ptr<LoadedConfig>> configs;
        LoadConfigs(tables, enemies[i], configs, false, nullptr);

        CEnemy enemy;
        enemy.Bind(enemies[i].script, enemies[i].moveSet, ConfigMeshes(configs));
        enemy.SpawnForUI();
        if (enemy.GetPart(0).controller.GetMoveIndex() == kNoMoveIndex) {
            enemy.Spawn();
        }

        ReportPartTable(i, enemies[i], enemy);
        if (enemy.GetPartCount() > 1) {
            assembled++;
        }
        for (std::uint32_t part = 0; part < enemy.GetPartCount(); ++part) {
            if (enemy.GetPart(part).boneIndex != kEnemyNoBoneIndex) {
                attached++;
            }
        }
    }

    std::printf("\n%zu enemies, %u built from more than one part, "
                "%u parts hung off a bone\n",
                enemies.size(), assembled, attached);
    return 0;
}

int RunEnemyAnimationSurvey(const std::string &bigDirectory) {
    std::printf("=== M3.8: the animations each enemy script plays ===\n\n");

    CResTOCManager tocManager;
    if (!tocManager.Init(bigDirectory, kArtSetXga) || !tocManager.Bind()) {
        return 1;
    }

    PackTables tables(tocManager);
    std::vector<EnemyTemplate> enemies;
    CollectEnemies(tocManager, tables, enemies);

    unsigned animated = 0;
    unsigned inherited = 0;
    for (std::size_t i = 0; i < enemies.size(); ++i) {
        // Meshes but no textures and no GL: the timestamps a move's duration
        // is measured from live in the mesh, not in the move set.
        std::vector<std::unique_ptr<LoadedConfig>> configs;
        LoadConfigs(tables, enemies[i], configs, false, nullptr);

        ReportStates(i, enemies[i], configs);

        for (std::size_t stateId = 0;
             stateId < enemies[i].script.GetStates().size(); ++stateId) {
            const CScriptState &state = enemies[i].script.GetStates()[stateId];
            const std::vector<std::uint8_t> *sequence =
                SequenceOfState(enemies[i].script,
                                static_cast<std::uint8_t>(stateId));
            if (sequence == nullptr || sequence->empty()) {
                continue;
            }
            animated++;
            if (state.GetOwnSequence().empty()) {
                inherited++;
            }
        }
    }

    std::printf("\n%zu enemies, %u states carry an animation, %u of those "
                "inherit it from a parent state\n",
                enemies.size(), animated, inherited);
    return 0;
}

int RunM38Enemy(const std::string &bigDirectory, std::uint32_t startIndex,
                float spinDegrees, const std::string &screenshotPath,
                std::uint32_t advanceMs, std::int32_t bodyMoveIndex,
                bool stepStates, std::int32_t stateIndex) {
    const StepTarget stepTarget =
        stepStates ? StepTarget::States : StepTarget::Moves;
    std::printf("=== M3.8: an enemy, assembled by its own script ===\n\n");

    CResTOCManager tocManager;
    if (!tocManager.Init(bigDirectory, kArtSetXga) || !tocManager.Bind()) {
        return 1;
    }

    PackTables tables(tocManager);
    std::vector<EnemyTemplate> enemies;
    CollectEnemies(tocManager, tables, enemies);
    if (enemies.empty()) {
        std::printf("[m38] no enemy template parsed\n");
        return 1;
    }
    std::printf("\n[m38] %zu enemies\n", enemies.size());

    std::size_t slot = startIndex;
    if (slot >= enemies.size()) {
        slot = 0;
    }

    CWindow window;
    if (!window.Open("gun_bros_re -- M3.8", kDefaultWindowWidth,
                     kDefaultWindowHeight)) {
        return 1;
    }

    CShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) {
        return 1;
    }

    std::unique_ptr<LoadedEnemy> loaded(new LoadedEnemy());
    if (!BuildEnemy(tables, enemies[slot], program, *loaded)) {
        return 1;
    }
    if (stateIndex >= 0) {
        loaded->heldStateId = stateIndex;
        loaded->enemy.SetState(static_cast<std::uint8_t>(stateIndex));
        std::printf("[m38] entered state %d\n", stateIndex);
    }
    if (bodyMoveIndex >= 0) {
        SelectBodyMove(enemies[slot], *loaded, bodyMoveIndex);
    }
    for (std::uint32_t elapsed = 0; elapsed < advanceMs; elapsed += kWarmUpFrameMs) {
        loaded->enemy.Update(kWarmUpFrameMs);
        if (stepTarget == StepTarget::States) {
            HoldState(*loaded);
        }
    }
    std::printf("[m38] after %u ms: state %u, body on move %d%s\n", advanceMs,
                loaded->enemy.GetStateId(),
                loaded->enemy.GetPart(0).controller.GetMoveIndex(),
                loaded->enemy.IsBodyMoveLocked() ? " (held)" : " (script's)");

    glEnable(GL_DEPTH_TEST);

    // Meshes carry alpha: the turret's ground shadow is a faded disc, and
    // without this it draws as a white plate.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Turntable view;
    view.spinDegrees = kUiFacingDegrees + spinDegrees;
    view.extraTilt = 0.0f;
    view.zoom = 1.0f;

    // The game view is what an enemy normally looks like, so that is what the
    // viewer opens on; G swaps to the menu one.
    view.mode = ViewMode::Game;

    std::printf("\n[m38] left/right: enemy, up/down: ten at a time, "
                "M/N: %s, space: pause, period: step, drag: turn, "
                "wheel: zoom, G: game/menu view, Home: reset view, Esc: quit\n",
                stepTarget == StepTarget::States
                    ? "animation (a script state)"
                    : "body move (one raw move)");

    std::uint64_t previousTicks = window.GetTicksMs();
    bool paused = false;
    bool singleStep = false;
    bool reportedFirstFrame = false;

    while (window.PumpEvents()) {
        int drawableWidth = 0;
        int drawableHeight = 0;
        window.GetDrawableSize(drawableWidth, drawableHeight);

        const std::size_t previousSlot = slot;
        int moveStep = 0;
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None;
             key = window.TakeKeyPress()) {
            const std::size_t count = enemies.size();
            if (key == KeyCode::Right) {
                slot = (slot + 1) % count;
            } else if (key == KeyCode::Left) {
                slot = (slot + count - 1) % count;
            } else if (key == KeyCode::Down) {
                slot = (slot + 10) % count;
            } else if (key == KeyCode::Up) {
                slot = (slot + count - 10) % count;
            } else if (key == KeyCode::Space) {
                paused = !paused;
                std::printf("[m38] %s\n", paused ? "paused" : "playing");
            } else if (key == KeyCode::Period) {
                singleStep = true;
            } else if (key == KeyCode::M) {
                moveStep = 1;
            } else if (key == KeyCode::N) {
                moveStep = -1;
            } else if (key == KeyCode::G) {
                // Both halves swap together: the lean and the scale belong to
                // the same one of the game's two draw paths.
                if (view.mode == ViewMode::Game) {
                    view.mode = ViewMode::Ui;
                    std::printf("[m38] menu view, %.0f%% of its box\n",
                                enemies[slot].uiScalePercent);
                } else {
                    view.mode = ViewMode::Game;
                    std::printf("[m38] game view, %.0f units across\n",
                                enemies[slot].gameScale);
                }
            } else if (key == KeyCode::Home) {
                view.spinDegrees = kUiFacingDegrees + spinDegrees;
                view.extraTilt = 0.0f;
                view.zoom = 1.0f;
            }
        }

        if (slot != previousSlot) {
            // Zero-based, so it is the number --enemy and --enemies both use.
            std::printf("\n[m38] --- enemy %zu of %zu ---\n", slot,
                        enemies.size());

            std::unique_ptr<LoadedEnemy> replacement(new LoadedEnemy());
            if (BuildEnemy(tables, enemies[slot], program, *replacement)) {
                loaded = std::move(replacement);
            } else {
                std::printf("[m38] staying on the previous enemy\n");
                slot = previousSlot;
            }
        } else if (moveStep != 0) {
            if (stepTarget == StepTarget::States) {
                StepState(enemies[slot], *loaded, moveStep);
            } else {
                StepBodyMove(enemies[slot], *loaded, moveStep);
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
        if (elapsedMs > 0) {
            loaded->enemy.Update(static_cast<std::int32_t>(elapsedMs));
            if (stepTarget == StepTarget::States) {
                HoldState(*loaded);
            }
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
        const float scale = view.mode == ViewMode::Game
                                ? enemies[slot].gameScale
                                : enemies[slot].uiScalePercent;
        BuildBaseMatrix(EnemyBounds(*loaded, view.mode == ViewMode::Game), view,
                        scale, drawableWidth, drawableHeight, base);
        DrawEnemy(*loaded, program, base);

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

    std::printf("[m38] done\n");
    return 0;
}
