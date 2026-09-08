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
#include "milestones/M3Map.h"

#include "milestones/EnemyModel.h"
#include "milestones/SurvivalPilot.h"
#include "runtime/PlayerModel.h"
#include "runtime/WeaponCatalog.h"
#include "runtime/ArmorCatalog.h"
#include "runtime/SurvivalSession.h"
#include "runtime/CombatGeometry.h"
#include "runtime/StoreCatalog.h"
#include <sstream>
#include "runtime/SurvivalGameContext.h"
#include "runtime/HudText.h"
#include "runtime/MissionCatalog.h"
#include "gun_bros/WeaponEffects.h"
#include "runtime/PackTables.h"

#include "engine/CArrayInputStream.h"
#include "engine/CAudioPlayer.h"
#include "engine/CMarkerBatch.h"
#include "engine/CMatrix4d.h"
#include "engine/CPNG.h"
#include "engine/CQuadBatch.h"
#include "engine/CShaderProgram.h"
#include "engine/CTexture.h"
#include "engine/platform/CWindow.h"
#include "engine/platform/GLLoader.h"
#include "gun_bros/CGameObjectPack.h"
#include "gun_bros/CGameAssetRef.h"
#include "gun_bros/CLayerObject.h"
#include "gun_bros/CLayerTile.h"
#include "gun_bros/CLevel.h"
#include "gun_bros/CMap.h"
#include "gun_bros/CParticleEffect.h"
#include "gun_bros/CProp.h"
#include "gun_bros/CResTOCManager.h"
#include "gun_bros/TileSet.h"
#include "sprite_glu/CSpriteGlu.h"
#include "sprite_glu/CSpriteIterator.h"
#include "sprite_glu/CSpritePlayer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

namespace {

const char *const kShaderDirectory = ASSET_ROOT "/src/gun_bros_re/shaders";

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
                                int packIndex) {
    std::map<int, std::unique_ptr<PackResources>>::iterator found =
        loaded.packs.find(packIndex);
    if (found != loaded.packs.end()) {
        return found->second.get();
    }

    CResPackTOC *pack = tocManager.GetPack(packIndex);
    if (pack == nullptr) {
        return nullptr;
    }

    std::unique_ptr<PackResources> resources(new PackResources());
    resources->objectPackReady = resources->objectPack.Init(*pack);
    resources->spriteGluReady = resources->spriteGlu.Init(*pack);

    PackResources *result = resources.get();
    loaded.packs[packIndex] = std::move(resources);
    return result;
}

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
                         std::vector<std::uint8_t> &payload) {
    const int packIndex = tocManager.GetPackIndexFromHash(packHash);
    PackResources *resources = GetPackResources(tocManager, loaded, packIndex);
    if (resources == nullptr || !resources->objectPackReady) {
        std::printf("[m3] pack %08X has no section table\n", packHash);
        return false;
    }

    const std::uint32_t handle = resources->objectPack.GetHandle(section, localIndex);
    if (handle == 0) {
        std::printf("[m3] pack %08X section %u has no base\n", packHash,
                    static_cast<unsigned>(section));
        return false;
    }
    if (!tocManager.GetPack(packIndex)->GetResource(handle, payload)) {
        std::printf("[m3] handle 0x%08X unreadable\n", handle);
        return false;
    }

    return true;
}

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
                       std::uint32_t mapPackHash, std::uint32_t mapIndex) {
    const int packIndex = tocManager.GetPackIndexFromHash(mapPackHash);
    PackResources *resources = GetPackResources(tocManager, out, packIndex);
    CResPackTOC *pack = tocManager.GetPack(packIndex);
    if (resources == nullptr || !resources->objectPackReady || pack == nullptr) {
        return;
    }

    const std::uint32_t levelCount =
        resources->objectPack.GetObjectCount(GameSection::Level);
    std::vector<std::uint8_t> payload;

    for (std::uint32_t i = 0; i < levelCount; ++i) {
        const std::uint32_t handle =
            resources->objectPack.GetHandle(GameSection::Level, i);
        if (handle == 0 || !pack->GetResource(handle, payload)) {
            continue;
        }

        CArrayInputStream stream(payload);
        CLevel::Template levelTemplate;
        if (!levelTemplate.Init(stream)) {
            std::printf("[m3] level %u: template runs past the end of the resource\n", i);
            continue;
        }
        if (stream.Available() != 0) {
            std::printf("[m3] level %u: %u bytes left over after the template\n", i,
                        static_cast<unsigned>(stream.Available()));
        }

        if (levelTemplate.mapRef.packHash != mapPackHash ||
            levelTemplate.mapRef.localIndex != mapIndex) {
            continue;
        }

        CLevel level;
        level.Bind(levelTemplate, out.map);
        // The count covers CLevel's own functions. Calls aimed at the other
        // eleven classes are logged by ScriptResolver, which has no level to
        // count them against.
        std::printf("[m3]   level %u ran; %u level functions it wanted are not "
                    "implemented\n",
                    i, level.GetUnimplementedCallCount());
    }
}

/**
 * Load a map, its tile set and the atlases the tile set names.
 *
 * @param mapPackIndex Which pack the map itself lives in. Everything it
 *                     references is addressed by its own pack hash, not by
 *                     this one.
 */
bool LoadMap(CResTOCManager &tocManager, int mapPackIndex, std::uint32_t mapIndex,
             LoadedMap &out) {
    CResPackTOC *mapPack = tocManager.GetPack(mapPackIndex);
    if (mapPack == nullptr) {
        std::printf("[m3] no pack at index %d\n", mapPackIndex);
        return false;
    }

    PackResources *resources = GetPackResources(tocManager, out, mapPackIndex);
    if (resources == nullptr || !resources->objectPackReady) {
        return false;
    }

    // --- the map ---
    const std::uint32_t mapCount =
        resources->objectPack.GetObjectCount(GameSection::TileLayer);
    if (mapIndex >= mapCount) {
        std::printf("[m3] %s has %u maps; %u is out of range\n",
                    mapPack->GetShortName().c_str(), mapCount, mapIndex);
        return false;
    }

    std::vector<std::uint8_t> payload;
    if (!ReadSectionResource(tocManager, out, mapPack->GetPackHash(),
                             GameSection::TileLayer, mapIndex, payload)) {
        return false;
    }

    CArrayInputStream mapStream(payload);
    if (!out.map.Init(mapStream)) {
        return false;
    }

    std::printf("[m3] map %u: %ux%u tiles, %u tile layers, %u object layers",
                mapIndex, out.map.GetCanvasWidth(), out.map.GetCanvasHeight(),
                out.map.GetTileLayerCount(), out.map.GetObjectLayerCount());
    if (out.map.GetUnparsedLayerCount() > 0) {
        std::printf(", %u layers abandoned at an unknown type",
                    out.map.GetUnparsedLayerCount());
    }
    std::printf("\n");

    ApplyLevelScripts(tocManager, out, mapPack->GetPackHash(), mapIndex);

    // After the scripts, because setCameraLayer is one of the first things a
    // level does and it decides which of these rectangles is the live one.
    const MapRectangle visibleBounds = out.map.GetVisibleBounds();
    const MapRectangle extent = out.map.GetCameraExtent();
    std::printf("[m3]   %u camera layers; level opens on (%d, %d) %d x %d px, "
                "showing all of (%d, %d) %d x %d px\n",
                out.map.GetCameraLayerCount(), visibleBounds.x, visibleBounds.y,
                visibleBounds.width, visibleBounds.height, extent.x, extent.y,
                extent.width, extent.height);

    // --- the tile set ---
    const GameObjectRef &tileSetRef = out.map.GetTileSetRef();
    if (tileSetRef.IsNull()) {
        std::printf("[m3] map has no tile set\n");
        return false;
    }

    if (!ReadSectionResource(tocManager, out, tileSetRef.packHash,
                             GameSection::TileSet, tileSetRef.localIndex, payload)) {
        return false;
    }

    CArrayInputStream tileSetStream(payload);
    if (!out.tileSet.Init(tileSetStream)) {
        return false;
    }
    std::printf("[m3] tile set %u: %zu atlases, %u tiles, draw size %u\n",
                tileSetRef.localIndex, out.tileSet.GetImages().size(),
                out.tileSet.GetTileCount(), out.tileSet.GetDrawSize());

    // --- the atlases ---
    // A tile set's image refs are CGameAssetRefs, and TileSet::Load is what
    // decides they index the PNG section. The data does not say so.
    const std::vector<CGameAssetRef> &images = out.tileSet.GetImages();
    for (std::size_t i = 0; i < images.size(); ++i) {
        if (!ReadSectionResource(tocManager, out, images[i].packHash, GameSection::Png,
                                 static_cast<std::uint32_t>(images[i].assetId),
                                 payload)) {
            return false;
        }

        PNGImage decoded;
        if (!PNGDecode(payload, decoded)) {
            return false;
        }

        std::unique_ptr<CTexture> texture(new CTexture());
        if (!texture->Create(decoded)) {
            return false;
        }
        std::printf("[m3]   atlas %zu: PNG ordinal %d -> %ux%u\n",
                    i, images[i].assetId, decoded.width, decoded.height);
        out.textures.push_back(std::move(texture));
    }

    return true;
}

/**
 * Expand every step of one animation into the quads that step draws.
 *
 * An unused slot -- animation 255, which is how a prop says it has no
 * foreground or no main sprite -- comes back empty, and so does one whose
 * animation is out of range. Neither is an error: most templates fill one slot
 * of the three, and a player pointed at an empty slot simply never ticks.
 */
void ExpandSlot(CSpriteIterator &iterator, const CSpriteGluArchetype &archetype,
                std::uint8_t animationIndex, PropSlot &out) {
    if (animationIndex == kNoSpriteGluIndex) {
        return;
    }
    if (animationIndex >= archetype.GetAnimationCount()) {
        return;
    }

    const SpriteAnimation &animation = archetype.GetAnimation(animationIndex);
    const std::size_t stepCount = animation.steps.size();

    out.quadsByStep.resize(stepCount);
    out.stepDurationsMs.resize(stepCount);

    for (std::size_t step = 0; step < stepCount; ++step) {
        out.stepDurationsMs[step] = animation.steps[step].durationMs;
        iterator.Expand(animationIndex, static_cast<std::uint32_t>(step),
                        out.quadsByStep[step]);
    }
}

/**
 * Whether any of a template's slots has more than one step to play.
 *
 * Most scenery is a single step and stands perfectly still, which is correct
 * and also indistinguishable from a broken clock. Counting the ones that can
 * move is what tells the two apart without staring at the window.
 */
bool PropAnimates(const PropSprite &sprite) {
    if (sprite.interactiveKind != InteractivePropKind::None) {
        for (std::uint8_t state = 0; state < sprite.stateCount; ++state) {
            const PropVisualState &visual = sprite.states[state];
            if (visual.background.stepDurationsMs.size() > 1 ||
                visual.main.stepDurationsMs.size() > 1 ||
                visual.foreground.stepDurationsMs.size() > 1) {
                return true;
            }
        }
    }
    if (sprite.background.stepDurationsMs.size() > 1) {
        return true;
    }
    if (sprite.main.stepDurationsMs.size() > 1) {
        return true;
    }
    return sprite.foreground.stepDurationsMs.size() > 1;
}

/** Whether a slot draws anything on its first step. */
bool SlotDrawsAtStart(const PropSlot &slot) {
    if (slot.quadsByStep.empty()) {
        return false;
    }
    return !slot.quadsByStep[0].empty();
}

/** Whether a PROP reference names one of the two PvP barricade orientations. */
bool IsDestructibleCover(std::uint32_t packHash, std::uint8_t localIndex) {
    if (packHash != kCoverPackHash) {
        return false;
    }
    return localIndex == kHorizontalCoverTemplate ||
           localIndex == kVerticalCoverTemplate;
}

InteractivePropKind InteractiveKindFor(std::uint32_t packHash,
                                       std::uint8_t localIndex) {
    if (IsDestructibleCover(packHash, localIndex)) {
        return InteractivePropKind::Cover;
    }
    if ((packHash == kLavaPackHash && localIndex == kLavaBarrelTemplate) ||
        (packHash == kWaterPackHash && localIndex == kWaterBarrelTemplate)) {
        return InteractivePropKind::Barrel;
    }
    if (packHash == kSpirePackHash && localIndex == kSpireTemplate) {
        return InteractivePropKind::Spire;
    }
    return InteractivePropKind::None;
}

void ExpandPropState(CSpriteIterator &iterator,
                     const CSpriteGluArchetype &archetype,
                     std::uint8_t backgroundAnimation,
                     std::uint8_t mainAnimation,
                     std::uint8_t foregroundAnimation,
                     PropVisualState &state) {
    ExpandSlot(iterator, archetype, backgroundAnimation, state.background);
    ExpandSlot(iterator, archetype, mainAnimation, state.main);
    ExpandSlot(iterator, archetype, foregroundAnimation, state.foreground);
}

/** Build the known visual states named by the three original prop scripts. */
void BuildInteractiveStates(std::uint32_t packHash, std::uint8_t localIndex,
                            CSpriteIterator &iterator,
                            const CSpriteGluArchetype &archetype,
                            PropSprite &out) {
    out.interactiveKind = InteractiveKindFor(packHash, localIndex);
    if (out.interactiveKind == InteractivePropKind::Cover) {
        out.stateCount = 5;
        std::uint8_t firstAnimation = 0;
        if (localIndex == kVerticalCoverTemplate) {
            firstAnimation = 4;
        }
        for (std::uint8_t state = 0; state < kCoverVisibleStateCount; ++state) {
            ExpandPropState(iterator, archetype,
                            static_cast<std::uint8_t>(firstAnimation + state),
                            kNoSpriteGluIndex, kNoSpriteGluIndex,
                            out.states[state]);
        }
        out.states[3].collisionEnabled = false;
        out.states[4].collisionEnabled = false;
        return;
    }

    if (out.interactiveKind == InteractivePropKind::Barrel) {
        out.stateCount = 4;
        std::uint8_t firstAnimation = 29;
        if (packHash == kWaterPackHash) {
            firstAnimation = 21;
        }
        for (std::uint8_t state = 0; state < 3; ++state) {
            ExpandPropState(iterator, archetype, kNoSpriteGluIndex,
                            static_cast<std::uint8_t>(firstAnimation + state),
                            kNoSpriteGluIndex, out.states[state]);
        }
        if (packHash == kWaterPackHash) {
            ExpandPropState(iterator, archetype, 5, kNoSpriteGluIndex,
                            kNoSpriteGluIndex, out.states[3]);
        }
        out.states[3].collisionEnabled = false;
        return;
    }

    if (out.interactiveKind == InteractivePropKind::Spire) {
        out.stateCount = 3;
        for (std::uint8_t state = 0; state < out.stateCount; ++state) {
            ExpandPropState(iterator, archetype,
                            static_cast<std::uint8_t>(53 + state),
                            static_cast<std::uint8_t>(50 + state),
                            kNoSpriteGluIndex, out.states[state]);
        }
    }
}

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
                     PropSprite &out) {
    std::vector<std::uint8_t> payload;
    if (!ReadSectionResource(tocManager, loaded, propPackHash, GameSection::Prop,
                             localIndex, payload)) {
        return false;
    }

    CArrayInputStream stream(payload);
    CProp::Template propTemplate;
    if (!propTemplate.Init(stream)) {
        return false;
    }
    out.data = propTemplate;
    out.resource.packHash = propPackHash;
    out.resource.localIndex = localIndex;

    // The sprite lives in whichever pack the reference names, which need not
    // be the one the template came from.
    const CGameSpriteGluRef &spriteRef = propTemplate.GetSpriteRef();
    const int gluPackIndex = tocManager.GetPackIndexFromHash(spriteRef.packHash);
    PackResources *gluPack = GetPackResources(tocManager, loaded, gluPackIndex);
    if (gluPack == nullptr || !gluPack->spriteGluReady) {
        return false;
    }

    const CSpriteGluArchetype *archetype =
        gluPack->spriteGlu.GetArchetype(spriteRef.archetype);
    if (archetype == nullptr) {
        return false;
    }

    CSpriteIterator iterator(gluPack->spriteGlu, *archetype);
    if (propTemplate.GetScript().IsPresent()) {
        out.animations.resize(archetype->GetAnimationCount());
        out.durations.resize(archetype->GetAnimationCount());
        for (unsigned animation = 0; animation < archetype->GetAnimationCount(); ++animation) {
            ExpandSlot(iterator, *archetype, static_cast<std::uint8_t>(animation), out.animations[animation]);
            out.durations[animation] = out.animations[animation].stepDurationsMs;
        }
    }
    BuildInteractiveStates(propPackHash, localIndex, iterator, *archetype, out);
    out.transitionResources = propTemplate.GetScript().GetResources();
    if (out.interactiveKind == InteractivePropKind::None) {
        ExpandSlot(iterator, *archetype, propTemplate.GetBackgroundAnimation(),
                   out.background);
        ExpandSlot(iterator, *archetype, propTemplate.GetMainAnimation(), out.main);
        ExpandSlot(iterator, *archetype, propTemplate.GetForegroundAnimation(),
                   out.foreground);
    }
    out.collision = propTemplate.GetCollision();
    out.bulletCollision = propTemplate.GetBulletCollision();

    out.skippedParts = iterator.GetSkippedPartCount();
    out.unsupportedTransforms = iterator.GetUnsupportedTransformCount();

    // Judged on the first step alone, which is what M3.1 sorted on before
    // anything animated. Playback moves what a slot draws but never whether it
    // draws, so keeping the test on step 0 keeps the draw order fixed for the
    // life of the map -- and lets the queue stay sorted once, at load.
    bool backgroundDraws = SlotDrawsAtStart(out.background);
    bool mainDraws = SlotDrawsAtStart(out.main);
    bool foregroundDraws = SlotDrawsAtStart(out.foreground);
    if (out.interactiveKind != InteractivePropKind::None) {
        backgroundDraws = SlotDrawsAtStart(out.states[0].background);
        mainDraws = SlotDrawsAtStart(out.states[0].main);
        foregroundDraws = SlotDrawsAtStart(out.states[0].foreground);
    }

    if (mainDraws) {
        out.zOrderGroup = kZGroupNormal;
    } else if (backgroundDraws) {
        out.zOrderGroup = kZGroupBackgroundOnly;
    } else if (foregroundDraws) {
        out.zOrderGroup = kZGroupForegroundOnly;
    } else {
        out.zOrderGroup = kZGroupNormal;
    }

    return true;
}

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
std::uint32_t StartStepFor(std::size_t propOrdinal, std::size_t stepCount) {
    if (stepCount <= 1) {
        return 0;
    }

    const std::uint32_t scrambled =
        static_cast<std::uint32_t>(propOrdinal) * 2654435761u;
    return (scrambled >> 16) % static_cast<std::uint32_t>(stepCount);
}

/** Background slot selected by a prop's current cover state. */
const PropSlot *RuntimeSlotFor(const PlacedProp &prop, unsigned slot) {
    static const PropSlot empty;
    if (prop.runtime->IsRemoved()) { return &empty; }
    const int animation = prop.runtime->GetAnimation(slot);
    if (animation < 0 || animation >= static_cast<int>(prop.sprite->animations.size())) { return &empty; }
    return &prop.sprite->animations[animation];
}

const PropSlot *BackgroundSlotFor(const PlacedProp &prop) {
    if (prop.runtime != nullptr) { return RuntimeSlotFor(prop, 2); }
    if (prop.sprite->interactiveKind == InteractivePropKind::None) {
        return &prop.sprite->background;
    }
    if (prop.interactiveState >= prop.sprite->stateCount) {
        return &prop.sprite->background;
    }
    return &prop.sprite->states[prop.interactiveState].background;
}

const PropSlot *MainSlotFor(const PlacedProp &prop) {
    if (prop.runtime != nullptr) { return RuntimeSlotFor(prop, 1); }
    if (prop.sprite->interactiveKind == InteractivePropKind::None) {
        return &prop.sprite->main;
    }
    if (prop.interactiveState >= prop.sprite->stateCount) {
        return &prop.sprite->main;
    }
    return &prop.sprite->states[prop.interactiveState].main;
}

const PropSlot *ForegroundSlotFor(const PlacedProp &prop) {
    if (prop.runtime != nullptr) { return RuntimeSlotFor(prop, 0); }
    if (prop.sprite->interactiveKind == InteractivePropKind::None) {
        return &prop.sprite->foreground;
    }
    if (prop.interactiveState >= prop.sprite->stateCount) {
        return &prop.sprite->foreground;
    }
    return &prop.sprite->states[prop.interactiveState].foreground;
}

/**
 * Point one placed prop's three players at their slots, as CProp::Bind does.
 *
 * All three loop forwards, which is CSpritePlayer's constructed state and
 * which Bind never changes. Only the main slot starts part-way in; the other
 * two begin at step 0, so a prop's foreground and background stay in step with
 * each other however its body is phased.
 */
void StartPropPlayers(PlacedProp &prop, std::size_t propOrdinal) {
    const PropSprite &sprite = *prop.sprite;

    prop.interactiveState = 0;
    prop.hitFlashRemainingMs = 0.0f;
    const PropSlot *background = BackgroundSlotFor(prop);
    prop.background.SetAnimation(&background->stepDurationsMs);
    const PropSlot *foreground = ForegroundSlotFor(prop);
    prop.foreground.SetAnimation(&foreground->stepDurationsMs);

    const PropSlot *main = MainSlotFor(prop);
    prop.main.SetAnimation(&main->stepDurationsMs);
    prop.main.SetStep(StartStepFor(propOrdinal, main->stepDurationsMs.size()));
}

/** Human-readable state for the keyboard diagnostic. */
const char *CoverStateName(CoverState state) {
    switch (state) {
        case CoverState::Intact:
            return "intact";
        case CoverState::Damaged:
            return "damaged";
        case CoverState::BadlyDamaged:
            return "badly damaged";
        case CoverState::Destroyed:
            return "destroyed";
        case CoverState::Hidden:
            return "hidden";
        default:
            return "unknown";
    }
}

/** Advance the shared cover state, wrapping hidden back to intact. */
CoverState NextCoverState(CoverState state) {
    std::uint8_t next = static_cast<std::uint8_t>(state) + 1;
    if (next >= static_cast<std::uint8_t>(CoverState::Count)) {
        next = 0;
    }
    return static_cast<CoverState>(next);
}

/** Apply one state to every destructible cover on the current map. */
std::uint32_t SetCoverState(LoadedMap &loaded, CoverState state) {
    std::uint32_t changed = 0;
    for (std::size_t i = 0; i < loaded.props.size(); ++i) {
        PlacedProp &prop = loaded.props[i];
        if (prop.sprite->interactiveKind != InteractivePropKind::Cover) {
            continue;
        }

        prop.interactiveState = static_cast<std::uint8_t>(state);
        const PropSlot *background = BackgroundSlotFor(prop);
        prop.background.SetAnimation(&background->stepDurationsMs);
        prop.main.SetAnimation(&MainSlotFor(prop)->stepDurationsMs);
        prop.foreground.SetAnimation(&ForegroundSlotFor(prop)->stepDurationsMs);
        prop.hitFlashRemainingMs = 500.0f;
        changed++;
    }
    return changed;
}

const char *InteractiveStateName(InteractivePropKind kind, std::uint8_t state) {
    if (kind == InteractivePropKind::Barrel) {
        const char *const names[] = {"intact", "damaged", "critical", "exploded"};
        if (state < 4) {
            return names[state];
        }
    }
    if (kind == InteractivePropKind::Spire) {
        const char *const names[] = {"dormant", "charged", "shockwave"};
        if (state < 3) {
            return names[state];
        }
    }
    return "unknown";
}

/** Apply one visual state to every prop of a scripted interactive kind. */
std::uint32_t SetInteractiveState(LoadedMap &loaded, InteractivePropKind kind,
                                  std::uint8_t state) {
    std::uint32_t changed = 0;
    for (std::size_t i = 0; i < loaded.props.size(); ++i) {
        PlacedProp &prop = loaded.props[i];
        if (prop.sprite->interactiveKind != kind ||
            state >= prop.sprite->stateCount) {
            continue;
        }

        prop.interactiveState = state;
        prop.background.SetAnimation(&BackgroundSlotFor(prop)->stepDurationsMs);
        prop.main.SetAnimation(&MainSlotFor(prop)->stepDurationsMs);
        prop.foreground.SetAnimation(&ForegroundSlotFor(prop)->stepDurationsMs);
        if (kind != InteractivePropKind::Spire) {
            prop.hitFlashRemainingMs = 500.0f;
        }
        changed++;
    }
    return changed;
}

std::uint64_t AssetKey(std::uint32_t packHash, std::uint32_t localIndex) {
    return (static_cast<std::uint64_t>(packHash) << 32) | localIndex;
}

int SoundResourceForState(InteractivePropKind kind, std::uint8_t state) {
    if (kind == InteractivePropKind::Cover && state >= 1 && state <= 3) {
        return 4;
    }
    if (kind == InteractivePropKind::Barrel && state == 3) {
        return 2;
    }
    if (kind == InteractivePropKind::Spire && state == 1) {
        return 2;
    }
    if (kind == InteractivePropKind::Spire && state == 2) {
        return 3;
    }
    return -1;
}

/** Resolve SoundEffect -> WAV, cache it, then play one batch transition cue. */
void PlayTransitionSound(CResTOCManager &tocManager, LoadedMap &loaded,
                         CAudioPlayer &audio, InteractivePropKind kind,
                         std::uint8_t state) {
    const int resourceIndex = SoundResourceForState(kind, state);
    if (resourceIndex < 0) {
        return;
    }

    const PropSprite *sprite = nullptr;
    for (std::size_t i = 0; i < loaded.props.size(); ++i) {
        if (loaded.props[i].sprite->interactiveKind == kind) {
            sprite = loaded.props[i].sprite;
            break;
        }
    }
    if (sprite == nullptr ||
        resourceIndex >= static_cast<int>(sprite->transitionResources.size())) {
        return;
    }

    const ScriptResourceRef &soundEffect =
        sprite->transitionResources[resourceIndex];
    std::vector<std::uint8_t> soundPayload;
    if (!ReadSectionResource(tocManager, loaded, soundEffect.packHash,
                             GameSection::SoundEffect, soundEffect.resourceId,
                             soundPayload)) {
        return;
    }

    CArrayInputStream soundStream(soundPayload);
    CGameAssetRef wavReference;
    wavReference.Init(soundStream);
    if (wavReference.IsNull() || wavReference.assetId < 0) {
        return;
    }

    const std::uint64_t key = AssetKey(
        wavReference.packHash, static_cast<std::uint32_t>(wavReference.assetId));
    std::vector<std::uint8_t> wavPayload;
    if (!ReadSectionResource(tocManager, loaded, wavReference.packHash,
                             GameSection::Wav,
                             static_cast<std::uint32_t>(wavReference.assetId),
                             wavPayload)) {
        return;
    }
    if (audio.Load(key, wavPayload)) {
        audio.Play(key);
    }
}

/** Parse and expand one original particle template the first time it is used. */
bool EnsureParticleEffectVisual(CResTOCManager &tocManager, LoadedMap &loaded,
                                const ScriptResourceRef &reference,
                                std::uint64_t &visualKey) {
    visualKey = AssetKey(reference.packHash, reference.resourceId);
    if (loaded.particleEffects.find(visualKey) != loaded.particleEffects.end()) {
        return true;
    }

    std::vector<std::uint8_t> payload;
    if (!ReadSectionResource(tocManager, loaded, reference.packHash,
                             GameSection::ParticleEffect,
                             reference.resourceId, payload)) {
        return false;
    }

    ParticleEffectVisual visual;
    CArrayInputStream stream(payload);
    if (!visual.effect.Init(stream)) {
        std::printf("[particle] effect %08X/%u could not be parsed\n",
                    reference.packHash, reference.resourceId);
        return false;
    }

    const int spritePackIndex = tocManager.GetPackIndexFromHash(
        visual.effect.GetSpritePackHash());
    PackResources *spritePack = GetPackResources(tocManager, loaded,
                                                  spritePackIndex);
    if (spritePack == nullptr || !spritePack->spriteGluReady) {
        return false;
    }

    const std::vector<ParticleEmitterTemplate> &emitters =
        visual.effect.GetEmitters();
    visual.emitters.resize(emitters.size());
    for (std::size_t emitterIndex = 0; emitterIndex < emitters.size();
         ++emitterIndex) {
        const CSpriteGluArchetype *archetype =
            spritePack->spriteGlu.GetArchetype(emitters[emitterIndex].archetype);
        if (archetype == nullptr) {
            continue;
        }

        CSpriteIterator iterator(spritePack->spriteGlu, *archetype);
        for (int animation = 0; animation < 32; ++animation) {
            if ((emitters[emitterIndex].animationMask & (1u << animation)) != 0) {
                ExpandSlot(iterator, *archetype, static_cast<std::uint8_t>(animation),
                           visual.emitters[emitterIndex].animations[animation]);
            }
        }
    }

    loaded.particleEffects.insert(std::make_pair(visualKey, visual));
    return true;
}

/** Deterministic local random stream, independent from map animation timing. */
float NextParticleRandom(std::uint32_t &state) {
    state = state * 1664525u + 1013904223u;
    const std::uint32_t fraction = (state >> 8) & 0x00FFFFFFu;
    return static_cast<float>(fraction) / 16777215.0f;
}

float ParticleRandomRange(std::uint32_t &state, float minimum, float maximum) {
    return minimum + (maximum - minimum) * NextParticleRandom(state);
}

float ParticleRangeAt(float minimum, float maximum, float randomValue) {
    return minimum + (maximum - minimum) * randomValue;
}

/** Queue one cached effect at a prop's world position. */
void StartParticleEffect(LoadedMap &loaded, std::uint64_t visualKey,
                         float x, float y, int zOrderGroup,
                         std::uint32_t randomSalt) {
    const std::map<std::uint64_t, ParticleEffectVisual>::const_iterator found =
        loaded.particleEffects.find(visualKey);
    if (found == loaded.particleEffects.end()) {
        return;
    }

    ActiveParticleEffect active;
    active.visualKey = visualKey;
    active.x = x;
    active.y = y;
    active.zOrderGroup = zOrderGroup;
    active.ageMs = 0.0f;
    active.randomState = static_cast<std::uint32_t>(visualKey) ^ randomSalt;

    const std::vector<ParticleEmitterTemplate> &emitters =
        found->second.effect.GetEmitters();
    active.nextSpawnMs.resize(emitters.size());
    for (std::size_t emitter = 0; emitter < emitters.size(); ++emitter) {
        float startMs = emitters[emitter].startSeconds * kSecondsToMilliseconds;
        if (startMs < 0.0f) {
            startMs = 0.0f;
        }
        active.nextSpawnMs[emitter] = startMs;
        active.randomState ^= static_cast<std::uint32_t>(emitter * 7919u);
    }
    loaded.activeParticleEffects.push_back(active);
}

/** Original script resource indices and z groups for one state transition. */
void StartTransitionParticlesForProp(CResTOCManager &tocManager,
                                     LoadedMap &loaded,
                                     const PlacedProp &prop,
                                     std::uint8_t state,
                                     std::uint32_t randomSalt) {
    int firstResource = -1;
    int firstZGroup = kZGroupNormal;
    int secondResource = -1;
    int secondZGroup = kZGroupNormal;

    const InteractivePropKind kind = prop.sprite->interactiveKind;
    if (kind == InteractivePropKind::Cover && state >= 1 && state <= 3) {
        firstResource = 3;
        firstZGroup = 5;
        secondResource = 1;
        secondZGroup = 2;
    } else if (kind == InteractivePropKind::Barrel && state == 3) {
        firstResource = 0;
        secondResource = 1;
    } else if (kind == InteractivePropKind::Spire && state == 1) {
        firstResource = 0;
    } else if (kind == InteractivePropKind::Spire && state == 2) {
        firstResource = 1;
    }

    const int resources[2] = {firstResource, secondResource};
    const int zGroups[2] = {firstZGroup, secondZGroup};
    for (std::size_t cue = 0; cue < 2; ++cue) {
        const int resourceIndex = resources[cue];
        if (resourceIndex < 0 ||
            resourceIndex >=
                static_cast<int>(prop.sprite->transitionResources.size())) {
            continue;
        }

        const ScriptResourceRef &reference =
            prop.sprite->transitionResources[resourceIndex];
        std::uint64_t visualKey = 0;
        if (!EnsureParticleEffectVisual(tocManager, loaded, reference,
                                        visualKey)) {
            continue;
        }
        StartParticleEffect(loaded, visualKey, prop.x, prop.y, zGroups[cue],
                            randomSalt + static_cast<std::uint32_t>(cue));
    }
}

/** Start each matching prop's script-authored particle cues. */
void StartTransitionParticles(CResTOCManager &tocManager, LoadedMap &loaded,
                              InteractivePropKind kind, std::uint8_t state) {
    for (std::size_t i = 0; i < loaded.props.size(); ++i) {
        const PlacedProp &prop = loaded.props[i];
        if (prop.sprite->interactiveKind != kind) {
            continue;
        }
        StartTransitionParticlesForProp(tocManager, loaded, prop, state,
                                        static_cast<std::uint32_t>(i * 2654435761u));
    }
}

/** Create one live particle from an emitter's pattern and velocity. */
void SpawnParticle(ActiveParticleEffect &active,
                   const ParticleEmitterTemplate &emitter,
                   std::uint32_t emitterIndex) {
    if (active.particles.size() >= kMaximumParticlesPerEffect) {
        return;
    }

    LiveParticle particle;
    particle.emitterIndex = emitterIndex;
    const int animation = emitter.SelectAnimation(NextParticleRandom(active.randomState));
    if (animation < 0) { return; }
    particle.animationIndex = static_cast<std::uint8_t>(animation);
    particle.x = active.x;
    particle.y = active.y;
    particle.velocityX = 0.0f;
    particle.velocityY = 0.0f;
    particle.ageMs = 0.0f;
    particle.lifetimeMs = static_cast<float>(emitter.GetParticleLifetimeMs());
    if (particle.lifetimeMs <= 0.0f) {
        particle.lifetimeMs = kParticleFallbackLifetimeMs;
    }
    for (std::size_t channel = 0; channel < particle.randomValues.size();
         ++channel) {
        particle.randomValues[channel] = NextParticleRandom(active.randomState);
    }

    if (emitter.pattern == ParticleSpawnPattern::Line) {
        const float distance = NextParticleRandom(active.randomState);
        particle.x += emitter.patternValues[0] +
                      (emitter.patternValues[2] - emitter.patternValues[0]) *
                          distance;
        particle.y += emitter.patternValues[1] +
                      (emitter.patternValues[3] - emitter.patternValues[1]) *
                          distance;
    } else if (emitter.pattern == ParticleSpawnPattern::Rectangle) {
        particle.x += ParticleRandomRange(active.randomState,
                                          emitter.patternValues[0],
                                          emitter.patternValues[2]);
        particle.y += ParticleRandomRange(active.randomState,
                                          emitter.patternValues[1],
                                          emitter.patternValues[3]);
    } else {
        const float outerRadius = ParticleRandomRange(
            active.randomState, emitter.patternValues[2],
            emitter.patternValues[3]);
        const float innerWidth = ParticleRandomRange(
            active.randomState, emitter.patternValues[4],
            emitter.patternValues[5]);
        const float radius = ParticleRandomRange(active.randomState,
                                                 outerRadius - innerWidth,
                                                 outerRadius);
        const float angle = ParticleRandomRange(active.randomState, 0.0f,
                                                360.0f) /
                            kRadiansToDegrees;
        particle.x += emitter.patternValues[0] + std::sin(angle) * radius;
        particle.y += emitter.patternValues[1] - std::cos(angle) * radius;
    }

    if (emitter.velocity == ParticleSpawnVelocity::Linear) {
        particle.velocityX = ParticleRandomRange(
            active.randomState, emitter.velocityValues[0],
            emitter.velocityValues[1]);
        particle.velocityY = ParticleRandomRange(
            active.randomState, emitter.velocityValues[2],
            emitter.velocityValues[3]);
    } else {
        const float direction = ParticleRandomRange(
            active.randomState, emitter.velocityValues[0],
            emitter.velocityValues[1]) /
                                kRadiansToDegrees;
        const float speed = ParticleRandomRange(
            active.randomState, emitter.velocityValues[2],
            emitter.velocityValues[3]);
        particle.velocityX = std::sin(direction) * speed;
        particle.velocityY = std::cos(direction) * speed;
    }

    active.particles.push_back(particle);
}

/** Resolve one particle channel at an age using the original timed ranges. */
float ParticleChannelValue(const ParticleEmitterTemplate &emitter,
                           const LiveParticle &particle,
                           std::size_t channel, float defaultValue) {
    const std::vector<ParticleInterpolatorKey> &keys =
        emitter.interpolators[channel];
    float value = defaultValue;
    const float randomValue = particle.randomValues[channel];
    for (std::size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex) {
        const ParticleInterpolatorKey &key = keys[keyIndex];
        float startValue = value;
        if (!key.keepPreviousStart) {
            startValue = ParticleRangeAt(key.startMinimum, key.startMaximum,
                                         randomValue);
        }
        const float endValue = ParticleRangeAt(key.endMinimum, key.endMaximum,
                                               randomValue);
        const float startMs = static_cast<float>(key.startMs);
        const float endMs = startMs + static_cast<float>(key.durationMs);
        if (particle.ageMs < startMs) {
            return value;
        }
        if (key.durationMs == 0 || particle.ageMs >= endMs) {
            value = endValue;
            continue;
        }

        const float progress = (particle.ageMs - startMs) /
                               static_cast<float>(key.durationMs);
        return startValue + (endValue - startValue) * progress;
    }
    return value;
}

/** Advance emitters and their live particles. */
void AdvanceParticleEffects(LoadedMap &loaded, std::uint16_t deltaMs) {
    const float elapsedSeconds = static_cast<float>(deltaMs) /
                                 kSecondsToMilliseconds;
    std::size_t activeIndex = 0;
    while (activeIndex < loaded.activeParticleEffects.size()) {
        ActiveParticleEffect &active =
            loaded.activeParticleEffects[activeIndex];
        const std::map<std::uint64_t, ParticleEffectVisual>::const_iterator found =
            loaded.particleEffects.find(active.visualKey);
        if (found == loaded.particleEffects.end()) {
            loaded.activeParticleEffects.erase(
                loaded.activeParticleEffects.begin() + activeIndex);
            continue;
        }

        const std::vector<ParticleEmitterTemplate> &emitters =
            found->second.effect.GetEmitters();
        active.ageMs += static_cast<float>(deltaMs);
        bool futureSpawnExists = false;
        for (std::size_t emitterIndex = 0; emitterIndex < emitters.size();
             ++emitterIndex) {
            const ParticleEmitterTemplate &emitter = emitters[emitterIndex];
            float &nextSpawnMs = active.nextSpawnMs[emitterIndex];
            if (nextSpawnMs < 0.0f) {
                continue;
            }

            float startMs = emitter.startSeconds * kSecondsToMilliseconds;
            if (startMs < 0.0f) {
                startMs = 0.0f;
            }
            const float endMs = emitter.endSeconds * kSecondsToMilliseconds;
            const bool oneShot = endMs <= startMs;
            std::size_t spawned = 0;
            while (nextSpawnMs <= active.ageMs &&
                   spawned < kMaximumSpawnsPerEmitterPerFrame) {
                if (!oneShot && nextSpawnMs > endMs) {
                    nextSpawnMs = -1.0f;
                    break;
                }

                SpawnParticle(active, emitter,
                              static_cast<std::uint32_t>(emitterIndex));
                spawned++;
                if (oneShot) {
                    nextSpawnMs = -1.0f;
                    break;
                }

                float intervalMs = ParticleRandomRange(
                    active.randomState, emitter.intervalMinimumSeconds,
                    emitter.intervalMaximumSeconds) *
                                   kSecondsToMilliseconds;
                if (intervalMs < 1.0f) {
                    // Original UpdateEmitters emits once per update at zero interval.
                    nextSpawnMs = active.ageMs + 1.0f;
                    break;
                }
                nextSpawnMs += intervalMs;
            }
            if (nextSpawnMs >= 0.0f &&
                (oneShot || nextSpawnMs <= endMs)) {
                futureSpawnExists = true;
            }
        }

        std::size_t particleIndex = 0;
        while (particleIndex < active.particles.size()) {
            LiveParticle &particle = active.particles[particleIndex];
            if (particle.emitterIndex >= emitters.size()) {
                active.particles.erase(active.particles.begin() + particleIndex);
                continue;
            }

            const ParticleEmitterTemplate &emitter =
                emitters[particle.emitterIndex];
            particle.ageMs += static_cast<float>(deltaMs);
            if (particle.ageMs >= particle.lifetimeMs) {
                active.particles.erase(active.particles.begin() + particleIndex);
                continue;
            }

            const float speedScale = ParticleChannelValue(
                emitter, particle, 5, 1.0f);
            particle.velocityX += emitter.accelerationX * elapsedSeconds;
            particle.velocityY += emitter.accelerationY * elapsedSeconds;
            particle.x += particle.velocityX * speedScale * elapsedSeconds;
            particle.y += particle.velocityY * speedScale * elapsedSeconds;
            particleIndex++;
        }

        if (!futureSpawnExists && active.particles.empty()) {
            loaded.activeParticleEffects.erase(
                loaded.activeParticleEffects.begin() + activeIndex);
            continue;
        }
        activeIndex++;
    }
}

/** Select the looping sprite step belonging to a particle's current age. */
std::size_t ParticleAnimationStep(const PropSlot &animation, float ageMs) {
    if (animation.stepDurationsMs.empty()) {
        return 0;
    }

    std::uint32_t totalDuration = 0;
    for (std::size_t i = 0; i < animation.stepDurationsMs.size(); ++i) {
        totalDuration += animation.stepDurationsMs[i];
    }
    if (totalDuration == 0) {
        return 0;
    }

    const std::uint32_t wrappedAge =
        static_cast<std::uint32_t>(ageMs) % totalDuration;
    std::uint32_t stepEnd = 0;
    for (std::size_t step = 0; step < animation.stepDurationsMs.size(); ++step) {
        stepEnd += animation.stepDurationsMs[step];
        if (wrappedAge < stepEnd) {
            return step;
        }
    }
    return animation.stepDurationsMs.size() - 1;
}

/** Emit live particle sprites in the requested z-order interval. */
void AddParticleQuads(const LoadedMap &loaded, CQuadBatch &batch,
                      int minimumZGroup, int maximumZGroup) {
    for (std::size_t activeIndex = 0;
         activeIndex < loaded.activeParticleEffects.size(); ++activeIndex) {
        const ActiveParticleEffect &active =
            loaded.activeParticleEffects[activeIndex];
        if (active.zOrderGroup < minimumZGroup ||
            active.zOrderGroup > maximumZGroup) {
            continue;
        }

        const std::map<std::uint64_t, ParticleEffectVisual>::const_iterator found =
            loaded.particleEffects.find(active.visualKey);
        if (found == loaded.particleEffects.end()) {
            continue;
        }
        const ParticleEffectVisual &visual = found->second;
        const std::vector<ParticleEmitterTemplate> &emitters =
            visual.effect.GetEmitters();

        for (std::size_t particleIndex = 0;
             particleIndex < active.particles.size(); ++particleIndex) {
            const LiveParticle &particle = active.particles[particleIndex];
            if (particle.emitterIndex >= emitters.size() ||
                particle.emitterIndex >= visual.emitters.size()) {
                continue;
            }

            const ParticleEmitterTemplate &emitter =
                emitters[particle.emitterIndex];
            const ParticleEmitterVisual &emitterVisual =
                visual.emitters[particle.emitterIndex];
            const PropSlot *animation =
                &emitterVisual.animations[particle.animationIndex];
            if (animation->quadsByStep.empty()) {
                continue;
            }

            const std::size_t step = ParticleAnimationStep(*animation,
                                                           particle.ageMs);
            if (step >= animation->quadsByStep.size()) {
                continue;
            }

            const float scaleX = ParticleChannelValue(
                emitter, particle, 0, 1.0f);
            const float scaleY = ParticleChannelValue(
                emitter, particle, 1, 1.0f);
            const float uniformScale = ParticleChannelValue(
                emitter, particle, 2, 1.0f);
            float alpha = ParticleChannelValue(emitter, particle, 3, 1.0f);
            if (alpha < 0.0f) {
                alpha = 0.0f;
            }
            if (alpha > 1.0f) {
                alpha = 1.0f;
            }
            float rotation = ParticleChannelValue(emitter, particle, 4, 0.0f);
            if (emitter.alignToVelocity) {
                rotation += std::atan2(particle.velocityY,
                                       particle.velocityX) *
                            kRadiansToDegrees;
            }

            const std::vector<SpriteQuad> &quads = animation->quadsByStep[step];
            for (std::size_t quadIndex = 0; quadIndex < quads.size();
                 ++quadIndex) {
                const SpriteQuad &quad = quads[quadIndex];
                batch.AddTransformedQuad(
                    *quad.page,
                    particle.x + static_cast<float>(quad.offsetX),
                    particle.y + static_cast<float>(quad.offsetY),
                    static_cast<float>(quad.Width()),
                    static_cast<float>(quad.Height()), quad.source,
                    quad.flipHorizontal, quad.flipVertical, quad.blend,
                    particle.x, particle.y, scaleX * uniformScale,
                    scaleY * uniformScale, rotation, alpha, quad.rotateTexture);
            }
        }
    }
}

/** The original disables both collision shapes in the destroyed state. */
bool PropHasCollision(const PlacedProp &prop) {
    if (prop.runtime != nullptr) { return !prop.runtime->IsRemoved(); }
    if (prop.sprite->interactiveKind == InteractivePropKind::None) {
        return true;
    }
    if (prop.interactiveState >= prop.sprite->stateCount) {
        return true;
    }
    return prop.sprite->states[prop.interactiveState].collisionEnabled;
}

/** Order props the way CRenderQueue does: by group, then down the screen. */
bool PropDrawsBefore(const PlacedProp &left, const PlacedProp &right) {
    if (left.sprite->zOrderGroup != right.sprite->zOrderGroup) {
        return left.sprite->zOrderGroup < right.sprite->zOrderGroup;
    }
    return left.y < right.y;
}

/**
 * Turn the map's object layers into drawable props.
 *
 * Objects of other types are counted and left alone. Nothing here fails the
 * load: a prop whose template or sprite will not resolve is dropped and
 * reported, because one bad rock should not cost the whole level.
 */
void LoadProps(CResTOCManager &tocManager, LoadedMap &loaded, int selectedObjectLayer = -1) {
    loaded.props.clear();
    loaded.propSprites.clear();

    std::uint32_t placed = 0;
    std::uint32_t skipped = 0;
    std::uint32_t otherTypes = 0;

    for (std::uint32_t layerIndex = 0; layerIndex < loaded.map.GetObjectLayerCount();
         ++layerIndex) {
        if (selectedObjectLayer >= 0 && static_cast<int>(loaded.map.GetObjectLayer(layerIndex).GetLayerIndex()) != selectedObjectLayer) {
            continue;
        }
        const std::vector<PlacedObject> &objects =
            loaded.map.GetObjectLayer(layerIndex).GetObjects();

        for (std::size_t i = 0; i < objects.size(); ++i) {
            const PlacedObject &object = objects[i];
            if (object.objectType != static_cast<std::uint8_t>(PlacedObjectType::Prop)) {
                otherTypes++;
                continue;
            }

            // One template serves many instances, so its quads are built once.
            const std::uint64_t key =
                (static_cast<std::uint64_t>(object.packHash) << 8) | object.localIndex;

            std::map<std::uint64_t, PropSprite>::iterator found =
                loaded.propSprites.find(key);
            if (found == loaded.propSprites.end()) {
                PropSprite sprite;
                sprite.zOrderGroup = kZGroupNormal;
                sprite.skippedParts = 0;
                sprite.unsupportedTransforms = 0;
                if (!BuildPropSprite(tocManager, loaded, object.packHash,
                                     object.localIndex, sprite)) {
                    std::printf("[m3]   prop %08X/%u will not resolve\n",
                                object.packHash, object.localIndex);
                    skipped++;
                    continue;
                }
                found = loaded.propSprites.insert(std::make_pair(key, sprite)).first;
            }

            PlacedProp prop;
            prop.x = static_cast<float>(object.x);
            prop.y = static_cast<float>(object.y);
            prop.sprite = &found->second;
            prop.objectId = static_cast<int>(i);
            loaded.props.push_back(prop);
            placed++;
        }
    }

    std::stable_sort(loaded.props.begin(), loaded.props.end(), PropDrawsBefore);

    // After the sort, so a prop's phase follows from where it ends up in the
    // draw order rather than from which layer happened to place it.
    std::uint32_t animated = 0;
    for (std::size_t i = 0; i < loaded.props.size(); ++i) {
        StartPropPlayers(loaded.props[i], i);
        if (PropAnimates(*loaded.props[i].sprite)) {
            animated++;
        }
    }

    std::uint32_t skippedParts = 0;
    std::uint32_t unsupportedTransforms = 0;
    std::map<std::uint64_t, PropSprite>::const_iterator sprite;
    for (sprite = loaded.propSprites.begin(); sprite != loaded.propSprites.end();
         ++sprite) {
        skippedParts += sprite->second.skippedParts;
        unsupportedTransforms += sprite->second.unsupportedTransforms;
    }

    std::printf("[m3] %u props from %zu templates, %u of them animated "
                "(%u unresolved, %u non-prop objects ignored)\n",
                placed, loaded.propSprites.size(), animated, skipped, otherTypes);
    if (skippedParts > 0 || unsupportedTransforms > 0) {
        std::printf("[m3]   %u sprite parts undrawable, "
                    "%u drawn without a rotating transform\n",
                    skippedParts, unsupportedTransforms);
    }
}

/** Move every prop's three players on by one frame's worth of time. */
void AdvanceProps(std::vector<PlacedProp> &props, std::uint16_t deltaMs) {
    for (std::size_t i = 0; i < props.size(); ++i) {
        if (props[i].runtime != nullptr) { continue; }
        props[i].background.Update(deltaMs);
        props[i].main.Update(deltaMs);
        props[i].foreground.Update(deltaMs);
        props[i].hitFlashRemainingMs -= static_cast<float>(deltaMs);
        if (props[i].hitFlashRemainingMs < 0.0f) {
            props[i].hitFlashRemainingMs = 0.0f;
        }
    }
}

/** Move every drifting tile layer on by one frame's worth of time. */
void AdvanceTileLayers(CMap &map, std::uint16_t deltaMs) {
    for (std::uint32_t i = 0; i < map.GetTileLayerCount(); ++i) {
        CLayerTile &layer = map.GetTileLayer(i);
        if (!layer.IsScrolling()) {
            continue;
        }
        layer.Update(deltaMs);
    }
}

/**
 * The quads a slot draws at its player's current step.
 *
 * An empty slot -- an unused animation, or one that expanded to nothing --
 * lands on the out-of-range path and draws nothing, which is why the players
 * of empty slots never need a special case anywhere else.
 */
const std::vector<SpriteQuad> &CurrentQuads(const PropSlot &slot,
                                            const CSpritePlayer &player) {
    static const std::vector<SpriteQuad> kNothing;

    const std::uint32_t step = player.GetStep();
    if (step >= slot.quadsByStep.size()) {
        return kNothing;
    }
    return slot.quadsByStep[step];
}

/** Emit one prop's slot, positioned at the prop and offset by each quad. */
void AddSpriteQuads(const PlacedProp &prop, const std::vector<SpriteQuad> &quads,
                    CQuadBatch &batch) {
    for (std::size_t i = 0; i < quads.size(); ++i) {
        const SpriteQuad &quad = quads[i];

        batch.AddQuad(*quad.page, prop.x + static_cast<float>(quad.offsetX),
                      prop.y + static_cast<float>(quad.offsetY),
                      static_cast<float>(quad.source.width),
                      static_cast<float>(quad.source.height), quad.source,
                      quad.flipHorizontal, quad.flipVertical, quad.blend);

        if (prop.hitFlashRemainingMs > 0.0f) {
            const float flashAlpha = prop.hitFlashRemainingMs / 500.0f;
            batch.AddTransformedQuad(
                *quad.page, prop.x + static_cast<float>(quad.offsetX),
                prop.y + static_cast<float>(quad.offsetY),
                static_cast<float>(quad.source.width),
                static_cast<float>(quad.source.height), quad.source,
                quad.flipHorizontal, quad.flipVertical, BlendMode::Additive,
                prop.x, prop.y, 1.0f, 1.0f, 0.0f, flashAlpha);
        }
    }
}

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
                  PlacedObjectType wanted) {
    markers.Begin();

    const CMap &map = loaded.map;
    for (std::uint32_t layer = 0; layer < map.GetObjectLayerCount(); ++layer) {
        const std::vector<PlacedObject> &objects =
            map.GetObjectLayer(layer).GetObjects();
        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (objects[i].objectType != static_cast<std::uint8_t>(wanted)) {
                continue;
            }
            markers.AddOutline(static_cast<float>(objects[i].x) - 0.5f * kMarkerSize,
                               static_cast<float>(objects[i].y) - 0.5f * kMarkerSize,
                               kMarkerSize, kMarkerSize, kMarkerThickness);
        }
    }
}

/**
 * Assemble exactly the collision shapes CLayerCollision tests for a player.
 *
 * The level script selects one map collision layer. Static props then add
 * their template-local geometry at their placed position. Keeping this as one
 * scene lets the existing resolver choose the nearest edge across both kinds
 * instead of resolving each prop in an arbitrary order.
 */
void BuildCollisionScene(LoadedMap &loaded) {
    loaded.collisionScene.Clear();
    loaded.weaponCollision.walls.Clear();
    loaded.weaponCollision.terrain.Clear();
    const CLayerCollision *bulletLayer = loaded.map.GetCurrentBulletCollisionLayer();
    if (bulletLayer != nullptr) {
        loaded.weaponCollision.walls.AppendTranslated(bulletLayer->GetCollision(), 0, 0);
    }

    const CLayerCollision *mapLayer = loaded.map.GetCurrentCollisionLayer();
    if (mapLayer != nullptr) {
        loaded.collisionScene.AppendTranslated(mapLayer->GetCollision(), 0.0f,
                                               0.0f);
        loaded.weaponCollision.terrain.AppendTranslated(mapLayer->GetCollision(), 0, 0);
    }

    std::uint32_t propShapes = 0;
    for (std::size_t i = 0; i < loaded.props.size(); ++i) {
        const PlacedProp &prop = loaded.props[i];
        if (!PropHasCollision(prop)) {
            continue;
        }
        const CCollisionData *body = &prop.sprite->collision;
        const CCollisionData *bullets = &prop.sprite->bulletCollision;
        if (prop.runtime != nullptr) { body = &prop.runtime->GetCollision(); bullets = &prop.runtime->GetCollision(true); }
        loaded.weaponCollision.walls.AppendTranslated(*bullets, prop.x, prop.y);
        loaded.weaponCollision.terrain.AppendTranslated(*bullets, prop.x, prop.y);
        if (body->GetEdges().empty()) {
            continue;
        }
        if (!loaded.collisionScene.AppendTranslated(*body,
                                                    prop.x, prop.y)) {
            break;
        }
        propShapes++;
    }

    int mapLayerIndex = -1;
    if (mapLayer != nullptr) {
        mapLayerIndex = static_cast<int>(mapLayer->GetLayerIndex());
    }
    std::printf("[m4] effective collision: map layer %d, %u prop shapes, "
                "%zu vertices, %zu edges\n",
                mapLayerIndex, propShapes,
                loaded.collisionScene.GetVertices().size(),
                loaded.collisionScene.GetEdges().size());
}

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
        unsigned scripted = 0;
        for (PlacedProp &prop : m_map.props) {
            if (prop.runtime == nullptr) { continue; }
            ++scripted;
            prop.runtime->SetLevelContext(&m_level);
            prop.runtime->Bind(prop.sprite->data, &prop.sprite->durations);
            SyncPlayers(prop);
        }
        BuildCollisionScene(m_map);
        std::printf("[prop] runtime scripts=%u\n", scripted);
    }

    void SendMessage(int objectId, int message) override {
        for (PlacedProp &prop : m_map.props) {
            if (prop.objectId != objectId || prop.runtime == nullptr) { continue; }
            const unsigned previous = prop.runtime->GetStateId();
            prop.runtime->HandleMessage(message);
            SyncPlayers(prop);
            std::printf("[prop] id=%d message=%d state=%u->%u\n", objectId, message, previous, prop.runtime->GetStateId());
            return;
        }
    }

    void Update(int deltaMs) override {
        const CLayerCollision *bodyLayer = m_map.map.GetCurrentCollisionLayer();
        const CLayerCollision *bulletLayer = m_map.map.GetCurrentBulletCollisionLayer();
        bool changed = bodyLayer != m_bodyLayer || bulletLayer != m_bulletLayer;
        m_bodyLayer = bodyLayer;
        m_bulletLayer = bulletLayer;
        for (PlacedProp &prop : m_map.props) {
            if (prop.runtime == nullptr) { continue; }
            prop.runtime->Update(deltaMs, PlayerInside(prop));
            SyncPlayers(prop);
            for (const PropAction &action : prop.runtime->TakeActions()) { ApplyAction(prop, action); }
            if (prop.runtime->CollisionChanged()) { changed = true; prop.runtime->ClearCollisionChanged(); }
        }
        if (changed) { BuildCollisionScene(m_map); }
    }

    CombatTrace Trace(const CombatHit &hit, float x, float y, float dx, float dy,
        float radius, const std::vector<CombatId> &skip) override {
        CombatTrace nearest;
        for (const PlacedProp &prop : m_map.props) {
            if (prop.runtime == nullptr || prop.runtime->IsRemoved() || prop.runtime->GetHealth() <= 0 || hit.ownerType != 0) { continue; }
            const CombatId id = kPropIdBase + prop.objectId;
            if (std::find(skip.begin(), skip.end(), id) != skip.end()) { continue; }
            const auto &shape = prop.runtime->GetCollision(true);
            const auto &vertices = shape.GetVertices();
            for (const CollisionEdge &edge : shape.GetEdges()) {
                if (!edge.enabled) { continue; }
                const CollisionPoint &first = vertices[edge.firstVertex], &second = vertices[edge.secondVertex];
                const float fraction = CombatGeometry::EdgeFraction(x - prop.x, y - prop.y, dx, dy, first, second, radius);
                if (fraction < nearest.fraction) {
                    nearest = {id, fraction, -1, edge.group, first.y - second.y, second.x - first.x};
                }
            }
        }
        return nearest;
    }

    HitResult ApplyHit(CombatId target, const CombatHit &hit) override {
        for (PlacedProp &prop : m_map.props) {
            if (kPropIdBase + prop.objectId != target || prop.runtime == nullptr || prop.runtime->IsRemoved()) { continue; }
            prop.lastDamager = hit.owner;
            prop.runtime->Damage(hit.damage, hit.flags);
            SyncPlayers(prop);
            ++m_hitCount;
            return HitResult::Hit;
        }
        return HitResult::Ignored;
    }

    void Splash(const CombatHit &hit, float radius) override {
        // CProp::CanCollide accepts human/AI gun ownership, not enemy shots.
        if (hit.ownerType != 0) { return; }
        for (PlacedProp &prop : m_map.props) {
            if (prop.runtime == nullptr || prop.runtime->IsRemoved() || prop.runtime->GetHealth() <= 0) { continue; }
            const auto &vertices = prop.runtime->GetEntryCollision().GetVertices();
            if (vertices.empty()) { continue; }
            float left = vertices[0].x, right = left, top = vertices[0].y, bottom = top;
            for (const CollisionPoint &point : vertices) {
                left = std::min(left, point.x); right = std::max(right, point.x);
                top = std::min(top, point.y); bottom = std::max(bottom, point.y);
            }
            const float x = prop.x + (left + right) * 0.5f;
            const float y = prop.y + (top + bottom) * 0.5f;
            const float extent = std::max(right - left, bottom - top) * 0.5f;
            if (std::hypot(hit.x - x, hit.y - y) > radius + extent) { continue; }
            m_scene.ApplyHit(kPropIdBase + prop.objectId, hit);
        }
    }

    unsigned GetFailures() const {
        unsigned failures = 0;
        for (const PlacedProp &prop : m_map.props) {
            if (prop.runtime != nullptr) { failures += prop.runtime->GetUnsupportedCount(); }
        }
        return failures;
    }

    unsigned GetHitCount() const { return m_hitCount; }

    unsigned CheckDamageContracts() {
        unsigned tested = 0, failures = 0;
        for (const PlacedProp &prop : m_map.props) {
            if (prop.runtime == nullptr || prop.runtime->GetHealth() <= 0) { continue; }
            // Independent instance: checking a barrel must not damage the
            // account or change its real level-script progress.
            CProp probe;
            probe.Bind(prop.sprite->data, &prop.sprite->durations);
            const unsigned initialState = probe.GetStateId();
            const float initialHealth = probe.GetHealth();
            probe.Damage(10000, 0xffffffffu);
            for (int elapsed = 0; elapsed < 2500; elapsed += 16) { probe.Update(16, false); }
            if (probe.GetStateId() == initialState && probe.GetHealth() == initialHealth) { ++failures; }
            failures += probe.GetUnsupportedCount();
            ++tested;
        }
        std::printf("[prop-check] independent damage/timer/animation templates=%u failures=%u\n", tested, failures);
        return failures;
    }

private:
    static constexpr CombatId kPropIdBase = 1ull << 62;
    void SyncPlayers(PlacedProp &prop) {
        prop.foreground = prop.runtime->GetPlayer(0);
        prop.main = prop.runtime->GetPlayer(1);
        prop.background = prop.runtime->GetPlayer(2);
    }

    bool PlayerInside(const PlacedProp &prop) const {
        const auto &vertices = prop.runtime->GetEntryCollision().GetVertices();
        if (vertices.empty()) { return false; }
        bool inside = false;
        std::size_t previous = vertices.size() - 1;
        const float x = m_scene.playerX - prop.x;
        const float y = m_scene.playerY - prop.y;
        for (std::size_t index = 0; index < vertices.size(); ++index) {
            const auto &first = vertices[index];
            const auto &second = vertices[previous];
            if ((first.y > y) != (second.y > y)) {
                const float crossing = first.x + (second.x - first.x) * (y - first.y) / (second.y - first.y);
                if (x < crossing) { inside = !inside; }
            }
            previous = index;
        }
        return inside;
    }

    void ApplyAction(PlacedProp &prop, const PropAction &action) {
        if (action.kind == PropAction::Kind::Entered || action.kind == PropAction::Kind::Destroyed) {
            m_level.OnPropEvent(prop.objectId, prop.sprite->resource, action.kind == PropAction::Kind::Entered);
            return;
        }
        if (action.kind == PropAction::Kind::Splash) {
            CombatHit hit;
            hit.x = prop.x;
            hit.y = prop.y;
            hit.damage = static_cast<float>(action.damage);
            hit.owner = kPlayerCombatId;
            if (action.damageOwner == 1 && prop.lastDamager != 0) { hit.owner = prop.lastDamager; }
            // Self-owned environmental explosions can hurt both sides; the
            // original knockback native explicitly visits only the brothers.
            if (!action.playersOnly) { m_scene.Splash(hit, static_cast<float>(action.radius), 360, 0, 0); }
            if (action.playersOnly || action.damageOwner == 0) {
                hit.owner = 0;
                hit.ownerType = 1;
                m_scene.Splash(hit, static_cast<float>(action.radius), 360, static_cast<float>(action.force), action.forceMs);
            }
            return;
        }
        GunCue cue;
        cue.kind = GunCue::Kind::Effect;
        cue.resource = action.resource;
        float x = prop.x, y = prop.y;
        CombatId owner = 0;
        if (action.kind == PropAction::Kind::Sound) { cue.kind = GunCue::Kind::Sound; }
        if (action.kind == PropAction::Kind::Portal || action.kind == PropAction::Kind::AttachedEffect) {
            x = m_scene.playerX; y = m_scene.playerY;
        }
        if (action.kind == PropAction::Kind::AttachedEffect || action.kind == PropAction::Kind::StopEffect) {
            owner = kPlayerCombatId;
            cue.kind = GunCue::Kind::Trail;
            if (action.kind == PropAction::Kind::StopEffect) { cue.kind = GunCue::Kind::StopTrail; }
        }
        m_effects.Emit(cue, x, y, 0, 0, owner, prop.objectId + 1000);
        if (action.kind == PropAction::Kind::Portal) { m_level.HandleEvent(3); }
    }

    LoadedMap &m_map;
    const CLayerCollision *m_bodyLayer = nullptr;
    const CLayerCollision *m_bulletLayer = nullptr;
    CombatScene &m_scene;
    CLevel &m_level;
    WeaponEffects &m_effects;
    unsigned m_hitCount = 0;
};

/** Collect the exact collision scene used by player movement. */
void BuildCollisionMarkers(const LoadedMap &loaded, CMarkerBatch &markers) {
    markers.Begin();

    const std::vector<CollisionPoint> &vertices =
        loaded.collisionScene.GetVertices();
    const std::vector<CollisionEdge> &edges = loaded.collisionScene.GetEdges();
    for (std::size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        const CollisionEdge &edge = edges[edgeIndex];
        if (!edge.enabled) {
            continue;
        }
        const CollisionPoint &first = vertices[edge.firstVertex];
        const CollisionPoint &second = vertices[edge.secondVertex];
        markers.AddSegment(first.x, first.y, second.x, second.y,
                           kMarkerThickness);
    }
}

/**
 * The camera scale a level snaps to. Reference: :120747, where CLevel does
 * `CCamera::SnapScale(camera, 0.8)` right after binding the map.
 *
 * The real camera moves it afterwards; until there is a real camera this is
 * the number the game starts every level with.
 */
constexpr float kLevelCameraScale = 0.8f;

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
                       LoadedMap &loaded) {
    PackTables tables(tocManager);
    unsigned failed = 0;

    for (std::uint32_t layer = 0; layer < loaded.map.GetObjectLayerCount();
         ++layer) {
        const std::vector<PlacedObject> &objects =
            loaded.map.GetObjectLayer(layer).GetObjects();
        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (objects[i].objectType !=
                static_cast<std::uint8_t>(PlacedObjectType::Enemy)) {
                continue;
            }

            char label[128];
            std::snprintf(label, sizeof(label), "%s enemy %u",
                          tables.GetPackName(objects[i].packHash).c_str(),
                          objects[i].localIndex);

            PlacedEnemy placed;
            placed.templateData.reset(new EnemyTemplateData());
            if (!ReadEnemyTemplate(tables, objects[i].packHash,
                                   objects[i].localIndex, label,
                                   *placed.templateData)) {
                failed++;
                continue;
            }

            placed.x = static_cast<float>(objects[i].x);
            placed.y = static_cast<float>(objects[i].y);
            placed.gameScale = placed.templateData->gameScale;
            placed.model.reset(new EnemyModel());
            // A map is a level, so the level spawn export is the one to run.
            if (!LoadEnemyModel(tables, *placed.templateData, true, &program,
                                EnemySpawnMode::Level,
                                *placed.model)) {
                failed++;
                continue;
            }

            // One line each: a map places a couple of dozen at most, and which
            // template a leftover spawn names is exactly what is worth seeing.
            std::printf("[m3] %s at %d %d -- %zu configs, %u parts\n",
                        label, objects[i].x, objects[i].y,
                        placed.model->configs.size(),
                        placed.model->enemy.GetPartCount());

            loaded.enemies.push_back(std::move(placed));
        }
    }

    if (!loaded.enemies.empty() || failed > 0) {
        std::printf("[m3] %zu placed enemies drawn, %u unloadable\n",
                    loaded.enemies.size(), failed);
    }
}

/** Move every placed enemy's animation on. */
void AdvanceEnemies(LoadedMap &loaded, std::int32_t deltaMs) {
    for (std::size_t i = 0; i < loaded.enemies.size(); ++i) {
        loaded.enemies[i].model->enemy.Update(deltaMs);
    }
}

/**
 * Stand a player on every spawn point the object layer names.
 *
 * The default model and nothing else: no weapon, no armour. Which gun a
 * player carries is a loadout question and the loadout is not read yet, so
 * putting one in his hand here would be inventing data.
 */
void LoadPlacedPlayers(CResTOCManager &tocManager, const CShaderProgram &program,
                       LoadedMap &loaded) {
    PackTables tables(tocManager);

    for (std::uint32_t layer = 0; layer < loaded.map.GetObjectLayerCount();
         ++layer) {
        const std::vector<PlacedObject> &objects =
            loaded.map.GetObjectLayer(layer).GetObjects();
        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (objects[i].objectType !=
                static_cast<std::uint8_t>(PlacedObjectType::Player)) {
                continue;
            }

            // Looked up on the first spawn point rather than up front, so a
            // map with none never pays for the walk over every pack.
            if (loaded.playerTemplate == nullptr) {
                loaded.playerTemplate.reset(new PlayerTemplateData());
                if (!FindPlayerTemplate(tocManager, tables,
                                        *loaded.playerTemplate)) {
                    std::printf("[m3] no player template in the archives\n");
                    loaded.playerTemplate.reset();
                    return;
                }
            }

            PlacedPlayer placed;
            placed.x = static_cast<float>(objects[i].x);
            placed.y = static_cast<float>(objects[i].y);
            placed.model.reset(new PlayerModel());
            if (!BuildPlayerBody(tables, loaded.playerTemplate->moveSet,
                                 *placed.model) ||
                !CreatePlayerBuffers(*placed.model, program)) {
                std::printf("[m3] player at %d %d could not be built\n",
                            objects[i].x, objects[i].y);
                continue;
            }

            SelectPlayerMoveSlot(*placed.model, 0, false);
            PosePlayer(*placed.model);
            std::printf("[m3] %s at %d %d -- scale %.0f\n",
                        loaded.playerTemplate->owner.c_str(), objects[i].x,
                        objects[i].y, loaded.playerTemplate->gameScale);
            loaded.players.push_back(std::move(placed));
        }
    }
}

/** Move every placed player's animation on. */
void AdvancePlayers(LoadedMap &loaded, std::int32_t deltaMs) {
    for (std::size_t i = 0; i < loaded.players.size(); ++i) {
        AdvancePlayer(*loaded.players[i].model, deltaMs);
    }
}

/**
 * Drive the first player with WASD and resolve the requested movement.
 *
 * Maps contain one real player spawn. A few abandoned campaign maps contain
 * none; those remain valid viewers and simply ignore movement input.
 */
/** Swap equipment only after every referenced asset has loaded. */
bool EquipControlledPlayer(PackTables &tables, LoadedMap &loaded,
    const CShaderProgram &program, const WeaponEntry &weapon) {
    if (loaded.players.empty()) { return true; }
    // CombatScene retains this model's address. EquipPlayerWeapon stages the
    // weapon atomically and preserves the body, vitals pointer and armour.
    PlayerModel &player = *loaded.players[0].model;
    if (!EquipPlayerWeapon(tables, loaded.playerTemplate->script, weapon.data, weapon.owner, player) ||
        !CreatePlayerBuffers(player, program)) { return false; }
    PosePlayer(player);
    return true;
}

bool UpdateControlledPlayer(LoadedMap &loaded, const CWindow &window,
                            std::uint64_t elapsedMs) {
    if (loaded.players.empty()) {
        return false;
    }

    float directionX = 0.0f;
    float directionY = 0.0f;
    if (window.IsKeyDown(KeyCode::A)) {
        directionX -= 1.0f;
    }
    if (window.IsKeyDown(KeyCode::D)) {
        directionX += 1.0f;
    }
    if (window.IsKeyDown(KeyCode::W)) {
        directionY -= 1.0f;
    }
    if (window.IsKeyDown(KeyCode::S)) {
        directionY += 1.0f;
    }

    const bool moving = directionX != 0.0f || directionY != 0.0f;
    PlacedPlayer &player = loaded.players[0];
    if (moving != player.moving) {
        // The player data interleaves torso and leg moves. Slot zero is the
        // spawn/idle pair and slot one is the first locomotion pair.
        const std::size_t moveSlot = moving ? 1 : 0;
        SelectPlayerMoveSlot(*player.model, moveSlot, false);
        player.moving = moving;
    }

    if (!moving || elapsedMs == 0) {
        return moving;
    }

    const float directionLength = std::sqrt(directionX * directionX +
                                            directionY * directionY);
    directionX /= directionLength;
    directionY /= directionLength;
    player.facingDegrees = std::atan2(directionY, directionX) * kRadiansToDegrees + 90.0f;

    const float elapsedSeconds = static_cast<float>(elapsedMs) * 0.001f;
    CollisionPoint movement(directionX * kPlayerMovementUnitsPerSecond *
                                elapsedSeconds,
                            directionY * kPlayerMovementUnitsPerSecond *
                                elapsedSeconds);
    CollisionPoint resolved = loaded.collisionScene.ResolveCircleMovement(
        CollisionPoint(player.x, player.y), movement, kPlayerCollisionRadius);

    // CPlayer::Move clamps the body to the active camera bounds before it
    // resolves collision. Keep the whole circle inside the same rectangle.
    const MapRectangle bounds = loaded.map.GetVisibleBounds();
    if (!bounds.IsEmpty()) {
        const float minimumX = static_cast<float>(bounds.x) +
                               kPlayerCollisionRadius;
        const float maximumX = static_cast<float>(bounds.x + bounds.width) -
                               kPlayerCollisionRadius;
        const float minimumY = static_cast<float>(bounds.y) +
                               kPlayerCollisionRadius;
        const float maximumY = static_cast<float>(bounds.y + bounds.height) -
                               kPlayerCollisionRadius;
        if (resolved.x < minimumX) {
            resolved.x = minimumX;
        }
        if (resolved.x > maximumX) {
            resolved.x = maximumX;
        }
        if (resolved.y < minimumY) {
            resolved.y = minimumY;
        }
        if (resolved.y > maximumY) {
            resolved.y = maximumY;
        }
    }

    player.x = resolved.x;
    player.y = resolved.y;
    return true;
}

/**
 * Run the animation clock forward, in the bites playback would use.
 *
 * What makes a still screenshot able to prove anything about animation: shoot
 * the same map at two different times and diff them. Deterministic, because
 * the bite size is fixed rather than taken from the wall clock.
 */
void WarmUp(LoadedMap &loaded, std::uint32_t totalMs,
            WeaponEffects *effects = nullptr, bool firing = false) {
    if (effects && !loaded.players.empty()) { SetPlayerInput(*loaded.players[0].model, false, firing); }
    for (std::uint32_t elapsed = 0; elapsed < totalMs; elapsed += kWarmUpFrameMs) {
        AdvanceProps(loaded.props, kWarmUpFrameMs);
        AdvanceTileLayers(loaded.map, kWarmUpFrameMs);
        AdvanceEnemies(loaded, kWarmUpFrameMs);
        AdvancePlayers(loaded, kWarmUpFrameMs);
        if (effects && !loaded.players.empty()) {
            PlacedPlayer &player = loaded.players[0];
            float identity[kMatrix4dElements], modelToWorld[kMatrix4dElements];
            Matrix4dIdentity(identity);
            const float scale = PlayerModelWorldScale(*player.model, loaded.playerTemplate->gameScale, kLevelCameraScale);
            BuildPlayerGameMatrix(identity, player.x, player.y, scale, player.facingDegrees, modelToWorld);
            effects->Update(*player.model, modelToWorld, player.facingDegrees, kWarmUpFrameMs, &loaded.weaponCollision);
        }
    }
}

/**
 * Draw every model the object layer places -- enemies and players alike.
 *
 * Depth on, and the depth buffer cleared first: the terrain and props are 2D
 * and drawn in order, so they neither read nor write depth, but a model is
 * solid and needs it against itself.
 */
void DrawModels(LoadedMap &loaded, const CShaderProgram &program,
                const float *mapMvp) {
    if (loaded.enemies.empty() && loaded.players.empty()) {
        return;
    }

    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // The quad batch sets a blend FUNCTION per group but never touches the
    // enable, which is switched on once at start-up and stays on for the whole
    // frame. So only the function is ours to set, and it has to be set: the
    // last sprite group may have left an additive one behind. Switching
    // blending off instead is wrong twice over -- the sprites drawn afterwards
    // lose their alpha and turn into black rectangles, and a model's own
    // ground-shadow disc goes opaque white.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (std::size_t i = 0; i < loaded.enemies.size(); ++i) {
        PlacedEnemy &placed = loaded.enemies[i];
        const float scale = EnemyModelWorldScale(*placed.model, placed.gameScale,
                                                 kLevelCameraScale);

        float base[kMatrix4dElements];
        BuildEnemyGameMatrix(*placed.model, mapMvp, placed.x, placed.y, scale,
                             0.0f, base);
        DrawEnemyModel(*placed.model, program, base);
    }

    for (std::size_t i = 0; i < loaded.players.size(); ++i) {
        PlacedPlayer &placed = loaded.players[i];
        const float scale = PlayerModelWorldScale(
            *placed.model, loaded.playerTemplate->gameScale, kLevelCameraScale);

        float base[kMatrix4dElements];
        BuildPlayerGameMatrix(mapMvp, placed.x, placed.y, scale,
                              placed.facingDegrees, base);
        DrawPlayer(*placed.model, program, base);
    }

    // Put back what was found: the sprite path draws flat and in order.
    glDisable(GL_DEPTH_TEST);
}

/** How many objects of one type a map places. */
unsigned CountObjects(const LoadedMap &loaded, PlacedObjectType wanted) {
    const CMap &map = loaded.map;
    unsigned total = 0;
    for (std::uint32_t layer = 0; layer < map.GetObjectLayerCount(); ++layer) {
        const std::vector<PlacedObject> &objects =
            map.GetObjectLayer(layer).GetObjects();
        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (objects[i].objectType == static_cast<std::uint8_t>(wanted)) {
                total++;
            }
        }
    }
    return total;
}

void ReportSpawns(const LoadedMap &loaded) {
    std::printf("[m3] %u player spawns, %u enemy spawns\n",
                CountObjects(loaded, PlacedObjectType::Player),
                CountObjects(loaded, PlacedObjectType::Enemy));

    std::printf("[m4] %u collision layers in map data\n",
                loaded.map.GetCollisionLayerCount());
}

void BuildGeometry(const LoadedMap &loaded, CQuadBatch &batch, bool showTiles,
                   bool showProps, bool report) {
    const CMap &map = loaded.map;
    const TileSet &tileSet = loaded.tileSet;
    const std::vector<TileRect> &tiles = tileSet.GetTiles();
    const float drawSize = static_cast<float>(tileSet.GetDrawSize());

    batch.Begin();

    std::uint32_t skipped = 0;

    if (showTiles) {
        for (std::uint32_t layerIndex = 0; layerIndex < map.GetTileLayerCount();
             ++layerIndex) {
            const CLayerTile &layer = map.GetTileLayer(layerIndex);

            // A drifted layer leaves a strip of canvas uncovered at the edge it
            // has moved away from, so it needs one more cell on that side --
            // and only that side, which is what keeps the extra cells from
            // showing up as a border on all four. A layer at rest, or one
            // exactly on a tile boundary, uncovers nothing and draws the same
            // range M3 drew.
            int firstColumn = 0;
            int firstRow = 0;
            int lastColumn = static_cast<int>(map.GetCanvasWidth());
            int lastRow = static_cast<int>(map.GetCanvasHeight());
            if (layer.GetOffsetX() > 0.0f) {
                firstColumn = -1;
            }
            if (layer.GetOffsetX() < 0.0f) {
                lastColumn = lastColumn + 1;
            }
            if (layer.GetOffsetY() > 0.0f) {
                firstRow = -1;
            }
            if (layer.GetOffsetY() < 0.0f) {
                lastRow = lastRow + 1;
            }

            for (int row = firstRow; row < lastRow; ++row) {
                for (int column = firstColumn; column < lastColumn; ++column) {
                    // GetCell wraps by modulo and takes an unsigned index, so
                    // the ring's -1 is lifted past zero first. Adding a whole
                    // layer width or height leaves the wrapped result alone.
                    const std::uint32_t cellColumn =
                        static_cast<std::uint32_t>(column + layer.GetWidth());
                    const std::uint32_t cellRow =
                        static_cast<std::uint32_t>(row + layer.GetHeight());
                    const TileCell &cell = layer.GetCell(cellColumn, cellRow);

                    // 255 means nothing here; the layer below shows through.
                    if (cell.tileId == kEmptyTileId) {
                        skipped++;
                        continue;
                    }
                    if (cell.tileId >= tiles.size()) {
                        skipped++;
                        continue;
                    }

                    const TileRect &tile = tiles[cell.tileId];
                    if (tile.imageIndex >= loaded.textures.size()) {
                        skipped++;
                        continue;
                    }

                    SourceRect source;
                    source.x = tile.x;
                    source.y = tile.y;
                    source.width = tile.width;
                    source.height = tile.height;

                    batch.AddQuad(*loaded.textures[tile.imageIndex],
                                  (column + layer.GetOffsetX()) * drawSize,
                                  (row + layer.GetOffsetY()) * drawSize,
                                  drawSize, drawSize, source,
                                  (cell.flags & kTileFlagFlipHorizontal) != 0,
                                  (cell.flags & kTileFlagFlipVertical) != 0,
                                  BlendMode::Alpha);
                }
            }
        }
    }

    const std::uint32_t tileQuads = batch.GetQuadCount();

    if (showProps) {
        for (std::size_t i = 0; i < loaded.props.size(); ++i) {
            const PlacedProp &prop = loaded.props[i];
            const PropSlot *background = BackgroundSlotFor(prop);
            AddSpriteQuads(prop, CurrentQuads(*background, prop.background), batch);
        }
        // Script z=2 effects sit above background scenery but below bodies.
        AddParticleQuads(loaded, batch, 0, 2);
        for (std::size_t i = 0; i < loaded.props.size(); ++i) {
            const PlacedProp &prop = loaded.props[i];
            AddSpriteQuads(prop, CurrentQuads(*MainSlotFor(prop), prop.main), batch);
        }
        // Explosion/shockwave z=3 and cover debris z=5 sit above bodies.
        AddParticleQuads(loaded, batch, 3, 5);
        for (std::size_t i = 0; i < loaded.props.size(); ++i) {
            const PlacedProp &prop = loaded.props[i];
            AddSpriteQuads(prop,
                           CurrentQuads(*ForegroundSlotFor(prop), prop.foreground),
                           batch);
        }
    }

    batch.Upload();

    // This runs every frame now, so it only says anything when the caller has
    // just changed what is being drawn.
    if (report) {
        std::printf("[m3] %u tile quads + %u prop quads in %u draw calls "
                    "(%u cells empty or unusable)\n",
                    tileQuads, batch.GetQuadCount() - tileQuads,
                    batch.GetGroupCount(), skipped);
    }
}

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
std::vector<CatalogMap> BuildCatalog(CResTOCManager &tocManager) {
    std::vector<CatalogMap> catalog;

    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        CResPackTOC *pack = tocManager.GetPack(static_cast<int>(i));
        if (pack == nullptr) {
            continue;
        }

        CGameObjectPack objectPack;
        if (!objectPack.Init(*pack)) {
            continue;
        }

        const std::uint32_t mapCount =
            objectPack.GetObjectCount(GameSection::TileLayer);
        for (std::uint32_t m = 0; m < mapCount; ++m) {
            CatalogMap entry;
            entry.packIndex = static_cast<int>(i);
            entry.packName = pack->GetShortName();
            entry.mapIndex = m;
            catalog.push_back(entry);
        }
    }

    return catalog;
}

/** Slot of the first map of the pack `slot` belongs to. */
std::size_t FirstMapOfPack(const std::vector<CatalogMap> &catalog, std::size_t slot) {
    std::size_t first = slot;
    while (first > 0 && catalog[first - 1].packIndex == catalog[slot].packIndex) {
        first--;
    }
    return first;
}

/**
 * Slot of the first map of the next pack, wrapping round the end.
 *
 * Left and right already cross pack boundaries one map at a time; this is the
 * shortcut for skipping a whole pack, which matters when pack2 alone holds
 * nine maps.
 */
std::size_t NextPackSlot(const std::vector<CatalogMap> &catalog, std::size_t slot) {
    const int currentPack = catalog[slot].packIndex;

    for (std::size_t step = 1; step <= catalog.size(); ++step) {
        const std::size_t candidate = (slot + step) % catalog.size();
        if (catalog[candidate].packIndex != currentPack) {
            return FirstMapOfPack(catalog, candidate);
        }
    }
    return slot;  // only one pack has maps
}

/** Slot of the first map of the previous pack, wrapping round the start. */
std::size_t PreviousPackSlot(const std::vector<CatalogMap> &catalog,
                             std::size_t slot) {
    const std::size_t first = FirstMapOfPack(catalog, slot);

    // The map before this pack's first belongs to the previous pack; back up
    // from there to that pack's own first.
    const std::size_t previous = (first + catalog.size() - 1) % catalog.size();
    return FirstMapOfPack(catalog, previous);
}

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
MapRectangle ViewedRegion(const LoadedMap &loaded) {
    const MapRectangle bounds = loaded.map.GetCameraExtent();
    if (!bounds.IsEmpty()) {
        return bounds;
    }

    const float drawSize = static_cast<float>(loaded.tileSet.GetDrawSize());

    MapRectangle canvas;
    canvas.x = 0;
    canvas.y = 0;
    canvas.width = static_cast<std::int16_t>(loaded.map.GetCanvasWidth() * drawSize);
    canvas.height = static_cast<std::int16_t>(loaded.map.GetCanvasHeight() * drawSize);
    return canvas;
}

/** Zoom out far enough to see the whole viewed region, and centre it. */
Camera FitCamera(const LoadedMap &loaded, int viewWidth, int viewHeight) {
    const MapRectangle region = ViewedRegion(loaded);
    const float regionWidth = static_cast<float>(region.width);
    const float regionHeight = static_cast<float>(region.height);

    Camera camera;
    camera.zoom = 1.0f;
    camera.x = 0.0f;
    camera.y = 0.0f;
    if (regionWidth <= 0.0f || regionHeight <= 0.0f) {
        return camera;
    }

    const float fitX = static_cast<float>(viewWidth) / (regionWidth * kFitMargin);
    const float fitY = static_cast<float>(viewHeight) / (regionHeight * kFitMargin);
    camera.zoom = (fitX < fitY) ? fitX : fitY;

    camera.x = static_cast<float>(region.x) +
               (regionWidth - static_cast<float>(viewWidth) / camera.zoom) * 0.5f;
    camera.y = static_cast<float>(region.y) +
               (regionHeight - static_cast<float>(viewHeight) / camera.zoom) * 0.5f;
    return camera;
}

/** Centre the fixed GameView camera on the controlled player. */
void FollowPlayerCamera(const LoadedMap &loaded, int viewWidth, int viewHeight,
                        Camera &camera) {
    if (loaded.players.empty()) {
        return;
    }

    const float viewWorldWidth = static_cast<float>(viewWidth) / camera.zoom;
    const float viewWorldHeight = static_cast<float>(viewHeight) / camera.zoom;
    camera.x = loaded.players[0].x - viewWorldWidth * 0.5f;
    camera.y = loaded.players[0].y - viewWorldHeight * 0.5f;

    const MapRectangle bounds = loaded.map.GetVisibleBounds();
    if (bounds.IsEmpty()) {
        return;
    }

    const float left = static_cast<float>(bounds.x);
    const float top = static_cast<float>(bounds.y);
    const float right = static_cast<float>(bounds.x + bounds.width);
    const float bottom = static_cast<float>(bounds.y + bounds.height);

    if (viewWorldWidth >= static_cast<float>(bounds.width)) {
        camera.x = left + (static_cast<float>(bounds.width) - viewWorldWidth) *
                            0.5f;
    } else {
        if (camera.x < left) {
            camera.x = left;
        }
        if (camera.x + viewWorldWidth > right) {
            camera.x = right - viewWorldWidth;
        }
    }

    if (viewWorldHeight >= static_cast<float>(bounds.height)) {
        camera.y = top + (static_cast<float>(bounds.height) - viewWorldHeight) *
                           0.5f;
    } else {
        if (camera.y < top) {
            camera.y = top;
        }
        if (camera.y + viewWorldHeight > bottom) {
            camera.y = bottom - viewWorldHeight;
        }
    }
}

/** Keep the default stage's framing across maps; bounds only limit panning.
 * This viewer setting is deliberately independent of stage dimensions.
 */
float GameViewCameraZoom(int viewWidth,
                         int viewHeight) {
    const float logicalScaleX = static_cast<float>(viewWidth) /
                                kGameViewWorldWidth;
    const float logicalScaleY = static_cast<float>(viewHeight) /
                                kGameViewWorldHeight;
    float zoom = logicalScaleX;
    if (logicalScaleY > zoom) {
        zoom = logicalScaleY;
    }
    return zoom;
}

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
                          int drawableWidth, int drawableHeight, int scissor[4]) {
    const MapRectangle bounds = loaded.map.GetCameraExtent();
    if (bounds.IsEmpty()) {
        return false;
    }

    const float left = (static_cast<float>(bounds.x) - camera.x) * camera.zoom;
    const float top = (static_cast<float>(bounds.y) - camera.y) * camera.zoom;
    const float right = left + static_cast<float>(bounds.width) * camera.zoom;
    const float bottom = top + static_cast<float>(bounds.height) * camera.zoom;

    // Flip to GL's bottom-left origin, then clamp to the window.
    float x0 = left;
    float x1 = right;
    float y0 = static_cast<float>(drawableHeight) - bottom;
    float y1 = static_cast<float>(drawableHeight) - top;

    if (x0 < 0.0f) {
        x0 = 0.0f;
    }
    if (y0 < 0.0f) {
        y0 = 0.0f;
    }
    if (x1 > static_cast<float>(drawableWidth)) {
        x1 = static_cast<float>(drawableWidth);
    }
    if (y1 > static_cast<float>(drawableHeight)) {
        y1 = static_cast<float>(drawableHeight);
    }

    scissor[0] = static_cast<int>(x0);
    scissor[1] = static_cast<int>(y0);
    scissor[2] = 0;
    scissor[3] = 0;

    // Panned fully off screen. Still clipping, with nothing left to draw.
    if (x1 > x0) {
        scissor[2] = static_cast<int>(x1 - x0);
    }
    if (y1 > y0) {
        scissor[3] = static_cast<int>(y1 - y0);
    }

    return true;
}

/** Open the archives and pick out one pack, already bound. */
CResPackTOC *OpenPack(CResTOCManager &tocManager, const std::string &bigDirectory,
                      const std::string &packShortName) {
    if (!tocManager.Init(bigDirectory, kArtSetXga)) {
        return nullptr;
    }
    const int packIndex = tocManager.GetPackIndexFromName(packShortName.c_str());
    CResPackTOC *pack = tocManager.GetPack(packIndex);
    if (pack == nullptr || pack->GetShortName() != packShortName) {
        std::printf("[m3] no pack named %s\n", packShortName.c_str());
        return nullptr;
    }
    if (!tocManager.Bind()) {
        return nullptr;
    }
    return pack;
}

}  // namespace

int RunMapList(const std::string &bigDirectory) {
    CResTOCManager tocManager;
    if (!tocManager.Init(bigDirectory, kArtSetXga)) {
        return 1;
    }
    if (!tocManager.Bind()) {
        return 1;
    }

    std::printf("\n%-12s %6s %9s %8s\n", "pack", "maps", "tilesets", "levels");
    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        CResPackTOC *pack = tocManager.GetPack(static_cast<int>(i));

        CGameObjectPack objectPack;
        if (!objectPack.Init(*pack)) {
            continue;
        }

        const std::uint32_t maps = objectPack.GetObjectCount(GameSection::TileLayer);
        if (maps == 0) {
            continue;
        }
        std::printf("%-12s %6u %9u %8u\n", pack->GetShortName().c_str(), maps,
                    objectPack.GetObjectCount(GameSection::TileSet),
                    objectPack.GetObjectCount(GameSection::Level));
    }

    // The flat order the viewer's left and right keys walk, so a map can be
    // named by one number instead of a pack and an ordinal.
    const std::vector<CatalogMap> catalog = BuildCatalog(tocManager);
    std::printf("\nviewer order (%zu maps):\n", catalog.size());
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        std::printf("  %2zu  %-8s map %u\n", i + 1, catalog[i].packName.c_str(),
                    catalog[i].mapIndex);
    }
    return 0;
}

/** Checkpoint the rebuilt profile only. Research harnesses pass no context. */
bool SaveSurvivalProgress(SurvivalGameContext *context, const CPlayerProgress &progress,
    const CombatScene &scene, unsigned wave, std::uint64_t &accountedXplodium) {
    if (context == nullptr) { return true; }
    CProfileManager &profile = context->profile;
    profile.experience = progress.GetExperience();
    profile.xplodium += scene.GetXplodium() - accountedXplodium;
    accountedXplodium = scene.GetXplodium();
    profile.clearedWaves[context->planet] = std::max(profile.clearedWaves[context->planet], wave);
    return profile.SaveToDisk(context->savePath);
}

int RunSurvival(const std::string &bigDirectory, const std::string &packShortName,
    unsigned mapIndex, unsigned weaponIndex, int armorIndex, const std::string &screenshotPath,
    unsigned advanceMs, bool firePreview, bool showCollisions, bool check, unsigned checkWaves, unsigned startWave,
    SurvivalGameContext *gameContext, bool withBrother, bool powerupStudy, const MissionEntry *archiveMission) {
    std::string capturePath = screenshotPath;
    unsigned checkFailures = 0;
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    const int packIndex = toc.GetPackIndexFromName(packShortName.c_str());
    if (packIndex < 0) { return 1; }
    PackTables tables(toc);
    std::vector<WeaponEntry> weapons;
    std::vector<EnemyTemplateData> enemies;
    PlayerVitals vitals;
    vitals.invincible = false;
    CPlayerProgress::Template progressData;
    CPlayerProgress progress;
    if (!LoadPlayerProgress(toc, tables, progressData)) { return 1; }
    progress.Bind(progressData);
    if (gameContext != nullptr) { progress.SetExperience(gameContext->profile.experience); }
    if (!LoadWeaponCatalog(toc, tables, weapons) || !LoadEnemyCatalog(toc, tables, enemies) ||
        !LoadInitialPlayerHealth(toc, tables, vitals.maximum)) { return 1; }
    CWindow window;
    if (!window.Open("Gun Bros - Survival", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    CShaderProgram program, markerProgram;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0") ||
        !markerProgram.Load(kShaderDirectory, "ogles_vs_mvp_constcolor", "ogles_ps_constcolor")) { return 1; }
    CQuadBatch batch;
    CMarkerBatch markers;
    if (!batch.Create(program) || !markers.Create(markerProgram)) { return 1; }
    LoadedMap loaded;
    if (!LoadMap(toc, packIndex, mapIndex, loaded)) { return 1; }
    LoadPlacedPlayers(toc, program, loaded);
    if (loaded.players.empty()) { return 1; }
    // The second brother will be driven by the partner system, not a stationary clone.
    loaded.players.resize(1);
    PlayerModel &player = *loaded.players[0].model;
    player.vitals = &vitals;
    std::size_t weaponSlot = weaponIndex % weapons.size();
    unsigned equippedWeaponSlot = 0;
    if (gameContext != nullptr) {
        const GameObjectRef &ref = gameContext->profile.configuration.guns[0];
        bool found = false;
        for (std::size_t index = 0; index < weapons.size(); ++index) {
            if (weapons[index].packHash == ref.packHash && weapons[index].ordinal == ref.localIndex) {
                weaponSlot = index;
                found = true;
                break;
            }
        }
        if (!found) { return 1; }
    }
    if (!EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot])) { return 1; }
    if (armorIndex >= 0) {
        std::vector<ArmorEntry> armors;
        if (!LoadArmorCatalog(toc, tables, armors) || armorIndex >= static_cast<int>(armors.size()) ||
            !EquipPlayerArmor(tables, armors[armorIndex].data, program, player)) { return 1; }
    }
    if (gameContext != nullptr) {
        std::vector<ArmorEntry> armors;
        if (!LoadArmorCatalog(toc, tables, armors)) { return 1; }
        for (const GameObjectRef &ref : gameContext->profile.configuration.armor) {
            if (ref.IsNull()) { continue; }
            bool found = false;
            for (const ArmorEntry &entry : armors) {
                if (entry.packHash == ref.packHash && entry.ordinal == ref.localIndex) {
                    if (!EquipPlayerArmor(tables, entry.data, program, player)) { return 1; }
                    found = true;
                    break;
                }
            }
            if (!found) { return 1; }
        }
    }
    WeaponEffects effects(toc, tables, program);
    if (check) {
        // A separate world exercises empty-wave minimums and damage rejection
        // without putting fixture currency into the actual survival/profile run.
        CombatScene rewardProbe(tables, program, enemies, player, vitals, effects, loaded.playerTemplate->gameScale);
        rewardProbe.Reset();
        rewardProbe.OnWaveCleared(10);
        rewardProbe.OnWaveCleared(100);
        if (rewardProbe.GetXplodium() != 2 || rewardProbe.GetPerfectWaves() != 2) { ++checkFailures; }
        CombatHit wound;
        wound.ownerType = 1;
        wound.damage = 0.25f;
        rewardProbe.ApplyHit(kPlayerCombatId, wound);
        rewardProbe.OnWaveCleared(10);
        if (rewardProbe.GetLastWaveBonus() != 0 || rewardProbe.GetXplodium() != 2 ||
            rewardProbe.GetPerfectWaves() != 2 || rewardProbe.GetClearedWaves() != 3) { ++checkFailures; }
        rewardProbe.OnWaveCleared(10);
        if (rewardProbe.GetLastWaveBonus() != 1 || rewardProbe.GetXplodium() != 3) { ++checkFailures; }
        std::printf("[survival-check] minimum/previous-bonus/damage/next-wave failures=%u\n", checkFailures);
        CPlayerProgress pickupProgress;
        pickupProgress.Bind(progressData);
        rewardProbe.SetPlayerProgress(&pickupProgress);
        CRefinementManager::Template pickupRefinement;
        if (!LoadRefinementTemplate(toc, tables, pickupRefinement)) { return 1; }
        CProfileManager pickupProfile;
        pickupProfile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), pickupRefinement);
        PickupScene pickupProbe(toc, tables, program, &pickupProfile);
        if (!pickupProbe.Init()) { return 1; }
        GameObjectRef pickupRef;
        pickupRef.packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
        pickupRef.localIndex = 2;
        vitals.health = 1;
        pickupProbe.Spawn(pickupRef, rewardProbe.playerX, rewardProbe.playerY);
        pickupProbe.Update(16, rewardProbe, effects);
        if (vitals.health != vitals.maximum) { ++checkFailures; }
        pickupRef.localIndex = 0;
        pickupProbe.Spawn(pickupRef, rewardProbe.playerX, rewardProbe.playerY);
        pickupRef.localIndex = 1;
        pickupProbe.Spawn(pickupRef, rewardProbe.playerX, rewardProbe.playerY);
        pickupRef.localIndex = 7;
        pickupProbe.Spawn(pickupRef, rewardProbe.playerX, rewardProbe.playerY);
        pickupProbe.Update(16, rewardProbe, effects);
        pickupProbe.Update(16, rewardProbe, effects);
        GameObjectRef grenade = pickupRef;
        grenade.localIndex = 13;
        if (pickupProgress.GetExperience() != 500 || rewardProbe.GetXplodium() != 153 ||
            pickupProbe.collected != 4 || pickupProbe.GetCount() != 0 || pickupProbe.failures != 0 ||
            pickupProfile.GetPowerupCount(grenade) != 1) { ++checkFailures; }
        CProfileManager restoredPickupProfile;
        restoredPickupProfile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), pickupRefinement);
        if (!pickupProfile.SaveToDisk("out/pickup-profile-check.dat") ||
            !restoredPickupProfile.LoadFromDisk("out/pickup-profile-check.dat") ||
            restoredPickupProfile.GetPowerupCount(grenade) != 1) { ++checkFailures; }
        std::printf("[pickup-check] health/experience/xplodium/grenade/save-once failures=%u\n", checkFailures);
    }
    CombatScene scene(tables, program, enemies, player, vitals, effects, loaded.playerTemplate->gameScale);
    CBrotherAI brother;
    PlayerModel brotherModel;
    if (withBrother) {
        brother.vitals.maximum = progress.GetHealth();
        brother.vitals.invincible = false;
        brotherModel.vitals = &brother.vitals;
        brotherModel.human = false;
        brotherModel.brotherIndex = 1;
        if (!BuildPlayerBody(tables, player.moveSet, brotherModel) ||
            !EquipPlayerWeapon(tables, loaded.playerTemplate->script, weapons[weaponSlot].data,
                "AI brother", brotherModel) || !CreatePlayerBuffers(brotherModel, program)) { return 1; }
        for (const auto &armor : player.armor) {
            if (armor != nullptr && !EquipPlayerArmor(tables, armor->data, program, brotherModel)) { return 1; }
        }
        scene.SetBrother(&brotherModel, &brother);
    }
    scene.SetPlayerProgress(&progress);
    SurvivalSession session(scene, loaded.map, enemies);
    CProfileManager researchProfile;
    CProfileManager *pickupProfile = nullptr;
    if (gameContext != nullptr) { pickupProfile = &gameContext->profile; }
    else { pickupProfile = &researchProfile; }
    PowerupScene powerups(toc, tables, player, vitals, scene, effects, *pickupProfile);
    if (!powerups.Init()) { return 1; }
    if (powerupStudy) {
        GameObjectRef item;
        item.packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
        for (unsigned index = 0; index < 20; ++index) {
            item.localIndex = static_cast<std::uint8_t>(index);
            if (IsPlayablePowerup(item)) { researchProfile.AddPowerup(item, 10); }
        }
        std::printf("[powerup-study] isolated inventory: 10 of each supported item\n");
    }
    session.SetPowerups(&powerups);
    PickupScene pickups(toc, tables, program, pickupProfile);
    if (!pickups.Init()) { return 1; }
    session.SetPickups(&pickups, &effects);
    const GameObjectRef *archiveLevel = nullptr;
    if (archiveMission != nullptr) { archiveLevel = &archiveMission->data.level; }
    if (!session.Load(toc, tables, toc.GetPack(packIndex)->GetPackHash(), mapIndex, archiveLevel)) { return 1; }
    const float startX = loaded.players[0].x;
    const float startY = loaded.players[0].y;
    session.SetStartWave(static_cast<int>(startWave));
    scene.SetMap(loaded.map, loaded.collisionScene, loaded.weaponCollision, kLevelCameraScale, kPlayerCollisionRadius);
    session.Restart(startX, startY);
    std::uint64_t accountedXplodium = 0;
    int lastSavedWave = session.GetLevel().GetWave();
    bool savedDeath = false;
    // CMap::SetObjectLayer (:91935) activates one layer. Preview may combine
    // layers, but survival must not inherit deathmatch/campaign obstacles.
    LoadProps(toc, loaded, session.GetLevel().GetObjectLayer());
    BuildCollisionScene(loaded);
    MapPropWorld props(loaded, scene, session.GetLevel(), effects);
    session.SetProps(&props);
    scene.SetProps(&props);
    session.Restart(startX, startY);
    if (check) { checkFailures += session.CheckLevelSounds(); }
    if (check && archiveMission == nullptr) {
        checkFailures += props.CheckDamageContracts();
        // Original character animation, real bullet scripts and actual stock.
        // This account is isolated even when the full-menu check owns a profile.
        CProfileManager consumableProbe;
        CRefinementManager::Template consumableRefinement;
        if (!LoadRefinementTemplate(toc, tables, consumableRefinement)) { return 1; }
        consumableProbe.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), consumableRefinement);
        PowerupScene powerupProbe(toc, tables, player, vitals, scene, effects, consumableProbe);
        if (!powerupProbe.Init()) { return 1; }
        GameObjectRef consumable;
        consumable.packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
        for (unsigned index = 13; index <= 15; ++index) {
            session.Restart(startX, startY);
            consumable.localIndex = static_cast<std::uint8_t>(index);
            consumableProbe.AddPowerup(consumable, 2);
            powerupProbe.Select(index);
            const std::size_t before = effects.GetShotCount();
            if (!powerupProbe.Use() || powerupProbe.Use() || consumableProbe.GetPowerupCount(consumable) != 2) { ++checkFailures; }
            for (int elapsed = 0; elapsed < 5000; elapsed += 16) {
                scene.Update(16, 0, 0, false);
                powerupProbe.Update(16);
            }
            if (effects.GetShotCount() != before + 1 || consumableProbe.GetPowerupCount(consumable) != 1) { ++checkFailures; }
            std::printf("[powerup-play-check] item=%u shots=%zu stock=%u state=%d failures=%u\n", index,
                effects.GetShotCount() - before, consumableProbe.GetPowerupCount(consumable), player.weapon->brother.GetStateId(), checkFailures);
            if (!powerupProbe.Use()) { ++checkFailures; }
            CombatHit cancel;
            cancel.ownerType = 1;
            cancel.damage = 10000;
            scene.ApplyHit(kPlayerCombatId, cancel);
            for (int elapsed = 0; elapsed < 1000; elapsed += 16) {
                scene.Update(16, 0, 0, false);
                powerupProbe.Update(16);
            }
            if (consumableProbe.GetPowerupCount(consumable) != 1 || effects.GetShotCount() != before + 1) { ++checkFailures; }
        }
        session.Restart(startX, startY);
        consumable.localIndex = 1;
        consumableProbe.AddPowerup(consumable, 1);
        powerupProbe.Select(1);
        if (powerupProbe.Use()) { ++checkFailures; }
        vitals.health = 1;
        if (!powerupProbe.Use() || vitals.health != std::min(vitals.maximum, 9.0f) || powerupProbe.GetCount() != 0) { ++checkFailures; }
        checkFailures += powerupProbe.failures;
        std::printf("[powerup-play-check] healing/cancel/repeat consumed=%u failures=%u\n", powerupProbe.consumed, checkFailures);
        session.Restart(startX, startY);
        consumable.localIndex = 5;
        consumableProbe.AddPowerup(consumable, 2);
        powerupProbe.Select(5);
        if (!powerupProbe.Use() || powerupProbe.Use() || !player.weapon->brother.IsShield() || powerupProbe.GetCount() != 1) { ++checkFailures; }
        const float shieldHealth = vitals.health;
        player.weapon->brother.ReceiveDamage(1);
        if (vitals.health != shieldHealth) { ++checkFailures; }
        // Equipment must not reset an actor's active shield timer.
        const int shieldMs = player.powerups.shieldMs;
        if (!EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot]) || player.powerups.shieldMs != shieldMs) { ++checkFailures; }
        AdvancePlayer(player, shieldMs);
        if (player.weapon->brother.IsShield()) { ++checkFailures; }
        player.weapon->brother.ReceiveDamage(1);
        if (std::abs(vitals.health - (shieldHealth - 1)) > 0.001f) { ++checkFailures; }
        session.Restart(startX, startY);
        {
            // Auto Aim: a real stationary enemy, ordinary rifle and actual
            // projectiles. No caller-supplied aim or automatic pilot firing.
            if (!EquipControlledPlayer(tables, loaded, program, weapons[0])) { return 1; }
            WeaponEffects aimEffects(toc, tables, program);
            CombatScene aimScene(tables, program, enemies, player, vitals, aimEffects, loaded.playerTemplate->gameScale);
            aimScene.Reset();
            aimScene.playerX = 600;
            aimScene.playerY = 650;
            aimScene.facing = 270;
            CombatEnemy *target = aimScene.Spawn(0, 700, 650);
            if (target == nullptr) { return 1; }
            for (int elapsed = 0; elapsed < 1000; elapsed += 16) { target->model.enemy.Update(16); }
            // Maturation can queue an enemy shot before this fixture begins.
            // Discard only those setup actions; measured scene updates remain real.
            target->model.enemy.TakeActions();
            target->model.enemy.combat.health = 100000;
            target->model.enemy.combat.maxHealth = 100000;
            target->model.enemy.stun.SetStunned(6000, 0, 0);
            consumable.localIndex = 12;
            consumableProbe.AddPowerup(consumable, 2);
            PowerupScene aimPowerup(toc, tables, player, vitals, aimScene, aimEffects, consumableProbe);
            if (!aimPowerup.Init() || !aimPowerup.Select(12) || !aimPowerup.Use() || aimPowerup.Use() ||
                aimPowerup.GetCount() != 1 || player.powerups.autoFireMs != 90000) { ++checkFailures; }
            std::printf("[autoaim-probe] use stock=%u timer=%d failures=%u\n", aimPowerup.GetCount(), player.powerups.autoFireMs, checkFailures);
            for (int elapsed = 0; elapsed < 800; elapsed += 16) { aimScene.Update(16, 0, 0, false); }
            if (aimEffects.GetShotCount() != 0 || aimScene.GetAutoAimTarget() != 0) { ++checkFailures; }
            std::printf("[autoaim-probe] idle shots=%zu target=%llu failures=%u\n", aimEffects.GetShotCount(),
                static_cast<unsigned long long>(aimScene.GetAutoAimTarget()), checkFailures);
            for (int elapsed = 0; elapsed < 1800; elapsed += 16) { aimScene.Update(16, 0, 0, true); }
            if (aimEffects.GetShotCount() == 0 || target->model.enemy.combat.hitCount == 0 ||
                aimScene.GetAutoAimTarget() == 0 || std::abs(aimScene.facing - 90) > 5.1f) { ++checkFailures; }
            std::printf("[autoaim-probe] hold facing=%.2f target=%.1f,%.1f shots=%zu hits=%d failures=%u\n",
                aimScene.facing, target->model.enemy.combat.x, target->model.enemy.combat.y,
                aimEffects.GetShotCount(), target->model.enemy.combat.hitCount, checkFailures);
            aimScene.Update(16, 0, 0, false);
            const auto releasedShots = aimEffects.GetShotCount();
            for (int elapsed = 0; elapsed < 320; elapsed += 16) { aimScene.Update(16, 0, 0, false); }
            if (aimEffects.GetShotCount() != releasedShots || aimScene.GetAutoAimTarget() != 0) { ++checkFailures; }
            std::printf("[autoaim-probe] release shots=%zu previous=%zu failures=%u\n", aimEffects.GetShotCount(), releasedShots, checkFailures);
            const int remainingMs = player.powerups.autoFireMs;
            if (!EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot]) ||
                player.powerups.autoFireMs != remainingMs) { ++checkFailures; }
            AdvancePlayer(player, remainingMs - 1);
            if (!player.weapon->brother.IsAutoFire()) { ++checkFailures; }
            AdvancePlayer(player, 1);
            if (player.weapon->brother.IsAutoFire()) { ++checkFailures; }
            std::printf("[autoaim-play-check] shots=%zu hits=%d hold/release/swap/90sec/expiry failures=%u\n",
                releasedShots, target->model.enemy.combat.hitCount, checkFailures);
        }
        session.Restart(startX, startY);
        {
            WeaponEffects turretEffects(toc, tables, program);
            CombatScene turretScene(tables, program, enemies, player, vitals, turretEffects, loaded.playerTemplate->gameScale);
            turretScene.Reset();
            CombatEnemy *target = turretScene.Spawn(0, 600, 460);
            if (target == nullptr) { return 1; }
            for (int elapsed = 0; elapsed < 1000; elapsed += 16) { target->model.enemy.Update(16); }
            target->model.enemy.TakeActions();
            target->model.enemy.combat.x = 600;
            target->model.enemy.combat.y = 460;
            target->model.enemy.combat.health = 100000;
            target->model.enemy.combat.maxHealth = 100000;
            target->model.enemy.stun.SetStunned(60000, 0, 0);
            consumable.localIndex = 19;
            consumableProbe.AddPowerup(consumable, 2);
            PowerupScene turretPowerup(toc, tables, player, vitals, turretScene, turretEffects, consumableProbe);
            if (!turretPowerup.Init() || !turretPowerup.Select(19) || !turretPowerup.Use() ||
                turretPowerup.GetCount() != 2 || !player.weapon->brother.IsTurretActive() || turretPowerup.Use()) { ++checkFailures; }
            int firstActiveMs = -1, stoppedMs = -1;
            unsigned peakTurrets = 0;
            for (int elapsed = 0; elapsed < 40000; elapsed += 16) {
                turretScene.Update(16, 0, 0, false);
                turretPowerup.Update(16);
                unsigned liveTurrets = 0;
                for (const auto &actor : turretScene.enemies) {
                    if (actor->model.enemy.combat.turret && !actor->model.enemy.combat.removed) { ++liveTurrets; }
                }
                peakTurrets = std::max(peakTurrets, liveTurrets);
                if (player.weapon->brother.IsTurretActive() && liveTurrets == 1 && firstActiveMs < 0) {
                    firstActiveMs = elapsed + 16;
                    if (turretPowerup.Use() || turretPowerup.GetCount() != 1) { ++checkFailures; }
                }
                if (firstActiveMs >= 0 && !player.weapon->brother.IsTurretActive()) {
                    stoppedMs = elapsed + 16;
                    break;
                }
            }
            if (firstActiveMs < 0 || stoppedMs <= firstActiveMs || peakTurrets != 1 ||
                target->model.enemy.combat.hitCount == 0 || turretPowerup.GetCount() != 1 ||
                turretPowerup.consumed != 1 || turretPowerup.failures != 0) { ++checkFailures; }
            if (!turretPowerup.Use()) { ++checkFailures; }
            // Cancelled pre-throw requests release their reservation, without
            // consuming another item or replacing a live turret's state.
            if (!EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot])) { return 1; }
            turretPowerup.Update(16);
            if (player.weapon->brother.IsTurretActive() || turretPowerup.GetCount() != 1) { ++checkFailures; }
            if (!turretPowerup.Use()) { ++checkFailures; }
            vitals.dead = true;
            turretPowerup.Update(16);
            if (player.weapon->brother.IsTurretActive() || turretPowerup.GetCount() != 1) { ++checkFailures; }
            std::printf("[turret-play-check] active=%d stopped=%d peak=%u shots=%zu hits=%d stock=%u failures=%u\n",
                firstActiveMs, stoppedMs, peakTurrets, turretEffects.GetShotCount(),
                target->model.enemy.combat.hitCount, turretPowerup.GetCount(), checkFailures);
        }
        session.Restart(startX, startY);
        const unsigned boosts[] = {18, 17, 16};
        for (unsigned type = 0; type < 3; ++type) {
            consumable.localIndex = static_cast<std::uint8_t>(boosts[type]);
            consumableProbe.AddPowerup(consumable, 2);
            powerupProbe.Select(boosts[type]);
            if (!powerupProbe.Use() || powerupProbe.Use() || !player.weapon->brother.IsFrenzyType(type)) { ++checkFailures; }
            float expected = 332 / 256.0f;
            if (type == 2) { expected = 1.5f; }
            if (std::abs(scene.GetProjectilePowerupMultiplier(kPlayerCombatId) - expected) > 0.001f) { ++checkFailures; }
        }
        const float beforeDefense = vitals.health;
        player.weapon->brother.ReceiveDamage(1);
        if (std::abs(vitals.health - (beforeDefense - 256.0f / 332)) > 0.001f) { ++checkFailures; }
        AdvancePlayer(player, 15000);
        if (scene.GetProjectilePowerupMultiplier(kPlayerCombatId) != 1 || player.weapon->brother.IsFrenzyType(2)) { ++checkFailures; }
        if (!consumableProbe.SaveToDisk("out/powerup-profile-check.dat")) { ++checkFailures; }
        CProfileManager restoredConsumables;
        restoredConsumables.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), consumableRefinement);
        if (!restoredConsumables.LoadFromDisk("out/powerup-profile-check.dat") ||
            restoredConsumables.GetPowerupCount(consumable) != 1) { ++checkFailures; }
        std::printf("[powerup-play-check] shield/defense/priority/expiry/weapon-swap/save failures=%u\n", checkFailures);
        session.Restart(startX, startY);
        {
            // Isolate impact contracts from steering: a stationary original
            // enemy receives real CBullet -> CombatScene -> script splash hits.
            WeaponEffects blastEffects(toc, tables, program);
            CombatScene blastScene(tables, program, enemies, player, vitals, blastEffects, loaded.playerTemplate->gameScale);
            const unsigned grenadeBullets[] = {90, 93, 94};
            for (unsigned bulletIndex : grenadeBullets) {
                blastScene.Reset();
                CombatEnemy *target = blastScene.Spawn(0, 600, 450);
                if (target == nullptr) { return 1; }
                CEnemy &enemy = target->model.enemy;
                // Let the original spawn sequence reach its ordinary hit handler.
                for (int elapsed = 0; elapsed < 1000; elapsed += 16) { enemy.Update(16); }
                enemy.combat.health = 10000;
                enemy.combat.maxHealth = 10000;
                GameObjectRef bulletRef = consumable;
                bulletRef.localIndex = static_cast<std::uint8_t>(bulletIndex);
                if (blastEffects.SpawnProjectile(bulletRef, 600, 350, 0, 0, 0, kPlayerCombatId, 0) == 0) { ++checkFailures; }
                float matrix[16];
                blastScene.PlayerMatrix(matrix);
                unsigned impactState = 255;
                int maximumStunMs = 0;
                for (int elapsed = 0; elapsed < 4000; elapsed += 16) {
                    enemy.combat.x = 600;
                    enemy.combat.y = 450;
                    enemy.combat.targetAlive = false;
                    blastEffects.Update(player, matrix, 0, 16);
                    maximumStunMs = std::max(maximumStunMs, enemy.stun.GetRemainingMs());
                    enemy.Update(16);
                    if (enemy.combat.hitCount > 0 && impactState == 255) { impactState = enemy.GetStateId(); }
                }
                float expectedDamage = 100;
                if (bulletIndex == 93) { expectedDamage = 30; }
                expectedDamage *= PlayerArmorMultiplier(player, 1);
                if (std::abs(enemy.combat.totalDamage - expectedDamage) > 0.01f) { ++checkFailures; }
                int expectedStunMs = 0;
                if (bulletIndex == 93) { expectedStunMs = 750; }
                if (bulletIndex == 94) { expectedStunMs = 2000; }
                if (maximumStunMs != expectedStunMs || enemy.stun.IsActive()) { ++checkFailures; }
                std::printf("[powerup-impact-check] bullet=%u damage=%.3f expected=%.3f hits=%d state=%u failures=%u\n",
                    bulletIndex, enemy.combat.totalDamage, expectedDamage, enemy.combat.hitCount, impactState, checkFailures);
                std::printf("[powerup-impact-check] stun=%d expected=%d expired=%d\n", maximumStunMs, expectedStunMs, !enemy.stun.IsActive());
            }
            if (powerupStudy) {
                std::ofstream attributeReport("out/powerup-enemy-attributes.txt");
                const unsigned attributes[] = {19, 6171, 23};
                for (unsigned enemyIndex = 0; enemyIndex < enemies.size(); ++enemyIndex) {
                    if (!enemies[enemyIndex].script.IsPresent()) { continue; }
                    for (unsigned flags : attributes) {
                        blastScene.Reset();
                        CombatEnemy *target = blastScene.Spawn(enemyIndex, 600, 450);
                        if (target == nullptr) { return 1; }
                        CEnemy &enemy = target->model.enemy;
                        for (int elapsed = 0; elapsed < 1000; elapsed += 16) { enemy.Update(16); }
                        enemy.combat.health = 10000;
                        enemy.combat.maxHealth = 10000;
                        const unsigned before = enemy.GetStateId();
                        CombatHit probe;
                        probe.owner = kPlayerCombatId;
                        probe.ownerType = 0;
                        probe.x = 600;
                        probe.y = 350;
                        probe.flags = flags;
                        probe.damage = 1;
                        probe.splash = true;
                        enemy.ReceiveHit(probe);
                        const unsigned immediately = enemy.GetStateId();
                        const int durationMs = enemy.stun.GetRemainingMs();
                        if (durationMs > 100) {
                            const float frozenX = enemy.combat.x, frozenY = enemy.combat.y;
                            const int frozenTime = enemy.GetPart(0).controller.GetAnimation().GetTimeMs();
                            enemy.Update(100);
                            if (enemy.combat.x != frozenX || enemy.combat.y != frozenY ||
                                enemy.GetPart(0).controller.GetAnimation().GetTimeMs() != frozenTime ||
                                enemy.stun.GetRemainingMs() != durationMs - 100) { ++checkFailures; }
                        }
                        for (int elapsed = 0; elapsed < 4000; elapsed += 16) { enemy.Update(16); }
                        checkFailures += static_cast<unsigned>(enemy.GetUnsupportedFunctionCount());
                        if (enemy.stun.IsActive()) { ++checkFailures; }
                        attributeReport << enemyIndex << ' ' << enemies[enemyIndex].owner << " flags=" << flags
                            << " before=" << before << " impact=" << immediately << " end=" << unsigned(enemy.GetStateId())
                            << " stun=" << durationMs << " damage=" << enemy.combat.totalDamage << " unsupported=" << enemy.GetUnsupportedFunctionCount() << '\n';
                    }
                }
                // Controller phase is based on remaining time, with no expiry
                // event for CEnemy's original empty callback methods.
                CStunController clock;
                clock.SetStunned(750, 50, 2);
                clock.Update(50);
                if (clock.GetOffset() != -2) { ++checkFailures; }
                clock.Update(50);
                if (clock.GetOffset() != 2) { ++checkFailures; }
                if (!clock.Update(650) || clock.IsActive() || clock.GetOffset() != 0) { ++checkFailures; }
                if (clock.Update(16)) { ++checkFailures; }
                std::printf("[powerup-attribute-check] combinations=228 timing/movement/animation/expiry failures=%u\n", checkFailures);
            }
        }
        session.Restart(startX, startY);
        CombatHit forceProbe;
        const float originalMaximum = vitals.maximum;
        const float originalBrotherMaximum = brother.vitals.maximum;
        for (float maximum : {20.0f, 100.0f, 205.0f}) {
            session.Restart(startX, startY);
            vitals.maximum = maximum;
            vitals.health = maximum;
            vitals.invincible = false;
            if (withBrother) {
                brother.vitals.maximum = maximum;
                brother.vitals.health = maximum;
                brother.vitals.invincible = false;
            }
            CombatHit percentage;
            percentage.ownerType = 1;
            percentage.damage = 10;
            percentage.percentDamage = true;
            percentage.x = scene.playerX;
            percentage.y = scene.playerY;
            scene.Splash(percentage, 200, 360, 0, 0);
            const float expected = std::max(0.0f, maximum * 0.1f * (2 - PlayerArmorMultiplier(player, 0)));
            if (std::abs(vitals.health - (maximum - expected)) > 0.001f) { ++checkFailures; }
            if (withBrother) {
                const float brotherExpected = std::max(0.0f, maximum * 0.1f * (2 - PlayerArmorMultiplier(brotherModel, 0)));
                if (std::abs(brother.vitals.health - (maximum - brotherExpected)) > 0.001f) { ++checkFailures; }
            }
            std::printf("[splash-check] maximum=%.0f percent=10 damage=%.3f brother=%d failures=%u\n",
                maximum, maximum - vitals.health, withBrother, checkFailures);
        }
        vitals.maximum = originalMaximum;
        brother.vitals.maximum = originalBrotherMaximum;
        session.Restart(startX, startY);
        forceProbe.ownerType = 1;
        forceProbe.x = startX - 40;
        forceProbe.y = startY;
        scene.Splash(forceProbe, 60, 360, 100, 100);
        if (scene.playerX != startX || scene.playerY != startY) { ++checkFailures; }
        scene.Update(16, 0, 0, false);
        if (std::hypot(scene.playerX - startX, scene.playerY - startY) > 4) { ++checkFailures; }
        std::printf("[survival-check] map force gradual displacement=%.2f failures=%u\n",
            std::hypot(scene.playerX - startX, scene.playerY - startY), checkFailures);
        session.Restart(startX, startY);
        const std::size_t initialActors = scene.AliveCount();
        if (gameContext == nullptr) {
            vitals.health = vitals.maximum * 0.5f;
            scene.AddExperience(progress.GetExperienceDelta());
            if (progress.GetLevel() != 2 || vitals.maximum != 9 || vitals.health != 4.5f) { ++checkFailures; }
            progress.SetExperience(0);
            scene.SetPlayerProgress(&progress);
            vitals.Reset();
        }
        const PlayerModel *stablePlayer = &player;
        const float armorBefore = PlayerArmorMultiplier(player, 0);
        const std::size_t alternateWeapon = (weaponSlot + 1) % weapons.size();
        if (!EquipControlledPlayer(tables, loaded, program, weapons[alternateWeapon]) ||
            !EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot])) { return 1; }
        if (loaded.players[0].model.get() != stablePlayer || player.vitals != &vitals ||
            PlayerArmorMultiplier(player, 0) != armorBefore) { ++checkFailures; }
        // Kill through the actual shared hit path, then exercise the same R action.
        CombatHit fatal;
        fatal.ownerType = 1;
        fatal.damage = 10000;
        scene.ApplyHit(kPlayerCombatId, fatal);
        if (!vitals.dead || vitals.health != 0 || vitals.deaths != 1) { ++checkFailures; }
        session.Restart(startX, startY);
        if (vitals.dead || vitals.health != vitals.maximum || session.GetLevel().GetWave() != static_cast<int>(startWave) ||
            scene.AliveCount() != initialActors || effects.GetBulletCount() != 0 ||
            PlayerArmorMultiplier(player, 0) != armorBefore || scene.playerX != startX || scene.playerY != startY) { ++checkFailures; }
        std::printf("[survival-check] equipment/death/restart failures=%u\n", checkFailures);
        if (withBrother) {
            // A fatal shared hit must not kill the human. Run the original
            // death animation to its hold state, then the actual wave export.
            scene.ApplyHit(kBrotherCombatId, fatal);
            if (!brother.vitals.dead || vitals.dead) { ++checkFailures; }
            for (int elapsed = 0; elapsed < 8000; elapsed += 16) { AdvancePlayer(brotherModel, 16); }
            const int deadState = brotherModel.weapon->brother.GetStateId();
            brotherModel.weapon->brother.OnWaveCleared();
            for (int elapsed = 0; elapsed < 8000; elapsed += 16) { AdvancePlayer(brotherModel, 16); }
            if (brother.vitals.dead || brother.vitals.health != brother.vitals.maximum ||
                !brotherModel.weapon->brother.CanMove() || !brotherModel.weapon->brother.CanShoot()) { ++checkFailures; }
            std::printf("[brother-check] death-state=%d revived-state=%d health=%.1f failures=%u\n",
                deadState, brotherModel.weapon->brother.GetStateId(), brother.vitals.health, checkFailures);
            session.Restart(startX, startY);
            brother.vitals.invincible = true;
        }
        // This harness uses real projectiles and enemy death scripts, with
        // invincibility only to keep the automated pilot running deterministically.
        vitals.invincible = true;
        const int targetWave = std::min(static_cast<int>(startWave + checkWaves), session.GetLevel().GetWaveLimit());
        SurvivalPilot pilot(scene, loaded.map.GetVisibleBounds());
        float previousDamage = 0;
        int stalledMs = 0;
        // Late waves contain hundreds of actors. Bound a wave generously, but
        // stop promptly when actual damage and kills cease for two minutes.
        for (int elapsed = 0; elapsed < static_cast<int>(checkWaves * 600000); elapsed += 16) {
            float moveX = 0;
            float moveY = 0;
            pilot.Update(16, moveX, moveY);
            session.Update(16, moveX, moveY, !withBrother || gameContext != nullptr);
            float damage = scene.damageDealt;
            for (const auto &actor : scene.enemies) { damage += actor->model.enemy.combat.totalDamage; }
            stalledMs += 16;
            if (damage != previousDamage) { stalledMs = 0; previousDamage = damage; }
            if (stalledMs > 120000) {
                std::printf("[survival-check] stopped: no damage progress for 120 seconds\n");
                break;
            }
            if (session.GetLevel().GetWave() >= targetWave) { break; }
        }
        pilot.Report();
        checkFailures += pickups.failures;
        std::printf("[pickup-check] spawned=%u collected=%u remaining=%zu failures=%u\n",
            pickups.spawned, pickups.collected, pickups.GetCount(), pickups.failures);
        if (withBrother) {
            // AI-only research must acquire targets. In a profile run the
            // human's long-range gun may kill everything before the 200px AI scan.
            if (gameContext == nullptr && brother.GetTargetCount() == 0) { ++checkFailures; }
            std::printf("[brother-check] targets=%u shots=%zu position=%.1f,%.1f hp=%.1f failures=%u\n",
                brother.GetTargetCount(), effects.GetShotCount(), brother.x, brother.y, brother.vitals.health, checkFailures);
        }
        if (session.GetLevel().GetWave() < targetWave || session.GetKills() == 0 || scene.invalidSpawns != 0) { ++checkFailures; }
        if (targetWave == session.GetLevel().GetWaveLimit() && !session.GetLevel().IsCleared()) { ++checkFailures; }
        if (scene.GetClearedWaves() != targetWave - startWave) { ++checkFailures; }
        // AI-only kills grant XP but not the human's Xplodium kill streak.
        // CLevel::OnEnemyKilled :119566 tests GetBrotherType, not IsPlayer.
        if (progress.GetExperience() == 0 || progress.GetLevel() < 2 || (!withBrother && scene.GetXplodium() == 0) ||
            vitals.maximum != progress.GetHealth()) { ++checkFailures; }
        std::printf("[survival-check] progress level=%u xp=%llu xplodium=%llu\n",
            progress.GetLevel(), progress.GetExperience(), scene.GetXplodium());
        checkFailures += props.GetFailures();
        std::printf("[prop-check] actual-hits=%u failures=%u\n", props.GetHitCount(), props.GetFailures());
        std::printf("[survival-check] wave=%d kills=%u alive=%d spawned=%u invalid=%u failures=%u\n",
            session.GetLevel().GetWave(), session.GetKills(), session.CountEnemies(), scene.spawned, scene.invalidSpawns, checkFailures);
        std::printf("[survival-check] shots=%zu player=%.1f,%.1f stun=%d brother=%d\n",
            effects.GetShotCount(), scene.playerX, scene.playerY, vitals.stunMs, player.weapon->brother.GetStateId());
        for (const auto &actor : scene.enemies) {
            const EnemyCombat &enemy = actor->model.enemy.combat;
            if (!enemy.dead) {
                std::printf("[survival-check] alive %s pos=%.1f,%.1f health=%.1f state=%d behaviour=%d\n",
                    actor->data->owner.c_str(), enemy.x, enemy.y, enemy.health,
                    actor->model.enemy.GetStateId(), enemy.behaviour);
                ILayerPath *path = loaded.map.GetPathLayer(session.GetLevel().GetPathLayer());
                if (path != nullptr) {
                    const int first = path->FindNearest(enemy.x, enemy.y);
                    const int last = path->FindNearest(scene.playerX, scene.playerY);
                    std::printf("[survival-check] navigation=%d to=%.1f,%.1f radius=%.1f path=%d->%d next=%d\n",
                        enemy.hasNavigationTarget, enemy.navigationX, enemy.navigationY, actor->model.enemy.GetPart(0).radius,
                        first, last, path->FindNext(first, last));
                    const int containing = path->FindNode(enemy.x, enemy.y);
                    const int goal = path->FindNode(scene.playerX, scene.playerY);
                    const int next = path->FindNext(containing, goal);
                    if (next >= 0) {
                        const auto &point = path->GetNodes()[next];
                        std::printf("[survival-check] containing=%d goal=%d next=%d center=%.1f,%.1f\n", containing, goal, next, point.x, point.y);
                    }
                    const auto &vertices = loaded.collisionScene.GetVertices();
                    for (const auto &edge : loaded.collisionScene.GetEdges()) {
                        const CollisionPoint &a = vertices[edge.firstVertex];
                        const CollisionPoint &b = vertices[edge.secondVertex];
                        if (std::hypot((a.x + b.x) * 0.5f - enemy.x, (a.y + b.y) * 0.5f - enemy.y) < 85) {
                            std::printf("[survival-check] nearby edge %.1f,%.1f -> %.1f,%.1f\n", a.x, a.y, b.x, b.y);
                        }
                    }
                }
            }
        }
        capturePath = "out/survival-check-" + packShortName + ".png";
    }
    if (check && archiveMission != nullptr) {
        // Research input pilot: visit authored pickups and trigger edges without
        // teleporting actors or directly invoking trigger/death callbacks.
        vitals.invincible = true;
        std::vector<CollisionPoint> goals;
        for (unsigned index = 0; index < loaded.map.GetObjectLayerCount(); ++index) {
            const CLayerObject &layer = loaded.map.GetObjectLayer(index);
            if (static_cast<int>(layer.GetLayerIndex()) != session.GetLevel().GetObjectLayer()) { continue; }
            for (const PlacedObject &object : layer.GetObjects()) {
                if (object.objectType == static_cast<unsigned>(PlacedObjectType::Pickup)) { goals.emplace_back(object.x, object.y); }
            }
        }
        for (unsigned index = 0; index < loaded.map.GetCollisionLayerCount(); ++index) {
            const CLayerCollision &layer = loaded.map.GetCollisionLayer(index);
            if (static_cast<int>(layer.GetLayerIndex()) != session.GetLevel().GetTriggerLayer()) { continue; }
            for (const CollisionEdge &edge : layer.GetCollision().GetEdges()) {
                const auto &a = layer.GetCollision().GetVertices()[edge.firstVertex];
                const auto &b = layer.GetCollision().GetVertices()[edge.secondVertex];
                const float length = std::hypot(b.x - a.x, b.y - a.y);
                if (length < 1) { continue; }
                const float normalX = (a.y - b.y) / length * 40;
                const float normalY = (b.x - a.x) / length * 40;
                goals.emplace_back((a.x + b.x) * 0.5f + normalX, (a.y + b.y) * 0.5f + normalY);
                goals.emplace_back((a.x + b.x) * 0.5f - normalX, (a.y + b.y) * 0.5f - normalY);
            }
        }
        SurvivalPilot pilot(scene, loaded.map.GetVisibleBounds());
        unsigned goal = 0, reached = 0;
        int goalElapsed = 0;
        for (int elapsed = 0; elapsed < 360000 && !session.GetLevel().IsCleared(); elapsed += 16) {
            float moveX = 0, moveY = 0;
            pilot.Update(16, moveX, moveY);
            if (goal < goals.size()) {
                goalElapsed += 16;
                const CollisionPoint &target = goals[goal];
                float waypointX = target.x, waypointY = target.y;
                scene.GetBrotherWaypoint(scene.playerX, scene.playerY, target.x, target.y, waypointX, waypointY);
                moveX = waypointX - scene.playerX;
                moveY = waypointY - scene.playerY;
                if (std::hypot(target.x - scene.playerX, target.y - scene.playerY) < 20 || goalElapsed > 20000) {
                    const bool arrived = goalElapsed <= 20000;
                    if (arrived) { ++reached; }
                    std::printf("[campaign-check] goal=%u target=%.0f,%.0f reached=%d player=%.1f,%.1f\n",
                        goal, target.x, target.y, arrived, scene.playerX, scene.playerY);
                    ++goal;
                    goalElapsed = 0;
                }
            }
            session.Update(16, moveX, moveY, true);
            AdvanceProps(loaded.props, 16);
            AdvanceTileLayers(loaded.map, 16);
        }
        if (scene.spawned == 0 || session.GetKills() == 0 || scene.invalidSpawns != 0) { ++checkFailures; }
        std::printf("[campaign-check] goals=%u/%zu spawned=%u kills=%u pickups=%u cleared=%d failures=%u\n",
            reached, goals.size(), scene.spawned, session.GetKills(), pickups.collected, session.GetLevel().IsCleared(), checkFailures);
        capturePath = "out/campaign-check-" + packShortName + "-" + std::to_string(mapIndex) + ".png";
    }
    for (unsigned elapsed = 0; elapsed < advanceMs; elapsed += 16) {
        session.Update(16, 0, 0, firePreview);
        AdvanceProps(loaded.props, 16);
        AdvanceTileLayers(loaded.map, 16);
    }
    if (powerupStudy && !capturePath.empty()) {
        powerups.Select(5);
        powerups.Use();
        powerups.Select(16);
        powerups.Use();
        for (int elapsed = 0; elapsed < 256; elapsed += 16) {
            scene.Update(16, 0, 0, false);
            powerups.Update(16);
        }
        powerups.Select(19);
        powerups.Use();
        for (int elapsed = 0; elapsed < 1400; elapsed += 16) {
            scene.Update(16, 0, 0, false);
            powerups.Update(16);
        }
        capturePath = "out/powerup-play-check.png";
    }
    bool paused = false;
    int accumulator = 0;
    std::uint64_t previous = window.GetTicksMs();
    Camera camera;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    std::printf("[survival] WASD move, mouse aim/fire, R restart, space pause, 1-7/N/M weapon, C collision\n");
    while (window.PumpEvents()) {
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None; key = window.TakeKeyPress()) {
            if (key == KeyCode::Space) {
                if (!session.GetDialogText().empty()) { session.CompleteDialog(); }
                else { paused = !paused; }
            }
            if (key == KeyCode::C) { showCollisions = !showCollisions; }
            if (key == KeyCode::G && !paused && !session.IsTransitioning()) { powerups.Use(); }
            if (key == KeyCode::F) { powerups.Cycle(); }
            if (key == KeyCode::R) {
                if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel().GetWave(), accountedXplodium)) { return 1; }
                session.Restart(startX, startY);
                savedDeath = false;
                paused = false;
            }
            std::size_t nextWeapon = weaponSlot;
            if (gameContext == nullptr) { nextWeapon = SelectWeaponKey(weapons, weaponSlot, key); }
            else if (key == KeyCode::Digit1 || key == KeyCode::Digit2 || key == KeyCode::N || key == KeyCode::M) {
                equippedWeaponSlot = 1 - equippedWeaponSlot;
                if (key == KeyCode::Digit1) { equippedWeaponSlot = 0; }
                if (key == KeyCode::Digit2) { equippedWeaponSlot = 1; }
                const GameObjectRef &ref = gameContext->profile.configuration.guns[equippedWeaponSlot];
                for (std::size_t index = 0; index < weapons.size(); ++index) {
                    if (weapons[index].packHash == ref.packHash && weapons[index].ordinal == ref.localIndex) { nextWeapon = index; break; }
                }
            }
            if (nextWeapon != weaponSlot && !vitals.dead) {
                if (!EquipControlledPlayer(tables, loaded, program, weapons[nextWeapon])) { return 1; }
                // In-flight bullets and timed powerup effects belong to the
                // actor world. Only the old gun's beam/loop sound ends here.
                effects.RetireOwner(kPlayerCombatId);
                weaponSlot = nextWeapon;
            }
        }
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        loaded.players[0].x = scene.playerX;
        loaded.players[0].y = scene.playerY;
        const float baselineZoom = GameViewCameraZoom(width, height);
        camera.zoom = baselineZoom * loaded.map.GetCamera().GetScale() / kLevelCameraScale;
        session.SetViewSize(width / baselineZoom, height / baselineZoom);
        FollowPlayerCamera(loaded, width, height, camera);
        float mouseX = 0, mouseY = 0;
        if (capturePath.empty() && window.GetMousePosition(mouseX, mouseY) && !vitals.dead) {
            scene.facing = std::atan2(camera.y + mouseY / camera.zoom - scene.playerY,
                camera.x + mouseX / camera.zoom - scene.playerX) * kRadiansToDegrees + 90;
        }
        float moveX = 0, moveY = 0;
        if (window.IsKeyDown(KeyCode::A)) { --moveX; }
        if (window.IsKeyDown(KeyCode::D)) { ++moveX; }
        if (window.IsKeyDown(KeyCode::W)) { --moveY; }
        if (window.IsKeyDown(KeyCode::S)) { ++moveY; }
        const std::uint64_t now = window.GetTicksMs();
        if (!paused && capturePath.empty()) { accumulator += static_cast<int>(std::min<std::uint64_t>(now - previous, 100)); }
        previous = now;
        effects.SetPaused(paused);
        while (accumulator >= 16) {
            if (!vitals.dead) { session.Update(16, moveX, moveY, firePreview || window.IsLeftMouseDown()); }
            else { scene.Update(16, 0, 0, false); }
            AdvanceProps(loaded.props, 16);
            AdvanceTileLayers(loaded.map, 16);
            accumulator -= 16;
        }
        if (session.GetLevel().GetWave() != lastSavedWave || (vitals.dead && !savedDeath)) {
            if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel().GetWave(), accountedXplodium)) { return 1; }
            lastSavedWave = session.GetLevel().GetWave();
            savedDeath = vitals.dead;
        }
        loaded.players[0].x = scene.playerX;
        loaded.players[0].y = scene.playerY;
        loaded.players[0].facingDegrees = scene.facing;
        camera.zoom = baselineZoom * loaded.map.GetCamera().GetScale() / kLevelCameraScale;
        FollowPlayerCamera(loaded, width, height, camera);
        BuildGeometry(loaded, batch, true, true, false);
        glViewport(0, 0, width, height);
        glClearColor(0.04f, 0.05f, 0.07f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        float mvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(width / camera.zoom, height / camera.zoom, kMapDepthRange, mvp);
        Matrix4dTranslate(mvp, -camera.x, -camera.y);
        batch.Draw(program, mvp);
        pickups.Draw(mvp, kLevelCameraScale);
        effects.Draw(mvp, nullptr, kLevelCameraScale, WeaponDrawPass::BehindPlayer);
        DrawModels(loaded, program, mvp);
        if (withBrother) {
            float world[kMatrix4dElements], modelMvp[kMatrix4dElements];
            scene.BrotherMatrix(world);
            Matrix4dMultiply(mvp, world, modelMvp);
            DrawPlayer(brotherModel, program, modelMvp);
        }
        glEnable(GL_DEPTH_TEST);
        for (auto &actor : scene.enemies) {
            float world[kMatrix4dElements], modelMvp[kMatrix4dElements];
            scene.EnemyMatrix(*actor, world);
            Matrix4dMultiply(mvp, world, modelMvp);
            // Original stun shake is a screen-pixel draw offset, never collision motion.
            modelMvp[3] += 2.0f * actor->model.enemy.stun.GetOffset() / width;
            DrawEnemyModel(actor->model, program, modelMvp);
        }
        glDisable(GL_DEPTH_TEST);
        effects.Draw(mvp, nullptr, kLevelCameraScale, WeaponDrawPass::InFrontOfPlayer);
        if (check) {
            GLint sourceBlend = 0, destinationBlend = 0;
            glGetIntegerv(GL_BLEND_SRC, &sourceBlend);
            glGetIntegerv(GL_BLEND_DST, &destinationBlend);
            if (sourceBlend != GL_SRC_ALPHA || destinationBlend != GL_ONE_MINUS_SRC_ALPHA) { ++checkFailures; }
            std::printf("[render-check] after-particles blend=%x/%x failures=%u\n", sourceBlend, destinationBlend, checkFailures);
        }
        if (showCollisions) {
            BuildCollisionMarkers(loaded, markers);
            markers.Draw(markerProgram, mvp, 0.15f, 0.85f, 1, 0.8f);
        }
        float hud[kMatrix4dElements];
        Matrix4dOrthoTopLeft(800, 600, 100, hud);
        markers.Begin();
        markers.AddRect(0, 0, 800, 67);
        markers.AddRect(0, 67, 800, 28);
        markers.AddRect(0, 565, 800, 35);
        markers.Draw(markerProgram, hud, 0.015f, 0.025f, 0.04f, 0.88f);
        char line[192];
        const int displayWave = std::min(session.GetLevel().GetWave(), session.GetLevel().GetWaveLimit() - 1);
        std::snprintf(line, sizeof(line), "REVOLUTION %d/10    WAVE %d/50    ENEMIES %d",
            displayWave / 50 + 1, displayWave % 50 + 1, session.CountEnemies());
        if (archiveMission != nullptr) {
            std::snprintf(line, sizeof(line), "ARCHIVE  %s  ENEMIES %d", archiveMission->title.c_str(), session.CountEnemies());
        }
        markers.Begin();
        DrawHudText(markers, 20, 12, line, 2);
        std::snprintf(line, sizeof(line), "HP %.0f/%.0f  KILLS %u", vitals.health, vitals.maximum, session.GetKills());
        DrawHudText(markers, 20, 37, line, 2);
        if (withBrother) {
            std::snprintf(line, sizeof(line), "BRO %.0f/%.0f", brother.vitals.health, brother.vitals.maximum);
            DrawHudText(markers, 260, 40, line, 1.5f);
        }
        std::snprintf(line, sizeof(line), "LEVEL %u  XP %llu/%u    XPLODIUM %llu",
            progress.GetLevel(), progress.GetExperienceInLevel(), progress.GetExperienceDelta(), scene.GetXplodium());
        DrawHudText(markers, 20, 73, line, 1.7f);
        DrawHudText(markers, 20, 577, "WASD MOVE  MOUSE FIRE  1/2 GUN  G ITEM  F NEXT  R RETRY  SPACE PAUSE  ESC MENU", 1.25f);
        const char *buffNames[] = {"SHIELD", "ATTACK", "DEFENSE", "SPEED", "AUTO AIM"};
        const int buffTimers[] = {player.powerups.shieldMs, player.powerups.frenzyMs[0],
            player.powerups.frenzyMs[1], player.powerups.frenzyMs[2], player.powerups.autoFireMs};
        std::string activePowerups;
        for (unsigned index = 0; index < 5; ++index) {
            if (buffTimers[index] <= 0) { continue; }
            if (!activePowerups.empty()) { activePowerups += "   "; }
            activePowerups += buffNames[index];
            activePowerups += " " + std::to_string((buffTimers[index] + 999) / 1000) + "S";
        }
        if (player.weapon->brother.IsTurretActive()) { activePowerups += "   TURRET ACTIVE"; }
        if (!activePowerups.empty()) { DrawHudText(markers, 20, 520, activePowerups.c_str(), 1.3f); }
        if (powerups.GetSelected() != nullptr) {
            std::snprintf(line, sizeof(line), "G: %s  X%u", powerups.GetSelected()->name.c_str(), powerups.GetCount());
            DrawHudText(markers, 20, 548, line, 1.7f);
        }
        markers.Draw(markerProgram, hud, 0.85f, 0.92f, 1, 1);
        markers.Begin();
        markers.AddRect(400, 39, 360 * std::clamp(vitals.health / vitals.maximum, 0.0f, 1.0f), 12);
        markers.Draw(markerProgram, hud, 0.25f, 0.9f, 0.45f, 1);
        if (paused || session.GetLevel().IsCleared() || vitals.dead) {
            markers.Begin();
            markers.AddRect(170, 215, 480, 105);
            markers.Draw(markerProgram, hud, 0.015f, 0.025f, 0.04f, 0.9f);
        }
        markers.Begin();
        if (session.IsTransitioning()) { DrawHudText(markers, 230, 230, "GET READY", 6); }
        if (session.IsTransitioning() && scene.GetLastWaveBonus() > 0) {
            std::snprintf(line, sizeof(line), "PERFECT WAVE  +%llu XPLODIUM", scene.GetLastWaveBonus());
            DrawHudText(markers, 175, 285, line, 2.5f);
        }
        if (paused) { DrawHudText(markers, 290, 260, "PAUSED", 5); }
        if (withBrother && brother.vitals.dead && !vitals.dead) {
            DrawHudText(markers, 170, 533, "BRO RETURNS AFTER THIS WAVE", 2.5f);
        }
        if (session.GetLevel().IsCleared()) {
            if (archiveMission != nullptr) { DrawHudText(markers, 200, 235, "MISSION COMPLETE", 4); }
            else {
                DrawHudText(markers, 200, 235, "SURVIVAL COMPLETE", 4);
                DrawHudText(markers, 255, 280, "500 WAVES CLEARED", 3);
            }
        }
        if (vitals.dead) {
            DrawHudText(markers, 230, 235, "MISSION FAILED", 4);
            DrawHudText(markers, 250, 280, "PRESS R TO RETRY", 3);
        }
        markers.Draw(markerProgram, hud, 1, 0.75f, 0.25f, 1);
        if (!session.GetDialogText().empty()) {
            markers.Begin();
            markers.AddRect(25, 405, 750, 132);
            markers.Draw(markerProgram, hud, 0.02f, 0.03f, 0.05f, 0.95f);
            markers.Begin();
            std::istringstream words(session.GetDialogText());
            std::string word, textLine;
            float textY = 417;
            while (words >> word) {
                if (textLine.size() + word.size() > 70) {
                    DrawHudText(markers, 40, textY, textLine.c_str(), 1.65f);
                    textY += 17;
                    textLine.clear();
                }
                if (!textLine.empty()) { textLine += ' '; }
                textLine += word;
            }
            if (!textLine.empty()) { DrawHudText(markers, 40, textY, textLine.c_str(), 1.65f); }
            DrawHudText(markers, 40, 518, "SPACE: CONTINUE", 1.5f);
            markers.Draw(markerProgram, hud, 0.9f, 0.95f, 1, 1);
        }
        if (!capturePath.empty()) {
            if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel().GetWave(), accountedXplodium)) { return 1; }
            const unsigned errors = glGetError();
            if (errors != 0 || !window.SaveFrame(capturePath)) { return 1; }
            std::printf("[survival] wave=%d alive=%d spawned=%u kills=%u hp=%.1f\n",
                session.GetLevel().GetWave(), session.CountEnemies(), scene.spawned, session.GetKills(), vitals.health);
            window.Present();
            if (checkFailures != 0) { return 1; }
            return 0;
        }
        window.Present();
    }
    if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel().GetWave(), accountedXplodium)) { return 1; }
    return 0;
}

int RunM3Map(const std::string &bigDirectory, const std::string &packShortName,
             std::uint32_t mapIndex, const std::string &screenshotPath,
             std::uint32_t advanceMs, bool startWithSpawns,
             bool startWithCollisions, MapViewMode viewMode, std::uint32_t weaponIndex,
             bool firePreview) {
    const bool gameView = viewMode == MapViewMode::GameView;
    const char *modeName = "Preview";
    if (gameView) {
        modeName = "GameView";
    }
    std::printf("=== M3: %s ===\n\n", modeName);

    // --- resources, before any GL exists ---
    CResTOCManager tocManager;
    if (!tocManager.Init(bigDirectory, kArtSetXga) || !tocManager.Bind()) {
        return 1;
    }

    std::vector<CatalogMap> catalog = BuildCatalog(tocManager);
    if (catalog.empty()) {
        std::printf("[m3] no pack contains any maps\n");
        return 1;
    }

    // --map only picks where to start now; the whole catalogue is reachable
    // from the keyboard.
    std::size_t slot = 0;
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        if (catalog[i].packName != packShortName) {
            continue;
        }
        slot = i;
        if (catalog[i].mapIndex == mapIndex) {
            break;  // exact match; otherwise the pack's first map stands
        }
    }

    std::printf("[m3] %zu maps across the archives:", catalog.size());
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        if (i == 0 || catalog[i].packIndex != catalog[i - 1].packIndex) {
            std::printf(" %s(", catalog[i].packName.c_str());
        }
        std::printf("%u", catalog[i].mapIndex);
        const bool lastOfPack = (i + 1 == catalog.size()) ||
                                (catalog[i + 1].packIndex != catalog[i].packIndex);
        std::printf("%s", lastOfPack ? ")" : ",");
    }
    std::printf("\n");

    // --- window, then everything that needs a context ---
    CWindow window;
    std::string windowTitle = "gun_bros_re -- ";
    windowTitle += modeName;
    if (!window.Open(windowTitle, kDefaultWindowWidth, kDefaultWindowHeight)) {
        return 1;
    }

    CShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) {
        return 1;
    }

    CQuadBatch batch;
    // The constant-colour pair, for the spawn markers. Two programs, because
    // the shaders the engine ships are one job each.
    CShaderProgram markerProgram;
    if (!markerProgram.Load(kShaderDirectory, "ogles_vs_mvp_constcolor",
                            "ogles_ps_constcolor")) {
        return 1;
    }

    CMarkerBatch markers;
    if (!markers.Create(markerProgram)) {
        return 1;
    }

    if (!batch.Create(program)) {
        return 1;
    }
    CAudioPlayer audio;
    PackTables weaponTables(tocManager);
    std::vector<WeaponEntry> weapons;
    std::size_t weaponSlot = weaponIndex;
    std::unique_ptr<WeaponEffects> weaponEffects;
    if (gameView) {
        if (!LoadWeaponCatalog(tocManager, weaponTables, weapons)) { return 1; }
        if (weaponSlot >= weapons.size()) { weaponSlot = 0; }
        weaponEffects.reset(new WeaponEffects(tocManager, weaponTables, program));
        window.SetTitle("GameView | " + WeaponSelectionLabel(weapons, weaponSlot));
    }

    int drawableWidth = 0;
    int drawableHeight = 0;
    window.GetDrawableSize(drawableWidth, drawableHeight);

    std::printf("\n[m3] --- %s map %u (%zu of %zu) ---\n",
                catalog[slot].packName.c_str(), catalog[slot].mapIndex, slot + 1,
                catalog.size());

    LoadedMap loaded;
    if (!LoadMap(tocManager, catalog[slot].packIndex, catalog[slot].mapIndex,
                 loaded)) {
        return 1;
    }
    LoadProps(tocManager, loaded);
    BuildCollisionScene(loaded);
    LoadPlacedEnemies(tocManager, program, loaded);
    LoadPlacedPlayers(tocManager, program, loaded);
    if (gameView && !EquipControlledPlayer(weaponTables, loaded, program, weapons[weaponSlot])) { return 1; }
    ReportSpawns(loaded);
    WarmUp(loaded, advanceMs, weaponEffects.get(), firePreview);

    // Either layer can be hidden, which is how "is that rock in the right\n// place or is the ground wrong?" gets answered without a debugger.
    bool showTiles = true;
    bool showProps = true;
    bool showSpawns = startWithSpawns;
    bool showCollisions = startWithCollisions;
    CoverState coverState = CoverState::Intact;
    std::uint8_t barrelState = 0;
    std::uint8_t spireState = 0;

    // The props animate, so the geometry is rebuilt every frame from here on.
    // This flag only decides whether a rebuild says anything about itself.
    bool reportGeometry = true;
    Camera camera = FitCamera(loaded, drawableWidth, drawableHeight);
    bool followPlayer = gameView && !loaded.players.empty();
    if (followPlayer) {
        camera.zoom = GameViewCameraZoom(drawableWidth, drawableHeight);
        FollowPlayerCamera(loaded, drawableWidth, drawableHeight, camera);
    }

    // Time, and the two ways of taking it apart when something looks wrong:
    // stop it, or move it on one bite at a time.
    std::uint64_t previousTicks = window.GetTicksMs();
    bool paused = false;
    bool singleStep = false;

    // The factors themselves are the batch's business now: glows and fire are
    // additive, everything else is straight alpha.
    glEnable(GL_BLEND);

    if (gameView) {
        std::printf("\n[gameview] WASD: move, arrows: change map, "
                    "1-7: weapon category, N/M: weapon, mouse: aim, left mouse: fire, "
                    "T: tiles, P: props, K: spawns, B: covers, E: barrels, "
                    "F: spires, C: collision, "
                    "space: pause, '.': one step, Esc: quit\n");
    } else {
        std::printf("\n[preview] arrows: change map, Home: refit, "
                    "drag: pan, wheel: zoom, T: tiles, P: props, "
                    "K: spawns, B: covers, E: barrels, S/F: spires, "
                    "C: collision, space: pause, "
                    "'.': one step, Esc: quit\n");
    }

    bool reportedFirstFrame = false;
    while (window.PumpEvents()) {
        window.GetDrawableSize(drawableWidth, drawableHeight);

        // --- switching maps and packs ---
        bool reload = false;
        bool refit = false;
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None;
             key = window.TakeKeyPress()) {
            if (gameView) {
                const std::size_t nextWeapon = SelectWeaponKey(weapons, weaponSlot, key);
                if (nextWeapon != weaponSlot && EquipControlledPlayer(weaponTables, loaded, program, weapons[nextWeapon])) {
                    weaponEffects->Clear();
                    weaponSlot = nextWeapon;
                    window.SetTitle("GameView | " + WeaponSelectionLabel(weapons, weaponSlot));
                    std::printf("[weapon] %s\n", WeaponSelectionLabel(weapons, weaponSlot).c_str());
                }
            }
            if (key == KeyCode::T) {
                showTiles = !showTiles;
                reportGeometry = true;
            } else if (key == KeyCode::B) {
                coverState = NextCoverState(coverState);
                const std::uint32_t changed = SetCoverState(loaded, coverState);
                StartTransitionParticles(
                    tocManager, loaded, InteractivePropKind::Cover,
                    static_cast<std::uint8_t>(coverState));
                PlayTransitionSound(tocManager, loaded, audio,
                                    InteractivePropKind::Cover,
                                    static_cast<std::uint8_t>(coverState));
                BuildCollisionScene(loaded);
                reportGeometry = true;
                std::printf("[m4] %u covers: %s\n", changed,
                            CoverStateName(coverState));
            } else if (key == KeyCode::E) {
                barrelState = static_cast<std::uint8_t>((barrelState + 1) % 4);
                const std::uint32_t changed = SetInteractiveState(
                    loaded, InteractivePropKind::Barrel, barrelState);
                StartTransitionParticles(tocManager, loaded,
                                         InteractivePropKind::Barrel,
                                         barrelState);
                PlayTransitionSound(tocManager, loaded, audio,
                                    InteractivePropKind::Barrel, barrelState);
                BuildCollisionScene(loaded);
                reportGeometry = true;
                std::printf("[m4] %u barrels: %s\n", changed,
                            InteractiveStateName(InteractivePropKind::Barrel,
                                                 barrelState));
            } else if (key == KeyCode::F ||
                       (key == KeyCode::S && !gameView)) {
                spireState = static_cast<std::uint8_t>((spireState + 1) % 3);
                const std::uint32_t changed = SetInteractiveState(
                    loaded, InteractivePropKind::Spire, spireState);
                StartTransitionParticles(tocManager, loaded,
                                         InteractivePropKind::Spire,
                                         spireState);
                PlayTransitionSound(tocManager, loaded, audio,
                                    InteractivePropKind::Spire, spireState);
                reportGeometry = true;
                std::printf("[m4] %u spires: %s\n", changed,
                            InteractiveStateName(InteractivePropKind::Spire,
                                                 spireState));
            } else if (key == KeyCode::C) {
                showCollisions = !showCollisions;
                std::printf("[m4] collision %s\n",
                            showCollisions ? "on" : "off");
            } else if (key == KeyCode::K) {
                showSpawns = !showSpawns;
                std::printf("[m3] spawns %s\n", showSpawns ? "on" : "off");
            } else if (key == KeyCode::P) {
                showProps = !showProps;
                reportGeometry = true;
            } else if (key == KeyCode::Space) {
                paused = !paused;
                std::printf("[m3] %s\n", paused ? "paused" : "running");
            } else if (key == KeyCode::Period) {
                // Stepping implies pausing: otherwise the step is lost in the
                // real time that keeps flowing around it.
                paused = true;
                singleStep = true;
            } else if (key == KeyCode::Right) {
                // One step through the flat list, so the last map of a pack is
                // followed by the first map of the next.
                slot = (slot + 1) % catalog.size();
                reload = true;
            } else if (key == KeyCode::Left) {
                slot = (slot + catalog.size() - 1) % catalog.size();
                reload = true;
            } else if (key == KeyCode::Down) {
                slot = NextPackSlot(catalog, slot);
                reload = true;
            } else if (key == KeyCode::Up) {
                slot = PreviousPackSlot(catalog, slot);
                reload = true;
            } else if (key == KeyCode::Home && !gameView) {
                refit = true;
            }
        }

        if (reload) {
            std::printf("\n[m3] --- %s map %u (%zu of %zu) ---\n",
                        catalog[slot].packName.c_str(), catalog[slot].mapIndex,
                        slot + 1, catalog.size());

            // Replace wholesale: the old textures, including every atlas page
            // the props point at, go with the old LoadedMap.
            LoadedMap replacement;
            if (LoadMap(tocManager, catalog[slot].packIndex, catalog[slot].mapIndex,
                        replacement)) {
                LoadProps(tocManager, replacement);
                coverState = CoverState::Intact;
                barrelState = 0;
                spireState = 0;
                BuildCollisionScene(replacement);
                LoadPlacedEnemies(tocManager, program, replacement);
                LoadPlacedPlayers(tocManager, program, replacement);
                if (gameView) {
                    if (!EquipControlledPlayer(weaponTables, replacement, program, weapons[weaponSlot])) { return 1; }
                    weaponEffects->Clear();
                }
                ReportSpawns(replacement);
                WarmUp(replacement, advanceMs, weaponEffects.get(), firePreview);
                loaded = std::move(replacement);
                reportGeometry = true;
                followPlayer = gameView && !loaded.players.empty();
                if (followPlayer) {
                    camera.zoom = GameViewCameraZoom(
                        drawableWidth, drawableHeight);
                    FollowPlayerCamera(loaded, drawableWidth, drawableHeight,
                                       camera);
                    refit = false;
                } else {
                    refit = true;
                }
            } else {
                // A map that will not load leaves the previous one on screen
                // rather than a blank window.
                std::printf("[m3] staying on the previous map\n");
            }
        }
        if (refit) {
            camera = FitCamera(loaded, drawableWidth, drawableHeight);
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
            elapsedMs = kSingleStepMs;
            singleStep = false;
        }

        if (gameView) {
            UpdateControlledPlayer(loaded, window, elapsedMs);
            if (!loaded.players.empty()) {
                PlacedPlayer &player = loaded.players[0];
                float mouseX = 0, mouseY = 0;
                if (elapsedMs > 0 && window.GetMousePosition(mouseX, mouseY) && screenshotPath.empty()) {
                    const float aimX = camera.x + mouseX / camera.zoom - player.x;
                    const float aimY = camera.y + mouseY / camera.zoom - player.y;
                    if (aimX != 0 || aimY != 0) {
                        player.facingDegrees = std::atan2(aimY, aimX) * kRadiansToDegrees + 90.0f;
                    }
                }
                SetPlayerInput(*player.model, player.moving, firePreview || window.IsLeftMouseDown());
                weaponEffects->SetPaused(elapsedMs == 0);
            }
        }
        AdvanceProps(loaded.props, static_cast<std::uint16_t>(elapsedMs));
        AdvanceParticleEffects(loaded, static_cast<std::uint16_t>(elapsedMs));
        AdvanceEnemies(loaded, static_cast<std::int32_t>(elapsedMs));
        AdvancePlayers(loaded, static_cast<std::int32_t>(elapsedMs));
        if (gameView && !loaded.players.empty()) {
            PlacedPlayer &player = loaded.players[0];
            float identity[kMatrix4dElements], modelToWorld[kMatrix4dElements];
            Matrix4dIdentity(identity);
            const float scale = PlayerModelWorldScale(*player.model, loaded.playerTemplate->gameScale, kLevelCameraScale);
            BuildPlayerGameMatrix(identity, player.x, player.y, scale, player.facingDegrees, modelToWorld);
            weaponEffects->Update(*player.model, modelToWorld, player.facingDegrees,
                static_cast<int>(elapsedMs), &loaded.weaponCollision);
        }
        AdvanceTileLayers(loaded.map, static_cast<std::uint16_t>(elapsedMs));
        audio.Update();
        BuildGeometry(loaded, batch, showTiles, showProps, reportGeometry);
        reportGeometry = false;

        if (gameView) {
            // Window resizing changes only pixel scale, never the game field
            // of view. The camera remains entirely owned by GameView.
            camera.zoom = GameViewCameraZoom(drawableWidth,
                                             drawableHeight);
        }

        // --- zoom about the centre of the view ---
        const float wheel = window.TakeWheelDelta();
        if (!gameView && wheel != 0.0f) {
            const float viewWidthBefore = static_cast<float>(drawableWidth) / camera.zoom;
            const float viewHeightBefore = static_cast<float>(drawableHeight) / camera.zoom;

            float factor = 1.0f;
            for (float notch = 0.0f; notch < wheel; notch += 1.0f) {
                factor *= kZoomPerNotch;
            }
            for (float notch = 0.0f; notch > wheel; notch -= 1.0f) {
                factor /= kZoomPerNotch;
            }
            camera.zoom *= factor;
            if (camera.zoom < kMinZoom) {
                camera.zoom = kMinZoom;
            }
            if (camera.zoom > kMaxZoom) {
                camera.zoom = kMaxZoom;
            }

            // Keep whatever was in the middle of the view in the middle.
            camera.x += (viewWidthBefore - static_cast<float>(drawableWidth) / camera.zoom) * 0.5f;
            camera.y += (viewHeightBefore - static_cast<float>(drawableHeight) / camera.zoom) * 0.5f;
        }

        if (followPlayer) {
            FollowPlayerCamera(loaded, drawableWidth, drawableHeight, camera);
        }

        // --- pan ---
        int dragX = 0;
        int dragY = 0;
        window.TakeDragDelta(dragX, dragY);
        if (!gameView) {
            // Dragging right moves the view left, as if pulling the map along.
            // Divided by zoom so a drag tracks the cursor at any scale.
            camera.x -= static_cast<float>(dragX) / camera.zoom * kDragScale;
            camera.y -= static_cast<float>(dragY) / camera.zoom * kDragScale;
        }

        glViewport(0, 0, drawableWidth, drawableHeight);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Zoom is a wider or narrower slice of the world, so it goes into the
        // projection's extent rather than into a separate scale matrix.
        float mvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(static_cast<float>(drawableWidth) / camera.zoom,
                             static_cast<float>(drawableHeight) / camera.zoom,
                             kMapDepthRange, mvp);
        Matrix4dTranslate(mvp, -camera.x, -camera.y);

        int scissor[4];
        const bool clipping = VisibleBoundsScissor(
            loaded, camera, drawableWidth, drawableHeight, scissor);
        if (clipping) {
            // After the clear, so the background still fills the window.
            glEnable(GL_SCISSOR_TEST);
            glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        }

        batch.Draw(program, mvp);
        if (weaponEffects) {
            weaponEffects->Draw(mvp, nullptr, kLevelCameraScale, WeaponDrawPass::BehindPlayer);
        }
        DrawModels(loaded, program, mvp);
        if (weaponEffects) {
            weaponEffects->Draw(mvp, nullptr, kLevelCameraScale, WeaponDrawPass::InFrontOfPlayer);
        }

        if (showSpawns) {
            // On top of the terrain, and outside the prop batch, because a
            // marker is not part of the scene -- it is a note about it.
            BuildMarkers(loaded, markers, PlacedObjectType::Player);
            markers.Draw(markerProgram, mvp, 0.2f, 1.0f, 0.3f, 1.0f);

            BuildMarkers(loaded, markers, PlacedObjectType::Enemy);
            markers.Draw(markerProgram, mvp, 1.0f, 0.25f, 0.2f, 1.0f);
        }

        if (showCollisions) {
            BuildCollisionMarkers(loaded, markers);
            markers.Draw(markerProgram, mvp, 0.15f, 0.85f, 1.0f, 0.9f);
        }

        if (clipping) {
            glDisable(GL_SCISSOR_TEST);
        }

        if (!reportedFirstFrame) {
            GLCheckErrors("first frame");
            reportedFirstFrame = true;

            if (!screenshotPath.empty()) {
                if (gameView) {
                    std::printf("[gameview-camera] zoom=%.4f world=%.1fx%.1f\n", camera.zoom,
                        drawableWidth / camera.zoom, drawableHeight / camera.zoom);
                }
                if (!window.SaveFrame(screenshotPath)) {
                    return 1;
                }
                window.Present();
                break;
            }
        }

        window.Present();
    }

    std::printf("[m3] done\n");
    return 0;
}
