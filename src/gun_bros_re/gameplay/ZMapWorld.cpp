#include "engine/graphics/CMeshCamera.h"
#include "gun_bros_re/gameplay/ZMapWorldInternal.h"
using namespace MapDetail;

namespace MapDetail {

/**
 * Whether any of a template's slots has more than one step to play.
 *
 * Most scenery is a single step and stands perfectly still, which is correct
 * and also indistinguishable from a broken clock. Counting the ones that can
 * move is what tells the two apart without staring at the window.
 */
bool PropAnimates(const ZPropSprite &sprite) {
    if (sprite.interactiveKind != ZInteractivePropKind::None) {
        for (std::uint8_t state = 0; state < sprite.stateCount; ++state) {
            const ZPropVisualState &visual = sprite.states[state];
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
bool SlotDrawsAtStart(const ZPropSlot &slot) {
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

ZInteractivePropKind InteractiveKindFor(std::uint32_t packHash,
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

void ExpandPropState(CSpriteIterator &iterator,
                     const ZSpriteArchetype &archetype,
                     std::uint8_t backgroundAnimation,
                     std::uint8_t mainAnimation,
                     std::uint8_t foregroundAnimation,
                     ZPropVisualState &state) {
    ExpandSlot(iterator, archetype, backgroundAnimation, state.background);
    ExpandSlot(iterator, archetype, mainAnimation, state.main);
    ExpandSlot(iterator, archetype, foregroundAnimation, state.foreground);
}

/** Build the known visual states named by the three original prop scripts. */
void BuildInteractiveStates(std::uint32_t packHash, std::uint8_t localIndex,
                            CSpriteIterator &iterator,
                            const ZSpriteArchetype &archetype,
                            ZPropSprite &out) {
    out.interactiveKind = InteractiveKindFor(packHash, localIndex);
    if (out.interactiveKind == ZInteractivePropKind::Cover) {
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

    if (out.interactiveKind == ZInteractivePropKind::Barrel) {
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

    if (out.interactiveKind == ZInteractivePropKind::Spire) {
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
bool BuildPropSprite(CResTOCManager &tocManager, ZLoadedMap &loaded,
                     std::uint32_t propPackHash, std::uint8_t localIndex,
                     ZPropSprite &out) {
    std::vector<std::uint8_t> payload;
    if (!ReadSectionResource(tocManager, loaded, propPackHash, ZGameSection::Prop,
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
    ZPackResources *gluPack = GetPackResources(tocManager, loaded, gluPackIndex);
    if (gluPack == nullptr || !gluPack->spriteGluReady) {
        return false;
    }

    const ZSpriteArchetype *archetype =
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
    if (out.interactiveKind == ZInteractivePropKind::None) {
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
    if (out.interactiveKind != ZInteractivePropKind::None) {
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
const ZPropSlot *RuntimeSlotFor(const ZPlacedProp &prop, unsigned slot) {
    static const ZPropSlot empty;
    if (prop.runtime->IsRemoved()) { return &empty; }
    const int animation = prop.runtime->GetAnimation(slot);
    if (animation < 0 || animation >= static_cast<int>(prop.sprite->animations.size())) { return &empty; }
    return &prop.sprite->animations[animation];
}

const ZPropSlot *BackgroundSlotFor(const ZPlacedProp &prop) {
    static const ZPropSlot inactive;
    if (!prop.active) { return &inactive; }
    if (prop.runtime != nullptr) { return RuntimeSlotFor(prop, 0); }
    if (prop.sprite->interactiveKind == ZInteractivePropKind::None) {
        return &prop.sprite->background;
    }
    if (prop.interactiveState >= prop.sprite->stateCount) {
        return &prop.sprite->background;
    }
    return &prop.sprite->states[prop.interactiveState].background;
}

const ZPropSlot *MainSlotFor(const ZPlacedProp &prop) {
    static const ZPropSlot inactive;
    if (!prop.active) { return &inactive; }
    if (prop.runtime != nullptr) { return RuntimeSlotFor(prop, 1); }
    if (prop.sprite->interactiveKind == ZInteractivePropKind::None) {
        return &prop.sprite->main;
    }
    if (prop.interactiveState >= prop.sprite->stateCount) {
        return &prop.sprite->main;
    }
    return &prop.sprite->states[prop.interactiveState].main;
}

const ZPropSlot *ForegroundSlotFor(const ZPlacedProp &prop) {
    static const ZPropSlot inactive;
    if (!prop.active) { return &inactive; }
    if (prop.runtime != nullptr) { return RuntimeSlotFor(prop, 2); }
    if (prop.sprite->interactiveKind == ZInteractivePropKind::None) {
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
void StartPropPlayers(ZPlacedProp &prop, std::size_t propOrdinal) {
    const ZPropSprite &sprite = *prop.sprite;

    prop.interactiveState = 0;
    prop.hitFlashRemainingMs = 0.0f;
    const ZPropSlot *background = BackgroundSlotFor(prop);
    prop.background.SetAnimation(&background->stepDurationsMs);
    const ZPropSlot *foreground = ForegroundSlotFor(prop);
    prop.foreground.SetAnimation(&foreground->stepDurationsMs);

    const ZPropSlot *main = MainSlotFor(prop);
    prop.main.SetAnimation(&main->stepDurationsMs);
    prop.main.SetStep(StartStepFor(propOrdinal, main->stepDurationsMs.size()));
}

/** Human-readable state for the keyboard diagnostic. */
const char *CoverStateName(ZCoverState state) {
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
ZCoverState NextCoverState(ZCoverState state) {
    std::uint8_t next = static_cast<std::uint8_t>(state) + 1;
    if (next >= static_cast<std::uint8_t>(ZCoverState::Count)) {
        next = 0;
    }
    return static_cast<ZCoverState>(next);
}

/** Apply one state to every destructible cover on the current map. */
std::uint32_t SetCoverState(ZLoadedMap &loaded, ZCoverState state) {
    std::uint32_t changed = 0;
    for (std::size_t i = 0; i < loaded.props.size(); ++i) {
        ZPlacedProp &prop = loaded.props[i];
        if (prop.sprite->interactiveKind != ZInteractivePropKind::Cover) {
            continue;
        }

        prop.interactiveState = static_cast<std::uint8_t>(state);
        const ZPropSlot *background = BackgroundSlotFor(prop);
        prop.background.SetAnimation(&background->stepDurationsMs);
        prop.main.SetAnimation(&MainSlotFor(prop)->stepDurationsMs);
        prop.foreground.SetAnimation(&ForegroundSlotFor(prop)->stepDurationsMs);
        prop.hitFlashRemainingMs = 500.0f;
        changed++;
    }
    return changed;
}

const char *InteractiveStateName(ZInteractivePropKind kind, std::uint8_t state) {
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

/** Apply one visual state to every prop of a scripted interactive kind. */
std::uint32_t SetInteractiveState(ZLoadedMap &loaded, ZInteractivePropKind kind,
                                  std::uint8_t state) {
    std::uint32_t changed = 0;
    for (std::size_t i = 0; i < loaded.props.size(); ++i) {
        ZPlacedProp &prop = loaded.props[i];
        if (prop.sprite->interactiveKind != kind ||
            state >= prop.sprite->stateCount) {
            continue;
        }

        prop.interactiveState = state;
        prop.background.SetAnimation(&BackgroundSlotFor(prop)->stepDurationsMs);
        prop.main.SetAnimation(&MainSlotFor(prop)->stepDurationsMs);
        prop.foreground.SetAnimation(&ForegroundSlotFor(prop)->stepDurationsMs);
        if (kind != ZInteractivePropKind::Spire) {
            prop.hitFlashRemainingMs = 500.0f;
        }
        changed++;
    }
    return changed;
}

std::uint64_t AssetKey(std::uint32_t packHash, std::uint32_t localIndex) {
    return (static_cast<std::uint64_t>(packHash) << 32) | localIndex;
}

int SoundResourceForState(ZInteractivePropKind kind, std::uint8_t state) {
    if (kind == ZInteractivePropKind::Cover && state >= 1 && state <= 3) {
        return 4;
    }
    if (kind == ZInteractivePropKind::Barrel && state == 3) {
        return 2;
    }
    if (kind == ZInteractivePropKind::Spire && state == 1) {
        return 2;
    }
    if (kind == ZInteractivePropKind::Spire && state == 2) {
        return 3;
    }
    return -1;
}

/** Resolve SoundEffect -> WAV, cache it, then play one batch transition cue. */
void PlayTransitionSound(CResTOCManager &tocManager, ZLoadedMap &loaded,
                         ZAudioPlayer &audio, ZInteractivePropKind kind,
                         std::uint8_t state) {
    const int resourceIndex = SoundResourceForState(kind, state);
    if (resourceIndex < 0) {
        return;
    }

    const ZPropSprite *sprite = nullptr;
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

    const ZScriptResourceRef &soundEffect =
        sprite->transitionResources[resourceIndex];
    std::vector<std::uint8_t> soundPayload;
    if (!ReadSectionResource(tocManager, loaded, soundEffect.packHash,
                             ZGameSection::SoundEffect, soundEffect.resourceId,
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
                             ZGameSection::Wav,
                             static_cast<std::uint32_t>(wavReference.assetId),
                             wavPayload)) {
        return;
    }
    if (audio.Load(key, wavPayload)) {
        audio.Play(key);
    }
}

/** Order props the way CRenderQueue does: by group, then down the screen. */
bool PropDrawsBefore(const ZPlacedProp &left, const ZPlacedProp &right) {
    if (left.sprite->zOrderGroup != right.sprite->zOrderGroup) {
        return left.sprite->zOrderGroup < right.sprite->zOrderGroup;
    }
    // CProp::GetZOrder truncates the world ordinate, not the sprite bounds.
    return static_cast<int>(left.y) < static_cast<int>(right.y);
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
 * Assemble exactly the collision shapes CLayerCollision tests for a player.
 *
 * The level script selects one map collision layer. Static props then add
 * their template-local geometry at their placed position. Keeping this as one
 * scene lets the existing resolver choose the nearest edge across both kinds
 * instead of resolving each prop in an arbitrary order.
 */
void BuildCollisionScene(ZLoadedMap &loaded) {
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
        const ZPlacedProp &prop = loaded.props[i];
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

/**
 * Drive the first player with WASD and resolve the requested movement.
 *
 * Maps contain one real player spawn. A few abandoned campaign maps contain
 * none; those remain valid viewers and simply ignore movement input.
 */
/** Swap equipment only after every referenced asset has loaded. */
bool EquipControlledPlayer(ZPackTables &tables, ZLoadedMap &loaded,
    const ZShaderProgram &program, const ZWeaponEntry &weapon) {
    if (loaded.players.empty()) { return true; }
    // CombatScene retains this actor's address. CBrother::EquipWeapon stages the
    // weapon atomically and preserves the body, vitals pointer and armour.
    CBrother &player = *loaded.players[0].model;
    player.gunResource.packHash = weapon.packHash;
    player.gunResource.localIndex = static_cast<std::uint8_t>(weapon.ordinal);
    const std::uint64_t key = (static_cast<std::uint64_t>(weapon.packHash) << 8) | weapon.ordinal;
    player.masteryExperience = 0;
    const auto mastery = player.masteryByWeapon.find(key);
    if (mastery != player.masteryByWeapon.end()) { player.masteryExperience = mastery->second; }
    if (!player.EquipWeapon(tables, loaded.playerTemplate->GetScript(), weapon.data, weapon.owner) ||
        !player.CreateBuffers(program)) { return false; }
    return true;
}

void AppendSurvivalShortcut(std::vector<ZKeyCode> &inputs, ZKeyCode key) {
    // Desktop binding policy: F/R are not gameplay shortcuts. Pointer Retry
    // and NextItem actions are dispatched separately and remain available.
    if (key == ZKeyCode::F || key == ZKeyCode::R) { return; }
    inputs.push_back(key);
}

/** Slot of the first map of the pack `slot` belongs to. */
std::size_t FirstMapOfPack(const std::vector<ZCatalogMap> &catalog, std::size_t slot) {
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
std::size_t NextPackSlot(const std::vector<ZCatalogMap> &catalog, std::size_t slot) {
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
std::size_t PreviousPackSlot(const std::vector<ZCatalogMap> &catalog,
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
ZMapRectangle ViewedRegion(const ZLoadedMap &loaded) {
    const ZMapRectangle bounds = loaded.map.GetCameraExtent();
    if (!bounds.IsEmpty()) {
        return bounds;
    }

    const float drawSize = static_cast<float>(loaded.tileSet.GetDrawSize());

    ZMapRectangle canvas;
    canvas.x = 0;
    canvas.y = 0;
    canvas.width = static_cast<std::int16_t>(loaded.map.GetCanvasWidth() * drawSize);
    canvas.height = static_cast<std::int16_t>(loaded.map.GetCanvasHeight() * drawSize);
    return canvas;
}

/** Zoom out far enough to see the whole viewed region, and centre it. */
ZMapCamera FitCamera(const ZLoadedMap &loaded, int viewWidth, int viewHeight) {
    const ZMapRectangle region = ViewedRegion(loaded);
    const float regionWidth = static_cast<float>(region.width);
    const float regionHeight = static_cast<float>(region.height);

    ZMapCamera camera;
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
void FollowPlayerCamera(const ZLoadedMap &loaded, int viewWidth, int viewHeight,
                        ZMapCamera &camera) {
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

    const ZMapRectangle bounds = loaded.map.GetVisibleBounds();
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
bool VisibleBoundsScissor(const ZLoadedMap &loaded, const ZMapCamera &camera,
                          int drawableWidth, int drawableHeight, int scissor[4]) {
    const ZMapRectangle bounds = loaded.map.GetCameraExtent();
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
}
