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
#include "gun_bros/CMap.h"
#include "gun_bros/CProp.h"
#include "gun_bros/CResTOCManager.h"
#include "gun_bros/TileSet.h"
#include "sprite_glu/CSpriteGlu.h"
#include "sprite_glu/CSpriteIterator.h"

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

// Which animation step of a prop to show. The original picks a random frame
// per instance so a field of identical rocks does not pulse in unison; a
// viewer wants the same picture every run, so it always takes the first.
// Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:124916 (Utility::Random in Bind)
constexpr std::uint32_t kStaticAnimationStep = 0;

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

/**
 * One prop template's three sprite slots, already expanded to quads.
 *
 * Shared between instances: a map places a hundred props from a couple of
 * dozen templates, and expanding the same frame a hundred times would mean a
 * hundred archive reads for nothing.
 */
struct PropSprite {
    std::vector<SpriteQuad> background;
    std::vector<SpriteQuad> main;
    std::vector<SpriteQuad> foreground;
    int zOrderGroup;

    // What the walk could not draw, kept so the load can report a total
    // rather than a line per template.
    std::uint32_t skippedParts;
    std::uint32_t unsupportedTransforms;
};

/** One prop standing on the map. */
struct PlacedProp {
    float x;
    float y;
    const PropSprite *sprite;
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
    iterator.Expand(propTemplate.GetBackgroundAnimation(), kStaticAnimationStep,
                    out.background);
    iterator.Expand(propTemplate.GetMainAnimation(), kStaticAnimationStep, out.main);
    iterator.Expand(propTemplate.GetForegroundAnimation(), kStaticAnimationStep,
                    out.foreground);

    out.skippedParts = iterator.GetSkippedPartCount();
    out.unsupportedTransforms = iterator.GetUnsupportedTransformCount();

    if (!out.main.empty()) {
        out.zOrderGroup = kZGroupNormal;
    } else if (!out.background.empty()) {
        out.zOrderGroup = kZGroupBackgroundOnly;
    } else if (!out.foreground.empty()) {
        out.zOrderGroup = kZGroupForegroundOnly;
    } else {
        out.zOrderGroup = kZGroupNormal;
    }

    return true;
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

    std::uint32_t skippedParts = 0;
    std::uint32_t unsupportedTransforms = 0;
    std::map<std::uint64_t, PropSprite>::const_iterator sprite;
    for (sprite = loaded.propSprites.begin(); sprite != loaded.propSprites.end();
         ++sprite) {
        skippedParts += sprite->second.skippedParts;
        unsupportedTransforms += sprite->second.unsupportedTransforms;
    }

    std::printf("[m3] %u props from %zu templates (%u unresolved, "
                "%u non-prop objects ignored)\n",
                placed, loaded.propSprites.size(), skipped, otherTypes);
    if (skippedParts > 0 || unsupportedTransforms > 0) {
        std::printf("[m3]   %u sprite parts undrawable, "
                    "%u drawn without a rotating transform\n",
                    skippedParts, unsupportedTransforms);
    }
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
 */
void BuildGeometry(const LoadedMap &loaded, CQuadBatch &batch, bool showTiles,
                   bool showProps) {
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

            for (std::uint32_t row = 0; row < map.GetCanvasHeight(); ++row) {
                for (std::uint32_t column = 0; column < map.GetCanvasWidth(); ++column) {
                    const TileCell &cell = layer.GetCell(column, row);

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
                                  static_cast<float>(column) * drawSize,
                                  static_cast<float>(row) * drawSize,
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
            AddSpriteQuads(loaded.props[i], loaded.props[i].sprite->background, batch);
        }
        for (std::size_t i = 0; i < loaded.props.size(); ++i) {
            AddSpriteQuads(loaded.props[i], loaded.props[i].sprite->main, batch);
        }
        for (std::size_t i = 0; i < loaded.props.size(); ++i) {
            AddSpriteQuads(loaded.props[i], loaded.props[i].sprite->foreground, batch);
        }
    }

    batch.Upload();
    std::printf("[m3] %u tile quads + %u prop quads in %u draw calls "
                "(%u cells empty or unusable)\n",
                tileQuads, batch.GetQuadCount() - tileQuads, batch.GetGroupCount(),
                skipped);
}

/** Read the current frame back and save it, flipping to top-down. */
bool SaveFrame(int width, int height, const std::string &path) {
    PNGImage frame;
    frame.width = static_cast<std::uint32_t>(width);
    frame.height = static_cast<std::uint32_t>(height);
    frame.pixels.resize(static_cast<std::size_t>(width) * height * 4);

    std::vector<std::uint8_t> bottomUp(frame.pixels.size());
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bottomUp.data());
    if (!GLCheckErrors("glReadPixels")) {
        return false;
    }

    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
    for (int y = 0; y < height; ++y) {
        std::memcpy(&frame.pixels[static_cast<std::size_t>(y) * rowBytes],
                    &bottomUp[static_cast<std::size_t>(height - 1 - y) * rowBytes],
                    rowBytes);
    }
    return PNGEncode(frame, path);
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

/** Zoom out far enough to see a whole map, and centre it. */
Camera FitCamera(const LoadedMap &loaded, int viewWidth, int viewHeight) {
    const float drawSize = static_cast<float>(loaded.tileSet.GetDrawSize());
    const float mapWidth = loaded.map.GetCanvasWidth() * drawSize;
    const float mapHeight = loaded.map.GetCanvasHeight() * drawSize;

    Camera camera;
    camera.zoom = 1.0f;
    camera.x = 0.0f;
    camera.y = 0.0f;
    if (mapWidth <= 0.0f || mapHeight <= 0.0f) {
        return camera;
    }

    const float fitX = static_cast<float>(viewWidth) / (mapWidth * kFitMargin);
    const float fitY = static_cast<float>(viewHeight) / (mapHeight * kFitMargin);
    camera.zoom = (fitX < fitY) ? fitX : fitY;

    camera.x = (mapWidth - static_cast<float>(viewWidth) / camera.zoom) * 0.5f;
    camera.y = (mapHeight - static_cast<float>(viewHeight) / camera.zoom) * 0.5f;
    return camera;
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
             std::uint32_t mapIndex, const std::string &screenshotPath) {
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

    // Either layer can be hidden, which is how "is that rock in the right
    // place or is the ground wrong?" gets answered without a debugger.
    bool showTiles = true;
    bool showProps = true;

    BuildGeometry(loaded, batch, showTiles, showProps);
    Camera camera = FitCamera(loaded, drawableWidth, drawableHeight);

    // The factors themselves are the batch's business now: glows and fire are
    // additive, everything else is straight alpha.
    glEnable(GL_BLEND);

    std::printf("\n[m3] left/right: map (crosses packs), up/down: pack, "
                "Home: refit, T: tiles, P: props, drag: pan, wheel: zoom, "
                "Esc: quit\n");

    bool reportedFirstFrame = false;
    while (window.PumpEvents()) {
        window.GetDrawableSize(drawableWidth, drawableHeight);

        // --- switching maps and packs ---
        bool reload = false;
        bool refit = false;
        bool rebuild = false;
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None;
             key = window.TakeKeyPress()) {
            if (key == KeyCode::T) {
                showTiles = !showTiles;
                rebuild = true;
            } else if (key == KeyCode::P) {
                showProps = !showProps;
                rebuild = true;
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
                loaded = std::move(replacement);
                rebuild = true;
                refit = true;
            } else {
                // A map that will not load leaves the previous one on screen
                // rather than a blank window.
                std::printf("[m3] staying on the previous map\n");
            }
        }
        if (rebuild) {
            BuildGeometry(loaded, batch, showTiles, showProps);
        }
        if (refit) {
            camera = FitCamera(loaded, drawableWidth, drawableHeight);
        }

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

        batch.Draw(program, mvp);

        if (!reportedFirstFrame) {
            GLCheckErrors("first frame");
            reportedFirstFrame = true;

            if (!screenshotPath.empty()) {
                if (!SaveFrame(drawableWidth, drawableHeight, screenshotPath)) {
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
