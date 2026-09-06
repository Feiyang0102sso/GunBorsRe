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
 */

#include "milestones/M3Map.h"

#include "engine/CArrayInputStream.h"
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

// Leave a little room around a fitted map so its edges are visible.
constexpr float kFitMargin = 1.05f;

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

/** Everything one map needs to draw, held together for the render loop. */
struct LoadedMap {
    CMap map;
    TileSet tileSet;
    std::vector<std::unique_ptr<CTexture>> textures;

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
             std::uint32_t advanceMs) {
    std::printf("=== M3: a level, terrain and scenery, on screen ===\n\n");

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
    if (!window.Open("gun_bros_re -- M3", kDefaultWindowWidth, kDefaultWindowHeight)) {
        return 1;
    }

    CShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) {
        return 1;
    }

    CQuadBatch batch;
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
    WarmUp(loaded, advanceMs);

    // Either layer can be hidden, which is how "is that rock in the right\n// place or is the ground wrong?" gets answered without a debugger.
    bool showTiles = true;
    bool showProps = true;

    // The props animate, so the geometry is rebuilt every frame from here on.
    // This flag only decides whether a rebuild says anything about itself.
    bool reportGeometry = true;
    Camera camera = FitCamera(loaded, drawableWidth, drawableHeight);

    // Time, and the two ways of taking it apart when something looks wrong:
    // stop it, or move it on one bite at a time.
    std::uint64_t previousTicks = window.GetTicksMs();
    bool paused = false;
    bool singleStep = false;

    // The factors themselves are the batch's business now: glows and fire are
    // additive, everything else is straight alpha.
    glEnable(GL_BLEND);

    std::printf("\n[m3] left/right: map (crosses packs), up/down: pack, "
                "Home: refit, T: tiles, P: props, space: pause, "
                "'.': one step on, drag: pan, wheel: zoom, Esc: quit\n");

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
            } else if (key == KeyCode::Home) {
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
                WarmUp(replacement, advanceMs);
                loaded = std::move(replacement);
                reportGeometry = true;
                refit = true;
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

        AdvanceProps(loaded.props, static_cast<std::uint16_t>(elapsedMs));
        AdvanceTileLayers(loaded.map, static_cast<std::uint16_t>(elapsedMs));
        BuildGeometry(loaded, batch, showTiles, showProps, reportGeometry);
        reportGeometry = false;

        // --- zoom about the centre of the view ---
        const float wheel = window.TakeWheelDelta();
        if (wheel != 0.0f) {
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

        // --- pan ---
        int dragX = 0;
        int dragY = 0;
        window.TakeDragDelta(dragX, dragY);
        // Dragging right moves the view left, as if pulling the map along.
        // Divided by zoom so a drag tracks the cursor at any scale.
        camera.x -= static_cast<float>(dragX) / camera.zoom * kDragScale;
        camera.y -= static_cast<float>(dragY) / camera.zoom * kDragScale;

        glViewport(0, 0, drawableWidth, drawableHeight);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Zoom is a wider or narrower slice of the world, so it goes into the
        // projection's extent rather than into a separate scale matrix.
        float mvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(static_cast<float>(drawableWidth) / camera.zoom,
                             static_cast<float>(drawableHeight) / camera.zoom, mvp);
        Matrix4dTranslate(mvp, -camera.x, -camera.y);

        int scissor[4];
        const bool clipping = VisibleBoundsScissor(loaded, camera, drawableWidth,
                                                   drawableHeight, scissor);
        if (clipping) {
            // After the clear, so the background still fills the window.
            glEnable(GL_SCISSOR_TEST);
            glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        }

        batch.Draw(program, mvp);

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
