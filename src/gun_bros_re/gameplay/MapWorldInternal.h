#include "engine/core/Paths.h"
#pragma once
/**
 * @file M3Map.cpp
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
#include "gun_bros_re/gameplay/MapScene.h"
#include "gun_bros_re/gameplay/CBGM.h"

#include "gun_bros_re/gameplay/EnemyModel.h"
#include "gun_bros_re/gameplay/SurvivalInputDriver.h"
#include "gun_bros_re/gameplay/PlayerModel.h"
#include "gun_bros_re/data/WeaponCatalog.h"
#include "gun_bros_re/data/ArmorCatalog.h"
#include "gun_bros_re/gameplay/SurvivalSession.h"
#include "gun_bros_re/gameplay/CombatGeometry.h"
#include "gun_bros_re/debug/CollisionOverlay.h"
#include "gun_bros_re/data/StoreCatalog.h"
#include <sstream>
#include <chrono>
#include "gun_bros_re/gameplay/SurvivalGameContext.h"
#include "gun_bros_re/data/NativeProfile.h"
#include "gun_bros_re/ui/LoadingScreen.h"
#include "gun_bros_re/HostSettings.h"
#include "gun_bros_re/ui/SurvivalHud.h"
#include "gun_bros_re/data/MissionCatalog.h"
#include "gun_bros_re/gameplay/WeaponEffects.h"
#include "gun_bros_re/data/PackTables.h"

#include "engine/resources/CArrayInputStream.h"
#include "engine/platform/CAudioPlayer.h"
#include "engine/graphics/CMarkerBatch.h"
#include "engine/core/CMatrix4d.h"
#include "engine/graphics/CPNG.h"
#include "engine/graphics/CQuadBatch.h"
#include "engine/graphics/CShaderProgram.h"
#include "engine/graphics/CTexture.h"
#include "engine/platform/CWindow.h"
#include <SDL3/SDL_events.h>
#include "engine/platform/GLLoader.h"
#include "gun_bros_re/data/CGameObjectPack.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "gun_bros_re/gameplay/CLayerObject.h"
#include "gun_bros_re/gameplay/CLayerTile.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/gameplay/CMap.h"
#include "gun_bros_re/gameplay/CParticleEffect.h"
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

// Exercise the real SDL event queue and CWindow recognizer, including held S.
bool PushBossCheckKey(CWindow &window, char letter, bool repeat = false, bool checkMovement = false);

const char *const kShaderDirectory = Paths::Shaders().c_str();

// Pixels of drag per pixel of camera movement. One to one feels direct.
constexpr float kDragScale = 1.0f;

// One wheel notch scales the view by this much.
constexpr float kZoomPerNotch = 1.15f;

// Zoom limits: far enough out to see the largest map, far enough in to
// inspect a single tile's seams.
constexpr float kMinZoom = 0.05f;
constexpr float kMaxZoom = 4.0f;

// Fill the limiting window dimension. The old 1.05 margin made an already
// small overview smaller; exact fit keeps the whole map without dead padding.
constexpr float kFitMargin = 1.0f;

// GameView uses a fixed 4:3 world view independent of window pixels and map
// bounds. The framing matches the default stage, not an original engine constant.
constexpr float kGameViewWorldWidth = 572.0f;
constexpr float kGameViewWorldHeight = 429.0f;

// Temporary keyboard locomotion until the original control-stick module is
// ported. The movement and collision time step are frame-rate independent.
constexpr float kPlayerMovementUnitsPerSecond = 240.0f;
// CBrother::Bind stores 22.0 as the player diameter. CPlayer::Move passes half
// of it to CLayerCollision, whose half-unit edge allowance makes 11.5.
// Source correction: :139098 initializes a RADIUS of 22, not a diameter;
// only wall collision halves it. Player/enemy circles must use GetRadius().
constexpr float kPlayerCollisionRadius = 11.5f;
constexpr float kRadiansToDegrees = 180.0f / 3.14159265f;

// The PvP barricades are two orientations of one scripted prop. Their four
// consecutive background animations are intact, damaged twice, and destroyed.
constexpr std::uint32_t kCoverPackHash = 0x00267585;
constexpr std::uint8_t kHorizontalCoverTemplate = 0;
constexpr std::uint8_t kVerticalCoverTemplate = 1;
constexpr std::uint8_t kCoverVisibleStateCount = 4;
constexpr std::uint8_t kMaximumInteractiveStateCount = 5;

constexpr std::uint32_t kLavaPackHash = 0x00267582u;
constexpr std::uint8_t kLavaBarrelTemplate = 0;
constexpr std::uint32_t kWaterPackHash = 0x01675822u;
constexpr std::uint8_t kWaterBarrelTemplate = 22;
constexpr std::uint32_t kSpirePackHash = 0x00267587u;
constexpr std::uint8_t kSpireTemplate = 33;

constexpr float kSecondsToMilliseconds = 1000.0f;
constexpr float kParticleFallbackLifetimeMs = 750.0f;
constexpr std::size_t kMaximumParticlesPerEffect = 2048;
constexpr std::size_t kMaximumSpawnsPerEmitterPerFrame = 64;

enum class InteractivePropKind : std::uint8_t {
    None,
    Cover,
    Barrel,
    Spire,
};

enum class CoverState : std::uint8_t {
    Intact,
    Damaged,
    BadlyDamaged,
    Destroyed,
    Hidden,
    Count,
};

// Longest frame the animation clock will believe. Past this the wall clock has
// stopped meaning anything -- a debugger breakpoint, a dragged window, a lost
// context -- and the animations should carry on from where they were rather
// than lurch forward by however long the pause was.
constexpr std::uint64_t kMaxFrameMs = 100;

// How far a single-step of the viewer moves time on. A twelfth of a second is
// short enough to catch a fast animation changing frame and long enough that
// holding the key walks visibly.
constexpr std::uint16_t kSingleStepMs = 80;

// The bite --advance moves time on in. Animations only ever step once per
// tick, so winding the clock forward has to be done a frame at a time or it
// would advance every animation by exactly one step however far it was asked
// to go. Sixty hertz, because that is what it is imitating.
constexpr std::uint16_t kWarmUpFrameMs = 16;

/**
 * Z-order groups, from CProp::GetZOrderGroup.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:123406
 *
 * The render queue sorts on these before it sorts on y, so a prop that only
 * has a background sprite sits behind every prop that has a main one, however
 * far down the screen it is. The map is group 0 with a z of -100000, which is
 * why the tiles simply go in first rather than being sorted with the props.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:92795 (CMap::GetZOrder)
 */
constexpr int kZGroupBackgroundOnly = 0;
constexpr int kZGroupNormal = 3;
constexpr int kZGroupForegroundOnly = 6;

/** One animation slot of a prop template, expanded step by step. */
struct PropSlot {
    // One quad list per animation step, so playback is a subscript rather than
    // a walk back down the sprite tree. A template's steps run to a couple of
    // dozen at most, and expanding them all costs a fraction of the archive
    // read that got us the template in the first place.
    std::vector<std::vector<SpriteQuad>> quadsByStep;

    // The same steps' durations, which is all a CSpritePlayer needs.
    std::vector<std::uint16_t> stepDurationsMs;
};

/** The three sprite layers and collision rule used by one prop state. */
struct PropVisualState {
    PropSlot background;
    PropSlot main;
    PropSlot foreground;
    bool collisionEnabled;

    PropVisualState() : collisionEnabled(true) {}
};

/**
 * One prop template's three sprite slots, every step already expanded.
 *
 * Shared between instances: a map places a hundred props from a couple of
 * dozen templates, and expanding the same animation a hundred times would mean
 * a hundred archive reads for nothing. Instances differ only in where they
 * stand and how far into the animation they are, and both of those live on
 * PlacedProp.
 */
struct PropSprite {
    CProp::Template data;
    GameObjectRef resource;
    std::vector<PropSlot> animations;
    std::vector<std::vector<std::uint16_t>> durations;
    PropSlot background;
    PropSlot main;
    PropSlot foreground;
    std::array<PropVisualState, kMaximumInteractiveStateCount> states;
    std::vector<ScriptResourceRef> transitionResources;
    CCollisionData collision;
    CCollisionData bulletCollision;
    InteractivePropKind interactiveKind;
    std::uint8_t stateCount;
    int zOrderGroup;

    // What the walk could not draw, kept so the load can report a total
    // rather than a line per template.
    std::uint32_t skippedParts;
    std::uint32_t unsupportedTransforms;

    PropSprite()
        : interactiveKind(InteractivePropKind::None),
          stateCount(0),
          zOrderGroup(kZGroupNormal),
          skippedParts(0),
          unsupportedTransforms(0) {}
};

/** One prop standing on the map, each of its three slots playing its own. */
struct PlacedProp {
    std::shared_ptr<CProp> runtime;
    int objectId = -1;
    unsigned objectLayer = 0;
    bool active = true;
    CombatId lastDamager = 0;
    float x;
    float y;
    const PropSprite *sprite;
    std::uint8_t interactiveState;
    float hitFlashRemainingMs;

    CSpritePlayer background;
    CSpritePlayer main;
    CSpritePlayer foreground;

    PlacedProp()
        : x(0.0f),
          y(0.0f),
          sprite(nullptr),
          interactiveState(0),
          hitFlashRemainingMs(0.0f) {}
};

/** The per-pack tables a prop needs, built the first time that pack is used. */
struct PackResources {
    CGameObjectPack objectPack;
    CSpriteGlu spriteGlu;
    bool objectPackReady;
    bool spriteGluReady;

    PackResources() : objectPackReady(false), spriteGluReady(false) {}
};

/** Sprite animations belonging to one emitter in a particle template. */
struct ParticleEmitterVisual {
    std::array<PropSlot, 32> animations;
};

/** Parsed particle data plus the already-expanded atlas quads it draws. */
struct ParticleEffectVisual {
    CParticleEffect effect;
    std::vector<ParticleEmitterVisual> emitters;
};

/** One particle emitted by an active effect. */
struct LiveParticle {
    std::uint32_t emitterIndex;
    std::uint8_t animationIndex;
    float x;
    float y;
    float velocityX;
    float velocityY;
    float ageMs;
    float lifetimeMs;
    std::array<float, kParticleInterpolatorChannelCount> randomValues;
};

/** One effect attached to a prop position until its particles finish. */
struct ActiveParticleEffect {
    std::uint64_t visualKey;
    float x;
    float y;
    int zOrderGroup;
    float ageMs;
    std::uint32_t randomState;
    std::vector<float> nextSpawnMs;
    std::vector<LiveParticle> particles;
};

/** One enemy template standing on the map, with the model it draws as. */
struct PlacedEnemy {
    float x;
    float y;

    // Both by pointer, and both for the same reason: CEnemy::Bind keeps the
    // ADDRESS of the move set and the script, so the template has to stay put
    // for as long as the model does. Holding either by value here would leave
    // the model pointing at freed memory the moment this vector grew.
    std::unique_ptr<EnemyTemplateData> templateData;
    std::unique_ptr<EnemyModel> model;

    // The template's game scale, which is half of how big it is drawn.
    float gameScale;
};

/** A player standing on one of the map's spawn points. */
struct PlacedPlayer {
    float x;
    float y;
    float facingDegrees;
    bool moving;

    // By pointer for the same reason a PlacedEnemy's model is: it owns GL
    // buffers and its controllers point back into it, so it cannot be moved
    // once built.
    std::unique_ptr<PlayerModel> model;

    PlacedPlayer()
        : x(0.0f), y(0.0f), facingDegrees(0.0f), moving(false) {}
};

/** Everything one map needs to draw, held together for the render loop. */
struct LoadedMap {
    CMap map;
    TileSet tileSet;
    std::vector<std::unique_ptr<CTexture>> textures;

    // Effective player collision: the level-selected map layer plus every
    // placed prop's local collision translated into world space.
    CCollisionData collisionScene;
    WeaponCollision weaponCollision;

    // The enemies the object layer places. Held by pointer because an
    // EnemyModel owns GL buffers and points at its own meshes.
    std::vector<PlacedEnemy> enemies;

    // The player template, owned here because every PlacedPlayer's controllers
    // hold the ADDRESS of its move set -- the same trap PlacedEnemy documents.
    std::unique_ptr<PlayerTemplateData> playerTemplate;
    std::vector<PlacedPlayer> players;

    // Props, and everything they hang off. The caches own the atlas pages, so
    // they have to outlive the props that point into them -- replacing a
    // LoadedMap wholesale is what keeps that true.
    std::map<int, std::unique_ptr<PackResources>> packs;
    std::map<std::uint64_t, PropSprite> propSprites;
    std::vector<PlacedProp> props;

    // Effects are cached by their game-object address and instantiated only
    // when a transition asks for one.
    std::map<std::uint64_t, ParticleEffectVisual> particleEffects;
    std::vector<ActiveParticleEffect> activeParticleEffects;
};

/**
 * The section bases and SpriteGlu tables of one pack, built on first use.
 *
 * Keyed by pack index rather than assumed, because references cross packs:
 * pack2's maps borrow five props from pack1.
 * Returns null when the pack cannot be addressed at all.
 */
PackResources *GetPackResources(CResTOCManager &tocManager, LoadedMap &loaded,
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
bool ReadSectionResource(CResTOCManager &tocManager, LoadedMap &loaded,
                         std::uint32_t packHash, GameSection section,
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
void ApplyLevelScripts(CResTOCManager &tocManager, LoadedMap &out,
                       std::uint32_t mapPackHash, std::uint32_t mapIndex);

/**
 * Load a map, its tile set and the atlases the tile set names.
 *
 * @param mapPackIndex Which pack the map itself lives in. Everything it
 *                     references is addressed by its own pack hash, not by
 *                     this one.
 */
bool LoadMap(CResTOCManager &tocManager, int mapPackIndex, std::uint32_t mapIndex,
             LoadedMap &out);

/**
 * Expand every step of one animation into the quads that step draws.
 *
 * An unused slot -- animation 255, which is how a prop says it has no
 * foreground or no main sprite -- comes back empty, and so does one whose
 * animation is out of range. Neither is an error: most templates fill one slot
 * of the three, and a player pointed at an empty slot simply never ticks.
 */
void ExpandSlot(CSpriteIterator &iterator, const CSpriteGluArchetype &archetype,
                std::uint8_t animationIndex, PropSlot &out);

/**
 * Whether any of a template's slots has more than one step to play.
 *
 * Most scenery is a single step and stands perfectly still, which is correct
 * and also indistinguishable from a broken clock. Counting the ones that can
 * move is what tells the two apart without staring at the window.
 */
bool PropAnimates(const PropSprite &sprite);

/** Whether a slot draws anything on its first step. */
bool SlotDrawsAtStart(const PropSlot &slot);

/** Whether a PROP reference names one of the two PvP barricade orientations. */
bool IsDestructibleCover(std::uint32_t packHash, std::uint8_t localIndex);

InteractivePropKind InteractiveKindFor(std::uint32_t packHash,
                                       std::uint8_t localIndex);

void ExpandPropState(CSpriteIterator &iterator,
                     const CSpriteGluArchetype &archetype,
                     std::uint8_t backgroundAnimation,
                     std::uint8_t mainAnimation,
                     std::uint8_t foregroundAnimation,
                     PropVisualState &state);

/** Build the known visual states named by the three original prop scripts. */
void BuildInteractiveStates(std::uint32_t packHash, std::uint8_t localIndex,
                            CSpriteIterator &iterator,
                            const CSpriteGluArchetype &archetype,
                            PropSprite &out);

/**
 * Expand one prop template into the quads its three slots draw.
 *
 * This is CProp::Bind's job: it resolves the sprite reference to an archetype
 * and points three CSpritePlayers at three animations of it. The z-order group
 * falls out of which of the three ended up with an animation, exactly as
 * CProp::GetZOrderGroup computes it.
 */
bool BuildPropSprite(CResTOCManager &tocManager, LoadedMap &loaded,
                     std::uint32_t propPackHash, std::uint8_t localIndex,
                     PropSprite &out);

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
const PropSlot *RuntimeSlotFor(const PlacedProp &prop, unsigned slot);

const PropSlot *BackgroundSlotFor(const PlacedProp &prop);

const PropSlot *MainSlotFor(const PlacedProp &prop);

const PropSlot *ForegroundSlotFor(const PlacedProp &prop);

/**
 * Point one placed prop's three players at their slots, as CProp::Bind does.
 *
 * All three loop forwards, which is CSpritePlayer's constructed state and
 * which Bind never changes. Only the main slot starts part-way in; the other
 * two begin at step 0, so a prop's foreground and background stay in step with
 * each other however its body is phased.
 */
void StartPropPlayers(PlacedProp &prop, std::size_t propOrdinal);

/** Human-readable state for the keyboard diagnostic. */
const char *CoverStateName(CoverState state);

/** Advance the shared cover state, wrapping hidden back to intact. */
CoverState NextCoverState(CoverState state);

/** Apply one state to every destructible cover on the current map. */
std::uint32_t SetCoverState(LoadedMap &loaded, CoverState state);

const char *InteractiveStateName(InteractivePropKind kind, std::uint8_t state);

/** Apply one visual state to every prop of a scripted interactive kind. */
std::uint32_t SetInteractiveState(LoadedMap &loaded, InteractivePropKind kind,
                                  std::uint8_t state);

std::uint64_t AssetKey(std::uint32_t packHash, std::uint32_t localIndex);

int SoundResourceForState(InteractivePropKind kind, std::uint8_t state);

/** Resolve SoundEffect -> WAV, cache it, then play one batch transition cue. */
void PlayTransitionSound(CResTOCManager &tocManager, LoadedMap &loaded,
                         CAudioPlayer &audio, InteractivePropKind kind,
                         std::uint8_t state);

/** Parse and expand one original particle template the first time it is used. */
bool EnsureParticleEffectVisual(CResTOCManager &tocManager, LoadedMap &loaded,
                                const ScriptResourceRef &reference,
                                std::uint64_t &visualKey);

/** Deterministic local random stream, independent from map animation timing. */
float NextParticleRandom(std::uint32_t &state);

float ParticleRandomRange(std::uint32_t &state, float minimum, float maximum);

float ParticleRangeAt(float minimum, float maximum, float randomValue);

/** Queue one cached effect at a prop's world position. */
void StartParticleEffect(LoadedMap &loaded, std::uint64_t visualKey,
                         float x, float y, int zOrderGroup,
                         std::uint32_t randomSalt);

/** Original script resource indices and z groups for one state transition. */
void StartTransitionParticlesForProp(CResTOCManager &tocManager,
                                     LoadedMap &loaded,
                                     const PlacedProp &prop,
                                     std::uint8_t state,
                                     std::uint32_t randomSalt);

/** Start each matching prop's script-authored particle cues. */
void StartTransitionParticles(CResTOCManager &tocManager, LoadedMap &loaded,
                              InteractivePropKind kind, std::uint8_t state);

/** Create one live particle from an emitter's pattern and velocity. */
void SpawnParticle(ActiveParticleEffect &active,
                   const ParticleEmitterTemplate &emitter,
                   std::uint32_t emitterIndex);

/** Resolve one particle channel at an age using the original timed ranges. */
float ParticleChannelValue(const ParticleEmitterTemplate &emitter,
                           const LiveParticle &particle,
                           std::size_t channel, float defaultValue);

/** Advance emitters and their live particles. */
void AdvanceParticleEffects(LoadedMap &loaded, std::uint16_t deltaMs);

/** Select the looping sprite step belonging to a particle's current age. */
std::size_t ParticleAnimationStep(const PropSlot &animation, float ageMs);

/** Emit live particle sprites in the requested z-order interval. */
void AddParticleQuads(const LoadedMap &loaded, CQuadBatch &batch,
                      int minimumZGroup, int maximumZGroup);

/** The original disables both collision shapes in the destroyed state. */
bool PropHasCollision(const PlacedProp &prop);

/** Order props the way CRenderQueue does: by group, then down the screen. */
bool PropDrawsBefore(const PlacedProp &left, const PlacedProp &right);

/**
 * Turn the map's object layers into drawable props.
 *
 * Objects of other types are counted and left alone. Nothing here fails the
 * load: a prop whose template or sprite will not resolve is dropped and
 * reported, because one bad rock should not cost the whole level.
 */
void LoadProps(CResTOCManager &tocManager, LoadedMap &loaded, int selectedObjectLayer = -1);

/** Move every prop's three players on by one frame's worth of time. */
void AdvanceProps(std::vector<PlacedProp> &props, std::uint16_t deltaMs);

/** Move every drifting tile layer on by one frame's worth of time. */
void AdvanceTileLayers(CMap &map, std::uint16_t deltaMs);

/**
 * The quads a slot draws at its player's current step.
 *
 * An empty slot -- an unused animation, or one that expanded to nothing --
 * lands on the out-of-range path and draws nothing, which is why the players
 * of empty slots never need a special case anywhere else.
 */
const std::vector<SpriteQuad> &CurrentQuads(const PropSlot &slot,
                                            const CSpritePlayer &player);

/** Emit one prop's slot, positioned at the prop and offset by each quad. */
void AddSpriteQuads(const PlacedProp &prop, const std::vector<SpriteQuad> &quads,
                    CQuadBatch &batch);

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
void BuildMarkers(const LoadedMap &loaded, CMarkerBatch &markers,
                  PlacedObjectType wanted);

/**
 * Assemble exactly the collision shapes CLayerCollision tests for a player.
 *
 * The level script selects one map collision layer. Static props then add
 * their template-local geometry at their placed position. Keeping this as one
 * scene lets the existing resolver choose the nearest edge across both kinds
 * instead of resolving each prop in an arbitrary order.
 */
void BuildCollisionScene(LoadedMap &loaded);

/** Original script state lives beside the map instance, never in shared quads. */
class MapPropWorld : public IPropWorld {
public:
    MapPropWorld(LoadedMap &map, CombatScene &scene, CLevel &level, WeaponEffects &effects)
        : m_map(map), m_scene(scene), m_level(level), m_effects(effects) {
        for (PlacedProp &prop : m_map.props) {
            if (prop.sprite->data.GetScript().IsPresent()) { prop.runtime = std::make_shared<CProp>(); }
        }
    }

    void Reset() override {
        m_activeProps.clear();
        for (PlacedProp &prop : m_map.props) { prop.active = false; }
        BuildCollisionScene(m_map);
    }

    void StartLayer(int layer) override ;

    bool Spawn(int layer, int objectId) override ;

    void SendMessage(int objectId, int message) override ;

    unsigned ResolveIndicatorTarget(int objectId) const override {
        for (unsigned index = 0; index < m_activeProps.size(); ++index) {
            const auto &prop = *m_activeProps[index];
            if (prop.objectId == objectId && prop.active && (!prop.runtime || !prop.runtime->IsRemoved())) { return index + 1; }
        }
        return 0;
    }

    bool GetObjectPosition(int objectId, float &x, float &y) const override {
        const unsigned key = ResolveIndicatorTarget(objectId);
        if (key == 0) { return false; }
        x = m_activeProps[key - 1]->x;
        y = m_activeProps[key - 1]->y;
        return true;
    }

    bool GetIndicatorTarget(unsigned key, float &x, float &y) const override ;
    bool IsActivePortal(int objectId) const override {
        const unsigned key = ResolveIndicatorTarget(objectId);
        if (key == 0) { return false; }
        const auto &prop = *m_activeProps[key - 1];
        return prop.runtime != nullptr && prop.runtime->IsActivePortal();
    }

    void Update(int deltaMs) override ;

    CombatTrace Trace(const CombatHit &hit, float x, float y, float dx, float dy,
        float radius, const std::vector<CombatId> &skip) override ;

    HitResult ApplyHit(CombatId target, const CombatHit &hit) override ;

    void Splash(const CombatHit &hit, float radius) override ;

    unsigned GetFailures() const {
        unsigned failures = 0;
        for (const PlacedProp &prop : m_map.props) {
            if (prop.runtime != nullptr) { failures += prop.runtime->GetUnsupportedCount(); }
        }
        return failures;
    }

    unsigned GetHitCount() const { return m_hitCount; }
#if GB_ENABLE_TESTS

    unsigned CheckEntryRoutes();
#endif

#if GB_ENABLE_TESTS

    unsigned CheckDamageContracts();
#endif

private:
    static constexpr CombatId kPropIdBase = 1ull << 62;
    void SyncPlayers(PlacedProp &prop) {
        prop.foreground = prop.runtime->GetPlayer(2);
        prop.main = prop.runtime->GetPlayer(1);
        prop.background = prop.runtime->GetPlayer(0);
    }

    bool PlayerInside(const PlacedProp &prop) const ;

    void ApplyAction(PlacedProp &prop, const PropAction &action);

    LoadedMap &m_map;
    std::vector<PlacedProp *> m_activeProps; // Stable until the next level Reset.
    const CLayerCollision *m_bodyLayer = nullptr;
    const CLayerCollision *m_bulletLayer = nullptr;
    CombatScene &m_scene;
    CLevel &m_level;
    WeaponEffects &m_effects;
    unsigned m_hitCount = 0;
};

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
void ProjectEnemyHealthBars(std::vector<CombatScene::HealthBar> &bars,
    float cameraX, float cameraY, float zoom, int width, int height);

/** "3 player spawns, 41 enemy spawns" -- what the K overlay should show. */
void ReportSpawns(const LoadedMap &loaded);

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
void LoadPlacedEnemies(CResTOCManager &tocManager, const CShaderProgram &program,
                       LoadedMap &loaded);

/** Move every placed enemy's animation on. */
void AdvanceEnemies(LoadedMap &loaded, std::int32_t deltaMs);

/**
 * Stand a player on every spawn point the object layer names.
 *
 * The default model and nothing else: no weapon, no armour. Which gun a
 * player carries is a loadout question and the loadout is not read yet, so
 * putting one in his hand here would be inventing data.
 */
void LoadPlacedPlayers(CResTOCManager &tocManager, const CShaderProgram &program,
                       LoadedMap &loaded);

/** Move every placed player's animation on. */
void AdvancePlayers(LoadedMap &loaded, std::int32_t deltaMs);

/**
 * Drive the first player with WASD and resolve the requested movement.
 *
 * Maps contain one real player spawn. A few abandoned campaign maps contain
 * none; those remain valid viewers and simply ignore movement input.
 */
/** Swap equipment only after every referenced asset has loaded. */
bool EquipControlledPlayer(PackTables &tables, LoadedMap &loaded,
    const CShaderProgram &program, const WeaponEntry &weapon);

void AppendSurvivalShortcut(std::vector<KeyCode> &inputs, KeyCode key);

bool UpdateControlledPlayer(LoadedMap &loaded, const CWindow &window,
                            std::uint64_t elapsedMs);

/**
 * Run the animation clock forward, in the bites playback would use.
 *
 * What makes a still screenshot able to prove anything about animation: shoot
 * the same map at two different times and diff them. Deterministic, because
 * the bite size is fixed rather than taken from the wall clock.
 */
void WarmUp(LoadedMap &loaded, std::uint32_t totalMs,
            WeaponEffects *effects = nullptr, bool firing = false);

/**
 * Draw every model the object layer places -- enemies and players alike.
 *
 * Depth on, and the depth buffer cleared first: the terrain and props are 2D
 * and drawn in order, so they neither read nor write depth, but a model is
 * solid and needs it against itself.
 */
struct MapRenderItem {
    int group = kZGroupNormal;
    int y = 0;
    const PlacedProp *prop = nullptr;
    PlayerModel *player = nullptr;
    EnemyModel *enemy = nullptr;
    float matrix[kMatrix4dElements] = {};
};

/** CRenderQueue::Compare :145029: group, then integer world Y. */
bool MapItemDrawsBefore(const MapRenderItem &left, const MapRenderItem &right);

/** Shared main/foreground passes for the game and permanent map viewers.
 * CRenderQueue::Draw :145123 puts actors and scenery in the SAME main pass.
 * The caller has already drawn tiles and every prop's background slot.
 */
void DrawMapObjects(LoadedMap &loaded, CQuadBatch &batch, const CShaderProgram &program,
                const float *mapMvp, bool showProps = true, CombatScene *scene = nullptr,
                PlayerModel *brotherModel = nullptr, float brotherY = 0, int viewportWidth = 1);

/** How many objects of one type a map places. */
unsigned CountObjects(const LoadedMap &loaded, PlacedObjectType wanted);

void ReportSpawns(const LoadedMap &loaded);

void BuildGeometry(const LoadedMap &loaded, CQuadBatch &batch, bool showTiles,
                   bool showProps, bool report);

/** One map, wherever it lives. */
struct CatalogMap {
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
std::vector<CatalogMap> BuildCatalog(CResTOCManager &tocManager);

/** Slot of the first map of the pack `slot` belongs to. */
std::size_t FirstMapOfPack(const std::vector<CatalogMap> &catalog, std::size_t slot);

/**
 * Slot of the first map of the next pack, wrapping round the end.
 *
 * Left and right already cross pack boundaries one map at a time; this is the
 * shortcut for skipping a whole pack, which matters when pack2 alone holds
 * nine maps.
 */
std::size_t NextPackSlot(const std::vector<CatalogMap> &catalog, std::size_t slot);

/** Slot of the first map of the previous pack, wrapping round the start. */
std::size_t PreviousPackSlot(const std::vector<CatalogMap> &catalog,
                             std::size_t slot);

/** The camera state the viewer manipulates. */
struct Camera {
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
MapRectangle ViewedRegion(const LoadedMap &loaded);

/** Zoom out far enough to see the whole viewed region, and centre it. */
Camera FitCamera(const LoadedMap &loaded, int viewWidth, int viewHeight);

/** Centre the fixed GameView camera on the controlled player. */
void FollowPlayerCamera(const LoadedMap &loaded, int viewWidth, int viewHeight,
                        Camera &camera);

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
bool VisibleBoundsScissor(const LoadedMap &loaded, const Camera &camera,
                          int drawableWidth, int drawableHeight, int scissor[4]);

/** Open the archives and pick out one pack, already bound. */
CResPackTOC *OpenPack(CResTOCManager &tocManager, const std::string &bigDirectory,
                      const std::string &packShortName);

}

bool SaveSurvivalProgress(SurvivalGameContext *context, const CPlayerProgress &progress,
    const CombatScene &scene, const CLevel &level, std::uint64_t &accountedXplodium, bool missionEnded = false );


