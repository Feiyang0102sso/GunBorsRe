#pragma once
/** Viewer-only map selections. Original behavior is always executed by CProp Flow.
 * Resource IDs below select research subjects; they do not choose animations,
 * collisions, sounds, particle resources, prices, or formal gameplay behavior.
 */
#include "gun_bros_re/gameplay/map/CMapInternal.h"

namespace MapDetail {

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

// Temporary keyboard locomotion until the original control-stick module is
// ported. The movement and collision time step are frame-rate independent.
constexpr float kPlayerMovementUnitsPerSecond = 240.0f;

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
constexpr float kMarkerSize = 48.0f;
constexpr float kMarkerThickness = 6.0f;
void LoadPlacedEnemies(CResTOCManager &tocManager, const ZShaderProgram &program,
                       CMap &loaded);
void AdvanceEnemies(CMap &loaded, std::int32_t deltaMs);

struct ZCatalogMap {
    int packIndex;
    std::string packName;
    std::uint32_t mapIndex;  // ordinal within its own pack's TILELAYER section
};
/** Slot of the first map of the pack `slot` belongs to. */
inline std::size_t FirstMapOfPack(const std::vector<ZCatalogMap> &catalog, std::size_t slot) {
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
inline std::size_t NextPackSlot(const std::vector<ZCatalogMap> &catalog, std::size_t slot) {
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
inline std::size_t PreviousPackSlot(const std::vector<ZCatalogMap> &catalog,
                             std::size_t slot) {
    const std::size_t first = FirstMapOfPack(catalog, slot);

    // The map before this pack's first belongs to the previous pack; back up
    // from there to that pack's own first.
    const std::size_t previous = (first + catalog.size() - 1) % catalog.size();
    return FirstMapOfPack(catalog, previous);
}

/**
 * The rectangle worth looking at, in world pixels.
 *
 * The map's camera layer when it has one, and the whole canvas when it does
 * not. Two of the twenty-two maps declare no camera layer.
 */
inline CLayerCamera::Rectangle ViewedRegion(const CMap &loaded) {
    const CLayerCamera::Rectangle bounds = loaded.GetCameraExtent();
    if (!bounds.IsEmpty()) {
        return bounds;
    }

    const float drawSize = static_cast<float>(loaded.GetResources().tileSet.GetDrawSize());

    CLayerCamera::Rectangle canvas;
    canvas.x = 0;
    canvas.y = 0;
    canvas.width = static_cast<std::int16_t>(loaded.GetCanvasWidth() * drawSize);
    canvas.height = static_cast<std::int16_t>(loaded.GetCanvasHeight() * drawSize);
    return canvas;
}

/** Zoom out far enough to see the whole viewed region, and centre it. */
inline CCamera::Viewport FitCamera(const CMap &loaded, int viewWidth, int viewHeight) {
    const CLayerCamera::Rectangle region = ViewedRegion(loaded);
    const float regionWidth = static_cast<float>(region.width);
    const float regionHeight = static_cast<float>(region.height);

    CCamera::Viewport camera;
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

/** Collect one outline per object of the given type, centred on its own x,y. */
inline void BuildMarkers(const CMap &loaded, ZMarkerBatch &markers,
                  CLayerObject::ObjectType wanted) {
    markers.Begin();

    const CMap &map = loaded;
    for (std::uint32_t layer = 0; layer < map.GetObjectLayerCount(); ++layer) {
        const std::vector<CLayerObject::Object> &objects =
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

/** How many objects of one type a map places. */
inline unsigned CountObjects(const CMap &loaded, CLayerObject::ObjectType wanted) {
    const CMap &map = loaded;
    unsigned total = 0;
    for (std::uint32_t layer = 0; layer < map.GetObjectLayerCount(); ++layer) {
        const std::vector<CLayerObject::Object> &objects =
            map.GetObjectLayer(layer).GetObjects();
        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (objects[i].objectType == static_cast<std::uint8_t>(wanted)) {
                total++;
            }
        }
    }
    return total;
}

inline void ReportSpawns(const CMap &loaded) {
    std::printf("[m3] %u player spawns, %u enemy spawns\n",
                CountObjects(loaded, CLayerObject::ObjectType::Player),
                CountObjects(loaded, CLayerObject::ObjectType::Enemy));

    std::printf("[m4] %u collision layers in map data\n",
                loaded.GetCollisionLayerCount());
}

/**
 * Every map in every pack, as one flat list.
 *
 * Flat rather than grouped because a pack boundary is not something the viewer
 * should make anyone think about -- the packs are a packaging detail, and a map
 * reaches across them for its props anyway. Walking the list runs off the end
 * of one pack straight into the next.
 */
inline std::vector<ZCatalogMap> BuildCatalog(CResTOCManager &tocManager) {
    std::vector<ZCatalogMap> catalog;

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
            objectPack.GetObjectCount(ZGameSection::TileLayer);
        for (std::uint32_t m = 0; m < mapCount; ++m) {
            ZCatalogMap entry;
            entry.packIndex = static_cast<int>(i);
            entry.packName = pack->GetShortName();
            entry.mapIndex = m;
            catalog.push_back(entry);
        }
    }

    return catalog;
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
inline void ApplyLevelScripts(CResTOCManager &tocManager, CMap &out,
                       std::uint32_t mapPackHash, std::uint32_t mapIndex) {
    const int packIndex = tocManager.GetPackIndexFromHash(mapPackHash);
    CMap::Resources::Pack *resources = GetPackResources(tocManager, out, packIndex);
    CResPackTOC *pack = tocManager.GetPack(packIndex);
    if (resources == nullptr || !resources->objectPackReady || pack == nullptr) {
        return;
    }

    const std::uint32_t levelCount =
        resources->objectPack.GetObjectCount(ZGameSection::Level);
    std::vector<std::uint8_t> payload;

    for (std::uint32_t i = 0; i < levelCount; ++i) {
        const std::uint32_t handle =
            resources->objectPack.GetHandle(ZGameSection::Level, i);
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
        level.Bind(levelTemplate, out);
        // The count covers CLevel's own functions. Calls aimed at the other
        // eleven classes are logged by ScriptResolver, which has no level to
        // count them against.
        std::printf("[m3]   level %u ran; %u level functions it wanted are not "
                    "implemented\n",
                    i, level.GetUnimplementedCallCount());
    }
}

/** Move every prop's three players on by one frame's worth of time. */
inline void AdvanceProps(std::vector<CProp> &props, std::uint16_t deltaMs) {
    for (CProp &prop : props) {
        if (!prop.active) { continue; }
        prop.Update(deltaMs, false);
    }
}

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

inline bool IsDestructibleCover(std::uint32_t packHash, std::uint8_t localIndex) {
    if (packHash != kCoverPackHash) {
        return false;
    }
    return localIndex == kHorizontalCoverTemplate ||
           localIndex == kVerticalCoverTemplate;
}

inline ZInteractivePropKind InteractiveKindFor(std::uint32_t packHash,
                                       std::uint8_t localIndex) {
    if (IsDestructibleCover(packHash, localIndex)) {
        return ZInteractivePropKind::Cover;
    }
    if ((packHash == kLavaPackHash && localIndex == kLavaBarrelTemplate) ||
        (packHash == kWaterPackHash && localIndex == kWaterBarrelTemplate)) {
        return ZInteractivePropKind::Barrel;
    }
    if (packHash == kSpirePackHash && localIndex == kSpireTemplate) {
        return ZInteractivePropKind::Spire;
    }
    return ZInteractivePropKind::None;
}

/** Human-readable state for the keyboard diagnostic. */
inline const char *CoverStateName(ZCoverState state) {
    switch (state) {
        case ZCoverState::Intact:
            return "intact";
        case ZCoverState::Damaged:
            return "damaged";
        case ZCoverState::BadlyDamaged:
            return "badly damaged";
        case ZCoverState::Destroyed:
            return "destroyed";
        case ZCoverState::Hidden:
            return "hidden";
        default:
            return "unknown";
    }
}

/** Advance the shared cover state, wrapping hidden back to intact. */
inline ZCoverState NextCoverState(ZCoverState state) {
    std::uint8_t next = static_cast<std::uint8_t>(state) + 1;
    if (next >= static_cast<std::uint8_t>(ZCoverState::Count)) {
        next = 0;
    }
    return static_cast<ZCoverState>(next);
}

inline const char *InteractiveStateName(ZInteractivePropKind kind, std::uint8_t state) {
    if (kind == ZInteractivePropKind::Barrel) {
        const char *const names[] = {"intact", "damaged", "critical", "exploded"};
        if (state < 4) {
            return names[state];
        }
    }
    if (kind == ZInteractivePropKind::Spire) {
        const char *const names[] = {"dormant", "charged", "shockwave"};
        if (state < 3) {
            return names[state];
        }
    }
    return "unknown";
}

inline ZInteractivePropKind ResearchPropKind(const CProp &prop) {
    if (!prop.resources) { return ZInteractivePropKind::None; }
    return InteractiveKindFor(prop.resources->resource.packHash, prop.resources->resource.localIndex);
}

/** Enter an authored state; only Hidden is an explicit viewer visibility switch.
 * Evidence: pack5 PROP0/1 Flow states 1..4; pack2 PROP0 states 1..4;
 * pack12 PROP22 states 1..3/5; pack7 PROP33 states 0/2/3.
 */
/** Apply one visual state to every prop of a scripted interactive kind. */
inline std::uint32_t SetInteractiveState(CMap &map, ZInteractivePropKind kind, std::uint8_t state) {
    unsigned changed = 0;
    for (CProp &prop : map.GetResources().props) {
        if (ResearchPropKind(prop) != kind) { continue; }
        prop.active = true;
        prop.BindResources();
        if (kind == ZInteractivePropKind::Cover && state == 4) {
            prop.active = false;
            ++changed;
            continue;
        }
        unsigned authoredState = state + 1;
        if (kind == ZInteractivePropKind::Spire) {
            authoredState = 0;
            if (state > 0) { authoredState = state + 1; }
        }
        if (kind == ZInteractivePropKind::Barrel && state == 3 &&
            prop.resources->resource.packHash == kWaterPackHash) { authoredState = 5; }
        if (authoredState >= prop.resources->data.GetScript().GetStates().size()) {
            std::printf("[map-preview] state unavailable pack=%08x prop=%u state=%u\n",
                prop.resources->resource.packHash, prop.resources->resource.localIndex, authoredState);
            continue;
        }
        prop.SetResearchState(static_cast<std::uint8_t>(authoredState));
        ++changed;
    }
    return changed;
}

/** Apply one state to every destructible cover on the current map. */
inline std::uint32_t SetCoverState(CMap &map, ZCoverState state) {
    return SetInteractiveState(map, ZInteractivePropKind::Cover, static_cast<std::uint8_t>(state));
}

/** Consume actual Flow actions. A preview has no damage targets or account state. */
/** Resolve SoundEffect -> WAV, cache it, then play one batch transition cue. */
// The current preview consumes actual Flow actions, including particle cues.
inline void DispatchPreviewActions(CResTOCManager &toc, CMap &map, ZAudioPlayer *audio = nullptr) {
    for (CProp &prop : map.GetResources().props) {
        for (const CProp::Action &action : prop.TakeActions()) {
            if (action.kind == CProp::Action::Kind::Effect) {
                ZScriptResourceRef reference;
                reference.packHash = action.resource.packHash;
                reference.resourceId = action.resource.localIndex;
                std::uint64_t key = 0;
                if (EnsureParticleEffectVisual(toc, map, reference, key)) {
                    StartParticleEffect(map, key, prop.x, prop.y, action.group, static_cast<std::uint32_t>(prop.objectId));
                }
            } else if (action.kind == CProp::Action::Kind::Sound && audio) {
                std::vector<std::uint8_t> payload;
                if (!ReadSectionResource(toc, map, action.resource.packHash, ZGameSection::SoundEffect,
                        action.resource.localIndex, payload)) { continue; }
                CArrayInputStream stream(payload);
                CGameAssetRef wav;
                wav.Init(stream);
                if (wav.IsNull() || wav.assetId < 0) { continue; }
                if (!ReadSectionResource(toc, map, wav.packHash, ZGameSection::Wav,
                        static_cast<unsigned>(wav.assetId), payload)) { continue; }
                const auto key = AssetKey(wav.packHash, static_cast<unsigned>(wav.assetId));
                if (audio->Load(key, payload)) { audio->Play(key); }
            }
        }
    }
}
} // namespace MapDetail

namespace MapDetail {
/** A research map may preview each matching level; production selects one CLevel. */
inline bool LoadPreviewMap(CResTOCManager &toc, int packIndex, std::uint32_t mapIndex, CMap &map) {
    if (!map.Load(toc, packIndex, mapIndex)) { return false; }
    ApplyLevelScripts(toc, map, toc.GetPack(packIndex)->GetPackHash(), mapIndex);
    return true;
}
}

namespace MapDetail {

/** One enemy template standing on the map, with the model it draws as. */

inline void LoadPlacedEnemies(CResTOCManager &tocManager, const ZShaderProgram &program,
                       CMap &loaded) {
    ZPackTables tables(tocManager);
    unsigned failed = 0;

    for (std::uint32_t layer = 0; layer < loaded.GetObjectLayerCount();
         ++layer) {
        const std::vector<CLayerObject::Object> &objects =
            loaded.GetObjectLayer(layer).GetObjects();
        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (objects[i].objectType !=
                static_cast<std::uint8_t>(CLayerObject::ObjectType::Enemy)) {
                continue;
            }

            char label[128];
            std::snprintf(label, sizeof(label), "%s enemy %u",
                          tables.GetPackName(objects[i].packHash).c_str(),
                          objects[i].localIndex);

            auto entry = std::make_unique<CEnemy::Template>();
            auto placed = std::make_unique<CEnemy>();
            if (!entry->Load(tables, objects[i].packHash, objects[i].localIndex, label)) {
                failed++;
                continue;
            }

            placed->combat.x = static_cast<float>(objects[i].x);
            placed->combat.y = static_cast<float>(objects[i].y);
            // A map is a level, so the level spawn export is the one to run.
            if (!placed->Bind(tables, *entry, true, &program)) {
                failed++;
                continue;
            }

            placed->Spawn();

            // One line each: a map places a couple of dozen at most, and which
            // template a leftover spawn names is exactly what is worth seeing.
            std::printf("[m3] %s at %d %d -- %zu configs, %u parts\n",
                        label, objects[i].x, objects[i].y,
                        placed->configs.size(),
                        placed->GetPartCount());

            loaded.GetResources().enemyTemplates.push_back(std::move(entry));
            loaded.GetResources().enemies.push_back(std::move(placed));
        }
    }

    if (!loaded.GetResources().enemies.empty() || failed > 0) {
        std::printf("[m3] %zu placed enemies drawn, %u unloadable\n",
                    loaded.GetResources().enemies.size(), failed);
    }
}

/** Move every placed enemy's animation on. */
inline void AdvanceEnemies(CMap &loaded, std::int32_t deltaMs) {
    for (std::size_t i = 0; i < loaded.GetResources().enemies.size(); ++i) {
        loaded.GetResources().enemies[i]->Update(deltaMs);
    }
}

}

/** Viewer controls for the authored Haven turret and its separate indicator. */
class ZMapTurretPreview {
public:
    void Bind(CMap &map);
    void Cycle();
    void Update(int deltaMs);
    const char *StateName() const;
    bool Contains(const CProp &prop) const {
        return std::find(m_indicators.begin(), m_indicators.end(), &prop) != m_indicators.end();
    }
    bool Empty() const { return m_enemies.empty(); }

private:
    void ApplyState();
    unsigned m_state = 0;
    std::vector<CEnemy *> m_enemies;
    std::vector<CProp *> m_indicators;
};

namespace MapViewerTurretDetail {
// Research selections, not animation data: all moves, frames and timing come from BIG.
// pack9 ENEMY 0: physical 0006_0x1a5e, states 2/3/7/8.
// pack9 PROP 47: physical 0078_0x432a, states 2/3/0/1 (green/red/off/yellow).
// LEVEL 0008_0x1bf7 @0x288E..0x2930 links enemies 120/121 to props 37/38.
enum class TurretState { Idle, Active, Off, Charging, Count };
}

inline void ZMapTurretPreview::Bind(CMap &map) {
    m_enemies.clear();
    m_indicators.clear();
    m_state = 0;
    const auto packHash = CStringToKey("pack9");
    for (auto &placed : map.GetResources().enemies) {
        if (placed->data->packHash == packHash && placed->data->ordinal == 0) {
            m_enemies.push_back(placed.get());
        }
    }
    if (m_enemies.empty()) { return; }
    for (CProp &prop : map.GetResources().props) {
        if (prop.resources->resource.packHash != packHash || prop.resources->resource.localIndex != 47) { continue; }
        prop.Bind(prop.resources->data, &prop.resources->durations);
        m_indicators.push_back(&prop);
    }
    ApplyState();
}

inline void ZMapTurretPreview::Cycle() {
    if (Empty()) { return; }
    m_state = (m_state + 1) % static_cast<unsigned>(MapViewerTurretDetail::TurretState::Count);
    ApplyState();
}

inline void ZMapTurretPreview::ApplyState() {
    unsigned enemyState = 2;
    unsigned indicatorState = 2;
    switch (static_cast<MapViewerTurretDetail::TurretState>(m_state)) {
    case MapViewerTurretDetail::TurretState::Active: enemyState = 3; indicatorState = 3; break;
    case MapViewerTurretDetail::TurretState::Off: enemyState = 7; indicatorState = 0; break;
    case MapViewerTurretDetail::TurretState::Charging: enemyState = 8; indicatorState = 1; break;
    default: break;
    }
    // Enter original states; their sequences still own opening, closing and playback.
    for (CEnemy *enemy : m_enemies) { enemy->SetState(static_cast<std::uint8_t>(enemyState)); }
    for (CProp *prop : m_indicators) {
        prop->SetResearchState(static_cast<std::uint8_t>(indicatorState));
    }
    Update(0);
    std::printf("[map-turret] %s enemies=%zu indicators=%zu\n", StateName(), m_enemies.size(), m_indicators.size());
}

inline void ZMapTurretPreview::Update(int deltaMs) {
    for (CProp *prop : m_indicators) {
        prop->Update(deltaMs, false);
    }
    // Display states emit LEVEL callbacks. This viewer has no level or combat host.
    for (CEnemy *enemy : m_enemies) { enemy->TakeActions(); }
}

inline const char *ZMapTurretPreview::StateName() const {
    switch (static_cast<MapViewerTurretDetail::TurretState>(m_state)) {
    case MapViewerTurretDetail::TurretState::Active: return "Active (red)";
    case MapViewerTurretDetail::TurretState::Off: return "Off";
    case MapViewerTurretDetail::TurretState::Charging: return "Charging (yellow)";
    default: return "Idle (green)";
    }
}
