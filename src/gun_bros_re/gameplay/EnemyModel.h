/**
 * @file EnemyModel.h
 * @brief One enemy's models, assembled by its script and ready to draw.
 *
 * Harness scaffolding shared by the two places an enemy appears: the M3.8
 * viewer, which shows one on a turntable, and the M3 map viewer, which stands
 * them on the terrain their spawn points name. Both need the same four steps
 * -- read the template, load every mesh config, let the script build the part
 * table, draw the parts against a base matrix -- and only the base matrix
 * differs, so only the base matrix is left to the caller.
 *
 * The scale a model is drawn at is NOT part of the base matrix by accident:
 * see EnemyModelWorldScale, which is the one number this file exists to get
 * right.
 */

#ifndef GUN_BROS_RE_MILESTONES_ENEMYMODEL_H
#define GUN_BROS_RE_MILESTONES_ENEMYMODEL_H

#include "engine/graphics/CMeshBuffer.h"
#include "engine/graphics/CShaderProgram.h"
#include "engine/graphics/CTexture.h"
#include "engine/glu/script/CScript.h"
#include "gun_bros_re/gameplay/CEnemy.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "engine/graphics/CMesh.h"
#include "engine/graphics/CMoveSetMesh.h"
#include "gun_bros_re/data/PackTables.h"

#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include <vector>

/**
 * One enemy template, read as far as the two draw scales.
 *
 * CEnemy::Template::Init (:67174) reads, in order: a flag byte, a
 * CGameAssetRef, a CScript, the CMoveSetMesh, a GameObjectRef, four numbers,
 * the two scales, and finally a CCollisionData. Only that last one belongs to
 * the collision work, so this stops one field short of it.
 *
 * **The two scales are the pair the same model is drawn at in the two places
 * it appears.** CEnemy::Draw (:67499) uses the first in the world;
 * CEnemy::DrawUI (:68451) uses the second as a PERCENTAGE on top of a
 * fit-to-box, for the menus and the results screen.
 */
struct EnemyTemplateData {
    std::uint32_t packHash;
    std::uint32_t ordinal;
    std::string owner;
    // enemy_template.bt / CEnemy::Template::Init :67183, template mem+120.
    CGameAssetRef name;

    CScript script;
    CMoveSetMesh moveSet;

    // Template word 66, CEnemy::Bind's this[213].
    float gameScale;

    // Template word 67, CEnemy::Bind's this[214]. DrawUI divides it by 100.
    float uiScalePercent;

    // Named for their template offsets, which is the one thing certainly true
    // about them. Read so the two scales land at the right place.
    GameObjectRef objectRef104;
    std::uint16_t experienceReward;
    std::uint16_t xplodiumReward;
    std::uint8_t flag117;
    std::uint8_t radius116;
    CCollisionData collision;

    EnemyTemplateData();
};

/** Read one ENEMY resource. `owner` is filled in for logging. */
bool ReadEnemyTemplate(PackTables &tables, std::uint32_t packHash,
                       std::uint32_t ordinal, const std::string &owner,
                       EnemyTemplateData &out);

/** Stable full ENEMY directory, including unused templates without scripts. */
bool LoadEnemyCatalog(CResTOCManager &toc, PackTables &tables,
    std::vector<EnemyTemplateData> &entries);

/** One mesh config of a move set, decoded and -- optionally -- uploaded. */
struct EnemyModelConfig {
    CMesh mesh;
    CTexture texture;
    CMeshBuffer buffer;
    bool valid;

    EnemyModelConfig() : valid(false) {}
};

/**
 * An enemy's models plus the object that decides which of them to show.
 *
 * Never copied or moved once built: `CMeshBuffer` owns a GL name, and the
 * meshes CEnemy was bound to are the ones inside `configs`.
 */
struct EnemyModel {
    std::vector<std::shared_ptr<EnemyModelConfig>> configs;
    std::vector<const CMesh *> configMeshes;
    CEnemy enemy;

    // Reused between frames so the evaluator does not reallocate.
    std::vector<float> pose;
};

/** Per-scene GL resources: poses/controllers stay on each individual enemy.
 * Drawing uploads a part's pose immediately before its draw, so the immutable
 * meshes, textures and upload buffers can be reused by the next enemy.
 */
struct EnemyModelCache {
    std::map<std::uint64_t, std::vector<std::shared_ptr<EnemyModelConfig>>> entries;
    unsigned hits = 0, misses = 0;
};

/**
 * Which of the script's two spawn exports to run.
 *
 * They assemble different models. A turret's export 3 sets nothing at all, so
 * the menu shows only its base; its export 0 adds the barrel as a second part
 * and hangs it off a bone. Whichever the caller is imitating, pick that one.
 */
enum class EnemySpawnMode {
    // Export 0, what CEnemy::Spawn runs (:73239). What a level does.
    Level,

    // Export 3, what CEnemy::SpawnForUI runs (:72858). What the menus do.
    Menu,
};

/**
 * Load every model the template names, then let its script assemble them.
 *
 * The order is the original's: CMoveSetMesh::Load (:123213) queues every
 * config before anything runs, because the script that picks between them has
 * not run yet.
 *
 * @param program Null when `createBuffers` is false, which is how a survey
 *        reads the meshes without a GL context.
 */
bool LoadEnemyModel(PackTables &tables, const EnemyTemplateData &entry,
                    bool createBuffers, const CShaderProgram *program,
                    EnemySpawnMode spawnMode, EnemyModel &out, EnemyModelCache *cache = nullptr);

/** Decode/upload authored configs without spawning or running enemy scripts. */
bool PreloadEnemyModel(PackTables &tables, const EnemyTemplateData &entry,
    const CShaderProgram &program, EnemyModelCache &cache);

/** Which config a part is currently showing, or -1 when it shows nothing. */
std::int32_t EnemyPartConfig(const EnemyModel &model, std::uint32_t partIndex);

/**
 * Draw every live part against one base matrix.
 *
 * The attachment comes from PART 0 -- its mesh, at its animation time --
 * whichever part is being drawn. CEnemy::Draw (:67499) reads both off part 0's
 * controller and passes the same pair to every GetNodeAt it makes.
 *
 * @param base Row-major, and it must already carry the scale: the vertices go
 *        in raw, so `base` is what turns model units into world ones.
 */
void DrawEnemyModel(EnemyModel &model, const CShaderProgram &program,
                    const float *base);

/**
 * How much to scale an enemy's RAW vertices by to put it in the world.
 *
 * `mesh.inverseExtent * runtimeScale * gameScale * cameraScale`, copied from
 * CEnemy::Draw (:67499). The trap is that the model is NOT normalised first --
 * the inverse extent in the product is what normalises it, and the vertices
 * arrive at their authored size. So a turret whose mesh spans 136 units, with
 * a game scale of 150 and a camera at 0.8, is drawn at 136 x (150/136) x 0.8 =
 * 120 world units. Reading the product as if it applied to an already-unit
 * model gives a number a hundred times too small, which is how it looks when
 * this is got wrong.
 *
 * @return 0 when part 0 has no mesh to measure.
 */
float EnemyModelWorldScale(const EnemyModel &model, float gameScale,
                           float cameraScale);

/**
 * The matrix CEnemy::Draw stands a model up with, via OrientForGame (:98959).
 *
 * Term for term: `translate(x, y) * scale * translate(-pivot) * rotateX(30) *
 * translate(pivot) * rotateZ(facing) * translate(-pivot)`, where the pivot is
 * part 0's mesh bounds centre -- CEnemy::Draw passes exactly that.
 *
 * The 30-degree lean is why a Gun Bros character reads as three-dimensional
 * from a top-down camera at all; the unbalanced last translate is the
 * original's, and it is what makes a leaning model's feet stay put.
 *
 * @param out Row-major, 16 floats. Must not alias `base`.
 */
void BuildEnemyGameMatrix(const EnemyModel &model, const float *base, float x,
                          float y, float scale, float facingDegrees, float *out);

#endif  // GUN_BROS_RE_MILESTONES_ENEMYMODEL_H
