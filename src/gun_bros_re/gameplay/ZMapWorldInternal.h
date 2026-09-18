#include "engine/core/ZPaths.h"
#pragma once
#include "gun_bros_re/gameplay/ZMapResources.h"
#include "gun_bros_re/gameplay/ZMapPropWorld.h"
/**
 * @file ZMapWorldInternal.h
 * @brief M3 milestone harness: a whole level, terrain and scenery, on screen.
 *
 * Two chains meet here. The terrain one:
 *
 *   ___GAME_TOC_KEYSET -> 33 section bases          (CGameObjectPack)
 *   TILELAYER base + n -> map resource              (CMap)
 *   map.tileSetRef     -> TILESET base + local      (TileSet)
 *   tileset.images[i]  -> PNG base + assetId        (CTexture)
 *   layer cells        -> quads                     (CQuadBatch)
 *
 * and the scenery one, which is where the rocks, pipes and portals live:
 *
 *   object layer       -> placed objects            (CLayerObject)
 *   PROP base + local  -> prop template             (CProp::Template)
 *   template.sprite    -> pack + archetype          (CGameSpriteGluRef)
 *   archetype          -> frames and atlas pages    (CSpriteGluArchetype)
 *   frame              -> quads                     (CSpriteIterator)
 *
 * Objects other than props are read but not drawn: enemies are 3D meshes,
 * particle effects need the particle system, and player tags are invisible.
 * Between them they are seven per cent of what a map places.
 *
 * `K` puts a marker on every one of them anyway. A spawn point has no art, so
 * the only way to tell one that is in the right place from one that is merely
 * plausible is to draw the coordinate the data gives next to the terrain it
 * belongs to. That is what M4a needs before it can put anything anywhere.
 */

#define NOMINMAX
#include "engine/core/CStringToKey.h"
#include "gun_bros_re/gameplay/ZMapScene.h"
#include "gun_bros_re/gameplay/CBGM.h"

#include "gun_bros_re/gameplay/ZEnemyModel.h"
#include "gun_bros_re/gameplay/ZSurvivalInputDriver.h"
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "gun_bros_re/data/ZWeaponCatalog.h"
#include "gun_bros_re/data/ZArmorCatalog.h"
#include "gun_bros_re/gameplay/CGame.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
#include "gun_bros_re/debug/CollisionOverlay.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include <sstream>
#include <chrono>
#include "gun_bros_re/gameplay/ZSurvivalGameContext.h"
#include "gun_bros_re/data/ZProfileStorage.h"
#include "gun_bros_re/ui/ZLoadingScreen.h"
#include "gun_bros_re/ZHostSettings.h"
#include "gun_bros_re/ui/CInputPad.h"
#include "gun_bros_re/data/ZMissionCatalog.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/data/ZPackTables.h"

#include "engine/resources/CArrayInputStream.h"
#include "engine/platform/ZAudioPlayer.h"
#include "engine/graphics/ZMarkerBatch.h"
#include "engine/core/ZMatrix4d.h"
#include "engine/graphics/ZPNG.h"
#include "engine/graphics/ZQuadBatch.h"
#include "engine/graphics/ZShaderProgram.h"
#include "engine/graphics/ZTexture.h"
#include "engine/platform/ZWindow.h"
#include <SDL3/SDL_events.h>
#include "engine/platform/ZGLLoader.h"
#include "gun_bros_re/data/CGameObjectPack.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "gun_bros_re/gameplay/CLayerObject.h"
#include "gun_bros_re/gameplay/CLayerTile.h"
#include "gun_bros_re/gameplay/CMap.h"
#include "gun_bros_re/effects/CParticleEffect.h"
#include "gun_bros_re/gameplay/CProp.h"
#include "engine/resources/CResTOCManager.h"
#include "gun_bros_re/gameplay/TileSet.h"
#include "engine/glu/sprite/CSpriteGlu.h"
#include "engine/glu/sprite/CSpriteIterator.h"
#include "engine/glu/sprite/CSpritePlayer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

// Internal map state and resource lifetimes shared by loading, rendering, and game sessions.
namespace MapDetail {

/**
 * The section bases and SpriteGlu tables of one pack, built on first use.
 *
 * Keyed by pack index rather than assumed, because references cross packs:
 * pack2's maps borrow five props from pack1.
 * Returns null when the pack cannot be addressed at all.
 */
ZPackResources *GetPackResources(CResTOCManager &tocManager, ZLoadedMap &loaded,
                                int packIndex);

/**
 * Read the resource a (pack hash, section, ordinal) triple names.
 *
 * Every reference in the data is one of these, and the pack hash is part of
 * the address -- CGunBros::GetGameObject picks the pack first and only then
 * adds the section base. Resolving an ordinal against the pack that happened
 * to hold the reference works right up until something points elsewhere, and
 * props already do.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:78497
 */
bool ReadSectionResource(CResTOCManager &tocManager, ZLoadedMap &loaded,
                         std::uint32_t packHash, ZGameSection section,
                         std::uint32_t localIndex,
                         std::vector<std::uint8_t> &payload);

/**
 * Run the scripts of whichever levels use this map.
 *
 * Scroll speeds are not in the map. A level script sets them when the level
 * starts -- CLevel::Init binds the script, binds the map, then calls export 0
 * (:120988) -- so getting them means running that script, which is what this
 * does. Levels are walked in section order and the ones naming this map are
 * bound and started; a map with no level, or a level that asks for no
 * scrolling, leaves every layer at rest. That is sixteen of the twenty-two.
 *
 * Every level template is parsed, not just the matching ones, and each is
 * checked for leftover bytes -- the whole-archive check that the script format
 * is read correctly.
 *
 * The CLevel is local because nothing outlives the call yet: the script runs
 * once, at load, and what it changes it changes in the map. M4a gives levels
 * a lifetime.
 */
void ApplyLevelScripts(CResTOCManager &tocManager, ZLoadedMap &out,
                       std::uint32_t mapPackHash, std::uint32_t mapIndex);

/**
 * Load a map, its tile set and the atlases the tile set names.
 *
 * @param mapPackIndex Which pack the map itself lives in. Everything it
 *                     references is addressed by its own pack hash, not by
 *                     this one.
 */
bool LoadMap(CResTOCManager &tocManager, int mapPackIndex, std::uint32_t mapIndex,
             ZLoadedMap &out);

/**
 * Expand every step of one animation into the quads that step draws.
 *
 * An unused slot -- animation 255, which is how a prop says it has no
 * foreground or no main sprite -- comes back empty, and so does one whose
 * animation is out of range. Neither is an error: most templates fill one slot
 * of the three, and a player pointed at an empty slot simply never ticks.
 */
void ExpandSlot(CSpriteIterator &iterator, const ZSpriteArchetype &archetype,
                std::uint8_t animationIndex, ZPropSlot &out);

/**
 * Whether any of a template's slots has more than one step to play.
 *
 * Most scenery is a single step and stands perfectly still, which is correct
 * and also indistinguishable from a broken clock. Counting the ones that can
 * move is what tells the two apart without staring at the window.
 */
bool PropAnimates(const ZPropSprite &sprite);

/** Whether a slot draws anything on its first step. */
bool SlotDrawsAtStart(const ZPropSlot &slot);

/** Whether a PROP reference names one of the two PvP barricade orientations. */
bool IsDestructibleCover(std::uint32_t packHash, std::uint8_t localIndex);

ZInteractivePropKind InteractiveKindFor(std::uint32_t packHash,
                                       std::uint8_t localIndex);

void ExpandPropState(CSpriteIterator &iterator,
                     const ZSpriteArchetype &archetype,
                     std::uint8_t backgroundAnimation,
                     std::uint8_t mainAnimation,
                     std::uint8_t foregroundAnimation,
                     ZPropVisualState &state);

/** Build the known visual states named by the three original prop scripts. */
void BuildInteractiveStates(std::uint32_t packHash, std::uint8_t localIndex,
                            CSpriteIterator &iterator,
                            const ZSpriteArchetype &archetype,
                            ZPropSprite &out);

/**
 * Expand one prop template into the quads its three slots draw.
 *
 * This is CProp::Bind's job: it resolves the sprite reference to an archetype
 * and points three CSpritePlayers at three animations of it. The z-order group
 * falls out of which of the three ended up with an animation, exactly as
 * CProp::GetZOrderGroup computes it.
 */
bool BuildPropSprite(CResTOCManager &tocManager, ZLoadedMap &loaded,
                     std::uint32_t propPackHash, std::uint8_t localIndex,
                     ZPropSprite &out);

/**
 * A repeatable stand-in for the Utility::Random in CProp::Bind.
 *
 * Bind starts the main slot on a random step so a field of identical rocks
 * does not pulse in unison. A real random would cost --screenshot its one
 * useful property, that two runs produce the same image, so this hashes the
 * prop's place in the draw order instead: scattered between neighbours, and
 * the same on every run.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:124916
 *
 * The multiplier is Knuth's, and the shift drops the low bits, which move too
 * regularly between consecutive ordinals to scatter anything.
 */
std::uint32_t StartStepFor(std::size_t propOrdinal, std::size_t stepCount);

/** Background slot selected by a prop's current cover state. */
const ZPropSlot *RuntimeSlotFor(const ZPlacedProp &prop, unsigned slot);

const ZPropSlot *BackgroundSlotFor(const ZPlacedProp &prop);

const ZPropSlot *MainSlotFor(const ZPlacedProp &prop);

const ZPropSlot *ForegroundSlotFor(const ZPlacedProp &prop);

/**
 * Point one placed prop's three players at their slots, as CProp::Bind does.
 *
 * All three loop forwards, which is CSpritePlayer's constructed state and
 * which Bind never changes. Only the main slot starts part-way in; the other
 * two begin at step 0, so a prop's foreground and background stay in step with
 * each other however its body is phased.
 */
void StartPropPlayers(ZPlacedProp &prop, std::size_t propOrdinal);

/** Human-readable state for the keyboard diagnostic. */
const char *CoverStateName(ZCoverState state);

/** Advance the shared cover state, wrapping hidden back to intact. */
ZCoverState NextCoverState(ZCoverState state);

/** Apply one state to every destructible cover on the current map. */
std::uint32_t SetCoverState(ZLoadedMap &loaded, ZCoverState state);

const char *InteractiveStateName(ZInteractivePropKind kind, std::uint8_t state);

/** Apply one visual state to every prop of a scripted interactive kind. */
std::uint32_t SetInteractiveState(ZLoadedMap &loaded, ZInteractivePropKind kind,
                                  std::uint8_t state);

std::uint64_t AssetKey(std::uint32_t packHash, std::uint32_t localIndex);

int SoundResourceForState(ZInteractivePropKind kind, std::uint8_t state);

/** Resolve SoundEffect -> WAV, cache it, then play one batch transition cue. */
void PlayTransitionSound(CResTOCManager &tocManager, ZLoadedMap &loaded,
                         ZAudioPlayer &audio, ZInteractivePropKind kind,
                         std::uint8_t state);

/** Parse and expand one original particle template the first time it is used. */
bool EnsureParticleEffectVisual(CResTOCManager &tocManager, ZLoadedMap &loaded,
                                const ZScriptResourceRef &reference,
                                std::uint64_t &visualKey);

/** Queue one cached effect at a prop's world position. */
void StartParticleEffect(ZLoadedMap &loaded, std::uint64_t visualKey,
                         float x, float y, int zOrderGroup,
                         std::uint32_t randomSalt);

/** Original script resource indices and z groups for one state transition. */
void StartTransitionParticlesForProp(CResTOCManager &tocManager,
                                     ZLoadedMap &loaded,
                                     const ZPlacedProp &prop,
                                     std::uint8_t state,
                                     std::uint32_t randomSalt);

/** Start each matching prop's script-authored particle cues. */
void StartTransitionParticles(CResTOCManager &tocManager, ZLoadedMap &loaded,
                              ZInteractivePropKind kind, std::uint8_t state);

/** Advance emitters and their live particles. */
void AdvanceParticleEffects(ZLoadedMap &loaded, std::uint16_t deltaMs);

/** Select the looping sprite step belonging to a particle's current age. */
std::size_t ParticleAnimationStep(const ZPropSlot &animation, float ageMs);

/** Emit live particle sprites in the requested z-order interval. */
void AddParticleQuads(const ZLoadedMap &loaded, ZQuadBatch &batch,
                      int minimumZGroup, int maximumZGroup);

/** The original disables both collision shapes in the destroyed state. */
bool PropHasCollision(const ZPlacedProp &prop);

/** Order props the way CRenderQueue does: by group, then down the screen. */
bool PropDrawsBefore(const ZPlacedProp &left, const ZPlacedProp &right);

/**
 * Turn the map's object layers into drawable props.
 *
 * Objects of other types are counted and left alone. Nothing here fails the
 * load: a prop whose template or sprite will not resolve is dropped and
 * reported, because one bad rock should not cost the whole level.
 */
void LoadProps(CResTOCManager &tocManager, ZLoadedMap &loaded, int selectedObjectLayer = -1);

/** Move every prop's three players on by one frame's worth of time. */
void AdvanceProps(std::vector<ZPlacedProp> &props, std::uint16_t deltaMs);

/** Move every drifting tile layer on by one frame's worth of time. */
void AdvanceTileLayers(CMap &map, std::uint16_t deltaMs);

/**
 * The quads a slot draws at its player's current step.
 *
 * An empty slot -- an unused animation, or one that expanded to nothing --
 * lands on the out-of-range path and draws nothing, which is why the players
 * of empty slots never need a special case anywhere else.
 */
const std::vector<ZSpriteQuad> &CurrentQuads(const ZPropSlot &slot,
                                            const CSpritePlayer &player);

/** Emit one prop's slot, positioned at the prop and offset by each quad. */
void AddSpriteQuads(const ZPlacedProp &prop, const std::vector<ZSpriteQuad> &quads,
                    ZQuadBatch &batch);

/**
 * Turn the tile layers and the props into quads.
 *
 * Tiles walk the canvas rather than each layer's own extent, so a layer
 * smaller than the canvas repeats -- CLayerTile::GetCell does the wrapping.
 * Layers are emitted bottom first, matching CMap::DrawBackground's order.
 *
 * Then the props, in the order CRenderQueue::Draw produces: the queue is
 * sorted once and walked three times over, background slot for everything
 * first, then main, then foreground. That is why this cannot just draw each
 * prop's three slots together -- a prop's foreground has to land on top of the
 * NEXT prop's main sprite, not just its own.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:145235
 *
 * This runs every frame now, because a prop's quads change as it animates. The
 * tiles do not, and could in principle be kept -- but the largest map in these
 * archives is 190 tile quads and 342 prop ones, so the whole rebuild is a few
 * hundred quads into a buffer designed to be refilled every frame. Splitting
 * the batch in two to save half of nothing would cost a second buffer and the
 * code that decides which half is stale.
 */
/**
 * Where a spawn marker goes, and how big.
 *
 * The size is the viewer's, not the data's: a spawn point is a single
 * coordinate, and it has to be big enough to find against a 256-pixel tile.
 */
constexpr float kMarkerSize = 48.0f;

// Depth kept by the map's projection. Terrain and props are flat at z = 0;
// this only has to be deep enough for the tallest placed model, and the
// biggest one in the archives is a couple of hundred world units.
constexpr float kMapDepthRange = 4096.0f;
constexpr float kMarkerThickness = 6.0f;

/** Collect one outline per object of the given type, centred on its own x,y. */
void BuildMarkers(const ZLoadedMap &loaded, ZMarkerBatch &markers,
                  ZPlacedObjectType wanted);

/**
 * Assemble exactly the collision shapes CLayerCollision tests for a player.
 *
 * The level script selects one map collision layer. Static props then add
 * their template-local geometry at their placed position. Keeping this as one
 * scene lets the existing resolver choose the nearest edge across both kinds
 * instead of resolving each prop in an arbitrary order.
 */
void BuildCollisionScene(ZLoadedMap &loaded);

/** Collect the exact collision scene used by player movement. */

/**
 * The camera scale a level snaps to. Reference: :120747, where CLevel does
 * `CCamera::SnapScale(camera, 0.8)` right after binding the map.
 *
 * The real camera moves it afterwards; until there is a real camera this is
 * the number the game starts every level with.
 */
constexpr float kLevelCameraScale = 0.8f;

/** Convert world anchors and native pixel sizes to the HUD's logical canvas. */
void ProjectEnemyHealthBars(std::vector<CLevel::HealthBar> &bars,
    float cameraX, float cameraY, float zoom, int width, int height);

/** "3 player spawns, 41 enemy spawns" -- what the K overlay should show. */
void ReportSpawns(const ZLoadedMap &loaded);

/**
 * Load a model for every enemy the object layer places.
 *
 * **These are leftovers, not how the shipped game works.** Every shipped map
 * is survival: enemies arrive from off screen and close in, spawned by the
 * level rather than placed on it. The object layer's enemies are what was left
 * of a campaign mode, which is why most maps have none and the ones that do
 * cannot be checked against anything -- except pack9's two, which are turrets
 * and stand where they are placed.
 */
void LoadPlacedEnemies(CResTOCManager &tocManager, const ZShaderProgram &program,
                       ZLoadedMap &loaded);

/** Move every placed enemy's animation on. */
void AdvanceEnemies(ZLoadedMap &loaded, std::int32_t deltaMs);

/**
 * Stand a player on every spawn point the object layer names.
 *
 * The default model and nothing else: no weapon, no armour. Which gun a
 * player carries is a loadout question and the loadout is not read yet, so
 * putting one in his hand here would be inventing data.
 */
void LoadPlacedPlayers(CResTOCManager &tocManager, const ZShaderProgram &program,
                       ZLoadedMap &loaded);

/** Swap equipment only after every referenced asset has loaded. */
bool EquipControlledPlayer(ZPackTables &tables, ZLoadedMap &loaded,
    const ZShaderProgram &program, const ZWeaponEntry &weapon);

/**
 * Draw every model the object layer places -- enemies and players alike.
 *
 * Depth on, and the depth buffer cleared first: the terrain and props are 2D
 * and drawn in order, so they neither read nor write depth, but a model is
 * solid and needs it against itself.
 */
struct ZMapRenderItem {
    int group = kZGroupNormal;
    int y = 0;
    const ZPlacedProp *prop = nullptr;
    CBrother *player = nullptr;
    ZEnemyModel *enemy = nullptr;
    CParticleSystem::RenderItem particle;
    float matrix[kMatrix4dElements] = {};
};

/** CRenderQueue::Compare :145029: group, then integer world Y. */
bool MapItemDrawsBefore(const ZMapRenderItem &left, const ZMapRenderItem &right);

/** Shared main/foreground passes for the game and permanent map viewers.
 * CRenderQueue::Draw :145123 puts actors and scenery in the SAME main pass.
 * The caller has already drawn tiles and every prop's background slot.
 */
void DrawMapObjects(ZLoadedMap &loaded, ZQuadBatch &batch, const ZShaderProgram &program,
                const float *mapMvp, bool showProps = true, CLevel *scene = nullptr,
                CBrother *brotherModel = nullptr, float brotherY = 0, int viewportWidth = 1);

/** How many objects of one type a map places. */
unsigned CountObjects(const ZLoadedMap &loaded, ZPlacedObjectType wanted);

void ReportSpawns(const ZLoadedMap &loaded);

void BuildGeometry(const ZLoadedMap &loaded, ZQuadBatch &batch, bool showTiles,
                   bool showProps, bool report);

/** One map, wherever it lives. */
struct ZCatalogMap {
    int packIndex;
    std::string packName;
    std::uint32_t mapIndex;  // ordinal within its own pack's TILELAYER section
};

/**
 * Every map in every pack, as one flat list.
 *
 * Flat rather than grouped because a pack boundary is not something the viewer
 * should make anyone think about -- the packs are a packaging detail, and a map
 * reaches across them for its props anyway. Walking the list runs off the end
 * of one pack straight into the next.
 */
std::vector<ZCatalogMap> BuildCatalog(CResTOCManager &tocManager);

/** Slot of the first map of the pack `slot` belongs to. */
std::size_t FirstMapOfPack(const std::vector<ZCatalogMap> &catalog, std::size_t slot);

/**
 * Slot of the first map of the next pack, wrapping round the end.
 *
 * Left and right already cross pack boundaries one map at a time; this is the
 * shortcut for skipping a whole pack, which matters when pack2 alone holds
 * nine maps.
 */
std::size_t NextPackSlot(const std::vector<ZCatalogMap> &catalog, std::size_t slot);

/** Slot of the first map of the previous pack, wrapping round the start. */
std::size_t PreviousPackSlot(const std::vector<ZCatalogMap> &catalog,
                             std::size_t slot);

/** The camera state the viewer manipulates. */
struct ZMapCamera {
    float x;
    float y;
    float zoom;
};

/**
 * The rectangle worth looking at, in world pixels.
 *
 * The map's camera layer when it has one, and the whole canvas when it does
 * not. Two of the twenty-two maps declare no camera layer.
 */
ZMapRectangle ViewedRegion(const ZLoadedMap &loaded);

/** Zoom out far enough to see the whole viewed region, and centre it. */
ZMapCamera FitCamera(const ZLoadedMap &loaded, int viewWidth, int viewHeight);

/** Centre the fixed GameView camera on the controlled player. */
void FollowPlayerCamera(const ZLoadedMap &loaded, int viewWidth, int viewHeight,
                        ZMapCamera &camera);

/** Keep the default stage's framing across maps; bounds only limit panning.
 * This viewer setting is deliberately independent of stage dimensions.
 */
float GameViewCameraZoom(int viewWidth,
                         int viewHeight);

/**
 * Where the camera bounds land on the window, as a scissor rectangle.
 *
 * A map's tile layers run past the rectangle the game is ever allowed to show:
 * a lava or starfield layer wraps and keeps filling to the edge of the canvas,
 * and the terrain layer above it stops short. The engine never reveals that
 * because the camera stops at these bounds. This viewer fits whole maps on
 * screen, so it has to clip instead.
 *
 * @param scissor Filled with x, y, width, height in window pixels, GL's
 *                bottom-left origin.
 * @return false when the map declares no camera layer; nothing is clipped then.
 */
bool VisibleBoundsScissor(const ZLoadedMap &loaded, const ZMapCamera &camera,
                          int drawableWidth, int drawableHeight, int scissor[4]);

/** Open the archives and pick out one pack, already bound. */
CResPackTOC *OpenPack(CResTOCManager &tocManager, const std::string &bigDirectory,
                      const std::string &packShortName);

}

bool SaveSurvivalProgress(ZSurvivalGameContext *context, const CPlayerProgress &progress,
    const CLevel &scene, const CLevel &level, std::uint64_t &accountedXplodium, bool missionEnded = false );
