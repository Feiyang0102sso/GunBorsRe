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
#include "engine/CStringToKey.h"
#include "runtime/MapScene.h"
#include "gun_bros/CBGM.h"

#include "runtime/EnemyModel.h"
#include "runtime/SurvivalInputDriver.h"
#include "runtime/PlayerModel.h"
#include "runtime/WeaponCatalog.h"
#include "runtime/ArmorCatalog.h"
#include "runtime/SurvivalSession.h"
#include "runtime/CombatGeometry.h"
#include "runtime/StoreCatalog.h"
#include <sstream>
#include <chrono>
#include "runtime/SurvivalGameContext.h"
#include "runtime/NativeProfile.h"
#include "runtime/LoadingScreen.h"
#include "runtime/HostSettings.h"
#include "runtime/HudText.h"
#include "runtime/SurvivalHud.h"
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
#include <SDL3/SDL_events.h>
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

// Exercise the real SDL event queue and CWindow recognizer, including held S.
bool PushBossCheckKey(CWindow &window, char letter, bool repeat = false, bool checkMovement = false) {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = static_cast<SDL_Keycode>(letter);
    event.key.down = true;
    event.key.repeat = repeat;
    if (!SDL_PushEvent(&event) || !window.PumpEvents()) { return false; }
    if (checkMovement && !window.IsKeyDown(KeyCode::S)) { return false; }
    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    event.key.repeat = false;
    return SDL_PushEvent(&event) && window.PumpEvents();
}

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
    // That fixed order is now only the prop/animation table; the mixed actor
    // and prop main pass is sorted again each frame by DrawMapObjects.
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
    static const PropSlot inactive;
    if (!prop.active) { return &inactive; }
    if (prop.runtime != nullptr) { return RuntimeSlotFor(prop, 0); }
    if (prop.sprite->interactiveKind == InteractivePropKind::None) {
        return &prop.sprite->background;
    }
    if (prop.interactiveState >= prop.sprite->stateCount) {
        return &prop.sprite->background;
    }
    return &prop.sprite->states[prop.interactiveState].background;
}

const PropSlot *MainSlotFor(const PlacedProp &prop) {
    static const PropSlot inactive;
    if (!prop.active) { return &inactive; }
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
    static const PropSlot inactive;
    if (!prop.active) { return &inactive; }
    if (prop.runtime != nullptr) { return RuntimeSlotFor(prop, 2); }
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
    if (!prop.active) { return false; }
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
    // CProp::GetZOrder truncates the world ordinate, not the sprite bounds.
    return static_cast<int>(left.y) < static_cast<int>(right.y);
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
            prop.objectLayer = loaded.map.GetObjectLayer(layerIndex).GetLayerIndex();
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
        m_activeProps.clear();
        for (PlacedProp &prop : m_map.props) { prop.active = false; }
        BuildCollisionScene(m_map);
    }

    void StartLayer(int layer) override {
        // Original OnStart spawns this layer's props. Changing the update
        // layer does not remove objects already added to CLevel's pools.
        unsigned spawned = 0;
        // CLayerObject::OnStart :126250 visits the authored object indices.
        // Rendering sorts m_map.props spatially, so maintain a separate registration order.
        std::vector<PlacedProp *> layerProps;
        for (PlacedProp &prop : m_map.props) {
            if (prop.objectLayer == static_cast<unsigned>(layer)) { layerProps.push_back(&prop); }
        }
        std::sort(layerProps.begin(), layerProps.end(), [](const PlacedProp *first, const PlacedProp *second) {
            return first->objectId < second->objectId;
        });
        for (PlacedProp *instance : layerProps) {
            PlacedProp &prop = *instance;
            if (prop.objectLayer != static_cast<unsigned>(layer) || prop.active) { continue; }
            bool manual = false;
            for (unsigned index = 0; index < m_map.map.GetObjectLayerCount(); ++index) {
                const auto &objects = m_map.map.GetObjectLayer(index);
                if (objects.GetLayerIndex() == prop.objectLayer) {
                    manual = m_level.IsManualSpawnTag(objects.GetObjects()[prop.objectId].spawnTag);
                    break;
                }
            }
            // CLayerObject::OnStart skips instances whose auto-spawn bit was
            // cleared by SetSpawnMode. Native SpawnInstance activates them later.
            if (manual) { continue; }
            prop.active = true;
            m_activeProps.push_back(&prop);
            ++spawned;
            if (prop.runtime == nullptr) { continue; }
            prop.runtime->SetLevelContext(&m_level);
            prop.runtime->Bind(prop.sprite->data, &prop.sprite->durations);
            SyncPlayers(prop);
        }
        BuildCollisionScene(m_map);
        std::printf("[prop] start layer=%d spawned=%u\n", layer, spawned);
    }

    bool Spawn(int layer, int objectId) override {
        for (PlacedProp &prop : m_map.props) {
            if (prop.objectLayer != static_cast<unsigned>(layer) || prop.objectId != objectId) { continue; }
            if (prop.active) { return true; }
            prop.active = true;
            m_activeProps.push_back(&prop);
            if (prop.runtime != nullptr) {
                prop.runtime->SetLevelContext(&m_level);
                prop.runtime->Bind(prop.sprite->data, &prop.sprite->durations);
                SyncPlayers(prop);
            }
            BuildCollisionScene(m_map);
            return true;
        }
        return false;
    }

    void SendMessage(int objectId, int message) override {
        for (PlacedProp &prop : m_map.props) {
            if (!prop.active || prop.objectId != objectId || prop.runtime == nullptr) { continue; }
            const unsigned previous = prop.runtime->GetStateId();
            prop.runtime->HandleMessage(message);
            SyncPlayers(prop);
            if (previous != prop.runtime->GetStateId()) {
                std::printf("[prop] id=%d message=%d state=%u->%u\n", objectId, message, previous, prop.runtime->GetStateId());
            }
            return;
        }
    }

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

    bool GetIndicatorTarget(unsigned key, float &x, float &y) const override {
        if (key == 0 || key > m_activeProps.size()) { return false; }
        const auto &prop = *m_activeProps[key - 1];
        if (!prop.active || (prop.runtime && prop.runtime->IsRemoved())) { return false; }
        // CProp::GetBounds :123561 unions the active animation bounds of its
        // three SpritePlayers. GetOrientation :191317 tracks the rectangle center.
        const PropSlot *slots[] = {BackgroundSlotFor(prop), MainSlotFor(prop), ForegroundSlotFor(prop)};
        float left = 0, top = 0, right = 0, bottom = 0;
        bool hasBounds = false;
        for (const auto *slot : slots) {
            if (!slot) { continue; }
            for (const auto &frame : slot->quadsByStep) {
                for (const auto &quad : frame) {
                    if (!hasBounds) {
                        left = right = quad.offsetX;
                        top = bottom = quad.offsetY;
                        hasBounds = true;
                    }
                    left = std::min(left, float(quad.offsetX));
                    top = std::min(top, float(quad.offsetY));
                    right = std::max(right, float(quad.offsetX + quad.Width()));
                    bottom = std::max(bottom, float(quad.offsetY + quad.Height()));
                }
            }
        }
        x = prop.x + left + int(right - left) / 2;
        y = prop.y + top + int(bottom - top) / 2;
        return true;
    }

    void Update(int deltaMs) override {
        const CLayerCollision *bodyLayer = m_map.map.GetCurrentCollisionLayer();
        const CLayerCollision *bulletLayer = m_map.map.GetCurrentBulletCollisionLayer();
        bool changed = bodyLayer != m_bodyLayer || bulletLayer != m_bulletLayer;
        m_bodyLayer = bodyLayer;
        m_bulletLayer = bulletLayer;
        for (PlacedProp &prop : m_map.props) {
            if (!prop.active || prop.runtime == nullptr) { continue; }
            // CLevel::TransformObjectElapseMS :114280 scales every PROP,
            // including its timer. Brothers and human bullets are exempt.
            const int propDeltaMs = std::max(1, int(std::lround(deltaMs * m_level.GetObjectTimeScale())));
            prop.runtime->Update(propDeltaMs, PlayerInside(prop));
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
            if (!prop.active || prop.runtime == nullptr || prop.runtime->IsRemoved() || prop.runtime->GetHealth() <= 0 || hit.ownerType != 0) { continue; }
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
            if (!prop.active || kPropIdBase + prop.objectId != target || prop.runtime == nullptr || prop.runtime->IsRemoved()) { continue; }
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
            if (!prop.active || prop.runtime == nullptr || prop.runtime->IsRemoved() || prop.runtime->GetHealth() <= 0) { continue; }
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

    unsigned CheckEntryRoutes() {
        unsigned tested = 0, failures = 0;
        for (PlacedProp &prop : m_map.props) {
            if (!prop.active || prop.runtime == nullptr || !prop.runtime->ChecksEntry()) { continue; }
            const auto &vertices = prop.runtime->GetEntryCollision().GetVertices();
            if (vertices.empty()) { ++failures; continue; }
            float x = 0, y = 0;
            for (const auto &point : vertices) { x += point.x; y += point.y; }
            x = prop.x + x / vertices.size();
            y = prop.y + y / vertices.size();
            unsigned routes = 0;
            for (const auto &point : vertices) {
                const float dx = prop.x + point.x - x, dy = prop.y + point.y - y;
                if (m_scene.CanWalkTo(x + dx * 2, y + dy * 2, x, y)) { ++routes; }
            }
            std::printf("[map-entry-check] prop=%08x:%u id=%d centre=%.1f,%.1f vertices=%zu routes=%u\n",
                prop.sprite->resource.packHash, prop.sprite->resource.localIndex, prop.objectId, x, y, vertices.size(), routes);
            if (routes == 0) { ++failures; }
            ++tested;
        }
        std::printf("[map-entry-check] tested=%u failures=%u\n", tested, failures);
        return failures;
    }

    unsigned CheckDamageContracts() {
        unsigned tested = 0, failures = 0;
        for (const PlacedProp &prop : m_map.props) {
            if (!prop.active || prop.runtime == nullptr || prop.runtime->GetHealth() <= 0) { continue; }
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
        prop.foreground = prop.runtime->GetPlayer(2);
        prop.main = prop.runtime->GetPlayer(1);
        prop.background = prop.runtime->GetPlayer(0);
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
    std::vector<PlacedProp *> m_activeProps; // Stable until the next level Reset.
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

/** Convert world anchors and native pixel sizes to the HUD's logical canvas. */
void ProjectEnemyHealthBars(std::vector<CombatScene::HealthBar> &bars,
    float cameraX, float cameraY, float zoom, int width, int height) {
    for (auto &bar : bars) {
        bar.x = (bar.x - cameraX) * zoom * 1024 / width;
        bar.y = (bar.y - cameraY) * zoom * 768 / height;
        bar.width *= 1024.0f / width;
        bar.height *= 768.0f / height;
        bar.border *= 1024.0f / width;
        // Centre in screen space, after the world anchor has been projected.
        bar.x -= bar.width * 0.5f;
    }
}

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
            // CBrother::Spawn :135887 stores the PLAYER object's extra uint16 in
            // the angle member +1984. Maps without that field leave the original
            // reading uninitialised memory; this port keeps 0.
            placed.facingDegrees = static_cast<float>(objects[i].playerSpawnFacing);
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
            std::printf("[m3] %s at %d %d -- scale %.0f facing %.0f authored=%d\n",
                        loaded.playerTemplate->owner.c_str(), objects[i].x,
                        objects[i].y, loaded.playerTemplate->gameScale,
                        placed.facingDegrees, objects[i].hasPlayerSpawnFacing);
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
    player.gunResource.packHash = weapon.packHash;
    player.gunResource.localIndex = static_cast<std::uint8_t>(weapon.ordinal);
    const std::uint64_t key = (static_cast<std::uint64_t>(weapon.packHash) << 8) | weapon.ordinal;
    player.masteryExperience = 0;
    const auto mastery = player.masteryByWeapon.find(key);
    if (mastery != player.masteryByWeapon.end()) { player.masteryExperience = mastery->second; }
    if (!EquipPlayerWeapon(tables, loaded.playerTemplate->script, weapon.data, weapon.owner, player) ||
        !CreatePlayerBuffers(player, program)) { return false; }
    PosePlayer(player);
    return true;
}

void AppendSurvivalShortcut(std::vector<KeyCode> &inputs, KeyCode key) {
    // Desktop binding policy: F/R are not gameplay shortcuts. Pointer Retry
    // and NextItem actions are dispatched separately and remain available.
    if (key == KeyCode::F || key == KeyCode::R) { return; }
    inputs.push_back(key);
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
struct MapRenderItem {
    int group = kZGroupNormal;
    int y = 0;
    const PlacedProp *prop = nullptr;
    PlayerModel *player = nullptr;
    EnemyModel *enemy = nullptr;
    float matrix[kMatrix4dElements] = {};
};

/** CRenderQueue::Compare :145029: group, then integer world Y. */
bool MapItemDrawsBefore(const MapRenderItem &left, const MapRenderItem &right) {
    if (left.group != right.group) { return left.group < right.group; }
    return left.y < right.y;
}

/** Shared main/foreground passes for the game and permanent map viewers.
 * CRenderQueue::Draw :145123 puts actors and scenery in the SAME main pass.
 * The caller has already drawn tiles and every prop's background slot.
 */
void DrawMapObjects(LoadedMap &loaded, CQuadBatch &batch, const CShaderProgram &program,
                const float *mapMvp, bool showProps = true, CombatScene *scene = nullptr,
                PlayerModel *brotherModel = nullptr, float brotherY = 0, int viewportWidth = 1) {
    std::vector<MapRenderItem> items;
    items.reserve(loaded.props.size() + loaded.enemies.size() + loaded.players.size());
    if (showProps) {
        for (const PlacedProp &prop : loaded.props) {
            MapRenderItem item;
            item.group = prop.sprite->zOrderGroup;
            item.y = static_cast<int>(prop.y);
            item.prop = &prop;
            items.push_back(item);
        }
    }

    // The quad batch sets a blend FUNCTION per group but never touches the
    // enable, which is switched on once at start-up and stays on for the whole
    // frame. So only the function is ours to set, and it has to be set: the
    // last sprite group may have left an additive one behind. Switching
    // blending off instead is wrong twice over -- the sprites drawn afterwards
    // lose their alpha and turn into black rectangles, and a model's own
    // ground-shadow disc goes opaque white.
    for (std::size_t i = 0; i < loaded.enemies.size(); ++i) {
        PlacedEnemy &placed = loaded.enemies[i];
        const float scale = EnemyModelWorldScale(*placed.model, placed.gameScale,
                                                 kLevelCameraScale);

        MapRenderItem item;
        item.y = static_cast<int>(placed.y);
        item.enemy = placed.model.get();
        BuildEnemyGameMatrix(*placed.model, mapMvp, placed.x, placed.y, scale,
                             0.0f, item.matrix);
        items.push_back(item);
    }

    for (std::size_t i = 0; i < loaded.players.size(); ++i) {
        PlacedPlayer &placed = loaded.players[i];
        const float scale = PlayerModelWorldScale(
            *placed.model, loaded.playerTemplate->gameScale, kLevelCameraScale);

        MapRenderItem item;
        item.y = static_cast<int>(placed.y);
        item.player = placed.model.get();
        BuildPlayerGameMatrix(mapMvp, placed.x, placed.y, scale,
                              placed.facingDegrees, item.matrix);
        items.push_back(item);
    }
    if (scene != nullptr) {
        if (brotherModel != nullptr) {
            MapRenderItem item;
            item.y = static_cast<int>(brotherY);
            item.player = brotherModel;
            float world[kMatrix4dElements];
            scene->BrotherMatrix(world);
            Matrix4dMultiply(mapMvp, world, item.matrix);
            items.push_back(item);
        }
        for (const auto &actor : scene->enemies) {
            MapRenderItem item;
            item.y = static_cast<int>(actor->model.enemy.combat.y);
            item.enemy = &actor->model;
            float world[kMatrix4dElements];
            scene->EnemyMatrix(*actor, world);
            Matrix4dMultiply(mapMvp, world, item.matrix);
            // Original stun shake is a screen-pixel draw offset, never collision motion.
            item.matrix[3] += 2.0f * actor->model.enemy.stun.GetOffset() / viewportWidth;
            items.push_back(item);
        }
    }
    std::stable_sort(items.begin(), items.end(), MapItemDrawsBefore);
    glDisable(GL_DEPTH_TEST);
    batch.Begin();
    for (const MapRenderItem &item : items) {
        if (item.prop != nullptr) {
            AddSpriteQuads(*item.prop, CurrentQuads(*MainSlotFor(*item.prop), item.prop->main), batch);
            continue;
        }
        // Flush the preceding 2D run before submitting this model.
        if (batch.GetQuadCount() != 0) {
            batch.Upload();
            batch.Draw(program, mapMvp);
            batch.Begin();
        }
        // CMeshCamera::DrawHeirarchy :99263 clears depth per hierarchy.
        // Depth resolves parts of this model; cross-object order belongs to the queue.
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (item.player != nullptr) { DrawPlayer(*item.player, program, item.matrix); }
        if (item.enemy != nullptr) { DrawEnemyModel(*item.enemy, program, item.matrix); }
        glDisable(GL_DEPTH_TEST);
    }
    if (showProps) {
        // Explosion/shockwave z=3 and cover debris z=5 sit above bodies.
        AddParticleQuads(loaded, batch, 3, 5);
        for (const PlacedProp &prop : loaded.props) {
            AddSpriteQuads(prop, CurrentQuads(*ForegroundSlotFor(prop), prop.foreground), batch);
        }
    }
    batch.Upload();
    batch.Draw(program, mapMvp);
    // Put back what was found: the sprite path draws flat and in order.
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
        // Main scenery must be interleaved with actors; DrawMapObjects owns that
        // pass and the final foreground pass (CRenderQueue::Draw :145235).
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
    if (loaded.map.GetCamera().HasPosition()) {
        camera.x = loaded.map.GetCamera().GetX() - viewWorldWidth * 0.5f;
        camera.y = loaded.map.GetCamera().GetY() - viewWorldHeight * 0.5f;
    }

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

/** Ignore only sub-visible RGB rounding when comparing framebuffer captures. */
static unsigned CountOcclusionPixelChanges(const std::vector<unsigned char> &first,
                                          const std::vector<unsigned char> &second) {
    unsigned changed = 0;
    for (std::size_t pixel = 0; pixel < first.size(); pixel += 4) {
        int difference = 0;
        for (unsigned channel = 0; channel < 3; ++channel) {
            difference += std::abs(static_cast<int>(first[pixel + channel]) - second[pixel + channel]);
        }
        if (difference > 24) { ++changed; }
    }
    return changed;
}

int RunMapOcclusionCheck(const std::string &bigDirectory) {
    // Fixed research scene; scenery, collision and models still come from BIG.
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    CWindow window;
    if (!window.Open("Map occlusion check", 768, 768)) { return 1; }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    CShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    CQuadBatch batch;
    CQuadBatch cover;
    if (!batch.Create(program) || !cover.Create(program)) { return 1; }
    unsigned failures = 0;
    unsigned occludedPixels = 0;
    const char *packs[] = {"pack2", "pack7", "pack9", "pack12"};
    const unsigned maps[] = {7, 6, 0, 0};
    for (unsigned map = 0; map < 4; ++map) {
        LoadedMap loaded;
        if (!LoadMap(toc, toc.GetPackIndexFromName(packs[map]), maps[map], loaded)) { return 1; }
        LoadProps(toc, loaded);
        LoadPlacedPlayers(toc, program, loaded);
        if (loaded.players.empty()) { return 1; }
        // Find a real obstacle whose art extends above its collision footprint.
        std::size_t selected = loaded.props.size();
        float largestOverhangArea = 0;
        const MapRectangle bounds = loaded.map.GetVisibleBounds();
        for (std::size_t i = 0; i < loaded.props.size(); ++i) {
            const PlacedProp &prop = loaded.props[i];
            if (prop.sprite->data.GetCollision().GetVertices().empty()) { continue; }
            float collisionTop = 0;
            for (const CollisionPoint &point : prop.sprite->data.GetCollision().GetVertices()) {
                collisionTop = std::min(collisionTop, point.y);
            }
            // Use walkable interior fixtures, not pieces of the outer map wall.
            if (prop.y + collisionTop - kPlayerCollisionRadius <= bounds.y ||
                prop.x <= bounds.x || prop.x >= bounds.x + bounds.width) { continue; }
            float top = 0, left = 0, right = 0;
            for (const SpriteQuad &quad : CurrentQuads(*MainSlotFor(prop), prop.main)) {
                top = std::min(top, static_cast<float>(quad.offsetY));
                left = std::min(left, static_cast<float>(quad.offsetX));
                right = std::max(right, static_cast<float>(quad.offsetX + quad.source.width));
            }
            const float overhangArea = (collisionTop - top) * (right - left);
            if (overhangArea > largestOverhangArea) { largestOverhangArea = overhangArea; selected = i; }
        }
        if (selected == loaded.props.size()) { return 1; }
        PlacedProp prop = loaded.props[selected];
        loaded.props.clear();
        loaded.props.push_back(prop);
        loaded.players.resize(1);
        float collisionTop = 0;
        float collisionBottom = 0;
        for (const CollisionPoint &point : prop.sprite->data.GetCollision().GetVertices()) {
            collisionTop = std::min(collisionTop, point.y);
            collisionBottom = std::max(collisionBottom, point.y);
        }
        cover.Begin();
        AddSpriteQuads(prop, CurrentQuads(*MainSlotFor(prop), prop.main), cover);
        cover.Upload();
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        glViewport(0, 0, width, height);
        float mvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(static_cast<float>(width), static_cast<float>(height), kMapDepthRange, mvp);
        Matrix4dTranslate(mvp, -prop.x + width * 0.5f, -prop.y + height * 0.5f);
        for (unsigned side = 0; side < 2; ++side) {
            PlacedPlayer &player = loaded.players[0];
            player.x = prop.x;
            player.y = prop.y + collisionTop - kPlayerCollisionRadius;
            if (side == 1) { player.y = prop.y + collisionBottom + kPlayerCollisionRadius; }
            BuildGeometry(loaded, batch, true, true, false);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            batch.Draw(program, mvp);
            DrawMapObjects(loaded, batch, program, mvp);
            std::vector<unsigned char> actual(width * height * 4);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
            const std::string path = "out/map-occlusion-" + std::string(packs[map]) + "-" + std::to_string(side) + ".png";
            if (!window.SaveFrame(path)) { ++failures; }
            // Independent two-object reference: background, ordered bodies, foreground.
            // This also verifies alpha holes; no rectangular occlusion mask is used.
            BuildGeometry(loaded, batch, true, true, false);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            batch.Draw(program, mvp);
            if (side == 1) { cover.Draw(program, mvp); }
            std::vector<unsigned char> withoutPlayer(actual.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, withoutPlayer.data());
            DrawMapObjects(loaded, batch, program, mvp, false);
            if (side == 0) { cover.Draw(program, mvp); }
            batch.Begin();
            AddSpriteQuads(prop, CurrentQuads(*ForegroundSlotFor(prop), prop.foreground), batch);
            batch.Upload();
            batch.Draw(program, mvp);
            std::vector<unsigned char> covered(actual.size());
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, covered.data());
            const unsigned changed = CountOcclusionPixelChanges(actual, covered);
            if (changed != 0) { ++failures; }
            unsigned actorPixels = 0;
            if (side == 0) {
                // Replaying the old actor-last bug must visibly differ from the reference.
                DrawMapObjects(loaded, batch, program, mvp, false);
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, withoutPlayer.data());
                actorPixels = CountOcclusionPixelChanges(withoutPlayer, covered);
                occludedPixels += actorPixels;
            } else {
                actorPixels = CountOcclusionPixelChanges(withoutPlayer, covered);
                if (actorPixels < 100) { ++failures; }
            }
            std::printf("[map-occlusion-check] %s prop=%08X/%u y=%.0f player-y=%.0f side=%u covered-difference=%u failures=%u\n",
                packs[map], prop.sprite->resource.packHash, prop.sprite->resource.localIndex, prop.y, player.y, side, changed, failures);
            std::printf("[map-occlusion-check] side=%u actor-pixels=%u\n", side, actorPixels);
        }
    }
    if (occludedPixels < 1000) { ++failures; }
    std::printf("[map-occlusion-check] hidden-pixels=%u failures=%u\n", occludedPixels, failures);
    if (failures != 0) { return 1; }
    return 0;
}

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
    const CombatScene &scene, const CLevel &level, std::uint64_t &accountedXplodium, bool missionEnded = false) {
    if (context == nullptr) { return true; }
    CProfileManager &profile = context->profile;
    if (context->tutorial) {
        const int step = level.GetTutorialStep();
        if (step == -1) { profile.tutorialCompleted = true; profile.tutorialSteps |= 128; }
        else if (step < 8) { profile.tutorialSteps |= 1u << step; }
    }
    const unsigned wave = level.GetWave();
    profile.stat42Bits |= level.GetStat42Bits();
    profile.experience = progress.GetExperience();
    profile.xplodium += scene.GetXplodium() - accountedXplodium;
    accountedXplodium = scene.GetXplodium();
    if (context->hordeStart >= 0) {
        if (profile.nativeArchive) {
            if (!RecordNativeMissionWaves(profile, context->missionLevel, wave, scene.GetWavePerfectResults())) { return false; }
            if (missionEnded && !RecordNativeMissionScore(profile, context->mission, scene.GetScore())) { return false; }
        }
        const unsigned index = static_cast<unsigned>(context->hordeStart);
        profile.hordeBestKills[index] = std::max(profile.hordeBestKills[index], scene.GetTotalKills());
        profile.hordeBestWave[index] = std::max(profile.hordeBestWave[index], wave);
        profile.hordeBestScore[index] = std::max(profile.hordeBestScore[index], scene.GetScore());
    } else {
        profile.clearedWaves[context->planet] = std::max(profile.clearedWaves[context->planet], wave);
        const auto &perfectResults = scene.GetWavePerfectResults();
        if (wave >= perfectResults.size()) {
            const unsigned firstWave = wave - static_cast<unsigned>(perfectResults.size());
            for (unsigned index = 0; index < perfectResults.size() && firstWave + index < 500; ++index) {
                if (perfectResults[index]) { profile.perfectedWaves[context->planet].set(firstWave + index); }
            }
        }
        if (scene.GetTotalKills() < context->accountedKills) { context->accountedKills = 0; }
        profile.enemyKills[context->planet] += scene.GetTotalKills() - context->accountedKills;
    }
    context->accountedKills = scene.GetTotalKills();
    for (const auto &entry : scene.GetWeaponProgress()) {
        const std::uint64_t key = (static_cast<std::uint64_t>(entry.resource.packHash) << 8) | entry.resource.localIndex;
        unsigned &credited = context->accountedWeaponExperience[key];
        if (entry.experience < credited) { credited = 0; }
        profile.AddWeaponExperience(entry.resource, entry.experience - credited, entry.maximum);
        credited = entry.experience;
    }
    context->result.kills = scene.GetTotalKills();
    context->result.horde = context->hordeStart >= 0;
    context->result.score = scene.GetScore();
    context->result.bestKillStreak = scene.GetBestKillStreak();
    context->result.stopwatchMs = level.GetStopwatchTime();
    context->result.wavesPerRevolution = level.GetWavesPerRevolution();
    context->result.waveLimit = level.GetWaveLimit();
    if (context->hordeStart >= 0) { context->result.highScore = profile.hordeBestScore[context->hordeStart]; }
    context->result.wave = wave;
    context->result.waves = scene.GetClearedWaves();
    context->result.perfectWaves = scene.GetPerfectWaves();
    context->result.xplodium = scene.GetXplodium();
    context->result.experience = progress.GetExperience() - context->startingExperience;
    context->result.casualties = scene.GetCasualties();
    context->result.weapons = scene.GetWeaponProgress();
    return profile.SaveToDisk(context->savePath);
}

int RunBossCheck(const std::string &bigDirectory) {
    // Verified retail Mission -> LEVEL -> map fixtures; production selection
    // still follows the resource references inside RunSurvival.
    const char *packs[] = {"pack2", "pack7", "pack9", "pack12"};
    const unsigned maps[] = {7, 6, 0, 0};
    unsigned failures = 0;
    for (unsigned index = 0; index < 4; ++index) {
        const int result = RunSurvival(bigDirectory, packs[index], maps[index], 0, -1, "", 0,
            false, false, false, 2, 0, nullptr, false, false, nullptr, false, nullptr, false, true);
        if (result != 0) { ++failures; }
    }
    std::printf("[boss-check] maps=4 failed-maps=%u\n", failures);
    if (failures > 0) { return 1; }
    return 0;
}

int RunPlayerDeathCheck(const std::string &bigDirectory) {
    const char *packs[] = {"pack2", "pack7", "pack9", "pack12"};
    const unsigned maps[] = {7, 6, 0, 0};
    unsigned failures = 0;
    for (unsigned index = 0; index < 4; ++index) {
        if (RunSurvival(bigDirectory, packs[index], maps[index], 0, -1, "", 0,
            false, false, false, 0, 0, nullptr, true, false, nullptr, false, nullptr, false, false, true) != 0) {
            ++failures;
        }
    }
    std::printf("[death-check] maps=4 failed-maps=%u\n", failures);
    if (failures != 0) { return 1; }
    return 0;
}

int RunSurvival(const std::string &bigDirectory, const std::string &packShortName,
    unsigned mapIndex, unsigned weaponIndex, int armorIndex, const std::string &screenshotPath,
    unsigned advanceMs, bool firePreview, bool showCollisions, bool check, unsigned checkWaves, unsigned startWave,
    SurvivalGameContext *gameContext, bool withBrother, bool powerupStudy, const MissionEntry *archiveMission, bool performanceStudy, CWindow *sharedWindow, bool feedbackStudy, bool bossStudy, bool deathStudy) {
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
    if (gameContext != nullptr) {
        progress.SetExperience(gameContext->profile.experience);
        gameContext->startingExperience = gameContext->profile.experience;
    }
    if (gameContext != nullptr && gameContext->tutorial) {
        // The zero-price level-one rifle is pack3 STORE 4 -> pack5 GUN 4.
        GameObjectRef rifle;
        rifle.packHash = CStringToKey("pack5");
        rifle.localIndex = 4;
        gameContext->profile.Grant(6, rifle);
        gameContext->profile.configuration.guns[1] = rifle;
    }
    CWindow ownedWindow;
    CWindow &window = sharedWindow ? *sharedWindow : ownedWindow;
    if (!window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    window.SetEscapeCloses(false);
    // Both the retail frontend and standalone survival research use shortcuts.
    window.EnableCheats(true);
    CBGM ownedMusic;
    CBGM *activeMusic = &ownedMusic;
    if (gameContext != nullptr && gameContext->music != nullptr) { activeMusic = gameContext->music; }
    CBGM &music = *activeMusic;
    if (gameContext != nullptr) { music.SetEnabled(gameContext->profile.musicEnabled); }
    MovieRenderer loadingMovies;
    CResPackTOC *loadingCore = toc.GetPack(toc.GetCorePackIndex());
    if (!loadingMovies.Init(*loadingCore, *loadingCore)) { return 1; }
    const CProfileManager *loadingProfile = nullptr;
    if (gameContext != nullptr) { loadingProfile = &gameContext->profile; }
    LoadingScreen loading(window, loadingMovies, tables, loadingProfile, true, false, &music);
    if (!loading.IsValid()) { return 1; }
    if (!LoadWeaponCatalog(toc, tables, weapons) || !LoadEnemyCatalog(toc, tables, enemies) ||
        !LoadInitialPlayerHealth(toc, tables, vitals.maximum)) { return 1; }
    SurvivalHud survivalHud;
    if (!survivalHud.Init(toc, tables)) { return 1; }
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
    if (gameContext != nullptr) {
        for (const auto &entry : gameContext->profile.weaponMastery) {
            const std::uint64_t key = (static_cast<std::uint64_t>(entry.resource.packHash) << 8) | entry.resource.localIndex;
            player.masteryByWeapon[key] = entry.experience;
        }
    }
    std::size_t weaponSlot = weaponIndex % weapons.size();
    unsigned equippedWeaponSlot = 0;
    std::size_t pendingWeapon = weapons.size();
    unsigned pendingEquippedSlot = 0, primaryEquippedSlot = 0;
    bool swapEventAccepted = false;
    unsigned combatSwapEvents = 0;
    if (gameContext != nullptr) {
        // CBrother::Bind :135820 selects the saved 1001 activeWeaponSlot.
        equippedWeaponSlot = gameContext->profile.activeWeaponSlot;
        const GameObjectRef &ref = gameContext->profile.configuration.guns[equippedWeaponSlot];
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
        CLevel percentageLevel;
        CLevel::Template percentageTemplate;
        CMap percentageMap;
        percentageLevel.Bind(percentageTemplate, percentageMap);
        rewardProbe.SetLevel(&percentageLevel);
        const std::uint64_t beforePercentage = rewardProbe.GetXplodium();
        const std::int16_t percentage = 105;
        percentageLevel.FunctionResolver(56, &percentage, 1);
        for (unsigned award = 0; award < 20; ++award) { rewardProbe.AddXplodium(1); }
        if (rewardProbe.GetXplodium() != beforePercentage + 21) { ++checkFailures; }
        const std::int16_t increment = 95;
        percentageLevel.FunctionResolver(57, &increment, 1);
        rewardProbe.AddXplodium(1);
        if (rewardProbe.GetXplodium() != beforePercentage + 23) { ++checkFailures; }
        std::printf("[xplodium-check] fractional-carry=1 set-add-percent=1 failures=%u\n", checkFailures);
        // Three real enemy deaths distinguish player streak growth from a bro
        // assist. Expected points are 2E + 4E + 3E; only the first two grant XP.
        rewardProbe.Reset();
        pickupProgress.Bind(progressData);
        rewardProbe.SetPlayerProgress(&pickupProgress);
        rewardProbe.SetHorde(true);
        percentageLevel.Bind(percentageTemplate, percentageMap);
        vitals.invincible = true;
        unsigned expectedExperience = 0;
        for (unsigned death = 0; death < 3; ++death) {
            CombatEnemy *target = rewardProbe.Spawn(0, 600, 350);
            if (target == nullptr) { ++checkFailures; break; }
            const unsigned experience = static_cast<unsigned>(std::ceil(target->data->experienceReward * PlayerArmorMultiplier(player, 3)));
            if (death == 0) { expectedExperience = experience; }
            CombatHit hit;
            hit.owner = kPlayerCombatId;
            if (death == 2) { hit.owner = kBrotherCombatId; }
            hit.ownerType = 0;
            hit.damage = 1000000;
            hit.applyArmorAttack = false;
            const CombatId targetId = target->model.enemy.combat.id;
            for (unsigned tick = 0; tick < 300 && !target->deathReported; ++tick) {
                rewardProbe.ApplyHit(targetId, hit);
                rewardProbe.Update(16, 0, 0, false);
            }
            if (!target->deathReported) { ++checkFailures; }
            const auto &texts = rewardProbe.GetExperienceTexts();
            if (texts.empty() || texts.back().amount != experience) { ++checkFailures; }
            std::printf("[xp-text-check] death=%u amount=%u visible=%zu failures=%u\n",
                death, experience, texts.size(), checkFailures);
        }
        if (expectedExperience == 0 || rewardProbe.GetScore() != expectedExperience * 9 ||
            rewardProbe.GetKillStreak() != 2 || pickupProgress.GetExperience() != expectedExperience * 2) { ++checkFailures; }
        std::printf("[horde-score-check] enemy-xp=%u points=%u expected=%u streak=%u xp=%llu failures=%u\n",
            expectedExperience, rewardProbe.GetScore(), expectedExperience * 9,
            rewardProbe.GetKillStreak(), pickupProgress.GetExperience(), checkFailures);
        // A standard-mode real death must draw the original XP string, float
        // in screen space, fade, expire and stay absent after restart.
        rewardProbe.Reset();
        rewardProbe.SetHorde(false);
        rewardProbe.SetTextView(400, 100, 2, 1.5f);
        CombatEnemy *xpTarget = rewardProbe.Spawn(0, 600, 350);
        if (xpTarget == nullptr) { return 1; }
        CombatHit xpHit;
        xpHit.owner = kPlayerCombatId;
        xpHit.ownerType = 0;
        xpHit.damage = 1000000;
        xpHit.applyArmorAttack = false;
        for (unsigned tick = 0; tick < 300 && !xpTarget->deathReported; ++tick) {
            rewardProbe.ApplyHit(xpTarget->model.enemy.combat.id, xpHit);
            rewardProbe.Update(16, 0, 0, false);
        }
        if (!xpTarget->deathReported || rewardProbe.GetExperienceTexts().size() != 1) { return 1; }
        const auto bornText = rewardProbe.GetExperienceTexts().front();
        const auto &deadState = xpTarget->model.enemy.combat;
        if (bornText.amount != expectedExperience || bornText.x != int((deadState.x - 400) * 2) ||
            bornText.y != int((deadState.y - 100) * 1.5f) || bornText.alpha != 1) { ++checkFailures; }
        rewardProbe.SetTextView(900, 700, 4, 3);
        rewardProbe.UpdateExperienceTexts(0);
        if (rewardProbe.GetExperienceTexts().front().y != bornText.y) { ++checkFailures; }
        int frameWidth = 0, frameHeight = 0;
        window.GetDrawableSize(frameWidth, frameHeight);
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(frameWidth) * frameHeight * 4);
        std::uint64_t previousLight = 0;
        for (unsigned stage = 0; stage < 3; ++stage) {
            if (stage > 0) { rewardProbe.UpdateExperienceTexts(1000); }
            if (stage == 1) {
                const auto &text = rewardProbe.GetExperienceTexts().front();
                if (text.x != bornText.x || text.y != bornText.y - 100 || text.alpha != 0.5f) { ++checkFailures; }
            }
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!survivalHud.DrawExperienceTexts(rewardProbe.GetExperienceTexts(), false) ||
                !window.SaveFrame("out/xp-text-" + std::to_string(stage * 1000) + ".png")) { ++checkFailures; }
            glReadPixels(0, 0, frameWidth, frameHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            std::uint64_t light = 0;
            for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
                light += pixels[pixel] + pixels[pixel + 1] + pixels[pixel + 2];
            }
            if (stage == 0 && light == 0) { ++checkFailures; }
            if (stage == 1 && (light == 0 || light >= previousLight)) { ++checkFailures; }
            if (stage == 2 && (light != 0 || !rewardProbe.GetExperienceTexts().empty())) { ++checkFailures; }
            previousLight = light;
            std::printf("[xp-text-render-check] time=%u light=%llu failures=%u\n", stage * 1000, light, checkFailures);
        }
        rewardProbe.Update(16, 0, 0, false);
        if (!rewardProbe.GetExperienceTexts().empty()) { ++checkFailures; }
        rewardProbe.Reset();
        if (!rewardProbe.GetExperienceTexts().empty()) { ++checkFailures; }
        vitals.invincible = false;
    }
    CombatScene scene(tables, program, enemies, player, vitals, effects, loaded.playerTemplate->gameScale);
    if (gameContext != nullptr) {
        music.SetEnabled(gameContext->profile.musicEnabled);
        CAudioPlayer::SetEffectsEnabled(gameContext->profile.soundEnabled);
        player.brotherIndex = gameContext->profile.playerBrother;
    }
    CBrotherAI brother;
    PlayerModel brotherModel;
    CPlayerConfiguration brotherConfiguration;
    brotherConfiguration.SetDefaults(toc.GetPack(toc.GetCorePackIndex())->GetPackHash());
    // Local default partner: Whippersnappers and the free ER97E Elite rifle.
    // These are core gun 0 and pack5 gun 4 in the original store catalogue.
    brotherConfiguration.guns[1].packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
    brotherConfiguration.guns[1].localIndex = 4;
    if (withBrother) {
        brother.vitals.maximum = progress.GetHealth();
        brother.vitals.invincible = false;
        brotherModel.vitals = &brother.vitals;
        brotherModel.human = false;
        brotherModel.brotherIndex = 1;
        if (gameContext != nullptr) { brotherModel.brotherIndex = 1 - gameContext->profile.playerBrother; }
        std::size_t brotherWeaponSlot = 0;
        for (std::size_t index = 0; index < weapons.size(); ++index) {
            if (weapons[index].packHash == brotherConfiguration.guns[0].packHash &&
                weapons[index].ordinal == brotherConfiguration.guns[0].localIndex) {
                brotherWeaponSlot = index;
                break;
            }
        }
        if (!BuildPlayerBody(tables, player.moveSet, brotherModel) ||
            !EquipPlayerWeapon(tables, loaded.playerTemplate->script, weapons[brotherWeaponSlot].data,
                "AI brother", brotherModel) || !CreatePlayerBuffers(brotherModel, program)) { return 1; }
        for (const GameObjectRef &ref : brotherConfiguration.armor) {
            if (ref.IsNull()) { continue; }
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(ref.packHash, GameSection::Armor, ref.localIndex, payload)) { return 1; }
            CArrayInputStream input(payload);
            CArmor::Template armor;
            if (!armor.Init(input) || !EquipPlayerArmor(tables, armor, program, brotherModel)) { return 1; }
        }
        // Regression: a local default partner must never inherit premium gear.
        if (check) {
            const unsigned coreHash = toc.GetPack(toc.GetCorePackIndex())->GetPackHash();
            bool defaultEquipment = weapons[brotherWeaponSlot].packHash == coreHash && weapons[brotherWeaponSlot].ordinal == 0;
            for (unsigned slot = 0; slot < 3; ++slot) {
                if (brotherModel.armor[slot] == nullptr || PlayerArmorMultiplier(brotherModel, slot) != 1.0f) {
                    defaultEquipment = false;
                }
            }
            std::printf("[brother-equipment-check] gun=%s default=%d\n", weapons[brotherWeaponSlot].name.c_str(), defaultEquipment);
            if (!defaultEquipment) { return 1; }
        }
        scene.SetBrother(&brotherModel, &brother);
        const WeaponEntry *rifle = nullptr;
        for (const WeaponEntry &entry : weapons) {
            if (entry.packHash == brotherConfiguration.guns[1].packHash && entry.ordinal == brotherConfiguration.guns[1].localIndex) {
                rifle = &entry;
                break;
            }
        }
        if (rifle == nullptr) { return 1; }
        scene.SetBrotherWeapons(loaded.playerTemplate->script, weapons[brotherWeaponSlot].data, rifle->data);
        if (check) {
            if (!scene.SwapBrotherWeapon() || scene.GetBrotherWeaponSlot() != 1 ||
                !scene.SwapBrotherWeapon() || scene.GetBrotherWeaponSlot() != 0) { return 1; }
            std::printf("[brother-equipment-check] pistol-rifle-pistol=1 player-unchanged=1\n");
        }
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
    else if (gameContext != nullptr && gameContext->profile.nativeArchive && !gameContext->tutorial && gameContext->planet < 4) {
        archiveLevel = &gameContext->profile.nativeArchive->survivalLevels[gameContext->planet];
    }
    session.SetDialogHud(&survivalHud);
    if (!session.Load(toc, tables, toc.GetPack(packIndex)->GetPackHash(), mapIndex, archiveLevel, archiveMission != nullptr)) { return 1; }
    const bool horde = archiveMission != nullptr && archiveMission->data.type == 2;
    if (horde && gameContext != nullptr) {
        gameContext->mission = archiveMission->resource;
        gameContext->missionLevel = archiveMission->data.level;
    }
    if (horde) { session.SetHorde(true); }
    session.SetOriginalHud(&survivalHud);
    const float startX = loaded.players[0].x;
    const float startY = loaded.players[0].y;
    // The map PLAYER object's spawn angle; CBrother::Spawn :135887 writes the
    // same value to both brothers.
    const float startFacing = loaded.players[0].facingDegrees;
    session.SetStartWave(static_cast<int>(startWave));
    // The original seeds its one CRandGen from the clock (:370383), so a level
    // script's rolls differ every session. Real play does the same; research
    // runs keep the fixed default stream so their results stay comparable.
    if (!check && !bossStudy && capturePath.empty()) {
        session.SetScriptRandomSeed(static_cast<std::uint32_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    }
    const bool tutorial = gameContext != nullptr && gameContext->tutorial;
    session.GetLevel().EnableTutorial(tutorial);
    scene.SetMap(loaded.map, loaded.collisionScene, loaded.weaponCollision, kLevelCameraScale, kPlayerCollisionRadius);
    session.Restart(startX, startY, startFacing);
    std::uint64_t accountedXplodium = 0;
    int lastSavedWave = session.GetLevel().GetWave();
    bool savedDeath = false;
    // CMap::SetObjectLayer (:91935) activates one layer. Preview may combine
    // layers, but survival must not inherit deathmatch/campaign obstacles.
    // Correction: preload all layers, then OnStart activates only authored
    // layers in script order; previously spawned props survive layer switches.
    LoadProps(toc, loaded);
    BuildCollisionScene(loaded);
    MapPropWorld props(loaded, scene, session.GetLevel(), effects);
    session.SetProps(&props);
    scene.SetProps(&props);
    session.Restart(startX, startY, startFacing);
    loading.Finish();

    if (deathStudy) {
        // Fixtures use original actors and resources. Only input and fixed time
        // are supplied by this check; production exits through the same gate.
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        const auto captureDeath = [&](const std::string &suffix) {
            loaded.players[0].x = scene.playerX;
            loaded.players[0].y = scene.playerY;
            loaded.players[0].facingDegrees = scene.facing;
            const float zoom = GameViewCameraZoom(width, height);
            float mvp[kMatrix4dElements];
            Matrix4dOrthoTopLeft(width / zoom, height / zoom, kMapDepthRange, mvp);
            Matrix4dTranslate(mvp, -scene.playerX + width / zoom / 2, -scene.playerY + height / zoom / 2);
            glViewport(0, 0, width, height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            BuildGeometry(loaded, batch, true, true, false);
            batch.Draw(program, mvp);
            DrawMapObjects(loaded, batch, program, mvp, true, &scene, &brotherModel, brother.y, width);
            return window.SaveFrame("out/player-death-" + packShortName + "-" + suffix + ".png");
        };
        for (unsigned scenario = 0; scenario < 2; ++scenario) {
            session.Restart(startX, startY, startFacing);
            vitals.invincible = true;
            brother.vitals.invincible = true;
            // Finish the real intro so it cannot alter the scale under test.
            for (int elapsed = 0; elapsed < 5000; elapsed += 16) { session.Update(16, 0, 0, false); }
            if (scenario == 0) {
                vitals.invincible = false;
                CombatHit fatal;
                fatal.ownerType = 1;
                fatal.damage = 10000;
                if (scene.ApplyHit(kPlayerCombatId, fatal) != HitResult::Killed) { ++checkFailures; }
            } else {
                // Includes an autorepeated last letter, which must not complete.
                for (char letter : std::string("stsuicid")) {
                    if (!PushBossCheckKey(window, letter)) { return 1; }
                }
                if (!PushBossCheckKey(window, 'e', true) || !window.TakeCheatCode().empty()) { ++checkFailures; }
                if (!PushBossCheckKey(window, 'e') || window.TakeCheatCode() != "stsuicide" || !scene.Suicide()) { ++checkFailures; }
                if (!window.TakeCheatCode().empty() || window.IsKeyDown(KeyCode::S) || window.IsKeyDown(KeyCode::E)) { ++checkFailures; }
                for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None; key = window.TakeKeyPress()) {
                    if (key == KeyCode::E || key == KeyCode::C) { ++checkFailures; }
                }
            }
            if (!vitals.dead || vitals.deaths != 1 || session.IsDeathComplete() || !vitals.inputHidden ||
                std::abs(session.GetLevel().GetWorldTimeScale() - 76 / 256.0f) > 0.0001f) { ++checkFailures; }
            if (scene.Suicide() || vitals.deaths != 1) { ++checkFailures; }
            auto &torso = player.weapon->brother.GetTorso();
            const int startTime = torso.GetAnimation().GetTimeMs();
            const int duration = torso.GetAnimation().GetRangeDurationMs();
            const auto &move = torso.GetMoveSet()->GetMoves()[torso.GetMoveIndex()];
            const float deathX = scene.playerX, deathY = scene.playerY;
            if (duration <= 0 || player.weapon->brother.TorsoUsesWeapon()) { ++checkFailures; }
            if (scenario == 1) {
                // Native perturbation proves there is no fixed host death delay.
                const std::int16_t scale = 128;
                session.GetLevel().FunctionResolver(5, &scale, 1);
            }
            const int step = session.GetLevel().TransformWorldElapseMS(16);
            const int moveStep = std::max(1, static_cast<int>(step * move.speed + 0.5f));
            int elapsed = 0;
            int animationElapsed = 0;
            bool savedMiddle = false;
            if (scenario == 0 && !captureDeath("start")) { ++checkFailures; }
            while (!session.IsDeathComplete() && elapsed < 20000) {
                session.Update(16, 1, 1, true);
                // Other actors keep shooting during death. Inspect ownership,
                // not the scene-wide shot counter (pack2's enemies fire here).
                for (const auto &shot : effects.GetProjectileStates()) {
                    if (shot.owner == kPlayerCombatId) { ++checkFailures; }
                }
                elapsed += 16;
                animationElapsed += moveStep;
                if (animationElapsed < duration && session.IsDeathComplete()) { ++checkFailures; }
                if (!session.IsDeathComplete() && torso.GetAnimation().GetTimeMs() != startTime + animationElapsed) { ++checkFailures; }
                if (!savedMiddle && animationElapsed >= duration / 2) {
                    if (scenario == 0 && !captureDeath("middle")) { ++checkFailures; }
                    savedMiddle = true;
                }
            }
            if (!session.IsDeathComplete() || elapsed <= duration || !savedMiddle ||
                scene.playerX != deathX || scene.playerY != deathY) { ++checkFailures; }
            if (scenario == 0 && !captureDeath("complete")) { ++checkFailures; }
            std::printf("[death-check] %s scenario=%u range-ms=%d speed=%.3f step=%d wall-ms=%d complete=%d failures=%u\n",
                packShortName.c_str(), scenario, duration, move.speed, step, elapsed, session.IsDeathComplete(), checkFailures);
        }
        session.Restart(startX, startY, startFacing);
        if (vitals.dead || vitals.deathAnimationComplete || vitals.inputHidden || session.GetLevel().GetWorldTimeScale() != 1) { ++checkFailures; }
        brother.vitals.invincible = false;
        CombatHit fatal;
        fatal.ownerType = 1;
        fatal.damage = 10000;
        scene.ApplyHit(kBrotherCombatId, fatal);
        for (int elapsed = 0; elapsed < 8000; elapsed += 16) { AdvancePlayer(brotherModel, 16); }
        if (!brother.vitals.deathAnimationComplete || session.IsDeathComplete() || session.GetLevel().GetWorldTimeScale() != 1) { ++checkFailures; }
        brotherModel.weapon->brother.OnWaveCleared();
        for (int elapsed = 0; elapsed < 8000; elapsed += 16) { AdvancePlayer(brotherModel, 16); }
        if (brother.vitals.dead || brother.vitals.deathAnimationComplete) { ++checkFailures; }
        std::printf("[death-check] %s restart/brother failures=%u\n", packShortName.c_str(), checkFailures);
        if (checkFailures != 0) { return 1; }
        return 0;
    }
    if (!loading.IsValid()) { return 1; }
    if (loading.Cancelled()) { return 0; }
    // CGunBros::OnLoaded :79321: battle music begins only after game binding.
    music.SetPaused(false);
    music.SetVolume(1.0f);
    if (!music.NextTrack()) { return 1; }
    if (bossStudy) {
        // Test fixtures only. Retail resources and Flow still choose the Boss,
        // spawn node, armor transitions, damage and subsequent ordinary wave.
        vitals.invincible = true;
        // Placed map mechanisms (Haven's two turrets) participate in LEVEL
        // counts. The production shortcut now owns preservation and draining.
        if (!PushBossCheckKey(window, 's', false, true)) { return 1; }
        for (char letter : std::string("wasdchi")) {
            if (!PushBossCheckKey(window, letter)) { return 1; }
        }
        if (window.TakeCheatCode() != "chi") { ++checkFailures; }
        for (char letter : std::string("stbos")) {
            if (!PushBossCheckKey(window, letter)) { return 1; }
        }
        if (!PushBossCheckKey(window, 's', true) || !window.TakeCheatCode().empty()) { ++checkFailures; }
        if (!PushBossCheckKey(window, 's')) { return 1; }
        const std::string bossCode = window.TakeCheatCode();
        if (bossCode != "stboss" || !window.TakeCheatCode().empty() || window.IsKeyDown(KeyCode::S)) { ++checkFailures; }
        std::printf("[stboss-input-check] code=%s repeats-ignored=1 released-s=%d failures=%u\n",
            bossCode.c_str(), !window.IsKeyDown(KeyCode::S), checkFailures);
        if (bossCode != "stboss" || !session.SkipToBoss()) { return 1; }
        const unsigned introBeforeRepeat = session.GetLevel().GetBossIntroSerial();
        if (session.SkipToBoss() || session.GetLevel().GetBossIntroSerial() != introBeforeRepeat) { ++checkFailures; }
        CombatEnemy *boss = nullptr;
        for (auto &actor : scene.enemies) {
            if (actor->model.enemy.CanReceiveProjectile(0, kPlayerCombatId)) { boss = actor.get(); }
        }
        if (session.GetLevel().GetBossIntroSerial() != 1 || boss == nullptr) {
            std::printf("[boss-check] %s missing scripted boss state=%d\n", packShortName.c_str(), session.GetLevel().GetStateId());
            return 1;
        }
        const CombatId bossId = boss->model.enemy.combat.id;
        CCamera &camera = loaded.map.GetCamera();
        // Compare the real camera with the original target operation at the
        // same authored bounds. This also permits legitimate edge clamping.
        CCamera expected = camera;
        expected.SetTarget(boss->model.enemy.combat.x, boss->model.enemy.combat.y);
        expected.SetCameraMode(2);
        CCamera actual = camera;
        const MapRectangle bounds = loaded.map.GetVisibleBounds();
        actual.Update(1000);
        expected.Update(1000);
        actual.UpdatePosition(scene.playerX, scene.playerY, bounds.x, bounds.y, bounds.width, bounds.height, 480, 320);
        expected.UpdatePosition(scene.playerX, scene.playerY, bounds.x, bounds.y, bounds.width, bounds.height, 480, 320);
        const float targetError = std::hypot(actual.GetX() - expected.GetX(), actual.GetY() - expected.GetY());
        if (camera.GetMode() != 2 || targetError > 0.01f) { ++checkFailures; }
        std::printf("[boss-check] %s camera-error=%.3f boss=%s\n", packShortName.c_str(), targetError, boss->data->owner.c_str());
        int introElapsed = 0;
        while (introElapsed < 60000 && (!session.GetLevel().CanPlayerMove() ||
            !session.GetLevel().CanPlayerShoot() || camera.GetMode() != 0)) {
            session.Update(16, 0, 0, false);
            introElapsed += 16;
        }
        boss = scene.Find(bossId);
        if (boss == nullptr) { return 1; }
        if (!session.GetLevel().CanPlayerMove() || !session.GetLevel().CanPlayerShoot() || camera.GetMode() != 0) { ++checkFailures; }
        std::printf("[boss-check] intro-ms=%d state=%u mode=%u move=%d shoot=%d\n", introElapsed,
            boss->model.enemy.GetStateId(), camera.GetMode(), session.GetLevel().CanPlayerMove(), session.GetLevel().CanPlayerShoot());

        GameObjectRef grenade;
        grenade.packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
        const unsigned grenadeOrdinals[] = {90, 93, 94};
        std::array<CBullet::Template, 3> grenadeTemplates;
        for (unsigned index = 0; index < 3; ++index) {
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(grenade.packHash, GameSection::Bullet, grenadeOrdinals[index], bytes)) { return 1; }
            CArrayInputStream input(bytes);
            if (!grenadeTemplates[index].Init(input)) { return 1; }
        }
        CEnemy &enemy = boss->model.enemy;
        const unsigned initialParts = enemy.GetPartCount();
        const float initialHealth = enemy.combat.health;
        CombatHit hit;
        hit.owner = kPlayerCombatId;
        hit.ownerType = 0;
        hit.damage = grenadeTemplates[0].GetBaseDamage();
        hit.flags = grenadeTemplates[0].GetFlags();
        hit.x = enemy.combat.x;
        hit.y = enemy.combat.y;
        const unsigned contactState = enemy.GetStateId();
        // Exercise repeated direct contacts through the real world dispatcher.
        for (int contact = 0; contact < 5; ++contact) { scene.ApplyHit(bossId, hit); }
        if (enemy.GetPartCount() != initialParts || enemy.combat.health != initialHealth || enemy.GetStateId() != contactState) { ++checkFailures; }
        std::printf("[boss-check] direct contacts=5 parts=%u expected=%u hp=%.1f\n", enemy.GetPartCount(), initialParts, enemy.combat.health);
        // Restart this fixture after the deliberate failing contact probe so
        // independent explosion assertions stay meaningful on the old code.
        const EnemyTemplateData *bossData = boss->data;
        std::size_t bossEntry = static_cast<std::size_t>(bossData - enemies.data());
        WeaponEffects blastEffects(toc, tables, program);
        CombatScene blastScene(tables, program, enemies, player, vitals, blastEffects, loaded.playerTemplate->gameScale);
        blastScene.SetLevel(&session.GetLevel());
        for (unsigned kind = 0; kind < 3; ++kind) {
            blastScene.Reset();
            vitals.invincible = true;
            CombatEnemy *target = blastScene.Spawn(bossEntry, 600, 450);
            if (target == nullptr) { return 1; }
            CEnemy &blastEnemy = target->model.enemy;
            // Each authored intro emits LEVEL event 11 when its animation
            // completes. Wait for that cue instead of assuming a duration.
            bool introComplete = false;
            for (int elapsed = 0; elapsed < 60000 && !introComplete; elapsed += 16) {
                blastEnemy.Update(16);
                for (const EnemyAction &action : blastEnemy.combat.actions) {
                    if (action.kind == EnemyAction::Kind::LevelEvent && action.slot == 11) { introComplete = true; }
                }
                blastEnemy.combat.actions.clear();
            }
            if (!introComplete) { ++checkFailures; }
            const unsigned readyState = blastEnemy.GetStateId();
            const float health = blastEnemy.combat.health;
            const unsigned parts = blastEnemy.GetPartCount();
            grenade.localIndex = static_cast<std::uint8_t>(grenadeOrdinals[kind]);
            // A real stationary grenade overlaps the Boss throughout its fuse:
            // this catches repeated direct collisions before the splash cue.
            unsigned throws = 1;
            if (kind == 0) { throws = 3; }
            for (unsigned number = 0; number < throws; ++number) {
                blastEnemy.combat.x = 600;
                blastEnemy.combat.y = 450;
                if (blastEffects.SpawnProjectile(grenade, 600, 450, 0, 0, 0, kPlayerCombatId, 0) == 0) { return 1; }
                float matrix[16];
                blastScene.PlayerMatrix(matrix);
                for (int elapsed = 0; elapsed < 4000; elapsed += 16) {
                    blastEnemy.combat.x = 600;
                    blastEnemy.combat.y = 450;
                    blastEnemy.combat.behaviour = 7;
                    blastEnemy.combat.targetAlive = false;
                    blastEffects.Update(player, matrix, 0, 16);
                    blastEnemy.Update(16);
                }
                unsigned expectedParts = parts;
                // The ice handler calls internal 7 after applying stun: it
                // also removes one part. The earlier research missed this call.
                if (kind == 2) { expectedParts = parts - 1; }
                if (kind == 0) {
                    expectedParts = parts - number - 1;
                    if (parts == 7) {
                        const unsigned strippedParts[] = {5, 3, 2};
                        expectedParts = strippedParts[number];
                    }
                }
                if (blastEnemy.GetPartCount() != expectedParts || blastEnemy.combat.health != health) { ++checkFailures; }
                std::printf("[boss-check] grenade=%u throw=%u parts=%u expected=%u hp=%.1f expected-hp=%.1f state=%u\n",
                    grenade.localIndex, number + 1, blastEnemy.GetPartCount(), expectedParts, blastEnemy.combat.health, health, blastEnemy.GetStateId());
            }
            if (kind == 0) {
                // The hit animation's parent uses 2x for attribute 0; ordinary
                // behavior uses 4x. Let Flow return before probing that branch.
                for (int elapsed = 0; elapsed < 60000 && blastEnemy.GetStateId() != readyState; elapsed += 16) {
                    blastEnemy.Update(16);
                }
                hit.flags = 1;
                hit.damage = 1;
                blastScene.ApplyHit(blastEnemy.combat.id, hit);
                if (std::abs(health - blastEnemy.combat.health - 4) > 0.01f) { ++checkFailures; }
                std::printf("[boss-check] stripped-bullet-damage=%.1f expected=4\n", health - blastEnemy.combat.health);
            }
        }
        // Fixture Bosses changed the shared camera; restore normal gameplay
        // before exercising the real LEVEL death callback and wave transition.
        camera.SetCameraMode(0);
        vitals.invincible = true;
        enemy.Damage(enemy.combat.health);
        for (int elapsed = 0; elapsed < 12000; elapsed += 16) { session.Update(16, 0, 0, false); }
        if (session.GetLevel().HasLargeEnemyHealthBars() || !session.GetLevel().CanPlayerMove() ||
            !session.GetLevel().CanPlayerShoot() || session.GetLevel().GetBossIntroSerial() != 1 || session.GetLevel().GetWave() < 1) { ++checkFailures; }
        std::printf("[boss-check] %s return-wave=%d state=%d failures=%u\n",
            packShortName.c_str(), session.GetLevel().GetWave(), session.GetLevel().GetStateId(), checkFailures);
        // Mid-revolution, revolution end and final supported wave use their
        // own original spawn quotas; no caller kills enemies between requests.
        for (int wave : {24, 49, 249, 450, 499}) {
            session.SetStartWave(wave);
            session.Restart(startX, startY, startFacing);
            vitals.invincible = false;
            if (!session.SkipToBoss() || vitals.invincible || session.GetLevel().GetWave() != wave ||
                session.GetLevel().GetBossIntroSerial() != 1) { ++checkFailures; }
            // Compare authored health tiers and REV multipliers at the actual
            // production shortcut, including the first wave of REV10.
            for (const auto &actor : scene.enemies) {
                if (actor->mapPlaced || !actor->model.enemy.CanReceiveProjectile(0, kPlayerCombatId)) { continue; }
                const auto &combat = actor->model.enemy.combat;
                float baseHealth = 100;
                const int realWave = wave % 50;
                if (realWave >= 10) { baseHealth = 300; }
                if (realWave >= 20) { baseHealth = 600; }
                if (realWave >= 30) { baseHealth = 900; }
                if (realWave >= 40) { baseHealth = 1500; }
                const float multiplier = session.GetLevel().GetEnemyMultiplier(combat.templateRef, 1);
                float revolutionMultiplier = float(wave / 50 + 1);
                // Haven LEVEL adds two to each REV's health factor.
                if (packShortName == "pack9") { revolutionMultiplier += 2; }
                const float expectedHealth = baseHealth * revolutionMultiplier;
                if (std::abs(combat.health - expectedHealth) > 0.01f) { ++checkFailures; }
                std::printf("[boss-health-check] %s wave=%d hp=%.1f multiplier=%.2f expected=%.1f failures=%u\n",
                    packShortName.c_str(), wave, combat.health, multiplier, expectedHealth, checkFailures);
            }
            std::printf("[stboss-wave-check] %s requested=%d actual=%d failures=%u\n",
                packShortName.c_str(), wave, session.GetLevel().GetWave(), checkFailures);
            if (wave == 450 || wave == 499) {
                blastScene.Reset();
                CombatEnemy *target = blastScene.Spawn(bossEntry, 600, 450);
                if (target == nullptr) { return 1; }
                CEnemy &blastEnemy = target->model.enemy;
                bool ready = false;
                for (int time = 0; time < 60000 && !ready; time += 16) {
                    blastEnemy.Update(16);
                    for (const auto &action : blastEnemy.combat.actions) {
                        if (action.kind == EnemyAction::Kind::LevelEvent && action.slot == 11) { ready = true; }
                    }
                    blastEnemy.combat.actions.clear();
                }
                if (!ready) { ++checkFailures; }
                const auto readyState = blastEnemy.GetStateId();
                const float health = blastEnemy.combat.health;
                grenade.localIndex = 90;
                for (unsigned number = 0; number < 4; ++number) {
                    for (int time = 0; time < 60000 && blastEnemy.GetStateId() != readyState; time += 16) {
                        blastEnemy.Update(16);
                    }
                    const float before = blastEnemy.combat.health;
                    blastEffects.SpawnProjectile(grenade, 600, 450, 0, 0, 0, kPlayerCombatId, 0);
                    float matrix[16];
                    blastScene.PlayerMatrix(matrix);
                    for (int time = 0; time < 4000; time += 16) {
                        blastEnemy.combat.x = 600; blastEnemy.combat.y = 450;
                        blastEnemy.combat.behaviour = 7;
                        blastEnemy.combat.targetAlive = false;
                        blastEffects.Update(player, matrix, 0, 16);
                        blastEnemy.Update(16);
                    }
                    float expectedDamage = 0;
                    // Ordinary grenades use internal 5, not the 4x gun branch.
                    // pack5 Boss @0xACD explicitly sets HP to 1 * REV before
                    // ApplyCollision, so its fourth frag is an authored kill.
                    if (number == 3) {
                        expectedDamage = 100 * PlayerArmorMultiplier(player, 1);
                        if (packShortName == "pack12") { expectedDamage = before; }
                    }
                    if (std::abs(before - blastEnemy.combat.health - expectedDamage) > 0.01f) { ++checkFailures; }
                    std::printf("[boss-rev10-grenade-check] %s wave=%d throw=%u initial=%.1f hp=%.1f damage=%.1f expected=%.1f armor-attack=%.2f failures=%u\n",
                        packShortName.c_str(), wave, number + 1, health, blastEnemy.combat.health,
                        before - blastEnemy.combat.health, expectedDamage, PlayerArmorMultiplier(player, 1), checkFailures);
                }
            }
        }
        if (checkFailures > 0) { return 1; }
        return 0;
    }
    if (feedbackStudy) {
        // Real BIG instances and the same clocks as RunSurvival; no source save.
        vitals.invincible = true;
        session.SetOriginalHud(&survivalHud);
        session.Restart(startX, startY, startFacing);
        for (unsigned tick = 0; tick < 300; ++tick) { session.Update(16, 0, 0, false); }
        survivalHud.OnOriginalWaveClear(session.GetLevel().GetWave(), true, 100, false);
        const float beforeX = scene.playerX;
        const float beforeY = scene.playerY;
        for (unsigned tick = 0; tick < 20; ++tick) { session.Update(16, 1, 0, false); }
        const float moved = std::hypot(scene.playerX - beforeX, scene.playerY - beforeY);
        if (moved <= 0) { ++checkFailures; }
        std::printf("[feedback-check] notice-moving=%.3f interstitial=%d failures=%u\n", moved, survivalHud.HasInterstitial(), checkFailures);
        // Reproduce duplicate object index 0 in the actual layer-2/layer-3 map.
        for (const auto &prop : loaded.props) {
            if (!prop.active || prop.sprite->interactiveKind != InteractivePropKind::Spire) { continue; }
            float targetX = 0, targetY = 0;
            const unsigned target = props.ResolveIndicatorTarget(prop.objectId);
            const bool bound = target != 0 && props.GetIndicatorTarget(target, targetX, targetY);
            float placedX = 0, placedY = 0;
            const bool originalOrder = props.GetObjectPosition(prop.objectId, placedX, placedY) &&
                placedX == prop.x && placedY == prop.y;
            bool found = false;
            for (const auto &marker : session.GetLevel().GetIndicators()) {
                if (marker.type != 1 || marker.objectId != prop.objectId) { continue; }
                found = bound && marker.targetKey == ((2ULL << 32) | target) && marker.x == targetX && marker.y == targetY;
            }
            if (!found || !originalOrder) { ++checkFailures; }
            // A later enemy sharing the map index must not steal the marker.
            auto *duplicate = scene.Spawn(0, prop.x + 600, prop.y + 600);
            if (!duplicate) { return 1; }
            duplicate->objectId = prop.objectId;
            float afterX = 0, afterY = 0;
            const bool retained = session.GetIndicatorTarget((2ULL << 32) | target, afterX, afterY) &&
                afterX == targetX && afterY == targetY;
            if (!retained) { ++checkFailures; }
            duplicate->model.enemy.combat.removed = true;
            std::printf("[indicator-check] layer=%u object=%d placed=%.1f,%.1f target=%.1f,%.1f original-order=%d bound=%d duplicate-retained=%d failures=%u\n",
                prop.objectLayer, prop.objectId, prop.x, prop.y, targetX, targetY, originalOrder, found, retained, checkFailures);
        }
        // The spire's four phases come from pack7 PROP33's Flow. Compare the
        // runtime clock against native 58's actual Q8 scale, not guessed seconds.
        for (PlacedProp &prop : loaded.props) {
            if (!prop.active || prop.runtime == nullptr || prop.sprite->interactiveKind != InteractivePropKind::Spire) { continue; }
            CProp &spire = *prop.runtime;
            const bool layersCorrect = spire.GetAnimation(2) == 255 &&
                BackgroundSlotFor(prop) == RuntimeSlotFor(prop, 0) &&
                ForegroundSlotFor(prop) == RuntimeSlotFor(prop, 2);
            if (!layersCorrect) { ++checkFailures; }
            spire.HandleMessage(0);
            const unsigned activeState = spire.GetStateId();
            const int timer = spire.GetTimerMs();
            const int step = std::max(1, int(std::lround(16 * session.GetLevel().GetObjectTimeScale())));
            const int expected = (timer + step - 1) / step * 16;
            int elapsed = 0;
            while (spire.GetStateId() == activeState && elapsed < expected + 32) { props.Update(16); elapsed += 16; }
            if (elapsed != expected) { ++checkFailures; }
            const unsigned cooldownState = spire.GetStateId();
            const int cooldown = spire.GetTimerMs();
            int cooled = 0;
            while (spire.GetStateId() == cooldownState && cooled < cooldown + 32) { props.Update(16); cooled += 16; }
            if (cooled != (cooldown + 15) / 16 * 16) { ++checkFailures; }
            std::printf("[feedback-check] spire-layers=%d active=%d expected=%d cooldown=%d expected=%d failures=%u\n",
                layersCorrect, elapsed, expected, cooled, cooldown, checkFailures);
        }
        // Two real attacks in one update, with separate human and AI owners.
        // Probe pack1's first sixteen authored enemy templates (including
        // shields and dormant map objects), rather than assuming all accept hits.
        unsigned simultaneousHits = 0;
        for (std::size_t index = 0; index < enemies.size(); ++index) {
            if (enemies[index].packHash != CStringToKey("pack1") || enemies[index].ordinal > 15) { continue; }
            CombatEnemy *actor = scene.Spawn(index, scene.playerX + 250, scene.playerY);
            if (actor == nullptr) { ++checkFailures; continue; }
            CEnemy &enemy = actor->model.enemy;
            for (unsigned tick = 0; tick < 100; ++tick) { enemy.Update(16); }
            const float health = enemy.combat.health;
            CombatHit hit;
            hit.owner = kPlayerCombatId;
            hit.ownerType = 0;
            hit.damage = health / 10;
            hit.part = 0;
            hit.x = enemy.combat.x;
            hit.y = enemy.combat.y + 100;
            hit.projectile = 10001;
            const HitResult first = scene.ApplyHit(enemy.combat.id, hit);
            hit.owner = kBrotherCombatId;
            hit.projectile = 10002;
            const HitResult second = scene.ApplyHit(enemy.combat.id, hit);
            for (unsigned tick = 0; tick < 100; ++tick) { enemy.Update(16); }
            if (first == HitResult::Hit && second == HitResult::Hit) {
                ++simultaneousHits;
                if (enemy.combat.hitCount != 2 || std::abs(health - enemy.combat.health - hit.damage * 2) > 0.01f) { ++checkFailures; }
            }
            std::printf("[feedback-hit] enemy=%s first=%d second=%d hp=%.2f->%.2f pending=%d hits=%d\n",
                enemies[index].owner.c_str(), int(first), int(second), health, enemy.combat.health, enemy.combat.collisionPending, enemy.combat.hitCount);
        }
        {
            // Native HandleCollision does not pause a projectile when a state
            // has no hit handler. A dormant original turret must not swallow a
            // penetrating round before it reaches the ordinary enemy behind it.
            WeaponEffects probeEffects(toc, tables, program);
            CombatScene probe(tables, program, enemies, player, vitals, probeEffects, loaded.playerTemplate->gameScale);
            probe.Reset();
            CombatEnemy *front = nullptr, *back = nullptr;
            for (std::size_t index = 0; index < enemies.size(); ++index) {
                if (enemies[index].packHash != CStringToKey("pack1")) { continue; }
                if (enemies[index].ordinal == 6) { front = probe.Spawn(index, 600, 450); }
                if (enemies[index].ordinal == 0) { back = probe.Spawn(index, 600, 550); }
            }
            if (front == nullptr || back == nullptr) { return 1; }
            for (unsigned tick = 0; tick < 100; ++tick) { back->model.enemy.Update(16); }
            GameObjectRef round;
            for (const auto &gun : weapons) {
                std::vector<std::uint8_t> bytes;
                const auto &ref = gun.data.GetBulletRef();
                if (ref.IsNull() || !tables.ReadSectionResource(ref.packHash, GameSection::Bullet, ref.localIndex, bytes)) { continue; }
                CArrayInputStream input(bytes);
                CBullet::Template bullet;
                if (!bullet.Init(input)) { return 1; }
                if ((bullet.GetFlags() & 0x140) == 0x40) { round = ref; break; }
            }
            if (round.IsNull()) { return 1; }
            if (probeEffects.SpawnProjectile(round, 600, 350, 0, 90, 600, kBrotherCombatId, 0) == 0) { return 1; }
            float matrix[16];
            probe.PlayerMatrix(matrix);
            for (unsigned tick = 0; tick < 90; ++tick) { probeEffects.Update(player, matrix, 0, 16); }
            const float damage = back->model.enemy.combat.totalDamage;
            if (damage <= 0) { ++checkFailures; }
            std::printf("[feedback-piercing] bullet=%08x:%u front-pending=%d back-damage=%.2f failures=%u\n",
                round.packHash, round.localIndex, front->model.enemy.combat.collisionPending, damage, checkFailures);

            // CBullet::CanBeCulled :60583 retires a projectile that has left
            // the camera rectangle travelling away from it, and keeps one that
            // is still heading towards it. Same real BULLET template, same
            // update path; only the camera rectangle is supplied here.
            WeaponEffects cullEffects(toc, tables, program);
            CombatScene cullScene(tables, program, enemies, player, vitals, cullEffects, loaded.playerTemplate->gameScale);
            cullScene.Reset();
            cullScene.SetViewCenter(600, 450);
            cullScene.SetViewSize(200, 200);  // y in [350, 550]
            float cullMatrix[16];
            cullScene.PlayerMatrix(cullMatrix);
            // Both start just below the view. One travels away from it, one
            // towards it; the outbound one is the only one culled at once.
            if (cullEffects.SpawnProjectile(round, 600, 560, 0, 90, 600, kBrotherCombatId, 0) == 0) { return 1; }
            if (cullEffects.SpawnProjectile(round, 600, 560, 0, -90, 600, kBrotherCombatId, 0) == 0) { return 1; }
            // 160ms: the outbound one is well clear of the near edge and gone,
            // the inbound one has entered the view and is still travelling.
            for (unsigned tick = 0; tick < 10; ++tick) { cullEffects.Update(player, cullMatrix, 0, 16); }
            const std::size_t afterOutbound = cullEffects.GetBulletCount();
            // Out the far side, well before the 3000ms expiry could retire it.
            for (unsigned tick = 0; tick < 30; ++tick) { cullEffects.Update(player, cullMatrix, 0, 16); }
            const std::size_t afterCrossing = cullEffects.GetBulletCount();
            if (afterOutbound != 1 || afterCrossing != 0) { ++checkFailures; }
            std::printf("[feedback-cull] leaving-culled inbound-alive=%zu after-far-edge=%zu age=640ms failures=%u\n",
                afterOutbound, afterCrossing, checkFailures);
        }
        // Regression: the native bar size reads LEVEL variable 4, not the
        // revolution index. Exercise actual BIG enemies and the production draw data.
        {
            CLevel &level = session.GetLevel();
            const int savedWave = level.GetWave();
            const auto savedFlag = *level.VariableResolver(4);
            *level.VariableResolver(4) = 0;
            level.SetWave(0);
            const auto firstBars = scene.EnemyHealthBars();
            const auto screenBars = scene.EnemyHealthBars(1600.0f / 480);
            if (screenBars.empty() || screenBars[0].width != 100 || screenBars[0].height != 13) { ++checkFailures; }
            level.SetWave(level.GetWavesPerRevolution());
            const auto laterBars = scene.EnemyHealthBars();
            *level.VariableResolver(4) = 1;
            const auto flaggedBars = scene.EnemyHealthBars();
            if (firstBars.empty() || laterBars.empty() || flaggedBars.empty()) { ++checkFailures; }
            else {
                if (firstBars[0].width != laterBars[0].width || flaggedBars[0].width != firstBars[0].width * 2) { ++checkFailures; }
                std::printf("[audio-health-check] bar first=%.1f later=%.1f flag=%.1f failures=%u\n",
                    firstBars[0].width, laterBars[0].width, flaggedBars[0].width, checkFailures);
            }
            level.SetWave(savedWave);
            *level.VariableResolver(4) = savedFlag;
        }
        // A single-part original enemy supplies an independent GetBounds
        // centre. Check the final screen rectangle, not just its dimensions.
        {
            WeaponEffects anchorEffects(toc, tables, program);
            CombatScene anchorScene(tables, program, enemies, player, vitals, anchorEffects, loaded.playerTemplate->gameScale);
            CombatEnemy *target = nullptr;
            for (std::size_t index = 0; index < enemies.size(); ++index) {
                if (enemies[index].packHash == CStringToKey("pack1") && enemies[index].ordinal == 0) {
                    target = anchorScene.Spawn(index, 640, 480);
                    break;
                }
            }
            if (target == nullptr || target->model.enemy.GetPartCount() != 1) { ++checkFailures; }
            else {
                const auto *mesh = target->model.enemy.GetPart(0).controller.GetAnimation().GetMesh();
                if (mesh == nullptr) { ++checkFailures; }
                else {
                    const float centre = target->model.enemy.combat.x + int(mesh->GetBounds().centerX);
                    float maxError = 0;
                    unsigned cases = 0;
                    for (int screenWidth : {1024, 1600, 1920}) {
                        const int screenHeight = screenWidth * 3 / 4;
                        for (float zoom : {0.5f, 1.0f, 2.6f}) {
                            auto bars = anchorScene.EnemyHealthBars(std::min(screenWidth / 480.0f, screenHeight / 320.0f));
                            ProjectEnemyHealthBars(bars, 100, 200, zoom, screenWidth, screenHeight);
                            if (bars.size() != 1) { ++checkFailures; continue; }
                            const float actual = (bars[0].x + bars[0].width * 0.5f) * screenWidth / 1024;
                            const float expected = (centre - 100) * zoom;
                            maxError = std::max(maxError, std::abs(actual - expected));
                            ++cases;
                        }
                    }
                    if (maxError > 0.01f || cases != 9) { ++checkFailures; }
                    std::printf("[healthbar-alignment-check] cases=%u max-centre-error-px=%.3f failures=%u\n", cases, maxError, checkFailures);
                }
            }
        }
        // Reproduce a group death through the original enemy export and the
        // same CombatScene/WeaponEffects path used by ordinary combat.
        CAudioPlayer backendAudio;
        std::vector<std::uint64_t> deathWavs;
        // A real SOUNDEFFECT reference, for the one-voice-per-WAV check below.
        GameObjectRef effectSound;
        // Effects headroom: the configured 0..10 dial reaches the mix as
        // dial x 0.1, the same scale the original's voices use.
        const float configuredGain = GameHostSettings().effectsVolume * 0.1f;
        if (std::abs(CAudioPlayer::GetEffectsGain() - configuredGain) > 0.001f) { ++checkFailures; }
        std::printf("[audio-health-check] effects-dial=%d gain=%.2f music-gain=0.30 failures=%u\n",
            GameHostSettings().effectsVolume, CAudioPlayer::GetEffectsGain(), checkFailures);
        for (unsigned kinds = 1; kinds <= 2; ++kinds) {
            std::vector<GameObjectRef> batchDeathSounds;
            WeaponEffects deathEffects(toc, tables, program);
            CombatScene deathScene(tables, program, enemies, player, vitals, deathEffects, loaded.playerTemplate->gameScale);
            deathScene.Reset();
            for (std::size_t index = 0; index < enemies.size(); ++index) {
                if (enemies[index].packHash != CStringToKey("pack1") || enemies[index].ordinal >= kinds) { continue; }
                for (unsigned count = 0; count < 12; ++count) {
                    CombatEnemy *actor = deathScene.Spawn(index, 800, 500);
                    if (actor == nullptr) { ++checkFailures; continue; }
                    actor->model.enemy.Damage(actor->model.enemy.combat.health);
                    if (count == 0) {
                        const auto moveIndex = actor->model.enemy.GetPart(0).controller.GetMoveIndex();
                        if (moveIndex >= 0) {
                            for (const auto &sound : enemies[index].moveSet.GetMoves()[moveIndex].sounds) {
                                std::vector<std::uint8_t> bytes;
                                const auto pack = enemies[index].moveSet.GetPackHash();
                                const auto key = (std::uint64_t(pack) << 32) | sound.soundId;
                                if (!tables.ReadSectionResource(pack, GameSection::Wav, sound.soundId, bytes) || !backendAudio.Load(key, bytes)) { ++checkFailures; continue; }
                                if (std::find(deathWavs.begin(), deathWavs.end(), key) == deathWavs.end()) { deathWavs.push_back(key); }
                                GameObjectRef deathSound;
                                deathSound.packHash = pack;
                                deathSound.localIndex = sound.soundId;
                                batchDeathSounds.push_back(deathSound);
                                std::printf("[audio-health-check] death-move-wav=%08x:%u enemy=%s\n", pack, sound.soundId, enemies[index].owner.c_str());
                            }
                        }
                    }
                    if (kinds == 2 && count == 0) {
                        // Inspect, but do not consume, the original death export.
                        for (const auto &action : actor->model.enemy.combat.actions) {
                            if (action.kind != EnemyAction::Kind::Sound) { continue; }
                            std::vector<std::uint8_t> bytes;
                            if (!tables.ReadSectionResource(action.resource.packHash, GameSection::SoundEffect, action.resource.localIndex, bytes)) { ++checkFailures; continue; }
                            CArrayInputStream input(bytes);
                            CGameAssetRef wav;
                            wav.Init(input);
                            if (input.Overran() || wav.assetId < 0 || !tables.ReadSectionResource(wav.packHash, GameSection::Wav, wav.assetId, bytes)) { ++checkFailures; continue; }
                            const auto key = (std::uint64_t(wav.packHash) << 32) | wav.assetId;
                            if (!backendAudio.Load(key, bytes)) { ++checkFailures; continue; }
                            deathWavs.push_back(key);
                            effectSound = action.resource;
                            std::printf("[audio-health-check] death-wav=%08x:%d enemy=%s\n", wav.packHash, wav.assetId, enemies[index].owner.c_str());
                        }
                    }
                }
            }
            deathScene.Update(16, 0, 0, false);
            const auto sounds = deathEffects.GetSoundCueCount();
            if (sounds != kinds) { ++checkFailures; }
            std::printf("[audio-health-check] group-death kinds=%u actors=%u sounds=%zu expected=%u failures=%u\n",
                kinds, kinds * 12, sounds, kinds, checkFailures);
            // Repeat the actual authored sound across a production tick
            // boundary. Do not assume a random death move always cues at t=0.
            deathScene.Update(16, 0, 0, false);
            const auto beforeRepeat = deathEffects.GetSoundCueCount();
            for (const auto &sound : batchDeathSounds) { deathEffects.PlayMoveSound(sound); }
            // Host audio adaptation: the copy already playing still covers it.
            if (deathEffects.GetSoundCueCount() != beforeRepeat) { ++checkFailures; }
            std::printf("[audio-health-check] next-tick kinds=%u new-sounds=%zu failures=%u\n",
                kinds, deathEffects.GetSoundCueCount() - beforeRepeat, checkFailures);
            // ... and is audible again once that copy has finished. Its own
            // scene has no actors, so nothing else can cue a sound meanwhile.
            WeaponEffects windowEffects(toc, tables, program);
            CombatScene windowScene(tables, program, enemies, player, vitals, windowEffects, loaded.playerTemplate->gameScale);
            windowScene.Reset();
            const GameObjectRef &repeated = batchDeathSounds.front();
            windowEffects.PlayMoveSound(repeated);
            const auto opened = windowEffects.GetSoundCueCount();
            windowScene.Update(16, 0, 0, false);
            windowEffects.PlayMoveSound(repeated);
            const auto covered = windowEffects.GetSoundCueCount();
            for (unsigned tick = 0; tick < 250; ++tick) { windowScene.Update(16, 0, 0, false); }
            windowEffects.PlayMoveSound(repeated);
            const auto reopened = windowEffects.GetSoundCueCount();
            if (opened != 1 || covered != 1 || reopened != 2) { ++checkFailures; }
            std::printf("[audio-health-check] move-window wav=%08x:%u first=%zu covered=%zu after-4s=%zu failures=%u\n",
                repeated.packHash, repeated.localIndex, opened, covered, reopened, checkFailures);
            // Gun-style cues keep their rate but never stack: every repeat
            // restarts the one voice that WAV is allowed, so a fast weapon
            // stays at the level its WAV was authored at.
            if (effectSound.IsNull()) {
                // Any real SOUNDEFFECT entry will do; take the first that
                // resolves to a WAV rather than inventing a resource.
                const auto corePack = toc.GetPack(toc.GetCorePackIndex())->GetPackHash();
                const unsigned soundCount = tables.GetObjectPack(toc.GetCorePackIndex()).GetObjectCount(GameSection::SoundEffect);
                for (unsigned index = 0; index < soundCount && effectSound.IsNull(); ++index) {
                    std::vector<std::uint8_t> bytes;
                    if (!tables.ReadSectionResource(corePack, GameSection::SoundEffect, index, bytes)) { continue; }
                    CArrayInputStream input(bytes);
                    CGameAssetRef wav;
                    wav.Init(input);
                    if (input.Overran() || wav.assetId < 0) { continue; }
                    if (!tables.ReadSectionResource(wav.packHash, GameSection::Wav, wav.assetId, bytes)) { continue; }
                    effectSound.packHash = corePack;
                    effectSound.localIndex = static_cast<std::uint8_t>(index);
                }
            }
            if (!effectSound.IsNull()) {
                GunCue sound;
                sound.kind = GunCue::Kind::Sound;
                sound.resource = effectSound;
                const auto before = windowEffects.GetSoundCueCount();
                unsigned peakVoices = 0;
                for (unsigned tick = 0; tick < 5; ++tick) {
                    windowEffects.Emit(sound, 600, 450, 0, 0, kPlayerCombatId);
                    peakVoices = std::max(peakVoices, windowEffects.GetVoiceCount());
                    windowScene.Update(16, 0, 0, false);
                }
                const auto retriggers = windowEffects.GetSoundCueCount() - before;
                if (retriggers != 5 || peakVoices > 1) { ++checkFailures; }
                std::printf("[audio-health-check] one-voice sound=%08x:%u retriggers=%zu peak-voices=%u failures=%u\n",
                    effectSound.packHash, effectSound.localIndex, retriggers, peakVoices, checkFailures);
            }
        }
        if (deathWavs.size() != 2 || deathWavs[0] == deathWavs[1]) { ++checkFailures; }
        else { checkFailures += backendAudio.CheckSilentPlayback(deathWavs[0], deathWavs[1]); }
        if (simultaneousHits == 0) { ++checkFailures; }
        std::printf("[feedback-hit] simultaneous-templates=%u failures=%u\n", simultaneousHits, checkFailures);
        std::printf("[feedback-check] failures=%u\n", checkFailures);
        // Capture through the production map/HUD draw, centred on the spire.
        session.Restart(startX, startY, startFacing);
        for (unsigned tick = 0; tick < 300; ++tick) { session.Update(16, 0, 0, false); }
        for (const PlacedProp &prop : loaded.props) {
            if (!prop.active || prop.sprite->interactiveKind != InteractivePropKind::Spire) { continue; }
            scene.playerX = prop.x + 150;
            scene.playerY = prop.y - 30;
            for (std::size_t index = 0; index < enemies.size(); ++index) {
                // ENEMY20's actual states 4/6 show/hide the bar. ENEMY3's
                // actual spawn export hides it. Do not write variable 15 here.
                if (enemies[index].packHash == CStringToKey("pack1") && enemies[index].ordinal == 3) {
                    const auto before = scene.EnemyHealthBars().size();
                    CombatEnemy *hidden = scene.Spawn(index, prop.x + 270, prop.y - 80);
                    if (hidden == nullptr || hidden->model.enemy.combat.variables[15] != 0 || scene.EnemyHealthBars().size() != before) { ++checkFailures; }
                    std::printf("[audio-health-check] authored-hidden enemy=pack1:3 failures=%u\n", checkFailures);
                }
                if (enemies[index].packHash == CStringToKey("pack1") && enemies[index].ordinal == 20) {
                    CombatEnemy *target = scene.Spawn(index, prop.x + 270, prop.y + 80);
                    if (target != nullptr) {
                        const auto before = scene.EnemyHealthBars().size();
                        if (target->model.enemy.combat.variables[15] != 0) { ++checkFailures; }
                        target->model.enemy.SetState(4);
                        if (target->model.enemy.combat.variables[15] != 1 || scene.EnemyHealthBars().size() != before + 1) { ++checkFailures; }
                        target->model.enemy.SetState(6);
                        if (target->model.enemy.combat.variables[15] != 0 || scene.EnemyHealthBars().size() != before) { ++checkFailures; }
                        target->model.enemy.SetState(4);
                        std::printf("[audio-health-check] authored-toggle enemy=pack1:20 states=4/6 failures=%u\n", checkFailures);
                    }
                }
            }
            break;
        }
        capturePath = "out/healthbar-alignment.png";
        for (unsigned tick = 0; tick < 30; ++tick) { session.Update(16, 0, 0, false); }
    }
    if (check) { checkFailures += session.CheckLevelSounds(); }
    if (check) { checkFailures += props.CheckEntryRoutes(); }
    if (check) { checkFailures += session.CheckTriggerRoutes(startX, startY, startFacing); }
    if (check) {
        // Inspect the actual placed resources, after LEVEL messages and Bind.
        for (const PlacedProp &prop : loaded.props) {
            if (!prop.active || prop.runtime == nullptr) { continue; }
            std::printf("[map-interaction] prop=%08x:%u id=%d pos=%.1f,%.1f state=%u removed=%d health=%.1f animations=%d,%d,%d body=%zu bullet=%zu\n",
                prop.sprite->resource.packHash, prop.sprite->resource.localIndex, prop.objectId, prop.x, prop.y,
                prop.runtime->GetStateId(), prop.runtime->IsRemoved(), prop.runtime->GetHealth(),
                prop.runtime->GetAnimation(0), prop.runtime->GetAnimation(1), prop.runtime->GetAnimation(2),
                prop.runtime->GetCollision().GetEdges().size(), prop.runtime->GetCollision(true).GetEdges().size());
        }
    }
    if (check && withBrother) {
        const float distance = std::hypot(scene.playerX - brother.x, scene.playerY - brother.y);
        const bool separated = distance >= scene.GetPlayerRadius() * 2;
        std::printf("[brother-spawn-check] player=%.1f,%.1f brother=%.1f,%.1f distance=%.1f separated=%d\n",
            scene.playerX, scene.playerY, brother.x, brother.y, distance, separated);
        if (!separated) { ++checkFailures; }
        // The first visible frame must already use the selected idle pose,
        // even while the level intro postpones the first simulation tick.
        auto &torso = brotherModel.weapon->brother.GetTorso();
        const int torsoIndex = torso.GetMeshConfigIndex();
        PlayerPart *part = brotherModel.parts[torsoIndex].get();
        if (brotherModel.weapon->brother.TorsoUsesWeapon()) { part = brotherModel.weapon->configs[torsoIndex].get(); }
        std::vector<float> expectedPose;
        const bool ready = torso.GetAnimation().Evaluate(expectedPose) && !expectedPose.empty() && expectedPose == part->pose;
        std::printf("[brother-pose-check] evaluated=%zu uploaded=%zu ready=%d\n", expectedPose.size(), part->pose.size(), ready);
        if (!ready) { ++checkFailures; }
    }
    if (check && tutorial) {
        const CScript &script = loaded.playerTemplate->script;
        for (unsigned index = 0; index < script.GetFunctions().size(); ++index) {
            std::printf("[tutorial-script] function=%u ", index);
            const CScriptCode &code = script.GetFunctions()[index];
            for (unsigned byte = 0; byte <= code.GetByteLength(); ++byte) { std::printf("%02X ", code.Begin()[byte]); }
            std::printf("\n");
        }
        for (unsigned index = 0; index < script.GetStates().size(); ++index) {
            const CScriptState &state = script.GetStates()[index];
            for (const auto &handler : state.GetExports()) {
                std::printf("[tutorial-script] state=%u export=%u ", index, handler.id);
                for (unsigned byte = 0; byte <= handler.code.GetByteLength(); ++byte) { std::printf("%02X ", handler.code.Begin()[byte]); }
                std::printf("\n");
            }
            const auto &code = state.GetEnterCode();
            if (code.Begin() != nullptr) {
                std::printf("[tutorial-script] state=%u enter ", index);
                for (unsigned byte = 0; byte <= code.GetByteLength(); ++byte) { std::printf("%02X ", code.Begin()[byte]); }
                std::printf("\n");
            }
        }
        auto pilot = CreateSurvivalInputDriver(scene, loaded.map.GetVisibleBounds());
        if (!pilot) { return 1; }
        int previousStep = -2;
        int grenadeWaitMs = 0;
        bool grenadeGateChecked = false;
        // Exercise the original script with actual movement and projectiles.
        for (int elapsed = 0; elapsed < 180000; elapsed += 16) {
            const int step = session.GetLevel().GetTutorialStep();
            if (step != previousStep) {
                std::printf("[tutorial-check] time=%d step=%d enemies=%d kills=%u grenades=%u\n",
                    elapsed, step, session.CountEnemies(), session.GetKills(), powerups.GetCount(13));
                previousStep = step;
                if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium)) { return 1; }
            }
            if (step == -1) { break; }
            if (step == 2) {
                SetPlayerInput(player, false, false);
                player.weapon->brother.OnSwapGun();
            }
            float moveX = 0, moveY = 0;
            pilot->Update(16, moveX, moveY);
            if (step == 0) { moveX = 1; moveY = 0; }
            float pickupX = 0, pickupY = 0;
            if (pickups.GetObjectPosition(501, pickupX, pickupY)) {
                moveX = pickupX - scene.playerX;
                moveY = pickupY - scene.playerY;
            }
            if (step == 5 && powerups.GetCount(13) > 0) {
                grenadeWaitMs += 16;
                if (grenadeWaitMs >= 5000) {
                    // User reference: the large tutorial enemy ignores gunfire
                    // until a grenade lands. Keep real fire active for five seconds.
                    if (!grenadeGateChecked) {
                        grenadeGateChecked = true;
                        if (session.GetKills() != 1 || session.CountEnemies() != 1) { ++checkFailures; }
                        std::printf("[tutorial-check] grenade-gate gunfire-ms=%d kills=%u alive=%d failures=%u\n",
                            grenadeWaitMs, session.GetKills(), session.CountEnemies(), checkFailures);
                    }
                    bool inGrenadeRange = false;
                    for (const auto &actor : scene.enemies) {
                        const auto &enemy = actor->model.enemy.combat;
                        if (enemy.dead || !enemy.enabled) { continue; }
                        const float dx = enemy.x - scene.playerX, dy = enemy.y - scene.playerY;
                        moveX = dx; moveY = dy;
                        if (std::hypot(dx, dy) <= 95) { inGrenadeRange = true; moveX = 0; moveY = 0; }
                    }
                    if (inGrenadeRange) { powerups.Select(13); powerups.Use(); }
                }
            }
            vitals.invincible = true;
            const bool fireGun = step != 2 && (step != 5 || grenadeWaitMs < 5000);
            session.Update(16, moveX, moveY, fireGun);
            if (player.weapon->brother.TakeWeaponSwap()) {
                const GameObjectRef &rifle = pickupProfile->configuration.guns[1];
                for (std::size_t index = 0; index < weapons.size(); ++index) {
                    if (weapons[index].packHash != rifle.packHash || weapons[index].ordinal != rifle.localIndex) { continue; }
                    if (!EquipControlledPlayer(tables, loaded, program, weapons[index])) { return 1; }
                    weaponSlot = index;
                    equippedWeaponSlot = 1;
                    break;
                }
            }
        }
        if (session.GetLevel().GetTutorialStep() != -1 || !grenadeGateChecked) { ++checkFailures; }
        std::printf("[tutorial-check] final-step=%d failures=%d\n", session.GetLevel().GetTutorialStep(), checkFailures);
        capturePath = "out/tutorial-check.png";
    }
    if (check && archiveMission == nullptr && !tutorial) {
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
            session.Restart(startX, startY, startFacing);
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
        session.Restart(startX, startY, startFacing);
        consumable.localIndex = 1;
        consumableProbe.AddPowerup(consumable, 1);
        powerupProbe.Select(1);
        if (powerupProbe.Use()) { ++checkFailures; }
        vitals.health = 1;
        if (!powerupProbe.Use() || vitals.health != std::min(vitals.maximum, 9.0f) || powerupProbe.GetCount() != 0) { ++checkFailures; }
        checkFailures += powerupProbe.failures;
        std::printf("[powerup-play-check] healing/cancel/repeat consumed=%u failures=%u\n", powerupProbe.consumed, checkFailures);
        unsigned airstrikeCase = 0;
        for (unsigned airstrikeIndex : {0u, 10u, 11u, 0u, 10u, 11u}) {
            const bool fromSelector = airstrikeCase++ >= 3;
            session.Restart(startX, startY, startFacing);
            WeaponEffects airstrikeEffects(toc, tables, program);
            CombatScene airstrikeScene(tables, program, enemies, player, vitals, airstrikeEffects, loaded.playerTemplate->gameScale);
            airstrikeScene.Reset();
            // Exercise the same session update as gameplay: movie-only tests
            // cannot detect actors continuing to move during an air strike.
            SurvivalSession airstrikeSession(airstrikeScene, loaded.map, enemies);
            if (!airstrikeSession.Load(toc, tables, toc.GetPack(packIndex)->GetPackHash(), mapIndex, archiveLevel, archiveMission != nullptr)) { return 1; }
            airstrikeSession.Restart(startX, startY, startFacing);
            CombatEnemy *target = airstrikeScene.Spawn(0, 700, 650);
            CombatEnemy *outside = airstrikeScene.Spawn(0, 4600, 650);
            if (target == nullptr || outside == nullptr) { return 1; }
            for (int elapsed = 0; elapsed < 1000; elapsed += 16) {
                target->model.enemy.Update(16);
                outside->model.enemy.Update(16);
            }
            target->model.enemy.TakeActions();
            outside->model.enemy.TakeActions();
            // Arena clamps initial spawns; explicitly place the radius probe
            // outside the blast only after its authored spawn state has matured.
            outside->model.enemy.combat.x = 600;
            outside->model.enemy.combat.y = 650;
            target->model.enemy.combat.health = 100000;
            target->model.enemy.combat.maxHealth = 100000;
            // Put the blast at a camera center far from the player. The target
            // must be hit there; the player's vicinity is outside every radius.
            airstrikeScene.SetViewCenter(5000, 650);
            target->model.enemy.combat.x = 5100;
            target->model.enemy.combat.y = 650;
            consumable.localIndex = static_cast<std::uint8_t>(airstrikeIndex);
            consumableProbe.AddPowerup(consumable, 2);
            PowerupScene airstrike(toc, tables, player, vitals, airstrikeScene, airstrikeEffects, consumableProbe);
            airstrikeSession.SetPowerups(&airstrike);
            if (!airstrike.Init() || !airstrike.Select(airstrikeIndex) || !airstrike.Use(fromSelector) || airstrike.Use() || airstrike.GetCount() != 1) { ++checkFailures; }
            if (target->model.enemy.combat.hitCount != 0) { ++checkFailures; }
            // Paused presentation has no elapsed time and must not finish a movie.
            for (unsigned repeat = 0; repeat < 10; ++repeat) { airstrike.Update(0); }
            if (airstrike.GetMoviePlayer().GetElapsed() != 0) { ++checkFailures; }
            unsigned splashTime = 0;
            unsigned movingFrames = 0;
            unsigned warningFrames = 0;
            unsigned closingFrames = 0;
            const float frozenX = airstrikeScene.playerX;
            const float frozenY = airstrikeScene.playerY;
            const float frozenEnemyX = target->model.enemy.combat.x;
            const float frozenEnemyY = target->model.enemy.combat.y;
            const float frozenHealth = vitals.health;
            // Haven's LEVEL also creates two map turrets. Freeze preserves
            // the starting count; it does not imply only our two probes exist.
            const std::size_t frozenEnemyCount = airstrikeScene.enemies.size();
            for (int elapsed = 0; elapsed < 12000 && airstrike.IsMovieActive(); elapsed += 16) {
                airstrikeSession.Update(16, 1, 0, true);
                if (airstrike.GetMoviePlayer().IsForegroundMovie()) { ++warningFrames; }
                if (airstrike.GetMoviePlayer().IsSelectorFrameClosing()) {
                    ++closingFrames;
                    if (closingFrames == 4 && airstrikeIndex == 0) {
                        glClearColor(0.04f, 0.05f, 0.07f, 1);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                        if (!airstrike.DrawMovies() || !window.SaveFrame("out/airstrike-frame-closing.png")) { ++checkFailures; }
                    }
                }
                if (airstrikeScene.playerX != frozenX || airstrikeScene.playerY != frozenY || airstrikeEffects.GetShotCount() != 0) { ++movingFrames; }
                if (target->model.enemy.combat.x != frozenEnemyX || target->model.enemy.combat.y != frozenEnemyY ||
                    vitals.health != frozenHealth || airstrikeScene.enemies.size() != frozenEnemyCount) { ++movingFrames; }
                if (airstrike.GetMoviePlayer().splashCount > 0 && splashTime == 0) { splashTime = elapsed + 16; }
                if (elapsed == 992) {
                    glClearColor(0.04f, 0.05f, 0.07f, 1);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    std::string suffix = "-equipped";
                    if (fromSelector) { suffix = "-selector"; }
                    if (!airstrike.DrawMovies() || !window.SaveFrame("out/airstrike-check-" + std::to_string(airstrikeIndex) + suffix + ".png")) { ++checkFailures; }
                    if (fromSelector) {
                        // Sample the actual selector frame away from the title,
                        // character and thin moving streaks. A movie-active flag
                        // alone passed while the whole frame was missing.
                        std::vector<std::uint8_t> pixels(200 * 400 * 4);
                        glReadPixels(0, 640, 200, 400, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                        unsigned framePixels = 0;
                        for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
                            if (pixels[pixel + 1] > 40 && pixels[pixel + 2] > 50) { ++framePixels; }
                        }
                        if (framePixels < 300) { ++checkFailures; }
                        std::printf("[airstrike-frame-check] item=%u pixels=%u failures=%u\n", airstrikeIndex, framePixels, checkFailures);
                    }
                }
            }
            float expectedDamage = 240;
            if (movingFrames != 0) { ++checkFailures; }
            if (fromSelector && warningFrames == 0) { ++checkFailures; }
            if (fromSelector && closingFrames == 0) { ++checkFailures; }
            if (!fromSelector && closingFrames != 0) { ++checkFailures; }
            if (airstrike.GetMoviePlayer().HasSelectorFrame()) { ++checkFailures; }
            std::printf("[airstrike-frame-lifecycle] item=%u selector=%d closing-frames=%u failures=%u\n",
                airstrikeIndex, fromSelector, closingFrames, checkFailures);
            if (airstrikeSession.GetLevel().IsPaused()) { ++checkFailures; }
            std::printf("[airstrike-route-check] item=%u selector=%d warning-frames=%u\n", airstrikeIndex, fromSelector, warningFrames);
            std::printf("[airstrike-freeze-check] item=%u moving-frames=%u failures=%u\n", airstrikeIndex, movingFrames, checkFailures);
            if (airstrikeIndex == 10) { expectedDamage = 500; }
            if (airstrikeIndex == 11) { expectedDamage = 1600; }
            if (airstrike.IsMovieActive() || airstrike.GetMoviePlayer().splashCount != 1 || splashTime < 1200 ||
                std::abs(target->model.enemy.combat.totalDamage - expectedDamage) > 0.01f ||
                outside->model.enemy.combat.hitCount != 0 || airstrike.GetCount() != 1) { ++checkFailures; }
            std::printf("[airstrike-check] item=%u time=%u movies=%u effects=%u damage=%.2f expected=%.2f outside-hits=%d stock=%u failures=%u\n",
                airstrikeIndex, splashTime, airstrike.GetMoviePlayer().movieCompletions, airstrike.GetMoviePlayer().effectCount,
                target->model.enemy.combat.totalDamage, expectedDamage, outside->model.enemy.combat.hitCount, airstrike.GetCount(), checkFailures);
            // Release the original intro normally, then verify real input works
            // again. A cleared pause flag alone does not prove gameplay resumed.
            for (unsigned frame = 0; frame < 100; ++frame) { airstrikeSession.Update(16, 0, 0, false); }
            const float resumedX = airstrikeScene.playerX;
            airstrikeSession.Update(16, 1, 0, true);
            if (airstrikeScene.playerX == resumedX) { ++checkFailures; }
            std::printf("[airstrike-resume-check] item=%u selector=%d moved=%d failures=%u\n",
                airstrikeIndex, fromSelector, airstrikeScene.playerX != resumedX, checkFailures);
            if (!airstrike.Use()) { ++checkFailures; }
            airstrike.Update(400);
            airstrike.Reset();
            for (int elapsed = 0; elapsed < 8000; elapsed += 16) { airstrike.Update(16); }
            if (airstrike.GetMoviePlayer().splashCount != 1 || airstrike.IsMovieActive() || airstrike.GetCount() != 0) { ++checkFailures; }
            checkFailures += airstrike.failures + airstrike.GetMoviePlayer().failures;
        }
        session.Restart(startX, startY, startFacing);
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
        session.Restart(startX, startY, startFacing);
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
        session.Restart(startX, startY, startFacing);
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
        session.Restart(startX, startY, startFacing);
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
        consumable.localIndex = 6;
        consumableProbe.AddPowerup(consumable, 2);
        powerupProbe.Select(6);
        if (!powerupProbe.Use() || powerupProbe.Use() || !player.weapon->brother.IsFrenzy() ||
            player.powerups.legacyFrenzyMs != 21000 || scene.GetProjectilePowerupMultiplier(kPlayerCombatId) != 1) { ++checkFailures; }
        if (!EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot]) ||
            player.powerups.legacyFrenzyMs != 21000) { ++checkFailures; }
        AdvancePlayer(player, 20000);
        powerupProbe.Select(18);
        if (!powerupProbe.Use() || !player.weapon->brother.IsFrenzyType(0)) { ++checkFailures; }
        AdvancePlayer(player, 1000);
        if (player.weapon->brother.IsFrenzy() || player.weapon->brother.IsFrenzyType(0) ||
            player.powerups.legacyFrenzyMultiplier[0] != 1) { ++checkFailures; }
        std::printf("[tantrum-check] duration=21000 duplicate-blocked=1 equipment-preserved=1 stop-all-boosts=1 failures=%u\n", checkFailures);
        GameObjectRef equippedLeft = consumable;
        GameObjectRef equippedRight = consumable;
        equippedLeft.localIndex = 14;
        equippedRight.localIndex = 5;
        if (!powerupProbe.Equip(0, equippedLeft) || !powerupProbe.Equip(1, equippedRight)) { ++checkFailures; }
        if (!consumableProbe.SaveToDisk("out/powerup-profile-check.dat")) { ++checkFailures; }
        CProfileManager restoredConsumables;
        restoredConsumables.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), consumableRefinement);
        if (!restoredConsumables.LoadFromDisk("out/powerup-profile-check.dat") ||
            restoredConsumables.GetPowerupCount(consumable) != 1 ||
            restoredConsumables.configuration.powerups != consumableProbe.configuration.powerups) { ++checkFailures; }
        // Exercise Equip -> original DataStore write -> fresh battle host;
        // use an isolated copy, never the user's active profile or saves/.
        const auto equipDirectory = std::filesystem::path("out/powerup-equip-check") / std::to_string(window.GetTicksMs());
        CProfileManager equipProfile;
        if (!LoadNativeProfile(toc, tables, equipProfile, equipDirectory, "saves")) { return 1; }
        PowerupScene equipHost(toc, tables, player, vitals, scene, effects, equipProfile);
        if (!equipHost.Init() || !equipHost.Equip(0, equippedLeft) || !equipHost.Equip(1, equippedRight) ||
            !equipProfile.SaveToDisk(equipDirectory)) { ++checkFailures; }
        CProfileManager reloadProfile;
        if (!LoadNativeProfile(toc, tables, reloadProfile, equipDirectory)) { return 1; }
        PowerupScene reloadHost(toc, tables, player, vitals, scene, effects, reloadProfile);
        if (!reloadHost.Init()) { return 1; }
        const GameObjectRef reloadLeft = reloadHost.GetEquipped(0);
        const GameObjectRef reloadRight = reloadHost.GetEquipped(1);
        if (reloadLeft.packHash != equippedLeft.packHash || reloadLeft.localIndex != equippedLeft.localIndex ||
            reloadRight.packHash != equippedRight.packHash || reloadRight.localIndex != equippedRight.localIndex) { ++checkFailures; }
        std::printf("[powerup-equip-check] left=%u right=%u native-reload=1 host-reload=1 failures=%u\n",
            reloadLeft.localIndex, reloadRight.localIndex, checkFailures);
        std::printf("[powerup-play-check] shield/defense/priority/expiry/weapon-swap/save failures=%u\n", checkFailures);
        session.Restart(startX, startY, startFacing);
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
        session.Restart(startX, startY, startFacing);
        CombatHit forceProbe;
        const float originalMaximum = vitals.maximum;
        const float originalBrotherMaximum = brother.vitals.maximum;
        for (float maximum : {20.0f, 100.0f, 205.0f}) {
            session.Restart(startX, startY, startFacing);
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
        session.Restart(startX, startY, startFacing);
        forceProbe.ownerType = 1;
        forceProbe.x = startX - 40;
        forceProbe.y = startY;
        scene.Splash(forceProbe, 60, 360, 100, 100);
        if (scene.playerX != startX || scene.playerY != startY) { ++checkFailures; }
        scene.Update(16, 0, 0, false);
        if (std::hypot(scene.playerX - startX, scene.playerY - startY) > 4) { ++checkFailures; }
        std::printf("[survival-check] map force gradual displacement=%.2f failures=%u\n",
            std::hypot(scene.playerX - startX, scene.playerY - startY), checkFailures);
        session.Restart(startX, startY, startFacing);
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
        if (session.IsDeathComplete()) {
            std::printf("[death-check] FAIL: postgame opens on fatal hit before death animation\n");
            return 1;
        }
        session.Restart(startX, startY, startFacing);
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
            session.Restart(startX, startY, startFacing);
            brother.vitals.invincible = true;
        }
        // This harness uses real projectiles and enemy death scripts, with
        // invincibility only to keep the automated pilot running deterministically.
        vitals.invincible = true;
        const int targetWave = std::min(static_cast<int>(startWave + checkWaves), session.GetLevel().GetWaveLimit());
        auto pilot = CreateSurvivalInputDriver(scene, loaded.map.GetVisibleBounds());
        if (!pilot) { return 1; }
        float previousDamage = 0;
        int stalledMs = 0;
        // Late waves contain hundreds of actors. Bound a wave generously, but
        // stop promptly when actual damage and kills cease for two minutes.
        for (int elapsed = 0; elapsed < static_cast<int>(checkWaves * 600000); elapsed += 16) {
            float moveX = 0;
            float moveY = 0;
            pilot->Update(16, moveX, moveY);
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
        pilot->Report();
        const unsigned unsupportedLevel = session.GetLevel().GetUnimplementedCallCount();
        const unsigned unsupportedSpawner = session.GetLevel().GetSpawner().GetUnsupportedCount();
        checkFailures += unsupportedLevel + unsupportedSpawner;
        std::printf("[survival-script-check] level=%u spawner=%u\n", unsupportedLevel, unsupportedSpawner);
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
        // CEnemySpawner::GetSpawnPointOffScreen keeps rule-driven spawns out of
        // the camera rectangle; on-screen ones would be the "in your face" case.
        if (session.GetOnScreenSpawns() != 0) { ++checkFailures; }
        std::printf("[survival-check] closest-spawn=%.1f on-screen-spawns=%u failures=%u\n",
            session.GetClosestSpawnDistance(), session.GetOnScreenSpawns(), checkFailures);
        // Sound cues are the audible one-shots after per-tick coalescing; they
        // are what a spread-out kill streak turns into.
        std::printf("[survival-check] shots=%zu sounds=%zu player=%.1f,%.1f stun=%d brother=%d\n",
            effects.GetShotCount(), effects.GetSoundCueCount(), scene.playerX, scene.playerY,
            vitals.stunMs, player.weapon->brother.GetStateId());
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
    if (check && horde) {
        vitals.invincible = true;
        auto pilot = CreateSurvivalInputDriver(scene, loaded.map.GetVisibleBounds());
        if (!pilot) { return 1; }
        const int initialWave = session.GetLevel().GetWave();
        int targetWave = initialWave + 1;
        if (initialWave == 0) { targetWave = 2; }
        for (int elapsed = 0; elapsed < 1800000 && session.GetLevel().GetWave() < targetWave; elapsed += 16) {
            float moveX = 0, moveY = 0;
            pilot->Update(16, moveX, moveY);
            session.Update(16, moveX, moveY, true);
            AdvanceProps(loaded.props, 16);
            AdvanceTileLayers(loaded.map, 16);
        }
        // Finish the real HUD transition callback; it restores BOKOR time scale
        // and releases its next state. A first-wave-only check misses this seam.
        for (int elapsed = 0; elapsed < 60000 && session.IsTransitioning(); elapsed += 16) { session.Update(16, 0, 0, false); }
        if (session.GetLevel().GetWave() < targetWave || session.GetKills() == 0 || scene.GetScore() == 0 ||
            session.GetLevel().GetObjectTimeScale() != 1 ||
            scene.invalidSpawns != 0 || session.GetLevel().GetUnimplementedCallCount() != 0 ||
            session.GetLevel().GetSpawner().GetUnsupportedCount() != 0) { ++checkFailures; }
        std::printf("[horde-check] initial=%d next=%d spawned=%u kills=%u stopwatch=%d slow=%.4f failures=%u\n",
            initialWave, session.GetLevel().GetWave(), scene.spawned, session.GetKills(),
            session.GetLevel().GetStopwatchTime(), session.GetLevel().GetObjectTimeScale(), checkFailures);
        capturePath = "out/horde-check-" + std::to_string(startWave) + ".png";
    }
    if (check && archiveMission != nullptr && !horde) {
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
        auto pilot = CreateSurvivalInputDriver(scene, loaded.map.GetVisibleBounds());
        if (!pilot) { return 1; }
        unsigned goal = 0, reached = 0;
        int goalElapsed = 0;
        for (int elapsed = 0; elapsed < 360000 && !session.GetLevel().IsCleared(); elapsed += 16) {
            float moveX = 0, moveY = 0;
            pilot->Update(16, moveX, moveY);
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
    int lastSavedTutorialStep = session.GetLevel().GetTutorialStep();
    bool shopOpen = false, itemChoice = false;
    GameObjectRef leftPowerup = powerups.GetEquipped(0);
    GameObjectRef rightPowerup = powerups.GetEquipped(1);
    const bool checkControls = gameContext != nullptr && gameContext->checkControls;
    unsigned controlFrame = 0;
    // The keyboard fixture needs owned charges; selector purchases/equipping
    // are separately exercised by RunOriginalPowerupSelectorCheck.
    if (checkControls) { pickupProfile->AddPowerup(rightPowerup, 2); }
    const std::uint64_t controlsInitialBucks = pickupProfile->warbucks;
    const bool controlsInitialSound = pickupProfile->soundEnabled;
    const unsigned controlsInitialGrenades = pickupProfile->GetPowerupCount(rightPowerup);
    const GameObjectRef controlsInitialLeft = leftPowerup;
    // Run the real HUD hit tests and the same host action path. No OS input.
    constexpr SurvivalHudAction controlActions[] = {SurvivalHudAction::OpenShop, SurvivalHudAction::CloseShop,
        SurvivalHudAction::SwapWeapon, SurvivalHudAction::Pause, SurvivalHudAction::Resume};
    constexpr unsigned controlClickCount = sizeof(controlActions) / sizeof(controlActions[0]);
    // Test through the production key dispatcher after the mouse regression.
    const KeyCode controlKeys[] = {KeyCode::Digit1, KeyCode::Escape, KeyCode::Digit2,
        KeyCode::Q, KeyCode::None, KeyCode::E, KeyCode::F, KeyCode::R};
    const unsigned controlKeyCount = sizeof(controlKeys) / sizeof(controlKeys[0]);
    unsigned controlsBeforeKeys = 0;
    unsigned controlsLeftBeforeKeys = 0;
    // Presentation reads a snapshot; its actions re-enter the same keyboard path.
    const auto buildHudState = [&]() {
        SurvivalHudState state;
        state.health = vitals.health;
        state.maximumHealth = vitals.maximum;
        state.brotherHealth = brother.vitals.health;
        state.brotherMaximumHealth = brother.vitals.maximum;
        state.withBrother = withBrother;
        state.wave = std::min(session.GetLevel().GetWave(), session.GetLevel().GetWaveLimit() - 1);
        state.horde = horde;
        state.score = scene.GetScore();
        state.killStreak = scene.GetKillStreak();
        state.stopwatchMs = session.GetLevel().GetStopwatchTime();
        state.bossIntroSerial = session.GetLevel().GetBossIntroSerial();
        state.xplodiumMultiplier = session.GetLevel().GetXplodiumMultiplierPercent();
        state.level = progress.GetLevel();
        state.experience = progress.GetExperienceInLevel();
        state.experienceDelta = progress.GetExperienceDelta();
        state.xplodium = scene.GetXplodium();
        state.kills = scene.GetTotalKills();
        state.enemies = session.CountEnemies();
        state.weaponSlot = equippedWeaponSlot;
        state.swapKeyDown = window.IsKeyDown(KeyCode::Digit2) || window.IsKeyDown(KeyCode::N) || window.IsKeyDown(KeyCode::M);
        state.weapon = weapons[weaponSlot].name;
        if (gameContext != nullptr) {
            state.guns[0] = gameContext->profile.configuration.guns[0];
            state.guns[1] = gameContext->profile.configuration.guns[1];
        } else {
            state.guns[0].packHash = weapons[weaponSlot].packHash;
            state.guns[0].localIndex = static_cast<std::uint8_t>(weapons[weaponSlot].ordinal);
        }
        state.paused = paused;
        state.shopOpen = shopOpen;
        state.itemChoice = itemChoice;
        state.leftPowerup = leftPowerup;
        state.rightPowerup = rightPowerup;
        state.leftCount = pickupProfile->GetPowerupCount(leftPowerup);
        state.rightCount = pickupProfile->GetPowerupCount(rightPowerup);
        state.inventory = pickupProfile->powerups;
        state.coins = pickupProfile->coins;
        state.warbucks = pickupProfile->warbucks;
        state.soundEnabled = pickupProfile->soundEnabled;
        state.musicEnabled = pickupProfile->musicEnabled;
        state.originalUi = true;
        state.dockedSticks = pickupProfile->options.DockedSticks();
        state.powerupStatus.healthPercent = static_cast<int>(std::lround(vitals.health * 100 / vitals.maximum));
        state.powerupStatus.shield = player.weapon->brother.IsShield();
        state.powerupStatus.frenzy = player.weapon->brother.IsFrenzy();
        state.powerupStatus.autoFire = player.weapon->brother.IsAutoFire();
        state.powerupStatus.turret = player.weapon->brother.IsTurretActive();
        for (unsigned type = 0; type < 3; ++type) { state.powerupStatus.frenzyTypes[type] = player.weapon->brother.IsFrenzyType(type); }
        state.dead = vitals.dead;
        state.inputHidden = vitals.inputHidden;
        state.cleared = session.GetLevel().IsCleared();
        state.transitioning = session.IsTransitioning();
        state.transitionTime = session.GetTransitionElapsed();
        state.perfectBonus = scene.GetLastWaveBonus();
        state.dialog = session.GetDialogText();
        state.tutorialStep = session.GetLevel().GetTutorialStep();
        if (archiveMission != nullptr) { state.mission = archiveMission->title; }
        if (powerups.GetSelected() != nullptr) {
            state.item = powerups.GetSelected()->name;
            state.itemCount = powerups.GetCount();
        }
        const char *buffNames[] = {"SHIELD", "ATTACK", "DEFENSE", "SPEED", "AUTO AIM", "TANTRUM"};
        const int buffTimers[] = {player.powerups.shieldMs, player.powerups.frenzyMs[0],
            player.powerups.frenzyMs[1], player.powerups.frenzyMs[2], player.powerups.autoFireMs, player.powerups.legacyFrenzyMs};
        for (unsigned index = 0; index < 6; ++index) {
            if (buffTimers[index] <= 0) { continue; }
            if (!state.buffs.empty()) { state.buffs += "   "; }
            state.buffs += buffNames[index];
            state.buffs += " " + std::to_string((buffTimers[index] + 999) / 1000) + "S";
        }
        if (player.weapon->brother.IsTurretActive()) { state.buffs += "   TURRET ACTIVE"; }
        return state;
    };
    int accumulator = 0;
    std::uint64_t previous = window.GetTicksMs();
    Camera camera;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    std::printf("[survival] WASD move, mouse aim/fire, Q/E powerups, 1 shop, 2 swap weapon, Esc/space pause\n");
    auto debugTicks = window.GetTicksMs();
    float debugFrameMs = 16.7f;
    // Deterministic 1,200 rendered frames, with real waves, AI and projectiles.
    std::unique_ptr<ISurvivalInputDriver> performancePilot;
    std::ofstream performanceReport;
    std::vector<double> performanceCpu;
    unsigned performanceFrame = 0;
    if (performanceStudy) {
        performancePilot = CreateSurvivalInputDriver(scene, loaded.map.GetVisibleBounds());
        if (!performancePilot) { return 1; }
        vitals.invincible = true;
        if (!window.SetVSync(false)) { return 1; }
        std::printf("[performance] vsync=0 fixed-step=16ms rendered-frames=1200\n");
        performanceReport.open("out/performance-frames.csv");
        performanceReport << "frame,update_ms,geometry_ms,world_ms,hud_ms,present_ms,alive,spawned\n";
    }
    while (window.PumpEvents()) {
        const auto performanceStart = std::chrono::steady_clock::now();
        const auto frameTicks = window.GetTicksMs();
        debugFrameMs = debugFrameMs * 0.9f + static_cast<float>(frameTicks - debugTicks) * 0.1f;
        survivalHud.AdvanceMenu(static_cast<unsigned>(frameTicks - debugTicks));
        debugTicks = frameTicks;
        for (std::string cheat = window.TakeCheatCode(); !cheat.empty(); cheat = window.TakeCheatCode()) {
            if (cheat == "chd") { GameHostSettings().debugMode = !GameHostSettings().debugMode; }
            if (cheat == "chc") { GameHostSettings().isConnected = !GameHostSettings().isConnected; }
            if (cheat == "chi") { vitals.invincible = !vitals.invincible; }
            if (cheat == "chh" && !vitals.dead) { vitals.health = vitals.maximum; }
            if (cheat == "stsuicide" && !powerups.IsMovieActive() && scene.Suicide()) {
                paused = false;
                shopOpen = false;
                itemChoice = false;
                std::printf("[death] suicide started\n");
            }
            if (cheat == "stboss") {
                if (session.SkipToBoss()) { paused = false; shopOpen = false; itemChoice = false; }
                effects.SetPaused(paused || shopOpen);
                accumulator = 0;
                previous = window.GetTicksMs();
            }
            if (gameContext != nullptr) {
                if (cheat == "chm") { gameContext->profile.coins += 5000; gameContext->profile.warbucks += 500; }
                if (cheat == "cht") { ++gameContext->profile.dailyDayOffset; }
                if (cheat == "chw") { gameContext->profile.clearedWaves.fill(500); }
                if (!gameContext->profile.SaveToDisk(gameContext->savePath)) { return 1; }
            }
            std::printf("[cheat] %s invincible=%d\n", cheat.c_str(), vitals.invincible);
        }
        int inputWidth = 0, inputHeight = 0;
        window.GetDrawableSize(inputWidth, inputHeight);
        float inputX = -1, inputY = -1;
        window.GetMousePosition(inputX, inputY);
        inputX *= 1024.0f / std::max(1, inputWidth);
        inputY *= 768.0f / std::max(1, inputHeight);
        const SurvivalHudState inputState = buildHudState();
        float menuScroll = window.TakeWheelDelta();
        int menuDragX = 0, menuDragY = 0;
        window.TakeDragDelta(menuDragX, menuDragY);
        survivalHud.ScrollMenuInput(inputState, menuScroll,
            float(menuDragX) * 1024 / inputWidth, float(menuDragY) * 768 / inputHeight);
        bool pointerDown = window.IsLeftMouseDown();
        if (checkControls && controlFrame < controlClickCount) {
            // Bind and finish authored menu entrance before querying its live hitbox.
            if (!survivalHud.Draw(inputState)) { return 1; }
            survivalHud.AdvanceMenu(2000);
            if (!survivalHud.Draw(inputState)) { return 1; }
            MovieRegion target;
            if (!survivalHud.FindActionRegion(inputState, controlActions[controlFrame], target)) {
                std::printf("[combat-controls-check] missing original action=%d frame=%u\n", int(controlActions[controlFrame]), controlFrame);
                return 1;
            }
            inputX = target.x + target.width / 2;
            inputY = target.y + target.height / 2;
            survivalHud.Pointer(inputState, inputX, inputY, false);
            pointerDown = true;
        }
        const bool hudOwnsPointer = survivalHud.CapturesPointer(inputState, inputX, inputY);
        SurvivalHudAction action = survivalHud.Pointer(inputState, inputX, inputY, pointerDown);
        // Input-pad controls cannot interrupt the active powerup presentation.
        if (powerups.IsMovieActive()) { action = SurvivalHudAction::None; }
        if (action == SurvivalHudAction::Exit) {
            // Surrender leaves a paused menu; the same BGM continues into results.
            music.SetPaused(false);
            music.SetVolume(1.0f);
            break;
        }
        if (action == SurvivalHudAction::OpenShop) { shopOpen = true; itemChoice = false; accumulator = 0; }
        if (action == SurvivalHudAction::CloseShop) { shopOpen = false; itemChoice = false; }
        if (action == SurvivalHudAction::CancelItem) { itemChoice = false; }
        const StoreEntry *shopItem = survivalHud.SelectedItem();
        if (shopItem != nullptr && (action == SurvivalHudAction::BuyItem || action == SurvivalHudAction::SelectItem)) {
            const GameObjectRef &resource = shopItem->data.objects.front().object;
            if (action == SurvivalHudAction::BuyItem) {
                const PurchaseResult result = pickupProfile->AcquireItem(shopItem->data, progress.GetLevel());
                if (result == PurchaseResult::Purchased && gameContext != nullptr && !pickupProfile->SaveToDisk(gameContext->savePath)) { return 1; }
                // CPowerUpSelector::OnPurchase :184861 updates quantity in place.
                // An icon tap, not a purchase, enters the use/equip selection state.
                itemChoice = false;
                survivalHud.ReportSelectorPurchase(result, inputState);
            } else {
                itemChoice = pickupProfile->GetPowerupCount(resource) > 0;
            }
        }
        if (shopItem != nullptr && itemChoice && (action == SurvivalHudAction::EquipLeft || action == SurvivalHudAction::EquipRight || action == SurvivalHudAction::UseNow)) {
            const GameObjectRef &resource = shopItem->data.objects.front().object;
            if (action == SurvivalHudAction::EquipLeft || action == SurvivalHudAction::EquipRight) {
                unsigned slot = 0;
                if (action == SurvivalHudAction::EquipRight) { slot = 1; }
                if (powerups.Equip(slot, resource)) {
                    leftPowerup = powerups.GetEquipped(0);
                    rightPowerup = powerups.GetEquipped(1);
                    itemChoice = false;
                    if (gameContext != nullptr && !pickupProfile->SaveToDisk(gameContext->savePath)) { return 1; }
                }
            }
            if (action == SurvivalHudAction::UseNow) {
                if (powerups.SelectResource(resource) && powerups.Use(true)) { shopOpen = false; itemChoice = false; }
            }
        }
        if (action == SurvivalHudAction::UseLeft && !paused && !shopOpen && !session.IsTransitioning()) {
            if (powerups.SelectResource(leftPowerup)) { powerups.Use(); }
        }
        if (action == SurvivalHudAction::Sound || action == SurvivalHudAction::Music || action == SurvivalHudAction::DockedSticks) {
            if (action == SurvivalHudAction::Sound) { pickupProfile->soundEnabled = !pickupProfile->soundEnabled; }
            if (action == SurvivalHudAction::Music) { pickupProfile->musicEnabled = !pickupProfile->musicEnabled; }
            if (action == SurvivalHudAction::DockedSticks) { pickupProfile->options.ToggleDockedSticks(); }
            music.SetEnabled(pickupProfile->musicEnabled);
            CAudioPlayer::SetEffectsEnabled(pickupProfile->soundEnabled);
            if (gameContext != nullptr && !pickupProfile->SaveToDisk(gameContext->savePath)) { return 1; }
        }
        KeyCode pointerKey = KeyCode::None;
        if (action == SurvivalHudAction::Pause || action == SurvivalHudAction::Resume || action == SurvivalHudAction::Continue) { pointerKey = KeyCode::Space; }
        if (action == SurvivalHudAction::Retry) { pointerKey = KeyCode::R; }
        if (action == SurvivalHudAction::UseItem) { pointerKey = KeyCode::G; }
        if (action == SurvivalHudAction::NextItem) { pointerKey = KeyCode::F; }
        if (action == SurvivalHudAction::Weapon1) { pointerKey = KeyCode::Digit1; }
        if (action == SurvivalHudAction::Weapon2) { pointerKey = KeyCode::Digit2; }
        if (action == SurvivalHudAction::SwapWeapon) { pointerKey = KeyCode::Digit2; }
        std::vector<KeyCode> inputs;
        if (pointerKey != KeyCode::None) { inputs.push_back(pointerKey); }
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None; key = window.TakeKeyPress()) { AppendSurvivalShortcut(inputs, key); }
        const int controlsWaveBeforeInput = session.GetLevel().GetWave();
        if (checkControls && controlFrame >= controlClickCount && controlFrame < controlClickCount + controlKeyCount) {
            const unsigned step = controlFrame - controlClickCount;
            if (step == 0 || step == 4) {
                vitals.invincible = true;
                // Wait through real intro/cooldown updates; do not bypass Use().
                for (unsigned tick = 0; tick < 250; ++tick) { session.Update(16, 0, 0, false); }
            }
            if (step == 0) {
                // Distinct slots catch accidental Q -> right-item routing.
                leftPowerup = controlsInitialLeft;
                // The isolated legacy test account starts without this item.
                pickupProfile->AddPowerup(leftPowerup, 1);
                controlsLeftBeforeKeys = pickupProfile->GetPowerupCount(leftPowerup);
                controlsBeforeKeys = pickupProfile->GetPowerupCount(rightPowerup);
                vitals.health = 1;
            }
            AppendSurvivalShortcut(inputs, controlKeys[step]);
        }
        for (KeyCode key : inputs) {
            if (powerups.IsMovieActive()) { continue; }
            // The original death script hides the input pad. Do not open an
            // invisible pause menu while the formal death animation is running.
            if (vitals.dead && gameContext != nullptr) { continue; }
            if (shopOpen) {
                if (key == KeyCode::Space || key == KeyCode::Escape) {
                    if (survivalHud.BackFromSelectorPrompt()) { continue; }
                    if (itemChoice) { itemChoice = false; }
                    else { shopOpen = false; }
                }
                continue;
            }
            if (key == KeyCode::Space || key == KeyCode::Escape) {
                if (!paused || !survivalHud.BackFromHelp()) { paused = !paused; }
            }
            if (key == KeyCode::C) { showCollisions = !showCollisions; }
            if ((key == KeyCode::Q || key == KeyCode::E || key == KeyCode::G) &&
                !paused && !vitals.dead && !session.IsTransitioning()) {
                GameObjectRef item = rightPowerup;
                if (key == KeyCode::Q) { item = leftPowerup; }
                if (powerups.SelectResource(item)) { powerups.Use(); }
                continue;
            }
            if (key == KeyCode::Digit1 && gameContext != nullptr && !paused && !vitals.dead && !session.IsTransitioning()) {
                shopOpen = true;
                itemChoice = false;
                accumulator = 0;
                continue;
            }
            if (key == KeyCode::F) {
                powerups.SelectResource(rightPowerup);
                powerups.Cycle();
                if (powerups.GetSelected() != nullptr) { rightPowerup = powerups.GetSelected()->resource; }
            }
            if (key == KeyCode::R) {
                pendingWeapon = weapons.size();
                swapEventAccepted = false;
                if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium)) { return 1; }
                session.Restart(startX, startY, startFacing);
                if (gameContext != nullptr) { gameContext->accountedKills = 0; gameContext->accountedWeaponExperience.clear(); }
                if (!session.HasOriginalHud()) { survivalHud.ResetNotices(); }
                savedDeath = false;
                paused = false;
                shopOpen = false;
                itemChoice = false;
            }
            std::size_t nextWeapon = weaponSlot;
            if (gameContext == nullptr) {
                KeyCode weaponKey = key;
                nextWeapon = SelectWeaponKey(weapons, weaponSlot, weaponKey);
            }
            else if (!paused && !vitals.dead && pendingWeapon >= weapons.size() &&
                (key == KeyCode::Digit2 || key == KeyCode::N || key == KeyCode::M)) {
                pendingEquippedSlot = 1 - equippedWeaponSlot;
                const GameObjectRef &ref = gameContext->profile.configuration.guns[pendingEquippedSlot];
                for (std::size_t index = 0; index < weapons.size(); ++index) {
                    if (weapons[index].packHash == ref.packHash && weapons[index].ordinal == ref.localIndex) { nextWeapon = index; break; }
                }
            }
            if (nextWeapon != weaponSlot && !vitals.dead) {
                if (gameContext != nullptr) {
                    // Load both authored guns before starting the lowering move.
                    // Keep the brother, torso controller, health and effects alive.
                    if (player.uiOtherWeapon == nullptr) {
                        primaryEquippedSlot = equippedWeaponSlot;
                        if (!PreparePlayerUIWeapon(tables, weapons[nextWeapon].data, weapons[nextWeapon].owner, player) ||
                            !CreatePlayerBuffers(player, program)) { return 1; }
                    }
                    pendingWeapon = nextWeapon;
                    swapEventAccepted = false;
                    if (checkControls) {
                        std::printf("[combat-swap-check] queued old=%zu requested=%zu unchanged=1\n", weaponSlot, pendingWeapon);
                    }
                    continue;
                }
                if (!EquipControlledPlayer(tables, loaded, program, weapons[nextWeapon])) { return 1; }
                effects.RetireOwner(kPlayerCombatId);
                weaponSlot = nextWeapon;
            }
            if (gameContext != nullptr && !vitals.dead) { gameContext->profile.activeWeaponSlot = equippedWeaponSlot; }
        }
        if (checkControls && (controlFrame == controlClickCount + 3 || controlFrame == controlClickCount + 5)) {
            // Throwing consumes inventory at the authored animation event.
            for (unsigned tick = 0; tick < 60; ++tick) { session.Update(16, 0, 0, false); }
        }
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        loaded.players[0].x = scene.playerX;
        loaded.players[0].y = scene.playerY;
        const float baselineZoom = GameViewCameraZoom(width, height);
        camera.zoom = baselineZoom * loaded.map.GetCamera().GetScale() / kLevelCameraScale;
        session.SetViewSize(width / baselineZoom, height / baselineZoom);
        FollowPlayerCamera(loaded, width, height, camera);
        scene.SetViewCenter(camera.x + width / camera.zoom * 0.5f, camera.y + height / camera.zoom * 0.5f);
        scene.SetTextView(camera.x, camera.y, camera.zoom * 1024 / width, camera.zoom * 768 / height);
        float mouseX = 0, mouseY = 0;
        if (capturePath.empty() && window.GetMousePosition(mouseX, mouseY) && !vitals.dead && !powerups.IsMovieActive()) {
            scene.facing = std::atan2(camera.y + mouseY / camera.zoom - scene.playerY,
                camera.x + mouseX / camera.zoom - scene.playerX) * kRadiansToDegrees + 90;
        }
        float moveX = 0, moveY = 0;
        if (window.IsKeyDown(KeyCode::A)) { --moveX; }
        if (window.IsKeyDown(KeyCode::D)) { ++moveX; }
        if (window.IsKeyDown(KeyCode::W)) { --moveY; }
        if (window.IsKeyDown(KeyCode::S)) { ++moveY; }
        const std::uint64_t now = window.GetTicksMs();
        if (!paused && !shopOpen && capturePath.empty()) { accumulator += static_cast<int>(std::min<std::uint64_t>(now - previous, 100)); }
        previous = now;
        if (performanceStudy) {
            accumulator = 16;
            performancePilot->Update(16, moveX, moveY);
        }
        if (checkControls && !paused && !shopOpen) { accumulator = 960; }
        effects.SetPaused(paused || shopOpen);
        // CGunBros::OnSuspend :78263 lowers BGM to half without stopping it.
        // Gameplay and effects stay suspended; the music stream keeps advancing.
        float musicScale = 1.0f;
        if (paused || shopOpen) { musicScale = 0.5f; }
        music.SetVolume(musicScale);
        music.Update();
        if (checkControls && controlFrame < controlClickCount + controlKeyCount) {
            const auto playback = music.GetPlaybackState();
            float expectedVolume = 0;
            if (pickupProfile->musicEnabled) {
                expectedVolume = 0.3f;
                if (paused || shopOpen) { expectedVolume *= 0.5f; }
            }
            if (playback.paused || std::abs(playback.volume - expectedVolume) > 0.001f) { ++checkFailures; }
            std::printf("[pause-bgm-check] frame=%u menu=%d paused-stream=%d gain=%.2f expected=%.2f failures=%u\n",
                controlFrame, paused || shopOpen, playback.paused, playback.volume, expectedVolume, checkFailures);
        }
        const std::size_t shotsBeforeSwap = effects.GetShotCount();
        const bool checkSwapFiring = checkControls && (controlFrame == 2 || controlFrame == controlClickCount + 2);
        while (accumulator >= 16) {
            if (powerups.IsMovieActive()) {
                session.Update(16, 0, 0, false);
                accumulator -= 16;
                continue;
            }
            if (!session.HasOriginalHud()) { survivalHud.Advance(16); }
            if (!vitals.dead && pendingWeapon < weapons.size() && !swapEventAccepted) {
                SetPlayerInput(player, false, false);
                swapEventAccepted = player.weapon->brother.OnSwapGun();
            }
            if (!vitals.dead) {
                const bool shoot = pendingWeapon >= weapons.size() &&
                    (performanceStudy || firePreview || checkSwapFiring || (window.IsLeftMouseDown() && !hudOwnsPointer));
                session.Update(16, moveX, moveY, shoot);
            }
            else {
                // CLevel::UpdateAfterDeath keeps an already-used powerup alive.
                session.UpdateAfterDeath(16);
            }
            if (pendingWeapon < weapons.size() && player.weapon->brother.TakeWeaponSwap()) {
                effects.RetireOwner(kPlayerCombatId);
                const auto &torso = player.weapon->brother.GetTorso().GetAnimation();
                const CMesh *outgoingMesh = torso.GetMesh();
                const int outgoingTime = torso.GetTimeMs();
                SelectPlayerUIWeapon(player, pendingEquippedSlot == primaryEquippedSlot);
                if (checkControls && (torso.GetMesh() != outgoingMesh || torso.GetTimeMs() != outgoingTime)) { ++checkFailures; }
                weaponSlot = pendingWeapon;
                equippedWeaponSlot = pendingEquippedSlot;
                player.gunResource.packHash = weapons[weaponSlot].packHash;
                player.gunResource.localIndex = static_cast<std::uint8_t>(weapons[weaponSlot].ordinal);
                player.masteryExperience = gameContext->profile.GetWeaponExperience(player.gunResource);
                player.ActiveWeapon().gun.SetMasteryExperience(player.masteryExperience);
                gameContext->profile.activeWeaponSlot = equippedWeaponSlot;
                pendingWeapon = weapons.size();
                ++combatSwapEvents;
                if (checkControls) {
                    std::printf("[combat-swap-check] native-event=%u slot=%u torso-preserved=1\n", combatSwapEvents, equippedWeaponSlot);
                }
            }
            const int worldDeltaMs = session.GetLevel().TransformWorldElapseMS(16);
            AdvanceProps(loaded.props, worldDeltaMs);
            AdvanceTileLayers(loaded.map, worldDeltaMs);
            accumulator -= 16;
        }
        if (checkSwapFiring) {
            const std::size_t shots = effects.GetShotCount() - shotsBeforeSwap;
            if (shots == 0) { ++checkFailures; }
            std::printf("[combat-swap-check] fire-after-switch=%zu failures=%u\n", shots, checkFailures);
        }
        if (session.GetLevel().GetWave() != lastSavedWave || (session.IsDeathComplete() && !savedDeath) ||
            session.GetLevel().GetTutorialStep() != lastSavedTutorialStep) {
            if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium, session.IsDeathComplete())) { return 1; }
            lastSavedWave = session.GetLevel().GetWave();
            savedDeath = session.IsDeathComplete();
            lastSavedTutorialStep = session.GetLevel().GetTutorialStep();
        }
        // Gameplay death opens the original postgame flow; research keeps its
        // death/restart controls so existing isolated checks remain available.
        if (session.IsDeathComplete() && gameContext != nullptr && !check && capturePath.empty()) { break; }
        loaded.players[0].x = scene.playerX;
        loaded.players[0].y = scene.playerY;
        loaded.players[0].facingDegrees = scene.facing;
        camera.zoom = baselineZoom * loaded.map.GetCamera().GetScale() / kLevelCameraScale;
        FollowPlayerCamera(loaded, width, height, camera);
        const auto performanceUpdated = std::chrono::steady_clock::now();
        BuildGeometry(loaded, batch, true, true, false);
        const auto performanceGeometry = std::chrono::steady_clock::now();
        glViewport(0, 0, width, height);
        glClearColor(0.04f, 0.05f, 0.07f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        float mvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(width / camera.zoom, height / camera.zoom, kMapDepthRange, mvp);
        Matrix4dTranslate(mvp, -camera.x, -camera.y);
        batch.Draw(program, mvp);
        pickups.Draw(mvp, kLevelCameraScale);
        effects.Draw(mvp, nullptr, kLevelCameraScale, WeaponDrawPass::BehindPlayer);
        // Historical explanation of the old separate model pass:
        // The AI brother is a 3D model like the player and the enemies: with no
        // depth test his torso, legs and gun paint over each other in submission
        // order and the model's dark inside covers its front -- the black
        // speckles. DrawModels already cleared depth and drew the player, so this
        // shares the same depth buffer.
        // Correction: the shared queue now clears depth per model and sorts
        // BOTH brothers and enemies among props, preserving internal depth.
        PlayerModel *drawBrother = nullptr;
        if (withBrother) {
            drawBrother = &brotherModel;
        }
        DrawMapObjects(loaded, batch, program, mvp, true, &scene, drawBrother, brother.y, width);
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
        const auto performanceWorld = std::chrono::steady_clock::now();
        SurvivalHudState hudState = buildHudState();
        // CCamera constructor :64622: viewport factor is independent of zoom.
        const float healthBarViewportScale = std::min(width / 480.0f, height / 320.0f);
        hudState.enemyHealthBars = scene.EnemyHealthBars(healthBarViewportScale);
        ProjectEnemyHealthBars(hudState.enemyHealthBars, camera.x, camera.y, camera.zoom, width, height);
        hudState.indicators = session.GetLevel().GetIndicators();
        for (CLevelIndicator &indicator : hudState.indicators) {
            indicator.x = (indicator.x - camera.x) * camera.zoom * 1024 / width;
            indicator.y = (indicator.y - camera.y) * camera.zoom * 768 / height;
        }
        if (withBrother) {
            // CLevel::Bind :121881 leaves this empty for the default local
            // partner; only a selected friend or multiplayer peer supplies a name.
            // CLevel::DrawBrotherLabel :120351 anchors three collision radii
            // above the AI's world position and follows the level's alpha.
            hudState.brotherLabelX = (brother.x - camera.x) * camera.zoom * 1024 / width;
            hudState.brotherLabelY = (brother.y - scene.GetPlayerRadius() * 3 - camera.y) * camera.zoom * 768 / height;
            hudState.brotherLabelAlpha = session.GetLevel().GetBrotherLabelAlpha();
        }
        hudState.frameMs = debugFrameMs;
        hudState.playerX = scene.playerX;
        hudState.playerY = scene.playerY;
        hudState.damageDealt = scene.damageDealt;
        const float moveLength = std::max(1.0f, std::hypot(moveX, moveY));
        hudState.moveX = moveX / moveLength;
        hudState.moveY = moveY / moveLength;
        if (window.IsLeftMouseDown() && !hudOwnsPointer) {
            hudState.aimX = std::sin(scene.facing / kRadiansToDegrees);
            hudState.aimY = -std::cos(scene.facing / kRadiansToDegrees);
        }
        if (!survivalHud.DrawExperienceTexts(scene.GetExperienceTexts(), horde) || !survivalHud.Draw(hudState)) { return 1; }
        if (!powerups.DrawMovies()) { return 1; }
        if (checkControls && controlFrame < controlClickCount) {
            if (controlFrame == 0 && !window.SaveFrame("out/combat-controls-shop.png")) { return 1; }
            if (controlFrame == 3 && !window.SaveFrame("out/combat-controls-pause.png")) { return 1; }
            if (controlFrame + 1 == controlClickCount) {
                const bool inventoryUnchanged = pickupProfile->warbucks == controlsInitialBucks &&
                    pickupProfile->GetPowerupCount(rightPowerup) == controlsInitialGrenades;
                const bool controlsPassed = inventoryUnchanged && equippedWeaponSlot == 1 &&
                    pickupProfile->soundEnabled == controlsInitialSound && !paused && !shopOpen;
                std::printf("[combat-controls-check] original-hitboxes=5 inventory-unchanged=%d weapon=%u resume=%d failures=%d\n",
                    inventoryUnchanged, equippedWeaponSlot, !paused && !shopOpen, !controlsPassed);
                if (!controlsPassed) { ++checkFailures; }
            }
        }
        if (checkControls && controlFrame >= controlClickCount && controlFrame < controlClickCount + controlKeyCount) {
            const unsigned step = controlFrame - controlClickCount;
            bool passed = true;
            if (step == 0) { passed = shopOpen; }
            if (step == 1) { passed = !shopOpen && !paused; }
            if (step == 2) { passed = equippedWeaponSlot == 0 && combatSwapEvents == 2; }
            if (step == 3) { passed = equippedWeaponSlot == 0 && controlsLeftBeforeKeys > 0 &&
                pickupProfile->GetPowerupCount(leftPowerup) == controlsLeftBeforeKeys - 1 &&
                pickupProfile->GetPowerupCount(rightPowerup) == controlsBeforeKeys; }
            if (step == 5) { passed = equippedWeaponSlot == 0 && pickupProfile->GetPowerupCount(rightPowerup) == controlsBeforeKeys - 1; }
            if (step == 6 || step == 7) { passed = inputs.empty() && equippedWeaponSlot == 0 &&
                session.GetLevel().GetWave() == controlsWaveBeforeInput && rightPowerup.localIndex == 13; }
            std::printf("[shortcut-check] step=%u key=%d passed=%d inventory=%u\n", step,
                static_cast<int>(controlKeys[step]), passed, pickupProfile->GetPowerupCount(rightPowerup));
            if (!passed) { ++checkFailures; }
        }
        if (checkControls && controlFrame < controlClickCount + controlKeyCount) {
            ++controlFrame;
            if (controlFrame < controlClickCount + controlKeyCount) { window.Present(); continue; }
        }
        if (!capturePath.empty()) {
            if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium, check && horde)) { return 1; }
            const unsigned errors = glGetError();
            if (errors != 0 || !window.SaveFrame(capturePath)) { return 1; }
            std::printf("[survival] wave=%d alive=%d spawned=%u kills=%u hp=%.1f\n",
                session.GetLevel().GetWave(), session.CountEnemies(), scene.spawned, session.GetKills(), vitals.health);
            window.Present();
            if (check && horde) {
                CRefinementManager::Template refinement;
                if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
                CProfileManager hordeProfile;
                hordeProfile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
                SurvivalGameContext record{hordeProfile, "out/horde-progress-check.dat"};
                record.hordeStart = static_cast<int>(startWave);
                std::uint64_t credited = 0;
                if (!SaveSurvivalProgress(&record, progress, scene, session.GetLevel(), credited) ||
                    !SaveSurvivalProgress(&record, progress, scene, session.GetLevel(), credited)) { return 1; }
                CProfileManager restored = hordeProfile;
                if (!restored.LoadFromDisk(record.savePath) || restored.hordeBestScore[startWave] != scene.GetScore() ||
                    restored.hordeBestKills[startWave] != scene.GetTotalKills() || restored.clearedWaves[0] != 0 ||
                    restored.enemyKills[0] != 0) { ++checkFailures; }
                const unsigned points = scene.GetScore();
                CombatHit damage;
                damage.ownerType = 1;
                damage.damage = 1;
                scene.ApplyHit(kPlayerCombatId, damage);
                if (scene.GetKillStreak() != 0 || scene.GetScore() != points) { ++checkFailures; }
                session.Restart(startX, startY, startFacing);
                if (scene.GetScore() != 0 || scene.GetKillStreak() != 0 || session.GetKills() != 0 ||
                    session.GetLevel().GetStopwatchTime() != 0 || session.GetLevel().GetObjectTimeScale() != 1) { ++checkFailures; }
                std::printf("[horde-check] points=%u saved=1 damage-resets-streak=1 restart=1 failures=%u\n", points, checkFailures);
            }
            if (checkFailures != 0) { return 1; }
            return 0;
        }
        const auto performanceHud = std::chrono::steady_clock::now();
        window.Present();
        if (performanceStudy) {
            const auto performanceEnd = std::chrono::steady_clock::now();
            const double updateMs = std::chrono::duration<double, std::milli>(performanceUpdated - performanceStart).count();
            const double geometryMs = std::chrono::duration<double, std::milli>(performanceGeometry - performanceUpdated).count();
            const double worldMs = std::chrono::duration<double, std::milli>(performanceWorld - performanceGeometry).count();
            const double hudMs = std::chrono::duration<double, std::milli>(performanceHud - performanceWorld).count();
            const double presentMs = std::chrono::duration<double, std::milli>(performanceEnd - performanceHud).count();
            performanceReport << performanceFrame << ',' << updateMs << ',' << geometryMs << ',' << worldMs << ',' << hudMs << ',' << presentMs
                << ',' << scene.AliveCount() << ',' << scene.spawned << '\n';
            performanceCpu.push_back(updateMs + geometryMs + worldMs + hudMs);
            if (++performanceFrame % 300 == 0) {
                std::printf("[performance] frame=%u update=%.2f geometry=%.2f world=%.2f hud=%.2f present=%.2f alive=%zu\n",
                    performanceFrame, updateMs, geometryMs, worldMs, hudMs, presentMs, scene.AliveCount());
            }
            if (performanceFrame >= 1200) {
                std::sort(performanceCpu.begin(), performanceCpu.end());
                std::printf("[performance] cpu-p50=%.3f cpu-p95=%.3f cpu-p99=%.3f max=%.3f frames=%u kills=%u\n",
                    performanceCpu[600], performanceCpu[1140], performanceCpu[1188], performanceCpu.back(), performanceFrame, scene.GetTotalKills());
                std::printf("[performance] enemy-assets hits=%u misses=%u templates=%zu\n", scene.GetEnemyModelCache().hits,
                    scene.GetEnemyModelCache().misses, scene.GetEnemyModelCache().entries.size());
                break;
            }
        }
    }
    if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium)) { return 1; }
    return 0;
}

int RunMapPreview(const std::string &bigDirectory, const std::string &packShortName,
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
        DrawMapObjects(loaded, batch, program, mvp, showProps);
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

