#pragma once
/** Desktop resource caches and placed-object storage shared by game and viewer.
 * Original CMap, CProp and CParticleEffect still own their parsed data/behavior.
 * Declare storage before consumers so referenced templates outlive their actors.
 */
#include "engine/core/ZPaths.h"
#include "engine/glu/sprite/CSpriteGlu.h"
#include "engine/glu/sprite/CSpriteIterator.h"
#include "engine/glu/sprite/CSpritePlayer.h"
#include "gun_bros_re/gameplay/CMap.h"
#include "gun_bros_re/gameplay/CParticleEffect.h"
#include "gun_bros_re/gameplay/CProp.h"
#include "gun_bros_re/gameplay/TileSet.h"
#include "gun_bros_re/gameplay/ZEnemyModel.h"
#include "gun_bros_re/gameplay/ZPlayerModel.h"
#include "gun_bros_re/gameplay/ZWeaponEffects.h"
#include "gun_bros_re/data/CGameObjectPack.h"
#include <array>
#include <map>
#include <memory>
#include <vector>

namespace MapDetail {
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

enum class ZInteractivePropKind : std::uint8_t {
    None,
    Cover,
    Barrel,
    Spire,
};

enum class ZCoverState : std::uint8_t {
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
struct ZPropSlot {
    // One quad list per animation step, so playback is a subscript rather than
    // a walk back down the sprite tree. A template's steps run to a couple of
    // dozen at most, and expanding them all costs a fraction of the archive
    // read that got us the template in the first place.
    std::vector<std::vector<ZSpriteQuad>> quadsByStep;

    // The same steps' durations, which is all a CSpritePlayer needs.
    std::vector<std::uint16_t> stepDurationsMs;
};

/** The three sprite layers and collision rule used by one prop state. */
struct ZPropVisualState {
    ZPropSlot background;
    ZPropSlot main;
    ZPropSlot foreground;
    bool collisionEnabled;

    ZPropVisualState() : collisionEnabled(true) {}
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
struct ZPropSprite {
    CProp::Template data;
    GameObjectRef resource;
    std::vector<ZPropSlot> animations;
    std::vector<std::vector<std::uint16_t>> durations;
    ZPropSlot background;
    ZPropSlot main;
    ZPropSlot foreground;
    std::array<ZPropVisualState, kMaximumInteractiveStateCount> states;
    std::vector<ZScriptResourceRef> transitionResources;
    CCollisionData collision;
    CCollisionData bulletCollision;
    ZInteractivePropKind interactiveKind;
    std::uint8_t stateCount;
    int zOrderGroup;

    // What the walk could not draw, kept so the load can report a total
    // rather than a line per template.
    std::uint32_t skippedParts;
    std::uint32_t unsupportedTransforms;

    ZPropSprite()
        : interactiveKind(ZInteractivePropKind::None),
          stateCount(0),
          zOrderGroup(kZGroupNormal),
          skippedParts(0),
          unsupportedTransforms(0) {}
};

/** One prop standing on the map, each of its three slots playing its own. */
struct ZPlacedProp {
    std::shared_ptr<CProp> runtime;
    int objectId = -1;
    unsigned objectLayer = 0;
    bool active = true;
    ZCombatId lastDamager = 0;
    float x;
    float y;
    const ZPropSprite *sprite;
    std::uint8_t interactiveState;
    float hitFlashRemainingMs;

    CSpritePlayer background;
    CSpritePlayer main;
    CSpritePlayer foreground;

    ZPlacedProp()
        : x(0.0f),
          y(0.0f),
          sprite(nullptr),
          interactiveState(0),
          hitFlashRemainingMs(0.0f) {}
};

/** The per-pack tables a prop needs, built the first time that pack is used. */
struct ZPackResources {
    CGameObjectPack objectPack;
    CSpriteGlu spriteGlu;
    bool objectPackReady;
    bool spriteGluReady;

    ZPackResources() : objectPackReady(false), spriteGluReady(false) {}
};

/** Sprite animations belonging to one emitter in a particle template. */
struct ZParticleEmitterVisual {
    std::array<ZPropSlot, 32> animations;
};

/** Parsed particle data plus the already-expanded atlas quads it draws. */
struct ZParticleEffectVisual {
    CParticleEffect effect;
    std::vector<ZParticleEmitterVisual> emitters;
};

/** One particle emitted by an active effect. */
struct ZLiveParticle {
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
struct ZActiveParticleEffect {
    std::uint64_t visualKey;
    float x;
    float y;
    int zOrderGroup;
    float ageMs;
    std::uint32_t randomState;
    std::vector<float> nextSpawnMs;
    std::vector<ZLiveParticle> particles;
};

/** One enemy template standing on the map, with the model it draws as. */
struct ZPlacedEnemy {
    float x;
    float y;

    // Both by pointer, and both for the same reason: CEnemy::Bind keeps the
    // ADDRESS of the move set and the script, so the template has to stay put
    // for as long as the model does. Holding either by value here would leave
    // the model pointing at freed memory the moment this vector grew.
    std::unique_ptr<ZEnemyTemplateData> templateData;
    std::unique_ptr<ZEnemyModel> model;

    // The template's game scale, which is half of how big it is drawn.
    float gameScale;
};

/** A player standing on one of the map's spawn points. */
struct ZPlacedPlayer {
    float x;
    float y;
    float facingDegrees;
    bool moving;

    // By pointer for the same reason a PlacedEnemy's model is: it owns GL
    // buffers and its controllers point back into it, so it cannot be moved
    // once built.
    std::unique_ptr<ZPlayerModel> model;

    ZPlacedPlayer()
        : x(0.0f), y(0.0f), facingDegrees(0.0f), moving(false) {}
};

/** Everything one map needs to draw, held together for the render loop. */
struct ZLoadedMap {
    CMap map;
    TileSet tileSet;
    std::vector<std::unique_ptr<ZTexture>> textures;

    // Effective player collision: the level-selected map layer plus every
    // placed prop's local collision translated into world space.
    CCollisionData collisionScene;
    ZWeaponCollision weaponCollision;

    // The enemies the object layer places. Held by pointer because an
    // EnemyModel owns GL buffers and points at its own meshes.
    std::vector<ZPlacedEnemy> enemies;

    // The player template, owned here because every PlacedPlayer's controllers
    // hold the ADDRESS of its move set -- the same trap PlacedEnemy documents.
    std::unique_ptr<ZPlayerTemplateData> playerTemplate;
    std::vector<ZPlacedPlayer> players;

    // Props, and everything they hang off. The caches own the atlas pages, so
    // they have to outlive the props that point into them -- replacing a
    // LoadedMap wholesale is what keeps that true.
    std::map<int, std::unique_ptr<ZPackResources>> packs;
    std::map<std::uint64_t, ZPropSprite> propSprites;
    std::vector<ZPlacedProp> props;

    // Effects are cached by their game-object address and instantiated only
    // when a transition asks for one.
    std::map<std::uint64_t, ZParticleEffectVisual> particleEffects;
    std::vector<ZActiveParticleEffect> activeParticleEffects;
};

}
