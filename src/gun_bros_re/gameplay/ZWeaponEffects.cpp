#include "gun_bros_re/gameplay/CParticleEffectPlayer.h"
/** @file ZWeaponEffects.cpp
 * @brief BIG-backed projectile sprites, meshes, particles and weapon audio.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/ZWeaponEffects.h"
#include "engine/platform/ZAudioPlayer.h"
#include "engine/core/ZMatrix4d.h"
#include "engine/graphics/ZPNG.h"
#include "gun_bros_re/gameplay/CBullet.h"
#include "gun_bros_re/gameplay/CParticleEffect.h"
#include "engine/glu/sprite/CSpriteIterator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>

namespace {
constexpr float kRadians = 3.14159265f / 180.0f;
constexpr float kShotSpeed = 450.0f;
constexpr std::uint32_t kBeamFlag = 0x100;

/** Project the same animated muzzle transform used to draw the weapon. */
bool ProjectMuzzle(ZPlayerModel &player, const float *matrix, int hand, int node,
                   float &x, float &y, float &z) {
    ZMeshBoneTransform muzzle{};
    if (!GetPlayerMuzzle(player, hand, node, muzzle)) { return false; }
    x = matrix[0] * muzzle.posX + matrix[1] * muzzle.posY + matrix[2] * muzzle.posZ + matrix[3];
    y = matrix[4] * muzzle.posX + matrix[5] * muzzle.posY + matrix[6] * muzzle.posZ + matrix[7];
    z = matrix[8] * muzzle.posX + matrix[9] * muzzle.posY + matrix[10] * muzzle.posZ + matrix[11];
    return true;
}

std::uint64_t ResourceKey(const GameObjectRef &ref) {
    return (static_cast<std::uint64_t>(ref.packHash) << 32) | ref.localIndex;
}

struct ZVisualAnimation {
    std::vector<std::vector<ZSpriteQuad>> frames;
    std::vector<int> endMs;
    int durationMs = 0;
};

std::size_t AnimationFrame(const ZVisualAnimation &animation, float ageMs) {
    int time = 0;
    if (animation.durationMs > 0) { time = static_cast<int>(ageMs) % animation.durationMs; }
    std::size_t frame = 0;
    while (frame + 1 < animation.frames.size() && time >= animation.endMs[frame]) { ++frame; }
    return frame;
}

/** The beam tiles the complete frame bounds, including all layered quads. */
void FrameBounds(const ZVisualAnimation &animation, float ageMs, float &top, float &bottom) {
    top = 0; bottom = 0;
    if (animation.frames.empty()) { return; }
    bool first = true;
    for (const ZSpriteQuad &quad : animation.frames[AnimationFrame(animation, ageMs)]) {
        if (first) { top = static_cast<float>(quad.offsetY); bottom = top; first = false; }
        top = std::min(top, static_cast<float>(quad.offsetY));
        bottom = std::max(bottom, static_cast<float>(quad.offsetY + quad.Height()));
    }
}

/**
 * Whether a beam slot is the source cap rather than the tiled body.
 *
 * Beam sprite sets are authored body / source / end in consecutive slots.
 * CBullet::Draw :62998 walks the tiles towards -Y and centres the source on
 * the muzzle, so body and end frames hang backwards from their origin
 * (bounds span [-h, 0]) while the source frame hangs forwards ([0, +h]).
 * pack5 archetype 0 (player lasers) and 139 (Kraken / mech boss) both follow
 * it, which is what makes the bounds a usable test rather than a guess.
 */
bool IsBeamSourceAnimation(const ZVisualAnimation &animation) {
    if (animation.frames.empty()) { return false; }
    float top = 0, bottom = 0;
    FrameBounds(animation, 0, top, bottom);
    return bottom > 0;
}

/** Billboards keep their authored size while position and travel use world units. */
struct ZEffectProjection {
    const float *matrix = nullptr;
    float scale = 1;

    explicit ZEffectProjection(const float *projection) : matrix(projection) {
        if (matrix != nullptr) {
            const float x = std::hypot(matrix[0], matrix[4]);
            const float y = std::hypot(matrix[1], matrix[5]);
            const float z = std::hypot(matrix[2], matrix[6]);
            scale = std::max(x, std::max(y, z));
        }
    }
    void Position(float &x, float &y, float z) const {
        if (matrix == nullptr) { return; }
        const float originalX = x;
        x = matrix[0] * originalX + matrix[1] * y + matrix[2] * z + matrix[3];
        y = matrix[4] * originalX + matrix[5] * y + matrix[6] * z + matrix[7];
    }
    float Direction(float degrees) const {
        if (matrix == nullptr) { return degrees; }
        const float x = std::cos(degrees * kRadians), y = std::sin(degrees * kRadians);
        const float dx = matrix[0] * x + matrix[1] * y;
        const float dy = matrix[4] * x + matrix[5] * y;
        if (std::hypot(dx, dy) < scale * 0.0001f) { return -90; }
        return std::atan2(dy, dx) / kRadians;
    }
};

struct ZBulletVisual {
    CBullet::Template data;
    std::unique_ptr<ZPlayerPart> mesh;
};

struct ZShot {
    ZCombatId id = 0;
    ZCombatId owner = kPlayerCombatId;
    GameObjectRef weapon;
    unsigned weaponSlot = 0;
    unsigned weaponMasteryLimit = 0;
    float masteryDamageMultiplier = 1;
    bool critical = false;
    int ownerType = 0;
    float damageMultiplier = 1;
    float powerupMultiplier = 1;
    int part = 0;
    bool pendingHit = false;
    std::map<ZCombatId, int> hitUntil;
    CBullet script;
    CLightningArc lightning;
    ZBulletVisual *visual = nullptr;
    ZGunCue source;
    float x = 0, y = 0, z = 0;
    float direction = 0;
    float speed = 0;
    float length = 0;
    bool beam = false;
    // CEnemy::FireBullet passes no anchor callback; only gun beams follow a muzzle.
    bool followsMuzzle = false;
    bool spawnCollision = false;
    float spawnNormalX = 0, spawnNormalY = 0;
};

struct ZEffectInstance {
    ZCombatId burstActor = 0;
    std::uint64_t handle = 0;

    CParticleEffectPlayer player;
    ZCombatId actor = 0;
    int slot = 0;
    int part = 0;
    int node = 0;
    const CParticleEffect *data = nullptr;
    float x = 0, y = 0, z = 0, angle = 0;

    ZShot *owner = nullptr;
};

/** CRibbonTrailEffect / TrailEffectHolder state; detached trails drain in place. */
struct ZRibbonInstance {
    ZShot *owner = nullptr;
    ZBulletRibbonSettings settings;
    std::vector<ZCollisionPoint> points;
    unsigned remainingMs = 0;
    unsigned liveAmount = 0;
    float x = 0, y = 0, z = 0;
};


/** Nearest intersection prevents fast projectiles from tunnelling through walls. */
float SegmentFraction(float x, float y, float dx, float dy, const CCollisionData *scene,
    float *normalX = nullptr, float *normalY = nullptr, bool *hit = nullptr) {
    float fraction = 1.0f;
    if (scene == nullptr) { return fraction; }
    for (const ZCollisionEdge &edge : scene->GetEdges()) {
        if (!edge.enabled) { continue; }
        const ZCollisionPoint &a = scene->GetVertices()[edge.firstVertex];
        const ZCollisionPoint &b = scene->GetVertices()[edge.secondVertex];
        const float ex = b.x - a.x, ey = b.y - a.y;
        const float cross = dx * ey - dy * ex;
        if (std::abs(cross) < 0.00001f) { continue; }
        const float t = ((a.x - x) * ey - (a.y - y) * ex) / cross;
        const float u = ((a.x - x) * dy - (a.y - y) * dx) / cross;
        if (t >= 0.0f && t <= fraction && u >= 0.0f && u <= 1.0f) {
            if (hit != nullptr) { *hit = true; }
            fraction = t;
            const float length = std::hypot(ex, ey);
            if (normalX != nullptr && normalY != nullptr && length > 0) {
                *normalX = -ey / length;
                *normalY = ex / length;
            }
        }
    }
    return fraction;
}

}

struct ZWeaponEffects::Impl {
    ZProjectileWorld *world = nullptr;
    ZCombatId nextProjectile = 1;
    CResTOCManager &toc;
    ZPackTables &tables;
    const ZShaderProgram &program;
    ZQuadBatch batch;
    ZAudioPlayer audio;
    std::uint64_t loopSound = 0;
    bool hasViewBounds = false;
    float viewLeft = 0, viewTop = 0, viewWidth = 0, viewHeight = 0;
    std::uint32_t randomState = 1;
    std::size_t shotsFired = 0;
    std::size_t drawnBeamQuads = 0;
    std::size_t drawnLightningQuads = 0;
    std::size_t soundCues = 0;
    std::set<std::uint64_t> frameSounds;
    // Milliseconds of simulated combat, and when each move sound stops covering
    // repeats of itself.
    int audioClockMs = 0;
    std::map<std::uint64_t, int> moveSoundBusyMs;
    // The loop each owner currently has running, so it is not re-armed.
    std::map<ZCombatId, std::uint64_t> activeLoops;
    std::map<std::uint64_t, CGameAssetRef> soundReferences;
    std::uint64_t nextEffectHandle = 1;
    float playerX = 0, playerY = 0;
    std::map<std::uint32_t, std::unique_ptr<CSpriteGlu>> spritePacks;
    std::map<std::uint64_t, ZVisualAnimation> animations;
    std::map<std::uint64_t, std::unique_ptr<ZBulletVisual>> bullets;
    std::map<std::uint64_t, CParticleEffect> effects;
    std::vector<std::unique_ptr<ZShot>> shots;
    std::vector<ZEffectInstance> activeEffects;
    std::shared_ptr<CParticlePool> particlePool;
    std::map<ZCombatId, ZRibbonInstance> ribbons;
    std::map<std::uint64_t, std::unique_ptr<ZTexture>> ribbonColors;

    void BindRibbon(ZShot &shot) {
        if (shot.script.ribbon.capacity == 0) { return; }
        auto &ribbon = ribbons[shot.id];
        ribbon.owner = &shot;
        ribbon.settings = shot.script.ribbon;
    }

    void AdvanceRibbons(int deltaMs) {
        for (auto iterator = ribbons.begin(); iterator != ribbons.end();) {
            auto &ribbon = iterator->second;
            bool alive = false;
            if (ribbon.owner != nullptr) {
                ribbon.x = ribbon.owner->x; ribbon.y = ribbon.owner->y; ribbon.z = ribbon.owner->z;
                alive = !ribbon.owner->script.removed;
            }
            // TrailEffectHolder::Update :294907: at most one sample per tick,
            // including the strict boundary; intervening updates move the head.
            if (ribbon.remainingMs >= static_cast<unsigned>(deltaMs)) {
                ribbon.remainingMs -= deltaMs;
                if (!ribbon.points.empty()) { ribbon.points.back() = {ribbon.x, ribbon.y}; }
            } else {
                ribbon.remainingMs = ribbon.settings.intervalMs;
                if (alive) {
                    if (ribbon.points.size() == ribbon.settings.capacity) { ribbon.points.erase(ribbon.points.begin()); }
                    ribbon.points.push_back({ribbon.x, ribbon.y});
                    ribbon.liveAmount = static_cast<unsigned>(ribbon.points.size());
                } else if (!ribbon.points.empty()) { ribbon.points.erase(ribbon.points.begin()); }
            }
            if (!alive && ribbon.points.empty()) { iterator = ribbons.erase(iterator); }
            else { ++iterator; }
        }
    }

    const ZTexture *RibbonColor(const ZBulletRibbonSettings &settings) {
        const auto &color = settings.color;
        const std::uint64_t key = (static_cast<std::uint64_t>(color[0]) << 48) |
            (static_cast<std::uint64_t>(color[1]) << 32) |
            (static_cast<std::uint64_t>(color[2]) << 16) | color[3];
        auto &texture = ribbonColors[key];
        if (!texture) {
            ZPNGImage pixel;
            pixel.width = 1; pixel.height = 1;
            for (unsigned channel = 0; channel < 4; ++channel) {
                pixel.pixels.push_back(static_cast<std::uint8_t>(std::min<unsigned>(255, color[channel])));
            }
            texture = std::make_unique<ZTexture>();
            if (!texture->Create(pixel)) { return nullptr; }
        }
        return texture.get();
    }

    void DrawLightning(const ZShot &shot, const ZEffectProjection &projection) {
        if (!shot.lightning.IsReady() || shot.script.removed || shot.length <= 0) { return; }
        ZBulletRibbonSettings white;
        white.color = {255, 255, 255, 255}; // CLightningArc constructor :243194.
        const ZTexture *texture = RibbonColor(white);
        if (texture == nullptr) { return; }
        const float arcLength = std::floor(shot.lightning.GetLength());
        const unsigned count = static_cast<unsigned>(shot.length / arcLength) + 1;
        const float alongScale = shot.length / (count * arcLength);
        const float acrossScale = shot.visual->data.GetSpriteScale();
        const float dx = std::cos(shot.direction * kRadians);
        const float dy = std::sin(shot.direction * kRadians);
        // CBullet::Draw :63049: repeat arcs, fit their total length to the ray,
        // and shift each segment's interpolation by one animation frame.
        for (unsigned segment = 0; segment < count; ++segment) {
            auto vertices = shot.lightning.Interpolate(segment);
            for (auto &vertex : vertices) {
                const float across = vertex.x * acrossScale;
                const float along = (segment * arcLength + vertex.y) * alongScale;
                vertex.x = shot.x - dy * across + dx * along;
                vertex.y = shot.y + dx * across + dy * along;
                projection.Position(vertex.x, vertex.y, shot.z);
            }
            for (unsigned index = 2; index < vertices.size(); index += 2) {
                const float positions[] = {vertices[index - 2].x, vertices[index - 2].y,
                    vertices[index - 1].x, vertices[index - 1].y,
                    vertices[index].x, vertices[index].y,
                    vertices[index + 1].x, vertices[index + 1].y};
                const float alpha[] = {1, 1, 1, 1};
                batch.AddGradientQuad(*texture, positions, alpha);
                ++drawnLightningQuads;
            }
        }
    }

    void DrawRibbon(const ZRibbonInstance &ribbon, const ZEffectProjection &projection) {
        const std::size_t count = ribbon.points.size();
        if (count < 3) { return; } // CRibbonTrailEffect::Draw :243694.
        const ZTexture *color = RibbonColor(ribbon.settings);
        if (color == nullptr) { return; }
        float fade = 1;
        if (ribbon.owner == nullptr || ribbon.owner->script.removed) {
            fade = static_cast<float>(count) / ribbon.liveAmount;
        }
        const float opacity = fade;
        std::vector<ZCollisionPoint> points = ribbon.points;
        std::vector<ZCollisionPoint> normals;
        for (std::size_t index = 0; index < count; ++index) {
            projection.Position(points[index].x, points[index].y, ribbon.z);
        }
        for (std::size_t index = 0; index < count; ++index) {
            std::size_t first = index;
            std::size_t last = index + 1;
            if (index > 0) { first = index - 1; last = index; }
            const float dx = points[last].x - points[first].x;
            const float dy = points[last].y - points[first].y;
            const float length = std::hypot(dx, dy);
            ZCollisionPoint normal;
            if (length > 0) {
                const float halfWidth = ribbon.settings.width * 0.5f * projection.scale;
                normal = {-dy / length * halfWidth, dx / length * halfWidth};
            }
            normals.push_back(normal);
        }
        // CMeshLine::Update :243450 interpolates tail alpha 0 toward the
        // authored head color. InsertVertex :242774/:242847 fades each side.
        for (std::size_t index = 1; index < count; ++index) {
            const auto &a = points[index - 1], &b = points[index];
            const auto &na = normals[index - 1], &nb = normals[index];
            const float alpha[] = {0, opacity * (index - 1) / count, 0, opacity * index / count};
            for (int side = -1; side <= 1; side += 2) {
                const float positions[] = {a.x + na.x * side, a.y + na.y * side, a.x, a.y,
                    b.x + nb.x * side, b.y + nb.y * side, b.x, b.y};
                batch.AddGradientQuad(*color, positions, alpha);
            }
        }
    }

    Impl(CResTOCManager &manager, ZPackTables &resources, const ZShaderProgram &shader)
        : toc(manager), tables(resources), program(shader) { batch.Create(program); }

    float Random(float minimum, float maximum) {
        randomState = randomState * 1664525u + 1013904223u;
        return minimum + (maximum - minimum) * static_cast<float>(randomState >> 8) / 16777215.0f;
    }

    /**
     * CBullet::CanBeCulled :60583, as the original writes it.
     *
     * The camera rectangle is compared against the projectile's own bounds one
     * axis at a time, and only on the axis it is travelling along: a bullet is
     * culled once it has left the view moving away from it. A bullet still
     * approaching the view, or one crossing it sideways, is kept.
     */
    bool PastViewBounds(float x, float y, float radius, float velocityX, float velocityY) const {
        if (!hasViewBounds) { return false; }
        if (velocityX < 0 && x + radius < viewLeft) { return true; }
        if (velocityX > 0 && x - radius > viewLeft + viewWidth) { return true; }
        if (velocityY < 0 && y + radius < viewTop) { return true; }
        if (velocityY > 0 && y - radius > viewTop + viewHeight) { return true; }
        return false;
    }

    ZVisualAnimation &Animation(std::uint32_t packHash, int archetype, int animation) {
        const std::uint64_t key = (static_cast<std::uint64_t>(packHash) << 32) |
            (static_cast<std::uint32_t>(archetype) << 16) | static_cast<std::uint16_t>(animation);
        auto found = animations.find(key);
        if (found != animations.end()) { return found->second; }
        ZVisualAnimation &out = animations[key];
        auto &glu = spritePacks[packHash];
        if (!glu) {
            glu.reset(new CSpriteGlu());
            if (!glu->Init(*toc.GetPack(toc.GetPackIndexFromHash(packHash)))) { return out; }
        }
        const ZSpriteArchetype *source = glu->GetArchetype(static_cast<std::uint8_t>(archetype));
        if (source == nullptr || animation < 0 || static_cast<std::uint32_t>(animation) >= source->GetAnimationCount()) { return out; }
        CSpriteIterator iterator(*glu, *source);
        const ZSpriteAnimation &sequence = source->GetAnimation(animation);
        for (std::size_t i = 0; i < sequence.steps.size(); ++i) {
            out.frames.emplace_back();
            iterator.Expand(static_cast<std::uint8_t>(animation), static_cast<std::uint32_t>(i), out.frames.back());
            out.durationMs += sequence.steps[i].durationMs;
            out.endMs.push_back(out.durationMs);
        }
        return out;
    }

    /**
     * The slot CBullet::Draw tiles as the beam body.
     *
     * Bind :63647 tiles the slot the sprite ref names and caps it with the
     * next two. Every beam checked so far names its body slot, except pack5
     * BULLET104 -- the pack7 mech boss beam -- whose ref names the source slot
     * (archetype 139, animation 1), so the unpatched trio tiles the muzzle
     * flare and the beam draws as a bead chain instead of a line. This steps
     * back to the body slot when the named one is a source cap. It is a
     * deliberate deviation from the original bytes, written as the authoring
     * rule rather than as a patch on one resource id; drop it to get the
     * original data back verbatim.
     */
    int BeamBodyAnimation(const CGameSpriteGluRef &ref, int animation) {
        if (animation <= 0) { return animation; }
        ZVisualAnimation &named = Animation(ref.packHash, ref.archetype, animation);
        if (!IsBeamSourceAnimation(named)) { return animation; }
        return animation - 1;
    }

    void AddSprite(ZVisualAnimation &animation, float ageMs, float x, float y,
                   float scaleX, float scaleY, float angle, float alpha) {
        if (animation.frames.empty()) { return; }
        const std::size_t frame = AnimationFrame(animation, ageMs);
        for (const ZSpriteQuad &quad : animation.frames[frame]) {
            batch.AddTransformedQuad(*quad.page, x + quad.offsetX, y + quad.offsetY,
                static_cast<float>(quad.Width()), static_cast<float>(quad.Height()), quad.source, quad.flipHorizontal,
                quad.flipVertical, quad.blend, x, y, scaleX, scaleY, angle, alpha, quad.rotateTexture);
        }
    }

    ZBulletVisual *Bullet(const GameObjectRef &ref) {
        const std::uint64_t key = ResourceKey(ref);
        auto found = bullets.find(key);
        if (found != bullets.end()) { return found->second.get(); }
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Bullet, ref.localIndex, payload)) { return nullptr; }
        std::unique_ptr<ZBulletVisual> visual(new ZBulletVisual());
        CArrayInputStream stream(payload);
        if (!visual->data.Init(stream)) { return nullptr; }
        if (visual->data.HasMesh() && visual->data.HasImage()) {
            visual->mesh.reset(new ZPlayerPart());
            const CGameAssetRef &mesh = visual->data.GetMeshRef();
            const CGameAssetRef &atlas = visual->data.GetImageRef();
            if (!LoadMeshAndAtlas(tables, "projectile", mesh.packHash, mesh.assetId, atlas.packHash,
                atlas.assetId, visual->mesh->mesh, visual->mesh->texture)) { return nullptr; }
            if (!visual->mesh->buffer.Create(program) || !visual->mesh->buffer.SetMesh(visual->mesh->mesh)) { return nullptr; }
        }
        ZBulletVisual *result = visual.get();
        bullets[key] = std::move(visual);
        return result;
    }

    void PlaySound(const ZGunCue &cue, ZCombatId actor = kPlayerCombatId) {
        if (cue.kind == ZGunCue::Kind::StopSound) {
            audio.StopOwner(actor);
            activeLoops.erase(actor);
            if (actor == kPlayerCombatId) { loopSound = 0; }
            return;
        }
        const auto key = ResourceKey(cue.resource);
        auto found = soundReferences.find(key);
        if (found == soundReferences.end()) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(cue.resource.packHash, ZGameSection::SoundEffect,
                cue.resource.localIndex, payload)) {
                std::printf("[weapon-audio] missing sound %08x:%u\n", cue.resource.packHash, cue.resource.localIndex);
                return;
            }
            CArrayInputStream stream(payload);
            CGameAssetRef wav;
            wav.Init(stream);
            if (stream.Overran()) { return; }
            found = soundReferences.emplace(key, wav).first;
        }
        const CGameAssetRef &wav = found->second;
        if (wav.assetId < 0) { return; }
        PlayWav(wav.packHash, wav.assetId, cue.kind == ZGunCue::Kind::LoopSound, actor);
    }

    void PlayWav(std::uint32_t packHash, int ordinal, bool loop = false, ZCombatId actor = kPlayerCombatId,
                 bool moveSound = false) {
        CGameAssetRef wav;
        wav.packHash = packHash;
        wav.assetId = ordinal;
        const std::uint64_t key = (static_cast<std::uint64_t>(wav.packHash) << 32) | wav.assetId;
        // User-verified simultaneous-death behaviour. Coalesce by resolved WAV,
        // not enemy ID.
        if (!loop && frameSounds.find(key) != frameSounds.end()) { return; }
        // Host audio adaptation, not original behaviour: the original starts a
        // separate voice per actor (CMoveSetMeshController::Update :134066 ->
        // CSoundQueue::PlaySound :102896 -> CMediaPlayer::PlayInternal :363259
        // allocates a new sound event every call), so a group death is many
        // copies of one WAV on top of each other and reads as a single hit. One
        // voice per WAV cannot get louder, so a kill streak spread over a few
        // ticks would instead retrigger it over and over. Move-frame sounds --
        // the death and animation cues -- therefore wait out the copy already
        // playing. Gun cues keep the per-tick rule so rapid fire stays rapid.
        const auto busy = moveSoundBusyMs.find(key);
        if (moveSound && !loop && busy != moveSoundBusyMs.end() && busy->second > audioClockMs) { return; }
        if (!audio.HasSound(key)) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(wav.packHash, ZGameSection::Wav, wav.assetId, payload)) {
                std::printf("[weapon-audio] missing WAV %08x:%d\n", wav.packHash, wav.assetId);
                return;
            }
            if (!audio.Load(key, payload)) { return; }
        }
        if (loop) {
            // Re-arming a loop that is already running restarts its attack
            // every tick, which is what turns a machine's running sound into
            // a buzz. Leave the copy that is already playing alone.
            const auto running = activeLoops.find(actor);
            if (running != activeLoops.end() && running->second == key) { return; }
            audio.StopOwner(actor);
            activeLoops[actor] = key;
            if (actor == kPlayerCombatId) { loopSound = key; }
        } else if (!moveSound) {
            // Host audio adaptation: one voice per WAV. The original layers
            // copies (PlayInternal :363259 builds a new sound event per call)
            // and absorbs them in its own per-event gain (:303199), which this
            // host has no data for -- layering here just makes one effect
            // louder the faster it repeats, which is exactly how a turret ends
            // up drowning out everything else. Retrigger instead of stacking:
            // the rate is unchanged, the level stays where the WAV put it.
            audio.Stop(key);
        }
        if (audio.Play(key, loop, actor)) {
            ++soundCues;
            if (!loop) { frameSounds.insert(key); }
            if (moveSound && !loop) { moveSoundBusyMs[key] = audioClockMs + audio.GetDurationMs(key); }
        }
    }

    void StartEffect(const GameObjectRef &ref, float x, float y, float z, float angle, ZShot *owner,
        std::shared_ptr<CParticlePool> ownerPool = nullptr) {
        const std::uint64_t key = ResourceKey(ref);
        auto found = effects.find(key);
        if (found == effects.end()) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(ref.packHash, ZGameSection::ParticleEffect, ref.localIndex, payload)) { return; }
            CParticleEffect effect;
            CArrayInputStream stream(payload);
            if (!effect.Init(stream)) { return; }
            found = effects.emplace(key, effect).first;
        }
        ZEffectInstance effect;
        effect.data = &found->second;
        effect.owner = owner;
        effect.x = x; effect.y = y; effect.z = z; effect.angle = angle;
        if (!ownerPool) { ownerPool = particlePool; }
        effect.player.Init(*effect.data, std::move(ownerPool));
        effect.player.SetLooping(owner != nullptr);

        activeEffects.push_back(std::move(effect));
    }

    void DetachRibbon(ZShot *owner) {
        if (owner != nullptr) {
            const auto found = ribbons.find(owner->id);
            if (found != ribbons.end()) { found->second.owner = nullptr; }
        }
    }

    void StopTrail(ZShot *owner) {
        if (owner == nullptr) { return; }
        std::size_t index = 0;
        while (index < activeEffects.size()) {
            if (activeEffects[index].owner == owner) {
                activeEffects[index].player.StopSpawning();
                activeEffects[index].owner = nullptr;
            }
            ++index;
        }
    }

    void Cue(const ZGunCue &cue, float x, float y, float z, float direction, ZShot *owner = nullptr) {
        if (world != nullptr && owner != nullptr &&
            (cue.kind == ZGunCue::Kind::Splash || cue.kind == ZGunCue::Kind::SpawnEnemy)) {
            ZCombatHit hit;
            hit.projectile = owner->id;
            hit.owner = owner->owner;
            hit.weapon = owner->weapon;
            hit.weaponSlot = owner->weaponSlot;
            hit.bullet = owner->source.resource;
            hit.critical = owner->critical;
            hit.weaponMasteryLimit = owner->weaponMasteryLimit;
            hit.ownerType = owner->ownerType;
            hit.flags = owner->script.flags;
            hit.x = x;
            hit.y = y;
            hit.direction = direction;
            // Original splash natives use their authored damage and owner
            // armor/level multiplier, independently of the bullet's base damage.
            // CBullet::FunctionResolver :61169/:61242/:61379 uses native
            // arguments directly; Configure's mastery roll scales direct hits.
            hit.damage = cue.damage * world->GetDamageMultiplier(owner->owner, owner->damageMultiplier);
            hit.percentDamage = cue.percentDamage;
            hit.spawnObjectId = cue.spawnObjectId;
            hit.forceSpawn = cue.forceSpawn;
            if (cue.kind == ZGunCue::Kind::Splash) {
                world->Splash(hit, cue.radius, cue.cone, cue.force, cue.forceMs);
            } else { world->SpawnFromProjectile(cue.resource, hit); }
            return;
        }
        if (cue.kind == ZGunCue::Kind::StopTrail) { StopTrail(owner); return; }
        if (cue.kind == ZGunCue::Kind::Effect || cue.kind == ZGunCue::Kind::Trail) {
            float angle = 0;
            if (cue.alignEffect) { angle = direction + 90.0f; }
            if (cue.kind == ZGunCue::Kind::Trail) {
                StopTrail(owner);
                StartEffect(cue.resource, x, y, z, angle, owner, cue.particlePool);
            } else { StartEffect(cue.resource, x, y, z, angle, nullptr, cue.particlePool); }
        } else if (cue.kind == ZGunCue::Kind::Sound || cue.kind == ZGunCue::Kind::LoopSound ||
                   cue.kind == ZGunCue::Kind::StopSound) {
            ZCombatId soundOwner = kPlayerCombatId;
            if (owner != nullptr) { soundOwner = owner->owner; }
            PlaySound(cue, soundOwner);
        }
    }

    // CParticle::Spawn selects an animation once using RandomBit.
    void AdvanceParticles(int deltaMs) {
        if (deltaMs <= 0) { return; }
        for (std::size_t index = 0; index < activeEffects.size();) {
            auto &effect = activeEffects[index];
            if (effect.actor != 0 && world != nullptr) {
                float direction = 0;
                if (world->Anchor(effect.actor, effect.part, effect.node, effect.x, effect.y, effect.z, direction)) {
                    effect.angle = direction + 90;
                } else {
                    effect.player.StopSpawning();
                    effect.actor = 0;
                }
            }
            if (effect.owner != nullptr) {
                effect.x = effect.owner->x;
                effect.y = effect.owner->y;
                effect.z = effect.owner->z;
            }
            effect.player.SetPosition(effect.x, effect.y, effect.z, effect.angle);
            effect.player.Update(deltaMs, randomState);
            if (effect.player.IsDone()) { activeEffects.erase(activeEffects.begin() + index); }
            else { ++index; }
        }
    }
};

ZWeaponEffects::ZWeaponEffects(CResTOCManager &toc, ZPackTables &tables, const ZShaderProgram &program,
    std::shared_ptr<CParticlePool> particlePool) : m_impl(new Impl(toc, tables, program)) {
    // CMap allocates 200 slots (:91849). Menus/powerups supply
    // their shared owner pool explicitly instead of allocating per effect.
    if (!particlePool) { particlePool = std::make_shared<CParticlePool>(200); }
    m_impl->particlePool = std::move(particlePool);
}
ZWeaponEffects::~ZWeaponEffects() = default;

std::vector<ZWeaponProjectileState> ZWeaponEffects::GetProjectileStates() const {
    std::vector<ZWeaponProjectileState> result;
    for (const auto &shot : m_impl->shots) {
        result.push_back({shot->source.resource, shot->owner, shot->beam, shot->x, shot->y,
            shot->direction, shot->length, shot->script.animation, shot->script.ageMs});
        auto &state = result.back();
        if (!shot->beam) { state.collisionRadius = shot->visual->data.GetRadius(); }
        state.collisionEnabled = shot->script.HasActiveCollision() && !shot->script.removed;
    }
    return result;
}

void ZWeaponEffects::SetCombatWorld(ZProjectileWorld *world) { m_impl->world = world; }

std::uint64_t ZWeaponEffects::StartPersistentEffect(const GameObjectRef &resource, float x, float y, bool loop,
    std::shared_ptr<CParticlePool> particlePool) {
    const std::size_t previous = m_impl->activeEffects.size();
    m_impl->StartEffect(resource, x, y, 0, 0, nullptr, std::move(particlePool));
    if (m_impl->activeEffects.size() == previous) { return 0; }
    ZEffectInstance &effect = m_impl->activeEffects.back();
    // CParticleEffect::Init :131032 derives the period from emitter end times.
    // An attached infinite emitter stays alive between emissions.
    effect.player.SetLooping(loop || effect.data->GetDurationMs() == 0);
    effect.handle = m_impl->nextEffectHandle++;
    return effect.handle;
}

void ZWeaponEffects::StopEffect(std::uint64_t handle) {
    if (handle == 0) { return; }
    for (auto iterator = m_impl->activeEffects.begin(); iterator != m_impl->activeEffects.end(); ++iterator) {
        if (iterator->handle == handle) { m_impl->activeEffects.erase(iterator); return; }
    }
}

void ZWeaponEffects::StopSpawning(std::uint64_t handle) {
    if (handle == 0) { return; }
    for (auto &effect : m_impl->activeEffects) {
        if (effect.handle != handle) { continue; }
        effect.player.StopSpawning();
        effect.handle = 0;
        return;
    }
}

void ZWeaponEffects::AdvanceAmbientEffects(int deltaMs) {
    BeginAudioFrame();
    m_impl->AdvanceParticles(deltaMs);
    m_impl->audio.Update();
}

void ZWeaponEffects::BeginAudioFrame() { m_impl->frameSounds.clear(); }

unsigned ZWeaponEffects::GetVoiceCount() const { return m_impl->audio.GetVoiceCount(); }

void ZWeaponEffects::SetViewBounds(float centerX, float centerY, float width, float height) {
    m_impl->hasViewBounds = width > 0 && height > 0;
    m_impl->viewLeft = centerX - width * 0.5f;
    m_impl->viewTop = centerY - height * 0.5f;
    m_impl->viewWidth = width;
    m_impl->viewHeight = height;
}

ZCombatId ZWeaponEffects::SpawnProjectile(const GameObjectRef &resource, float x, float y,
    float z, float direction, float speed, ZCombatId owner, int ownerType, int part, int node) {
    Impl &scene = *m_impl;
    ZBulletVisual *visual = scene.Bullet(resource);
    if (visual == nullptr) { return 0; }
    std::unique_ptr<ZShot> shot(new ZShot());
    shot->id = scene.nextProjectile++;
    shot->owner = owner;
    shot->ownerType = ownerType;
    if (scene.world != nullptr) { shot->damageMultiplier = scene.world->GetDamageMultiplier(owner); }
    if (scene.world != nullptr) { shot->powerupMultiplier = scene.world->GetProjectilePowerupMultiplier(owner); }
    shot->part = part;
    shot->source.node = node;
    shot->source.resource = resource;
    shot->visual = visual;
    shot->x = x;
    shot->y = y;
    shot->z = z;
    shot->direction = direction;
    shot->speed = speed;
    shot->beam = (visual->data.GetFlags() & kBeamFlag) != 0;
    if (scene.world != nullptr) { shot->script.SetLevelContext(scene.world->GetScriptLevel()); }
    shot->script.Bind(visual->data, false);
    const ZCombatId id = shot->id;
    for (const ZGunCue &cue : shot->script.TakeCues()) {
        scene.Cue(cue, x, y, z, direction, shot.get());
    }
    scene.shots.push_back(std::move(shot));
    ++scene.shotsFired;
    return id;
}

void ZWeaponEffects::ResolveHit(ZCombatId projectile, ZHitResult result) {
    for (auto &shot : m_impl->shots) {
        if (shot->id == projectile && shot->pendingHit) {
            shot->pendingHit = false;
            shot->script.OnCollision(result);
            return;
        }
    }
}

bool ZWeaponEffects::RemoveOldestProjectile(ZCombatId owner) {
    for (auto &shot : m_impl->shots) {
        if (shot->owner == owner && !shot->script.removed) {
            shot->script.ForceRemoval();
            return true;
        }
    }
    return false;
}

void ZWeaponEffects::RetireOwner(ZCombatId owner) {
    m_impl->audio.StopOwner(owner);
    // The id can come back on a new actor; do not let a stale entry keep its
    // loop silent.
    m_impl->activeLoops.erase(owner);
    for (auto &shot : m_impl->shots) {
        if (shot->owner == owner && shot->beam) {
            shot->script.removed = true;
            m_impl->StopTrail(shot.get());
        }
    }
    if (owner == kPlayerCombatId) {
        m_impl->loopSound = 0;
    }
}

void ZWeaponEffects::PlayMoveSound(const GameObjectRef &sound) {
    m_impl->PlayWav(sound.packHash, sound.localIndex, false, kPlayerCombatId, true);
}

void ZWeaponEffects::Emit(const ZGunCue &cue, float x, float y, float z, float direction,
    ZCombatId actor, int slot, int part, int node) {
    Impl &scene = *m_impl;
    if (cue.kind == ZGunCue::Kind::Sound || cue.kind == ZGunCue::Kind::LoopSound || cue.kind == ZGunCue::Kind::StopSound) {
        scene.PlaySound(cue, actor);
        return;
    }
    if (actor != 0 && (cue.kind == ZGunCue::Kind::Trail || cue.kind == ZGunCue::Kind::StopTrail)) {
        for (std::size_t i = 0; i < scene.activeEffects.size();) {
            const ZEffectInstance &effect = scene.activeEffects[i];
            if (effect.actor == actor && effect.slot == slot) {
                if (cue.stopParticlesImmediately) { scene.activeEffects[i].player.Stop(); }
                else { scene.activeEffects[i].player.StopSpawning(); }
                scene.activeEffects[i].actor = 0;
            }
            ++i;
        }
        if (cue.kind == ZGunCue::Kind::Trail) {
            const std::size_t previous = scene.activeEffects.size();
            scene.StartEffect(cue.resource, x, y, z, direction + 90, nullptr, cue.particlePool);
            if (scene.activeEffects.size() > previous) {
                ZEffectInstance &effect = scene.activeEffects.back();
                effect.actor = actor;
                effect.player.SetLooping(cue.loopParticles);
                effect.slot = slot;
                effect.part = part;
                effect.node = node;
            }
        }
        return;
    }
    const auto previous = scene.activeEffects.size();
    scene.Cue(cue, x, y, z, direction);
    // Keep attribution separate from the anchor: death bursts stay in place.
    if (scene.activeEffects.size() > previous) { scene.activeEffects.back().burstActor = actor; }
}

bool ZWeaponEffects::HasActorBurst(ZCombatId actor) const {
    for (const auto &effect : m_impl->activeEffects) {
        if (effect.burstActor == actor) { return true; }
    }
    return false;
}

void ZWeaponEffects::Clear() {
    m_impl->moveSoundBusyMs.clear();
    m_impl->activeLoops.clear();
    m_impl->audioClockMs = 0;
    m_impl->shots.clear();
    m_impl->ribbons.clear();
    m_impl->activeEffects.clear();

    m_impl->audio.StopAll();
    m_impl->frameSounds.clear();
    m_impl->loopSound = 0;
}

void ZWeaponEffects::SetPaused(bool paused) { m_impl->audio.SetPaused(paused); }
std::size_t ZWeaponEffects::GetBulletCount() const { return m_impl->shots.size(); }
std::size_t ZWeaponEffects::GetRibbonCount() const { return m_impl->ribbons.size(); }
std::size_t ZWeaponEffects::GetDrawnBeamQuadCount() const { return m_impl->drawnBeamQuads; }
std::size_t ZWeaponEffects::GetDrawnLightningQuadCount() const { return m_impl->drawnLightningQuads; }
std::size_t ZWeaponEffects::GetParticleCount() const {
    std::size_t count = 0;
    for (const auto &effect : m_impl->activeEffects) { count += effect.player.GetParticleCount(); }
    return count;
}
std::size_t ZWeaponEffects::GetEffectCount() const {
    std::size_t count = 0;
    for (const auto &effect : m_impl->activeEffects) {
        if (!effect.player.IsDone()) { ++count; }
    }
    return count;
}
std::size_t ZWeaponEffects::GetTrailCount() const {
    std::size_t count = 0;
    for (const ZEffectInstance &effect : m_impl->activeEffects) {
        if (effect.owner != nullptr) { ++count; }
    }
    return count;
}
std::size_t ZWeaponEffects::GetShotCount() const { return m_impl->shotsFired; }
std::size_t ZWeaponEffects::GetSoundCueCount() const { return m_impl->soundCues; }

void ZWeaponEffects::EmitBrother(ZPlayerModel &player, const float *modelToScene, float facingDegrees,
    ZCombatId owner, const ZWeaponCollision *collision) {
    Impl &scene = *m_impl;
    if (!player.weapon) { return; }
    for (const ZGunCue &cue : player.weapon->brother.TakeCues()) {
        if (cue.kind == ZGunCue::Kind::Grenade) {
            if (!player.weapon->brother.CanThrowGrenade(cue.hand)) { continue; }
            ZMeshBoneTransform origin{};
            // GetGunNodeLocation(1) uses torso node 2, independently of the gun.
            if (!player.weapon->brother.GetTorso().GetAnimation().GetNodeAt(2, origin)) { continue; }
            const float x = modelToScene[0] * origin.posX + modelToScene[1] * origin.posY + modelToScene[2] * origin.posZ + modelToScene[3];
            const float y = modelToScene[4] * origin.posX + modelToScene[5] * origin.posY + modelToScene[6] * origin.posZ + modelToScene[7];
            const float z = modelToScene[8] * origin.posX + modelToScene[9] * origin.posY + modelToScene[10] * origin.posZ + modelToScene[11];
            if (SpawnProjectile(cue.resource, x, y, z, facingDegrees - 90, 430, owner, 0) != 0) {
                player.weapon->brother.OnGrenadeThrown(cue.hand);
            }
            continue;
        }
        if (cue.kind == ZGunCue::Kind::Splash && scene.world != nullptr) {
            ZCombatHit hit;
            hit.owner = owner;
            hit.ownerType = 0;
            hit.damage = cue.damage;
            hit.percentDamage = cue.percentDamage;
            hit.x = modelToScene[3];
            hit.y = modelToScene[7];
            scene.world->Splash(hit, cue.radius, cue.cone, cue.force, cue.forceMs);
            continue;
        }
        Emit(cue, modelToScene[3], modelToScene[7], 0, facingDegrees - 90, owner, cue.hand, -1, -1);
    }
    for (const ZMoveSoundRef &sound : player.weapon->brother.GetTorso().TakeSounds()) {
        scene.PlayWav(sound.packHash, sound.localIndex, false, owner);
    }
    for (const ZMoveSoundRef &sound : player.weapon->brother.GetLegs().TakeSounds()) {
        scene.PlayWav(sound.packHash, sound.localIndex, false, owner);
    }
    const float direction = facingDegrees - 90.0f;
    // CLevel::UpdateNormal iterates a growing object list: bullets spawned
    // by a player are advanced before their first draw in the same tick.
    for (const ZGunCue &cue : player.ActiveWeapon().gun.TakeCues()) {
        if (cue.kind == ZGunCue::Kind::Sound || cue.kind == ZGunCue::Kind::LoopSound || cue.kind == ZGunCue::Kind::StopSound) {
            scene.PlaySound(cue, owner);
            continue;
        }
        int copies = 1;
        if (cue.hand == 2) { copies = 2; }
        for (int copy = 0; copy < copies; ++copy) {
            int hand = cue.hand;
            if (copies == 2) { hand = copy; }
            float x = 0, y = 0, z = 0;
            if (!ProjectMuzzle(player, modelToScene, hand, cue.node, x, y, z)) { continue; }
            if (cue.kind != ZGunCue::Kind::Bullet) { scene.Cue(cue, x, y, z, direction); continue; }
            ZBulletVisual *visual = scene.Bullet(cue.resource);
            if (visual == nullptr) { continue; }
            std::unique_ptr<ZShot> shot(new ZShot());
            shot->id = scene.nextProjectile++;
            shot->owner = owner;
            shot->weapon = player.gunResource;
            shot->weaponSlot = player.gunSlot;
            shot->followsMuzzle = true;
            shot->weaponMasteryLimit = player.ActiveWeapon().data.GetMasteryLimit();
            float masteryRoll = 1;
            if (player.ActiveWeapon().gun.GetMasteryLevel() > 0) { masteryRoll = scene.Random(0, 1); }
            shot->masteryDamageMultiplier = player.ActiveWeapon().gun.GetMasteryDamageMultiplier(masteryRoll, &shot->critical);
            if (scene.world != nullptr) { shot->powerupMultiplier = scene.world->GetProjectilePowerupMultiplier(owner); }
            shot->part = hand;
            shot->visual = visual;
            shot->source = cue;
            shot->source.hand = hand;
            shot->x = x; shot->y = y; shot->z = z;
            shot->direction = direction + scene.Random(cue.minimumAngle, cue.maximumAngle);
            shot->speed = kShotSpeed * cue.speed;
            shot->beam = (visual->data.GetFlags() & kBeamFlag) != 0;
            if (m_impl->world != nullptr) { shot->script.SetLevelContext(m_impl->world->GetScriptLevel()); }
            shot->script.Bind(visual->data, cue.alternate);
            player.ActiveWeapon().gun.AddBullet(shot->script);
            // CBullet::Fire :62212-62243 tests owner -> muzzle before movement.
            // A zero-speed mine can already be beyond the terrain at birth.
            if (collision != nullptr) {
                const CCollisionData *birthCollision = &collision->walls;
                if ((shot->script.flags & 0x20) != 0) { birthCollision = &collision->terrain; }
                SegmentFraction(modelToScene[3], modelToScene[7], x - modelToScene[3], y - modelToScene[7],
                    birthCollision, &shot->spawnNormalX, &shot->spawnNormalY, &shot->spawnCollision);
            }
            if (shot->beam) {
                const float beamLength = static_cast<float>(shot->script.maximumBeamLength);
                const float dx = std::cos(shot->direction * kRadians) * beamLength;
                const float dy = std::sin(shot->direction * kRadians) * beamLength;
                const CCollisionData *walls = nullptr;
                if (collision != nullptr) {
                    walls = &collision->walls;
                    if ((visual->data.GetFlags() & 0x20) != 0) { walls = &collision->terrain; }
                }
                shot->length = beamLength * SegmentFraction(x, y, dx, dy, walls);
            }
            for (const ZGunCue &spawnCue : shot->script.TakeCues()) { scene.Cue(spawnCue, x, y, z, shot->direction, shot.get()); }
            scene.shots.push_back(std::move(shot));
            ++scene.shotsFired;
        }
    }
}

void ZWeaponEffects::Update(ZPlayerModel &player, const float *modelToScene, float facingDegrees,
    int deltaMs, const ZWeaponCollision *collision) {
    Impl &scene = *m_impl;
    // Combat time, for the move-sound window in PlayWav. Advanced before the
    // early exit so a scene without a player still ages its cues.
    if (deltaMs > 0) { scene.audioClockMs += deltaMs; }
    if (!player.weapon || deltaMs <= 0) { return; }
    if (scene.world == nullptr) { BeginAudioFrame(); }
    scene.playerX = modelToScene[3];
    scene.playerY = modelToScene[7];
    EmitBrother(player, modelToScene, facingDegrees, kPlayerCombatId, collision);
    const float direction = facingDegrees - 90;
    for (auto &shot : scene.shots) {
        int shotDeltaMs = deltaMs;
        if (shot->ownerType == 1 && scene.world != nullptr) {
            shotDeltaMs = std::max(1, static_cast<int>(std::lround(deltaMs * scene.world->GetEnemyTimeScale())));
        }
        const CCollisionData *shotCollision = nullptr;
        if (collision != nullptr) {
            shotCollision = &collision->walls;
            if ((shot->script.flags & 0x20) != 0) { shotCollision = &collision->terrain; }
        }
        const CGameSpriteGluRef &sprite = shot->visual->data.GetSpriteRef();
        const int duration = scene.Animation(sprite.packHash, sprite.archetype, shot->script.animation).durationMs;
        shot->script.Update(shotDeltaMs, duration);
        if (shot->beam && !shot->script.removed) {
            shot->lightning.Update(shot->script.lightning, shotDeltaMs, scene.randomState);
        }
        scene.BindRibbon(*shot);
        if (shot->script.removed) {
            for (const ZGunCue &cue : shot->script.TakeCues()) {
                scene.Cue(cue, shot->x, shot->y, shot->z, shot->direction, shot.get());
            }
            continue;
        }
        ZCombatHit hit;
        hit.projectile = shot->id;
        hit.owner = shot->owner;
        hit.weapon = shot->weapon;
        hit.weaponSlot = shot->weaponSlot;
        hit.bullet = shot->source.resource;
        hit.critical = shot->critical;
        hit.weaponMasteryLimit = shot->weaponMasteryLimit;
        hit.ownerType = shot->ownerType;
        hit.flags = shot->script.flags;
        hit.x = shot->x;
        hit.y = shot->y;
        hit.damage = shot->script.GetDamage() * shot->powerupMultiplier * shot->masteryDamageMultiplier;
        if (scene.world != nullptr) {
            hit.damage *= scene.world->GetDamageMultiplier(shot->owner, shot->damageMultiplier);
        }
        if (scene.world != nullptr && shot->script.seekRadius > 0 && !shot->beam) {
            float targetX = 0, targetY = 0;
            if (scene.world->FindTarget(hit, shot->script.seekRadius, targetX, targetY)) {
                shot->direction = std::atan2(targetY - shot->y, targetX - shot->x) / kRadians;
            }
        }
        const float startX = shot->x, startY = shot->y;
        bool hitWall = shot->spawnCollision;
        float wallNormalX = shot->spawnNormalX, wallNormalY = shot->spawnNormalY;
        shot->spawnCollision = false;
        const float radians = shot->direction * kRadians;
        if (!hitWall && shot->beam) {
            const float beamLength = static_cast<float>(shot->script.maximumBeamLength);
            if (shot->followsMuzzle && shot->owner == kPlayerCombatId) {
                if (!player.ActiveWeapon().gun.IsShooting()) { shot->script.removed = true; }
                ProjectMuzzle(player, modelToScene, shot->source.hand, shot->source.node, shot->x, shot->y, shot->z);
                shot->direction = direction;
            } else if (shot->followsMuzzle && scene.world != nullptr && !scene.world->Anchor(shot->owner, shot->part,
                shot->source.node, shot->x, shot->y, shot->z, shot->direction)) {
                shot->script.removed = true;
            }
            const float dx = std::cos(shot->direction * kRadians) * beamLength;
            const float dy = std::sin(shot->direction * kRadians) * beamLength;
            shot->length = beamLength * SegmentFraction(shot->x, shot->y, dx, dy, shotCollision);
        } else if (!hitWall) {
            shot->speed = std::max(0.0f, shot->speed + shot->script.acceleration * shotDeltaMs * 0.001f);
            const float distance = shot->speed * shot->script.velocityScale * shotDeltaMs * 0.001f;
            const float dx = std::cos(radians) * distance, dy = std::sin(radians) * distance;
            const float fraction = SegmentFraction(shot->x, shot->y, dx, dy, shotCollision, &wallNormalX, &wallNormalY);
            shot->x += dx * fraction; shot->y += dy * fraction;
            hitWall = fraction < 1.0f;
            // CBullet::Update :63537 culls a moved projectile as soon as
            // CanBeCulled agrees, and CBullet::Remove(this, 1) :60862 retires
            // it with no hit event and no wall event. Template flag 0x10 keeps
            // a projectile alive off screen; beams are never culled.
            if ((shot->script.flags & 0x10) == 0 &&
                scene.PastViewBounds(shot->x, shot->y, shot->visual->data.GetRadius(), dx, dy)) {
                shot->script.removed = true;
                hitWall = false;
            }
        }
        // CEnemy::HandleCollision :71435 leaves an unhandled event pending in
        // the enemy, but single-player bullets keep moving/testing collisions.
        // Only the multiplayer ApplyCollision branch :71676 pauses a bullet.
        if (scene.world != nullptr && shot->script.HasActiveCollision() && !shot->script.removed) {
            float x = startX, y = startY;
            float dx = shot->x - startX, dy = shot->y - startY;
            if (shot->beam) {
                x = shot->x;
                y = shot->y;
                dx = std::cos(shot->direction * kRadians) * shot->length;
                dy = std::sin(shot->direction * kRadians) * shot->length;
            }
            std::vector<ZCombatId> skip;
            for (auto it = shot->hitUntil.begin(); it != shot->hitUntil.end();) {
                if (it->second <= shot->script.ageMs) { it = shot->hitUntil.erase(it); }
                else { skip.push_back(it->first); ++it; }
            }
            // Sweep the full segment, so a fast projectile cannot jump over a
            // target. Penetrating projectiles continue through remaining actors.
            for (int contact = 0; contact < 64; ++contact) {
                // CBullet::UpdateBeam -> RayCastNearest uses a ray, not the
                // bullet's large authored sprite/collision radius (up to 345).
                float radius = shot->visual->data.GetRadius();
                if (shot->beam) { radius = 0; }
                const ZCombatTrace trace = scene.world->Trace(hit, x, y, dx, dy, radius, skip);
                if (trace.target == 0) { break; }
                hit.x = x + dx * trace.fraction;
                hit.y = y + dy * trace.fraction;
                hit.direction = shot->direction;
                hit.part = trace.part;
                hit.edge = trace.edge;
                const ZHitResult result = scene.world->ApplyHit(trace.target, hit);
                shot->pendingHit = result == ZHitResult::Pending;
                shot->script.OnCollision(result);
                skip.push_back(trace.target);
                if (!shot->beam) { shot->hitUntil[trace.target] = shot->script.ageMs + 100; }
                if (shot->beam) {
                    shot->length *= trace.fraction;
                    break;
                }
                if (shot->script.removed) {
                    shot->x = hit.x;
                    shot->y = hit.y;
                    break;
                }
                if ((shot->script.flags & 0x1000) != 0) {
                    const float normalLength = std::hypot(trace.normalX, trace.normalY);
                    if (normalLength > 0) {
                        const float nx = trace.normalX / normalLength, ny = trace.normalY / normalLength;
                        const float vx = std::cos(shot->direction * kRadians), vy = std::sin(shot->direction * kRadians);
                        const float dot = vx * nx + vy * ny;
                        shot->direction = std::atan2(vy - 2 * dot * ny, vx - 2 * dot * nx) / kRadians;
                    }
                    shot->x = hit.x;
                    shot->y = hit.y;
                    break;
                }
            }
        }
        // Resolve actor/prop contacts along the clipped segment before the
        // wall event retires the shot. Otherwise destructible props are walls
        // that can never receive their original on-hit script callback.
        if (hitWall && !shot->script.removed) {
            if ((shot->script.flags & 0x800) != 0) {
                const float vx = std::cos(shot->direction * kRadians);
                const float vy = std::sin(shot->direction * kRadians);
                const float dot = vx * wallNormalX + vy * wallNormalY;
                const float reflectedX = vx - 2 * dot * wallNormalX;
                const float reflectedY = vy - 2 * dot * wallNormalY;
                shot->direction = std::atan2(reflectedY, reflectedX) / kRadians;
                // Separate the next sweep from this exact edge contact.
                shot->x += reflectedX * 0.01f;
                shot->y += reflectedY * 0.01f;
            }
            shot->script.OnWallCollision();
        }
        for (const ZGunCue &cue : shot->script.TakeCues()) { scene.Cue(cue, shot->x, shot->y, shot->z, shot->direction, shot.get()); }
    }
    scene.AdvanceRibbons(deltaMs);
    std::size_t i = 0;
    while (i < scene.shots.size()) {
        if (scene.shots[i]->script.removed) {
            scene.StopTrail(scene.shots[i].get());
            scene.DetachRibbon(scene.shots[i].get());
            scene.shots.erase(scene.shots.begin() + i);
        }
        else { ++i; }
    }
    scene.AdvanceParticles(deltaMs);
    scene.audio.Update();
}

void ZWeaponEffects::Draw(const float *sceneMvp, const float *previewProjection, float meshCameraScale, ZWeaponDrawPass pass) {
    Impl &scene = *m_impl;
    const ZEffectProjection projection(previewProjection);
    glDisable(GL_DEPTH_TEST);
    scene.batch.Begin();
    scene.drawnBeamQuads = 0;
    scene.drawnLightningQuads = 0;
    for (const auto &entry : scene.ribbons) {
        const auto &ribbon = entry.second;
        const bool behindPlayer = std::hypot(ribbon.x - scene.playerX, ribbon.y - scene.playerY) < 100 || ribbon.y + 10 < scene.playerY;
        if (pass == ZWeaponDrawPass::BehindPlayer && !behindPlayer) { continue; }
        if (pass == ZWeaponDrawPass::InFrontOfPlayer && behindPlayer) { continue; }
        scene.DrawRibbon(ribbon, projection);
    }
    for (auto &shot : scene.shots) {
        // CBullet::GetZOrder (:60280) puts a player's bullet behind its
        // shooter while within 100 world units. Otherwise use world Y + 10.
        // This hides the backward half of long tracers inside the gun mesh.
        const float distance = std::hypot(shot->x - scene.playerX, shot->y - scene.playerY);
        const bool behindPlayer = distance < 100.0f || shot->y + 10.0f < scene.playerY;
        if (pass == ZWeaponDrawPass::BehindPlayer && !behindPlayer) { continue; }
        if (pass == ZWeaponDrawPass::InFrontOfPlayer && behindPlayer) { continue; }
        if (!shot->script.visible) { continue; }
        std::size_t beforeQuads = 0;
        if (shot->beam) { beforeQuads = scene.batch.GetQuadCount(); }
        const CGameSpriteGluRef &ref = shot->visual->data.GetSpriteRef();
        int bodyAnimation = shot->script.animation;
        if (shot->beam) { bodyAnimation = scene.BeamBodyAnimation(ref, bodyAnimation); }
        ZVisualAnimation &animation = scene.Animation(ref.packHash, ref.archetype, bodyAnimation);
        const float age = static_cast<float>(shot->script.animationAgeMs);
        const float scale = shot->visual->data.GetSpriteScale() * projection.scale;
        float x = shot->x, y = shot->y;
        projection.Position(x, y, shot->z);
        const float direction = projection.Direction(shot->direction);
        if (shot->beam && (shot->script.flags & 0x400) == 0) {
            // Beam sprites have body / end / source animations in consecutive slots.
            // Corrected from CBullet::Draw: base+1 is source, base+2 is end.
            ZVisualAnimation &start = scene.Animation(ref.packHash, ref.archetype, bodyAnimation + 1);
            ZVisualAnimation &end = scene.Animation(ref.packHash, ref.archetype, bodyAnimation + 2);
            float endX = shot->x + std::cos(shot->direction * kRadians) * shot->length;
            float endY = shot->y + std::sin(shot->direction * kRadians) * shot->length;
            projection.Position(endX, endY, shot->z);
            const float length = std::hypot(endX - x, endY - y);
            const float dx = std::cos(direction * kRadians), dy = std::sin(direction * kRadians);
            float startTop, startBottom, endTop, endBottom, bodyTop, bodyBottom;
            FrameBounds(start, age, startTop, startBottom);
            FrameBounds(end, age, endTop, endBottom);
            FrameBounds(animation, age, bodyTop, bodyBottom);
            float startHalf = (startBottom - startTop) * scale * 0.5f;
            float endHalf = (endBottom - endTop) * scale * 0.5f;
            const bool caps = (shot->script.flags & 0x200) == 0;
            if (!caps) { startHalf = 0; endHalf = 0; }
            const float bodyLength = std::max(0.0f, length - startHalf - endHalf);
            const float tileHeight = (bodyBottom - bodyTop) * scale;
            if (tileHeight > 0 && bodyLength > 0) {
                const int count = static_cast<int>(bodyLength / tileHeight) + 1;
                const float tileLength = bodyLength / count;
                const float scaleY = tileLength / (bodyBottom - bodyTop);
                for (int tile = 0; tile < count; ++tile) {
                    const float distance = startHalf + tile * tileLength + bodyBottom * scaleY;
                    scene.AddSprite(animation, age, x + dx * distance, y + dy * distance,
                                    scale, scaleY, direction + 90, 1);
                }
            }
            // CBullet::Draw :62998/:63026 always draws both caps. When the
            // remaining body is non-positive, the end uses the source origin.
            if (caps) {
                const float startOffset = (startTop + startBottom) * scale * 0.5f;
                const float endOffset = (endTop + endBottom) * scale * 0.5f;
                scene.AddSprite(start, age, x + dx * startOffset, y + dy * startOffset,
                                scale, scale, direction + 90, 1);
                float endPivotX = endX + dx * endOffset;
                float endPivotY = endY + dy * endOffset;
                if (bodyLength <= 0) {
                    endPivotX = x + dx * startOffset;
                    endPivotY = y + dy * startOffset;
                }
                scene.AddSprite(end, age, endPivotX, endPivotY,
                                scale, scale, direction + 90, 1);
            }
        } else if (!shot->beam) {
            float angle = direction + 90;
            if ((shot->script.flags & 0x80) != 0) { angle = 0; }
            const float fraction = shot->script.GetTrajectoryFraction();
            const float phase = shot->script.GetTrajectoryPhaseScale();
            const float shadowScale = scale + fraction * phase * projection.scale;
            const float alpha = 1 - 0.75f * fraction;
            scene.AddSprite(animation, age, x, y, shadowScale, shadowScale, angle, alpha);
        }
        scene.DrawLightning(*shot, projection);
        if (shot->visual->mesh) {
            ZPlayerPart &part = *shot->visual->mesh;
            float base[kMatrix4dElements];
            const float apparentScale = shot->visual->data.GetMeshScale() + 25 * shot->script.GetTrajectoryHeight();
            const float meshScale = apparentScale * part.mesh.GetBounds().inverseExtent * projection.scale * meshCameraScale;
            BuildPlayerGameMatrix(sceneMvp, x, y, meshScale, direction + 90, base);
            part.buffer.Draw(scene.program, base, part.texture);
        }
        if (shot->beam) { scene.drawnBeamQuads += scene.batch.GetQuadCount() - beforeQuads; }
    }
    if (pass == ZWeaponDrawPass::BehindPlayer) {
        scene.batch.Upload();
        scene.batch.Draw(scene.program, sceneMvp);
        return;
    }
    for (const auto &effect : scene.activeEffects) {
        for (std::size_t index = 0; index < effect.player.GetParticleCount(); ++index) {
            const auto &particle = effect.player.GetParticle(index);
            const ZParticleEmitterTemplate &emitter = effect.data->GetEmitters()[particle.emitterIndex];
            ZVisualAnimation *animation = &scene.Animation(effect.data->GetSpritePackHash(), emitter.archetype, particle.animation);
            const float uniformScale = particle.Value(2) * projection.scale;
            float angle = particle.Rotation(emitter);

            angle = projection.Direction(angle - 90) + 90;
            float x = particle.x, y = particle.y;
            projection.Position(x, y, particle.z);
            scene.AddSprite(*animation, particle.ageMs, x, y,
                particle.Value(0) * uniformScale,
                particle.Value(1) * uniformScale, angle,
                std::clamp(particle.Value(3), 0.0f, 1.0f));
        }
    }
    scene.batch.Upload();
    scene.batch.Draw(scene.program, sceneMvp);
}
