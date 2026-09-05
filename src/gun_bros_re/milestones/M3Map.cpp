/**
 * @file M3Map.cpp
 * @brief M3 milestone harness: a whole level's terrain, on screen.
 *
 * The chain this exercises:
 *
 *   ___GAME_TOC_KEYSET -> 33 section bases          (CGameObjectPack)
 *   TILELAYER base + n -> map resource              (CMap)
 *   map.tileSetRef     -> TILESET base + local      (TileSet)
 *   tileset.images[i]  -> PNG base + assetId        (CTexture)
 *   layer cells        -> quads                     (CQuadBatch)
 *
 * Terrain only. The rocks, pipes and portals are PROP instances drawn through
 * SpriteGlu, which is a separate subsystem and a separate step; without them a
 * map looks emptier than the game does.
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
#include "gun_bros/CLayerTile.h"
#include "gun_bros/CMap.h"
#include "gun_bros/CResTOCManager.h"
#include "gun_bros/TileSet.h"

#include <cstdio>
#include <cstring>
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

/** Everything one map needs to draw, held together for the render loop. */
struct LoadedMap {
    CMap map;
    TileSet tileSet;
    std::vector<std::unique_ptr<CTexture>> textures;
};

/** Fetch a resource by handle and run a parser over it. */
bool ReadResource(CResPackTOC &pack, std::uint32_t handle,
                  std::vector<std::uint8_t> &payload) {
    if (handle == 0) {
        std::printf("[m3] null handle\n");
        return false;
    }
    if (!pack.GetResource(handle, payload)) {
        std::printf("[m3] handle 0x%08X unreadable\n", handle);
        return false;
    }
    return true;
}

/**
 * Load a map, its tile set and the atlases the tile set names.
 */
bool LoadMap(CResPackTOC &pack, const CGameObjectPack &objectPack,
             std::uint32_t mapIndex, LoadedMap &out) {
    // --- the map ---
    const std::uint32_t mapCount = objectPack.GetObjectCount(GameSection::TileLayer);
    if (mapIndex >= mapCount) {
        std::printf("[m3] %s has %u maps; %u is out of range\n",
                    pack.GetShortName().c_str(), mapCount, mapIndex);
        return false;
    }

    std::vector<std::uint8_t> payload;
    const std::uint32_t mapHandle = objectPack.GetHandle(GameSection::TileLayer, mapIndex);
    if (!ReadResource(pack, mapHandle, payload)) {
        return false;
    }

    CArrayInputStream mapStream(payload);
    if (!out.map.Init(mapStream)) {
        return false;
    }

    std::printf("[m3] map %u: %ux%u tiles, %u tile layers",
                mapIndex, out.map.GetCanvasWidth(), out.map.GetCanvasHeight(),
                out.map.GetTileLayerCount());
    if (out.map.GetUnparsedLayerCount() > 0) {
        std::printf(", %u further layers not parsed (collision/objects/paths)",
                    out.map.GetUnparsedLayerCount());
    }
    std::printf("\n");

    // --- the tile set ---
    const GameObjectRef &tileSetRef = out.map.GetTileSetRef();
    if (tileSetRef.IsNull()) {
        std::printf("[m3] map has no tile set\n");
        return false;
    }

    const std::uint32_t tileSetHandle =
        objectPack.GetHandle(GameSection::TileSet, tileSetRef.localIndex);
    if (!ReadResource(pack, tileSetHandle, payload)) {
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
        const std::uint32_t imageHandle =
            objectPack.GetHandle(GameSection::Png,
                                 static_cast<std::uint32_t>(images[i].assetId));
        if (!ReadResource(pack, imageHandle, payload)) {
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
 * Turn the tile layers into quads.
 *
 * Walks the canvas rather than each layer's own extent, so a layer smaller
 * than the canvas repeats -- CLayerTile::GetCell does the wrapping. Layers are
 * emitted bottom first, matching CMap::DrawBackground's order.
 */
void BuildGeometry(const LoadedMap &loaded, CQuadBatch &batch) {
    const CMap &map = loaded.map;
    const TileSet &tileSet = loaded.tileSet;
    const std::vector<TileRect> &tiles = tileSet.GetTiles();
    const float drawSize = static_cast<float>(tileSet.GetDrawSize());

    batch.Begin();

    std::uint32_t skipped = 0;
    for (std::uint32_t layerIndex = 0; layerIndex < map.GetTileLayerCount(); ++layerIndex) {
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
                              (cell.flags & kTileFlagFlipVertical) != 0);
            }
        }
    }

    batch.Upload();
    std::printf("[m3] %u quads in %u draw calls (%u cells empty or unusable)\n",
                batch.GetQuadCount(), batch.GetGroupCount(), skipped);
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

/** One pack that actually contains maps, with its addressing ready to use. */
struct CatalogEntry {
    CResPackTOC *pack;
    CGameObjectPack objectPack;
    std::uint32_t mapCount;
};

/**
 * Every pack holding at least one map, so the viewer can walk between them
 * without reopening the archives.
 */
std::vector<CatalogEntry> BuildCatalog(CResTOCManager &tocManager) {
    std::vector<CatalogEntry> catalog;

    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        CatalogEntry entry;
        entry.pack = tocManager.GetPack(static_cast<int>(i));
        if (entry.pack == nullptr || !entry.objectPack.Init(*entry.pack)) {
            continue;
        }
        entry.mapCount = entry.objectPack.GetObjectCount(GameSection::TileLayer);
        if (entry.mapCount == 0) {
            continue;
        }
        catalog.push_back(entry);
    }

    return catalog;
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
    return 0;
}

int RunM3Map(const std::string &bigDirectory, const std::string &packShortName,
             std::uint32_t mapIndex, const std::string &screenshotPath) {
    std::printf("=== M3: a level's terrain, on screen ===\n\n");

    // --- resources, before any GL exists ---
    CResTOCManager tocManager;
    if (!tocManager.Init(bigDirectory, kArtSetXga) || !tocManager.Bind()) {
        return 1;
    }

    std::vector<CatalogEntry> catalog = BuildCatalog(tocManager);
    if (catalog.empty()) {
        std::printf("[m3] no pack contains any maps\n");
        return 1;
    }

    // Start on whichever pack was asked for.
    std::size_t packSlot = 0;
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        if (catalog[i].pack->GetShortName() == packShortName) {
            packSlot = i;
            break;
        }
    }
    std::uint32_t currentMap = mapIndex;
    if (currentMap >= catalog[packSlot].mapCount) {
        currentMap = 0;
    }

    std::printf("[m3] %zu packs with maps:", catalog.size());
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        std::printf(" %s(%u)", catalog[i].pack->GetShortName().c_str(),
                    catalog[i].mapCount);
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

    LoadedMap loaded;
    if (!LoadMap(*catalog[packSlot].pack, catalog[packSlot].objectPack,
                 currentMap, loaded)) {
        return 1;
    }
    BuildGeometry(loaded, batch);
    Camera camera = FitCamera(loaded, drawableWidth, drawableHeight);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::printf("\n[m3] left/right: map, up/down: pack, Home: refit, "
                "drag: pan, wheel: zoom, Esc: quit\n");

    bool reportedFirstFrame = false;
    while (window.PumpEvents()) {
        window.GetDrawableSize(drawableWidth, drawableHeight);

        // --- switching maps and packs ---
        bool reload = false;
        bool refit = false;
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None;
             key = window.TakeKeyPress()) {
            const std::uint32_t mapCount = catalog[packSlot].mapCount;

            if (key == KeyCode::Right) {
                currentMap = (currentMap + 1) % mapCount;
                reload = true;
            } else if (key == KeyCode::Left) {
                currentMap = (currentMap + mapCount - 1) % mapCount;
                reload = true;
            } else if (key == KeyCode::Down) {
                packSlot = (packSlot + 1) % catalog.size();
                currentMap = 0;
                reload = true;
            } else if (key == KeyCode::Up) {
                packSlot = (packSlot + catalog.size() - 1) % catalog.size();
                currentMap = 0;
                reload = true;
            } else if (key == KeyCode::Home) {
                refit = true;
            }
        }

        if (reload) {
            std::printf("\n[m3] --- %s map %u ---\n",
                        catalog[packSlot].pack->GetShortName().c_str(), currentMap);

            // Replace wholesale: the old textures go with the old LoadedMap.
            LoadedMap replacement;
            if (LoadMap(*catalog[packSlot].pack, catalog[packSlot].objectPack,
                        currentMap, replacement)) {
                loaded = std::move(replacement);
                BuildGeometry(loaded, batch);
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
