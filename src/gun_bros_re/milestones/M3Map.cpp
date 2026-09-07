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

#include "milestones/M3Map.h"

#include "milestones/EnemyModel.h"
#include "milestones/PlayerModel.h"
#include "milestones/PackTables.h"

#include "engine/CArrayInputStream.h"
#include "engine/CMarkerBatch.h"
#include "engine/CMatrix4d.h"
#include "engine/CPNG.h"
#include "engine/CQuadBatch.h"
#include "engine/CShaderProgram.h"
#include "engine/CTexture.h"
#include "engine/platform/CWindow.h"
#include "engine/platform/GLLoader.h"
#include "gun_bros/CGameObjectPack.h"
#include "gun_bros/CLayerObject.h"
#include "gun_bros/CLayerTile.h"
#include "gun_bros/CLevel.h"
#include "gun_bros/CMap.h"
#include "gun_bros/CProp.h"
#include "gun_bros/CResTOCManager.h"
#include "gun_bros/TileSet.h"
#include "sprite_glu/CSpriteGlu.h"
#include "sprite_glu/CSpriteIterator.h"
#include "sprite_glu/CSpritePlayer.h"

#include <algorithm>
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

// GameView keeps the original 4:3 logical field of view independent of the
// desktop window's pixel size. A 1600x1200 window therefore renders the same
// world area as 1024x768 instead of zooming the game farther in or out.
constexpr float kGameViewWorldWidth = 1024.0f;
constexpr float kGameViewWorldHeight = 768.0f;

// Temporary keyboard locomotion until the original control-stick module is
// ported. The movement and collision time step are frame-rate independent.
constexpr float kPlayerMovementUnitsPerSecond = 240.0f;
// CBrother::Bind stores 22.0 as the player diameter. CPlayer::Move passes half
// of it to CLayerCollision, whose half-unit edge allowance makes 11.5.
constexpr float kPlayerCollisionRadius = 11.5f;
constexpr float kRadiansToDegrees = 180.0f / 3.14159265f;

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
    PropSlot background;
    PropSlot main;
    PropSlot foreground;
    CCollisionData collision;
    int zOrderGroup;

    // What the walk could not draw, kept so the load can report a total
    // rather than a line per template.
    std::uint32_t skippedParts;
    std::uint32_t unsupportedTransforms;
};

/** One prop standing on the map, each of its three slots playing its own. */
struct PlacedProp {
    float x;
    float y;
    const PropSprite *sprite;

    CSpritePlayer background;
    CSpritePlayer main;
    CSpritePlayer foreground;
};

/** The per-pack tables a prop needs, built the first time that pack is used. */
struct PackResources {
    CGameObjectPack objectPack;
    CSpriteGlu spriteGlu;
    bool objectPackReady;
    bool spriteGluReady;

    PackResources() : objectPackReady(false), spriteGluReady(false) {}
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
    ExpandSlot(iterator, *archetype, propTemplate.GetBackgroundAnimation(),
               out.background);
    ExpandSlot(iterator, *archetype, propTemplate.GetMainAnimation(), out.main);
    ExpandSlot(iterator, *archetype, propTemplate.GetForegroundAnimation(),
               out.foreground);
    out.collision = propTemplate.GetCollision();

    out.skippedParts = iterator.GetSkippedPartCount();
    out.unsupportedTransforms = iterator.GetUnsupportedTransformCount();

    // Judged on the first step alone, which is what M3.1 sorted on before
    // anything animated. Playback moves what a slot draws but never whether it
    // draws, so keeping the test on step 0 keeps the draw order fixed for the
    // life of the map -- and lets the queue stay sorted once, at load.
    if (SlotDrawsAtStart(out.main)) {
        out.zOrderGroup = kZGroupNormal;
    } else if (SlotDrawsAtStart(out.background)) {
        out.zOrderGroup = kZGroupBackgroundOnly;
    } else if (SlotDrawsAtStart(out.foreground)) {
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

    prop.background.SetAnimation(&sprite.background.stepDurationsMs);
    prop.foreground.SetAnimation(&sprite.foreground.stepDurationsMs);

    prop.main.SetAnimation(&sprite.main.stepDurationsMs);
    prop.main.SetStep(StartStepFor(propOrdinal, sprite.main.stepDurationsMs.size()));
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
void LoadProps(CResTOCManager &tocManager, LoadedMap &loaded) {
    loaded.props.clear();
    loaded.propSprites.clear();

    std::uint32_t placed = 0;
    std::uint32_t skipped = 0;
    std::uint32_t otherTypes = 0;

    for (std::uint32_t layerIndex = 0; layerIndex < loaded.map.GetObjectLayerCount();
         ++layerIndex) {
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
        props[i].background.Update(deltaMs);
        props[i].main.Update(deltaMs);
        props[i].foreground.Update(deltaMs);
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

    const CLayerCollision *mapLayer = loaded.map.GetCurrentCollisionLayer();
    if (mapLayer != nullptr) {
        loaded.collisionScene.AppendTranslated(mapLayer->GetCollision(), 0.0f,
                                               0.0f);
    }

    std::uint32_t propShapes = 0;
    for (std::size_t i = 0; i < loaded.props.size(); ++i) {
        const PlacedProp &prop = loaded.props[i];
        if (prop.sprite->collision.GetEdges().empty()) {
            continue;
        }
        if (!loaded.collisionScene.AppendTranslated(prop.sprite->collision,
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
    player.facingDegrees = std::atan2(directionY, directionX) * kRadiansToDegrees;

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
void WarmUp(LoadedMap &loaded, std::uint32_t totalMs) {
    for (std::uint32_t elapsed = 0; elapsed < totalMs; elapsed += kWarmUpFrameMs) {
        AdvanceProps(loaded.props, kWarmUpFrameMs);
        AdvanceTileLayers(loaded.map, kWarmUpFrameMs);
        AdvanceEnemies(loaded, kWarmUpFrameMs);
        AdvancePlayers(loaded, kWarmUpFrameMs);
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
            AddSpriteQuads(prop, CurrentQuads(prop.sprite->background, prop.background),
                           batch);
        }
        for (std::size_t i = 0; i < loaded.props.size(); ++i) {
            const PlacedProp &prop = loaded.props[i];
            AddSpriteQuads(prop, CurrentQuads(prop.sprite->main, prop.main), batch);
        }
        for (std::size_t i = 0; i < loaded.props.size(); ++i) {
            const PlacedProp &prop = loaded.props[i];
            AddSpriteQuads(prop, CurrentQuads(prop.sprite->foreground, prop.foreground),
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

/**
 * Pick a close gameplay scale that never reveals empty space around the
 * current camera region.
 *
 * The original 1024x768 logical view is the floor. Narrow stage-specific
 * camera regions may need a larger scale to cover the 4:3 window; cropping
 * those regions is correct for a following game camera and avoids revealing
 * space outside the playable region.
 */
float GameViewCameraZoom(const LoadedMap &loaded, int viewWidth,
                         int viewHeight) {
    const float logicalScaleX = static_cast<float>(viewWidth) /
                                kGameViewWorldWidth;
    const float logicalScaleY = static_cast<float>(viewHeight) /
                                kGameViewWorldHeight;
    float zoom = logicalScaleX;
    if (logicalScaleY > zoom) {
        zoom = logicalScaleY;
    }
    const MapRectangle bounds = loaded.map.GetVisibleBounds();
    if (bounds.IsEmpty() || bounds.width <= 0 || bounds.height <= 0) {
        return zoom;
    }

    const float fillX = static_cast<float>(viewWidth) /
                        static_cast<float>(bounds.width);
    const float fillY = static_cast<float>(viewHeight) /
                        static_cast<float>(bounds.height);
    if (fillX > zoom) {
        zoom = fillX;
    }
    if (fillY > zoom) {
        zoom = fillY;
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

int RunM3Map(const std::string &bigDirectory, const std::string &packShortName,
             std::uint32_t mapIndex, const std::string &screenshotPath,
             std::uint32_t advanceMs, bool startWithSpawns,
             bool startWithCollisions, MapViewMode viewMode) {
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
    ReportSpawns(loaded);
    WarmUp(loaded, advanceMs);

    // Either layer can be hidden, which is how "is that rock in the right\n// place or is the ground wrong?" gets answered without a debugger.
    bool showTiles = true;
    bool showProps = true;
    bool showSpawns = startWithSpawns;
    bool showCollisions = startWithCollisions;

    // The props animate, so the geometry is rebuilt every frame from here on.
    // This flag only decides whether a rebuild says anything about itself.
    bool reportGeometry = true;
    Camera camera = FitCamera(loaded, drawableWidth, drawableHeight);
    bool followPlayer = gameView && !loaded.players.empty();
    if (followPlayer) {
        camera.zoom = GameViewCameraZoom(loaded, drawableWidth, drawableHeight);
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
                    "T: tiles, P: props, K: spawns, C: collision, "
                    "space: pause, '.': one step, Esc: quit\n");
    } else {
        std::printf("\n[preview] arrows: change map, Home: refit, "
                    "drag: pan, wheel: zoom, T: tiles, P: props, "
                    "K: spawns, C: collision, space: pause, "
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
            if (key == KeyCode::T) {
                showTiles = !showTiles;
                reportGeometry = true;
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
                BuildCollisionScene(replacement);
                LoadPlacedEnemies(tocManager, program, replacement);
                LoadPlacedPlayers(tocManager, program, replacement);
                ReportSpawns(replacement);
                WarmUp(replacement, advanceMs);
                loaded = std::move(replacement);
                reportGeometry = true;
                followPlayer = gameView && !loaded.players.empty();
                if (followPlayer) {
                    camera.zoom = GameViewCameraZoom(
                        loaded, drawableWidth, drawableHeight);
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
        }
        AdvanceProps(loaded.props, static_cast<std::uint16_t>(elapsedMs));
        AdvanceEnemies(loaded, static_cast<std::int32_t>(elapsedMs));
        AdvancePlayers(loaded, static_cast<std::int32_t>(elapsedMs));
        AdvanceTileLayers(loaded.map, static_cast<std::uint16_t>(elapsedMs));
        BuildGeometry(loaded, batch, showTiles, showProps, reportGeometry);
        reportGeometry = false;

        if (gameView) {
            // Window resizing changes only pixel scale, never the game field
            // of view. The camera remains entirely owned by GameView.
            camera.zoom = GameViewCameraZoom(loaded, drawableWidth,
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
        DrawModels(loaded, program, mvp);

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
