#include "gun_bros_re/gameplay/map/CMapResources.h"
#include "engine/graphics/ZPNG.h"
#include "engine/graphics/ZTexture.h"
#include "engine/resources/CResTOCManager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace MapDetail;

/**
 * Load a map, its tile set and the atlases the tile set names.
 *
 * @param mapPackIndex Which pack the map itself lives in. Everything it
 *                     references is addressed by its own pack hash, not by
 *                     this one.
 */
bool CMap::Load(CResTOCManager &tocManager, int mapPackIndex, std::uint32_t mapIndex) {
    CMap &out = *this;
    m_resources = std::make_unique<Resources>();
    CResPackTOC *mapPack = tocManager.GetPack(mapPackIndex);
    if (mapPack == nullptr) {
        std::printf("[m3] no pack at index %d\n", mapPackIndex);
        return false;
    }

    CMap::Resources::Pack *resources = GetPackResources(tocManager, out, mapPackIndex);
    if (resources == nullptr || !resources->objectPackReady) {
        return false;
    }

    // --- the map ---
    const std::uint32_t mapCount =
        resources->objectPack.GetObjectCount(ZGameSection::TileLayer);
    if (mapIndex >= mapCount) {
        std::printf("[m3] %s has %u maps; %u is out of range\n",
                    mapPack->GetShortName().c_str(), mapCount, mapIndex);
        return false;
    }

    std::vector<std::uint8_t> payload;
    if (!ReadSectionResource(tocManager, out, mapPack->GetPackHash(),
                             ZGameSection::TileLayer, mapIndex, payload)) {
        return false;
    }

    CArrayInputStream mapStream(payload);
    if (!out.Init(mapStream)) {
        return false;
    }

    std::printf("[m3] map %u: %ux%u tiles, %u tile layers, %u object layers",
                mapIndex, out.GetCanvasWidth(), out.GetCanvasHeight(),
                out.GetTileLayerCount(), out.GetObjectLayerCount());
    if (out.GetUnparsedLayerCount() > 0) {
        std::printf(", %u layers abandoned at an unknown type",
                    out.GetUnparsedLayerCount());
    }
    std::printf("\n");

    // CLevel binds and runs the selected level script after map loading.
    // Previewing matching scripts is an explicit Viewer-only operation.

    // After the scripts, because setCameraLayer is one of the first things a
    // level does and it decides which of these rectangles is the live one.
    const CLayerCamera::Rectangle visibleBounds = out.GetVisibleBounds();
    const CLayerCamera::Rectangle extent = out.GetCameraExtent();
    std::printf("[m3]   %u camera layers; level opens on (%d, %d) %d x %d px, "
                "showing all of (%d, %d) %d x %d px\n",
                out.GetCameraLayerCount(), visibleBounds.x, visibleBounds.y,
                visibleBounds.width, visibleBounds.height, extent.x, extent.y,
                extent.width, extent.height);

    if (!out.GetMovieLayers().empty()) {
        std::printf("[map] movie layers=%zu retained; map movie rendering is not implemented\n",
            out.GetMovieLayers().size());
    }

    // --- the tile set ---
    const GameObjectRef &tileSetRef = out.GetTileSetRef();
    if (tileSetRef.IsNull()) {
        std::printf("[m3] map has no tile set\n");
        return false;
    }

    if (!ReadSectionResource(tocManager, out, tileSetRef.packHash,
                             ZGameSection::TileSet, tileSetRef.localIndex, payload)) {
        return false;
    }

    CArrayInputStream tileSetStream(payload);
    if (!out.GetResources().tileSet.Init(tileSetStream)) {
        return false;
    }
    std::printf("[m3] tile set %u: %zu atlases, %u tiles, draw size %u\n",
                tileSetRef.localIndex, out.GetResources().tileSet.GetImages().size(),
                out.GetResources().tileSet.GetTileCount(), out.GetResources().tileSet.GetDrawSize());

    // --- the atlases ---
    // A tile set's image refs are CGameAssetRefs, and TileSet::Load is what
    // decides they index the PNG section. The data does not say so.
    const std::vector<CGameAssetRef> &images = out.GetResources().tileSet.GetImages();
    for (std::size_t i = 0; i < images.size(); ++i) {
        if (!ReadSectionResource(tocManager, out, images[i].packHash, ZGameSection::Png,
                                 static_cast<std::uint32_t>(images[i].assetId),
                                 payload)) {
            return false;
        }

        ZPNGImage decoded;
        if (!PNGDecode(payload, decoded)) {
            return false;
        }

        std::unique_ptr<ZTexture> texture(new ZTexture());
        if (!texture->Create(decoded)) {
            return false;
        }
        std::printf("[m3]   atlas %zu: PNG ordinal %d -> %ux%u\n",
                    i, images[i].assetId, decoded.width, decoded.height);
        out.GetResources().textures.push_back(std::move(texture));
    }

    return true;
}

/**
 * Turn the map's object layers into drawable props.
 *
 * Objects of other types are counted and left alone. Nothing here fails the
 * load: a prop whose template or sprite will not resolve is dropped and
 * reported, because one bad rock should not cost the whole level.
 */
void CMap::LoadProps(CResTOCManager &tocManager, int selectedObjectLayer) {
    CMap &loaded = *this;
    loaded.GetResources().props.clear();
    loaded.GetResources().propSprites.clear();

    std::uint32_t placed = 0;
    std::uint32_t skipped = 0;
    std::uint32_t otherTypes = 0;

    for (std::uint32_t layerIndex = 0; layerIndex < loaded.GetObjectLayerCount();
         ++layerIndex) {
        if (selectedObjectLayer >= 0 && static_cast<int>(loaded.GetObjectLayer(layerIndex).GetLayerIndex()) != selectedObjectLayer) {
            continue;
        }
        const std::vector<CLayerObject::Object> &objects =
            loaded.GetObjectLayer(layerIndex).GetObjects();

        for (std::size_t i = 0; i < objects.size(); ++i) {
            const CLayerObject::Object &object = objects[i];
            if (object.objectType != static_cast<std::uint8_t>(CLayerObject::ObjectType::Prop)) {
                otherTypes++;
                continue;
            }

            // One template serves many instances, so its quads are built once.
            const std::uint64_t key =
                (static_cast<std::uint64_t>(object.packHash) << 8) | object.localIndex;

            std::map<std::uint64_t, CProp::Resources>::iterator found =
                loaded.GetResources().propSprites.find(key);
            if (found == loaded.GetResources().propSprites.end()) {
                CProp::Resources sprite;
                sprite.skippedParts = 0;
                sprite.unsupportedTransforms = 0;
                if (!sprite.Load(tocManager, loaded, object.packHash, object.localIndex)) {
                    std::printf("[m3]   prop %08X/%u will not resolve\n",
                                object.packHash, object.localIndex);
                    skipped++;
                    continue;
                }
                found = loaded.GetResources().propSprites.insert(std::make_pair(key, sprite)).first;
            }

            CProp prop;
            prop.x = static_cast<float>(object.x);
            prop.y = static_cast<float>(object.y);
            prop.resources = &found->second;
            prop.objectId = static_cast<int>(i);
            prop.objectLayer = loaded.GetObjectLayer(layerIndex).GetLayerIndex();
            loaded.GetResources().props.push_back(std::move(prop));
            placed++;
        }
    }

    std::stable_sort(loaded.GetResources().props.begin(), loaded.GetResources().props.end(), PropSpawnOrder);

    // After the sort, so a prop's phase follows from where it ends up in the
    // draw order rather than from which layer happened to place it.
    std::uint32_t animated = 0;
    for (std::size_t i = 0; i < loaded.GetResources().props.size(); ++i) {
        StartPropPlayers(loaded.GetResources().props[i], i);
        if (PropAnimates(*loaded.GetResources().props[i].resources)) {
            animated++;
        }
    }

    std::uint32_t skippedParts = 0;
    std::uint32_t unsupportedTransforms = 0;
    std::map<std::uint64_t, CProp::Resources>::const_iterator sprite;
    for (sprite = loaded.GetResources().propSprites.begin(); sprite != loaded.GetResources().propSprites.end();
         ++sprite) {
        skippedParts += sprite->second.skippedParts;
        unsupportedTransforms += sprite->second.unsupportedTransforms;
    }

    std::printf("[m3] %u props from %zu templates, %u of them animated "
                "(%u unresolved, %u non-prop objects ignored)\n",
                placed, loaded.GetResources().propSprites.size(), animated, skipped, otherTypes);
    if (skippedParts > 0 || unsupportedTransforms > 0) {
        std::printf("[m3]   %u sprite parts undrawable, "
                    "%u drawn without a rotating transform\n",
                    skippedParts, unsupportedTransforms);
    }
}
