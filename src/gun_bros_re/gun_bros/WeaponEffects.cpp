/** @file WeaponEffects.cpp
 * @brief BIG-backed projectile sprites, meshes, particles and weapon audio.
 */
#define NOMINMAX
#include "gun_bros/WeaponEffects.h"
#include "engine/CAudioPlayer.h"
#include "engine/CMatrix4d.h"
#include "gun_bros/CBullet.h"
#include "gun_bros/CParticleEffect.h"
#include "sprite_glu/CSpriteIterator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace {
constexpr float kRadians = 3.14159265f / 180.0f;
constexpr float kShotSpeed = 450.0f;
constexpr float kMaximumBeamLength = 1400.0f;
constexpr std::uint32_t kBeamFlag = 0x100;

/** Project the same animated muzzle transform used to draw the weapon. */
bool ProjectMuzzle(PlayerModel &player, const float *matrix, int hand, int node,
                   float &x, float &y, float &z) {
    MeshBoneTransform muzzle{};
    if (!GetPlayerMuzzle(player, hand, node, muzzle)) { return false; }
    x = matrix[0] * muzzle.posX + matrix[1] * muzzle.posY + matrix[2] * muzzle.posZ + matrix[3];
    y = matrix[4] * muzzle.posX + matrix[5] * muzzle.posY + matrix[6] * muzzle.posZ + matrix[7];
    z = matrix[8] * muzzle.posX + matrix[9] * muzzle.posY + matrix[10] * muzzle.posZ + matrix[11];
    return true;
}

std::uint64_t ResourceKey(const GameObjectRef &ref) {
    return (static_cast<std::uint64_t>(ref.packHash) << 32) | ref.localIndex;
}

struct VisualAnimation {
    std::vector<std::vector<SpriteQuad>> frames;
    std::vector<int> endMs;
    int durationMs = 0;
};

std::size_t AnimationFrame(const VisualAnimation &animation, float ageMs) {
    int time = 0;
    if (animation.durationMs > 0) { time = static_cast<int>(ageMs) % animation.durationMs; }
    std::size_t frame = 0;
    while (frame + 1 < animation.frames.size() && time >= animation.endMs[frame]) { ++frame; }
    return frame;
}

/** The beam tiles the complete frame bounds, including all layered quads. */
void FrameBounds(const VisualAnimation &animation, float ageMs, float &top, float &bottom) {
    top = 0; bottom = 0;
    if (animation.frames.empty()) { return; }
    bool first = true;
    for (const SpriteQuad &quad : animation.frames[AnimationFrame(animation, ageMs)]) {
        if (first) { top = static_cast<float>(quad.offsetY); bottom = top; first = false; }
        top = std::min(top, static_cast<float>(quad.offsetY));
        bottom = std::max(bottom, static_cast<float>(quad.offsetY + quad.Height()));
    }
}

/** Billboards keep their authored size while position and travel use world units. */
struct EffectProjection {
    const float *matrix = nullptr;
    float scale = 1;

    explicit EffectProjection(const float *projection) : matrix(projection) {
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

struct BulletVisual {
    CBullet::Template data;
    std::unique_ptr<PlayerPart> mesh;
};

struct Shot {
    CBullet script;
    BulletVisual *visual = nullptr;
    GunCue source;
    float x = 0, y = 0, z = 0;
    float direction = 0;
    float speed = 0;
    float length = kMaximumBeamLength;
    bool beam = false;
};

struct EffectInstance {
    const CParticleEffect *data = nullptr;
    float x = 0, y = 0, z = 0, angle = 0;
    float ageMs = 0;
    std::vector<float> nextSpawn;
    Shot *owner = nullptr;
};

struct Particle {
    const CParticleEffect *data = nullptr;
    std::size_t emitter = 0;
    float x = 0, y = 0, velocityX = 0, velocityY = 0;
    float ageMs = 0, lifetimeMs = 0, angle = 0;
    float z = 0;
    int animation = 0;
    std::array<float, kParticleInterpolatorChannelCount> random{};
};

/** Nearest intersection prevents fast projectiles from tunnelling through walls. */
float SegmentFraction(float x, float y, float dx, float dy, const CCollisionData *scene) {
    float fraction = 1.0f;
    if (scene == nullptr) { return fraction; }
    for (const CollisionEdge &edge : scene->GetEdges()) {
        if (!edge.enabled) { continue; }
        const CollisionPoint &a = scene->GetVertices()[edge.firstVertex];
        const CollisionPoint &b = scene->GetVertices()[edge.secondVertex];
        const float ex = b.x - a.x, ey = b.y - a.y;
        const float cross = dx * ey - dy * ex;
        if (std::abs(cross) < 0.00001f) { continue; }
        const float t = ((a.x - x) * ey - (a.y - y) * ex) / cross;
        const float u = ((a.x - x) * dy - (a.y - y) * dx) / cross;
        if (t >= 0.0f && t < fraction && u >= 0.0f && u <= 1.0f) { fraction = t; }
    }
    return fraction;
}

float ParticleValue(const ParticleEmitterTemplate &emitter, const Particle &particle,
                    std::size_t channel, float value) {
    const float random = particle.random[channel];
    for (const ParticleInterpolatorKey &key : emitter.interpolators[channel]) {
        if (particle.ageMs < key.startMs) { return value; }
        float start = value;
        if (!key.keepPreviousStart) { start = key.startMinimum + (key.startMaximum - key.startMinimum) * random; }
        const float end = key.endMinimum + (key.endMaximum - key.endMinimum) * random;
        if (key.durationMs == 0 || particle.ageMs >= key.startMs + key.durationMs) { value = end; }
        else { return start + (end - start) * (particle.ageMs - key.startMs) / key.durationMs; }
    }
    return value;
}
}

struct WeaponEffects::Impl {
    CResTOCManager &toc;
    PackTables &tables;
    const CShaderProgram &program;
    CQuadBatch batch;
    CAudioPlayer audio;
    std::uint64_t loopSound = 0;
    std::uint32_t randomState = 1;
    std::size_t shotsFired = 0;
    std::size_t soundCues = 0;
    float playerX = 0, playerY = 0;
    std::map<std::uint32_t, std::unique_ptr<CSpriteGlu>> spritePacks;
    std::map<std::uint64_t, VisualAnimation> animations;
    std::map<std::uint64_t, std::unique_ptr<BulletVisual>> bullets;
    std::map<std::uint64_t, CParticleEffect> effects;
    std::vector<std::unique_ptr<Shot>> shots;
    std::vector<EffectInstance> activeEffects;
    std::vector<Particle> particles;

    Impl(CResTOCManager &manager, PackTables &resources, const CShaderProgram &shader)
        : toc(manager), tables(resources), program(shader) { batch.Create(program); }

    float Random(float minimum, float maximum) {
        randomState = randomState * 1664525u + 1013904223u;
        return minimum + (maximum - minimum) * static_cast<float>(randomState >> 8) / 16777215.0f;
    }

    VisualAnimation &Animation(std::uint32_t packHash, int archetype, int animation) {
        const std::uint64_t key = (static_cast<std::uint64_t>(packHash) << 32) |
            (static_cast<std::uint32_t>(archetype) << 16) | static_cast<std::uint16_t>(animation);
        auto found = animations.find(key);
        if (found != animations.end()) { return found->second; }
        VisualAnimation &out = animations[key];
        auto &glu = spritePacks[packHash];
        if (!glu) {
            glu.reset(new CSpriteGlu());
            if (!glu->Init(*toc.GetPack(toc.GetPackIndexFromHash(packHash)))) { return out; }
        }
        const CSpriteGluArchetype *source = glu->GetArchetype(static_cast<std::uint8_t>(archetype));
        if (source == nullptr || animation < 0 || static_cast<std::uint32_t>(animation) >= source->GetAnimationCount()) { return out; }
        CSpriteIterator iterator(*glu, *source);
        const SpriteAnimation &sequence = source->GetAnimation(animation);
        for (std::size_t i = 0; i < sequence.steps.size(); ++i) {
            out.frames.emplace_back();
            iterator.Expand(static_cast<std::uint8_t>(animation), static_cast<std::uint32_t>(i), out.frames.back());
            out.durationMs += sequence.steps[i].durationMs;
            out.endMs.push_back(out.durationMs);
        }
        return out;
    }

    void AddSprite(VisualAnimation &animation, float ageMs, float x, float y,
                   float scaleX, float scaleY, float angle, float alpha) {
        if (animation.frames.empty()) { return; }
        const std::size_t frame = AnimationFrame(animation, ageMs);
        for (const SpriteQuad &quad : animation.frames[frame]) {
            batch.AddTransformedQuad(*quad.page, x + quad.offsetX, y + quad.offsetY,
                static_cast<float>(quad.Width()), static_cast<float>(quad.Height()), quad.source, quad.flipHorizontal,
                quad.flipVertical, quad.blend, x, y, scaleX, scaleY, angle, alpha, quad.rotateTexture);
        }
    }

    BulletVisual *Bullet(const GameObjectRef &ref) {
        const std::uint64_t key = ResourceKey(ref);
        auto found = bullets.find(key);
        if (found != bullets.end()) { return found->second.get(); }
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(ref.packHash, GameSection::Bullet, ref.localIndex, payload)) { return nullptr; }
        std::unique_ptr<BulletVisual> visual(new BulletVisual());
        CArrayInputStream stream(payload);
        if (!visual->data.Init(stream)) { return nullptr; }
        if (visual->data.HasMesh() && visual->data.HasImage()) {
            visual->mesh.reset(new PlayerPart());
            const CGameAssetRef &mesh = visual->data.GetMeshRef();
            const CGameAssetRef &atlas = visual->data.GetImageRef();
            if (!LoadMeshAndAtlas(tables, "projectile", mesh.packHash, mesh.assetId, atlas.packHash,
                atlas.assetId, visual->mesh->mesh, visual->mesh->texture)) { return nullptr; }
            if (!visual->mesh->buffer.Create(program) || !visual->mesh->buffer.SetMesh(visual->mesh->mesh)) { return nullptr; }
        }
        BulletVisual *result = visual.get();
        bullets[key] = std::move(visual);
        return result;
    }

    void PlaySound(const GunCue &cue) {
        if (cue.kind == GunCue::Kind::StopSound) {
            audio.Stop(loopSound);
            loopSound = 0;
            return;
        }
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(cue.resource.packHash, GameSection::SoundEffect,
            cue.resource.localIndex, payload)) {
            std::printf("[weapon-audio] missing sound %08x:%u\n", cue.resource.packHash, cue.resource.localIndex);
            return;
        }
        CArrayInputStream stream(payload);
        CGameAssetRef wav;
        wav.Init(stream);
        if (wav.assetId < 0) { return; }
        PlayWav(wav.packHash, wav.assetId, cue.kind == GunCue::Kind::LoopSound);
    }

    void PlayWav(std::uint32_t packHash, int ordinal, bool loop = false) {
        CGameAssetRef wav;
        wav.packHash = packHash;
        wav.assetId = ordinal;
        std::vector<std::uint8_t> payload;
        const std::uint64_t key = (static_cast<std::uint64_t>(wav.packHash) << 32) | wav.assetId;
        if (!tables.ReadSectionResource(wav.packHash, GameSection::Wav, wav.assetId, payload)) {
            std::printf("[weapon-audio] missing WAV %08x:%d\n", wav.packHash, wav.assetId);
            return;
        }
        if (!audio.Load(key, payload)) { return; }
        if (loop) {
            audio.Stop(loopSound);
            loopSound = key;
        }
        if (audio.Play(key, loop)) { ++soundCues; }
    }

    void StartEffect(const GameObjectRef &ref, float x, float y, float z, float angle, Shot *owner) {
        const std::uint64_t key = ResourceKey(ref);
        auto found = effects.find(key);
        if (found == effects.end()) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(ref.packHash, GameSection::ParticleEffect, ref.localIndex, payload)) { return; }
            CParticleEffect effect;
            CArrayInputStream stream(payload);
            if (!effect.Init(stream)) { return; }
            found = effects.emplace(key, effect).first;
        }
        EffectInstance effect;
        effect.data = &found->second;
        effect.owner = owner;
        effect.x = x; effect.y = y; effect.z = z; effect.angle = angle;
        for (const ParticleEmitterTemplate &emitter : effect.data->GetEmitters()) {
            effect.nextSpawn.push_back(std::max(0.0f, emitter.startSeconds * 1000.0f));
        }
        activeEffects.push_back(effect);
    }

    void StopTrail(Shot *owner) {
        std::size_t index = 0;
        while (index < activeEffects.size()) {
            if (activeEffects[index].owner == owner) { activeEffects.erase(activeEffects.begin() + index); }
            else { ++index; }
        }
    }

    void Cue(const GunCue &cue, float x, float y, float z, float direction, Shot *owner = nullptr) {
        if (cue.kind == GunCue::Kind::StopTrail) { StopTrail(owner); return; }
        if (cue.kind == GunCue::Kind::Effect || cue.kind == GunCue::Kind::Trail) {
            float angle = 0;
            if (cue.alignEffect) { angle = direction + 90.0f; }
            if (cue.kind == GunCue::Kind::Trail) {
                StopTrail(owner);
                StartEffect(cue.resource, x, y, z, angle, owner);
            } else { StartEffect(cue.resource, x, y, z, angle, nullptr); }
        } else if (cue.kind == GunCue::Kind::Sound || cue.kind == GunCue::Kind::LoopSound ||
                   cue.kind == GunCue::Kind::StopSound) {
            PlaySound(cue);
        }
    }

    void SpawnParticle(const EffectInstance &effect, std::size_t index) {
        const ParticleEmitterTemplate &emitter = effect.data->GetEmitters()[index];
        Particle particle;
        particle.data = effect.data;
        particle.emitter = index;
        particle.lifetimeMs = static_cast<float>(emitter.GetParticleLifetimeMs());
        if (particle.lifetimeMs <= 0) { return; }
        particle.angle = effect.angle;
        particle.z = effect.z;
        // CParticle::Spawn selects an animation once using RandomBit.
        particle.animation = emitter.SelectAnimation(Random(0, 1));
        if (particle.animation < 0) { return; }
        for (float &value : particle.random) { value = Random(0, 1); }
        float x = 0, y = 0;
        if (emitter.pattern == ParticleSpawnPattern::Line) {
            const float fraction = Random(0, 1);
            x = emitter.patternValues[0] + (emitter.patternValues[2] - emitter.patternValues[0]) * fraction;
            y = emitter.patternValues[1] + (emitter.patternValues[3] - emitter.patternValues[1]) * fraction;
        } else if (emitter.pattern == ParticleSpawnPattern::Rectangle) {
            x = Random(emitter.patternValues[0], emitter.patternValues[2]);
            y = Random(emitter.patternValues[1], emitter.patternValues[3]);
        } else {
            const float outer = Random(emitter.patternValues[2], emitter.patternValues[3]);
            const float width = Random(emitter.patternValues[4], emitter.patternValues[5]);
            const float radius = Random(outer - width, outer);
            const float angle = Random(0, 360) * kRadians;
            x = emitter.patternValues[0] + std::sin(angle) * radius;
            y = emitter.patternValues[1] - std::cos(angle) * radius;
        }
        float vx = 0, vy = 0;
        if (emitter.velocity == ParticleSpawnVelocity::Linear) {
            vx = Random(emitter.velocityValues[0], emitter.velocityValues[1]);
            vy = Random(emitter.velocityValues[2], emitter.velocityValues[3]);
        } else {
            const float angle = Random(emitter.velocityValues[0], emitter.velocityValues[1]) * kRadians;
            const float speed = Random(emitter.velocityValues[2], emitter.velocityValues[3]);
            vx = std::sin(angle) * speed; vy = std::cos(angle) * speed;
        }
        const float cosine = std::cos(effect.angle * kRadians), sine = std::sin(effect.angle * kRadians);
        particle.x = effect.x + x * cosine - y * sine;
        particle.y = effect.y + x * sine + y * cosine;
        particle.velocityX = vx * cosine - vy * sine;
        particle.velocityY = vx * sine + vy * cosine;
        particles.push_back(particle);
    }

    void AdvanceParticles(int deltaMs) {
        for (Particle &particle : particles) {
            const ParticleEmitterTemplate &emitter = particle.data->GetEmitters()[particle.emitter];
            particle.ageMs += deltaMs;
            const float seconds = deltaMs * 0.001f;
            particle.velocityX += emitter.accelerationX * seconds;
            particle.velocityY += emitter.accelerationY * seconds;
            const float speed = ParticleValue(emitter, particle, 5, 1);
            particle.x += particle.velocityX * seconds * speed;
            particle.y += particle.velocityY * seconds * speed;
        }
        std::size_t p = 0;
        while (p < particles.size()) {
            if (particles[p].ageMs >= particles[p].lifetimeMs) { particles.erase(particles.begin() + p); }
            else { ++p; }
        }
        std::size_t i = 0;
        while (i < activeEffects.size()) {
            EffectInstance &effect = activeEffects[i];
            if (effect.owner != nullptr) {
                effect.x = effect.owner->x; effect.y = effect.owner->y; effect.z = effect.owner->z;
            }
            effect.ageMs += deltaMs;
            bool pending = false;
            for (std::size_t j = 0; j < effect.nextSpawn.size(); ++j) {
                const ParticleEmitterTemplate &emitter = effect.data->GetEmitters()[j];
                float end = std::max(0.0f, std::max(emitter.startSeconds, emitter.endSeconds) * 1000.0f);
                // An attached infinite emitter stays alive between emissions.
                const bool continuous = effect.owner != nullptr && emitter.endSeconds < 0;
                while (effect.nextSpawn[j] <= effect.ageMs && (continuous || effect.nextSpawn[j] <= end)) {
                    SpawnParticle(effect, j);
                    const float interval = Random(emitter.intervalMinimumSeconds, emitter.intervalMaximumSeconds) * 1000.0f;
                    // UpdateEmitters stops after one spawn when the authored
                    // interval rounds to zero; it does not emit 1000 per second.
                    if (interval < 1.0f) {
                        effect.nextSpawn[j] = effect.ageMs + 1.0f;
                        break;
                    }
                    effect.nextSpawn[j] += interval;
                }
                if (continuous || effect.nextSpawn[j] <= end) { pending = true; }
            }
            if (!pending) { activeEffects.erase(activeEffects.begin() + i); }
            else { ++i; }
        }
    }
};

WeaponEffects::WeaponEffects(CResTOCManager &toc, PackTables &tables, const CShaderProgram &program)
    : m_impl(new Impl(toc, tables, program)) {}
WeaponEffects::~WeaponEffects() = default;

void WeaponEffects::Clear() {
    m_impl->shots.clear();
    m_impl->activeEffects.clear();
    m_impl->particles.clear();
    m_impl->audio.StopAll();
    m_impl->loopSound = 0;
}

void WeaponEffects::SetPaused(bool paused) { m_impl->audio.SetPaused(paused); }
std::size_t WeaponEffects::GetBulletCount() const { return m_impl->shots.size(); }
std::size_t WeaponEffects::GetParticleCount() const { return m_impl->particles.size(); }
std::size_t WeaponEffects::GetTrailCount() const {
    std::size_t count = 0;
    for (const EffectInstance &effect : m_impl->activeEffects) {
        if (effect.owner != nullptr) { ++count; }
    }
    return count;
}
std::size_t WeaponEffects::GetShotCount() const { return m_impl->shotsFired; }
std::size_t WeaponEffects::GetSoundCueCount() const { return m_impl->soundCues; }

void WeaponEffects::Update(PlayerModel &player, const float *modelToScene, float facingDegrees,
                           int deltaMs, const WeaponCollision *collision) {
    Impl &scene = *m_impl;
    if (!player.weapon || deltaMs <= 0) { return; }
    scene.playerX = modelToScene[3];
    scene.playerY = modelToScene[7];
    for (const GameObjectRef &sound : player.weapon->brother.GetTorso().TakeSounds()) {
        scene.PlayWav(sound.packHash, sound.localIndex);
    }
    for (const GameObjectRef &sound : player.weapon->brother.GetLegs().TakeSounds()) {
        scene.PlayWav(sound.packHash, sound.localIndex);
    }
    const float direction = facingDegrees - 90.0f;
    const float beamLength = kMaximumBeamLength;
    // CLevel::UpdateNormal iterates a growing object list: bullets spawned
    // by a player are advanced before their first draw in the same tick.
    for (const GunCue &cue : player.weapon->gun.TakeCues()) {
        if (cue.kind == GunCue::Kind::RemoveBullet) {
            if (!scene.shots.empty()) {
                scene.StopTrail(scene.shots.front().get());
                scene.shots.erase(scene.shots.begin());
            }
            continue;
        }
        if (cue.kind == GunCue::Kind::Sound || cue.kind == GunCue::Kind::LoopSound || cue.kind == GunCue::Kind::StopSound) {
            scene.PlaySound(cue);
            continue;
        }
        int copies = 1;
        if (cue.hand == 2) { copies = 2; }
        for (int copy = 0; copy < copies; ++copy) {
            int hand = cue.hand;
            if (copies == 2) { hand = copy; }
            float x = 0, y = 0, z = 0;
            if (!ProjectMuzzle(player, modelToScene, hand, cue.node, x, y, z)) { continue; }
            if (cue.kind != GunCue::Kind::Bullet) { scene.Cue(cue, x, y, z, direction); continue; }
            BulletVisual *visual = scene.Bullet(cue.resource);
            if (visual == nullptr) { continue; }
            std::unique_ptr<Shot> shot(new Shot());
            shot->visual = visual;
            shot->source = cue;
            shot->source.hand = hand;
            shot->x = x; shot->y = y; shot->z = z;
            shot->direction = direction + scene.Random(cue.minimumAngle, cue.maximumAngle);
            shot->speed = kShotSpeed * cue.speed;
            shot->beam = (visual->data.GetFlags() & kBeamFlag) != 0;
            if (shot->beam) {
                const float dx = std::cos(shot->direction * kRadians) * beamLength;
                const float dy = std::sin(shot->direction * kRadians) * beamLength;
                const CCollisionData *walls = nullptr;
                if (collision != nullptr) {
                    walls = &collision->walls;
                    if ((visual->data.GetFlags() & 0x20) != 0) { walls = &collision->terrain; }
                }
                shot->length = beamLength * SegmentFraction(x, y, dx, dy, walls);
            }
            shot->script.Bind(visual->data, cue.alternate);
            for (const GunCue &spawnCue : shot->script.TakeCues()) { scene.Cue(spawnCue, x, y, z, shot->direction, shot.get()); }
            scene.shots.push_back(std::move(shot));
            ++scene.shotsFired;
        }
    }
    for (auto &shot : scene.shots) {
        const CCollisionData *shotCollision = nullptr;
        if (collision != nullptr) {
            shotCollision = &collision->walls;
            if ((shot->script.flags & 0x20) != 0) { shotCollision = &collision->terrain; }
        }
        const CGameSpriteGluRef &sprite = shot->visual->data.GetSpriteRef();
        const int duration = scene.Animation(sprite.packHash, sprite.archetype, shot->script.animation).durationMs;
        shot->script.Update(deltaMs, duration);
        if (shot->script.removed) {
            for (const GunCue &cue : shot->script.TakeCues()) {
                scene.Cue(cue, shot->x, shot->y, shot->z, shot->direction, shot.get());
            }
            continue;
        }
        const float radians = shot->direction * kRadians;
        if (shot->beam) {
            if (!player.weapon->gun.IsShooting()) { shot->script.removed = true; }
            ProjectMuzzle(player, modelToScene, shot->source.hand, shot->source.node, shot->x, shot->y, shot->z);
            shot->direction = direction;
            const float dx = std::cos(direction * kRadians) * beamLength;
            const float dy = std::sin(direction * kRadians) * beamLength;
            shot->length = beamLength * SegmentFraction(shot->x, shot->y, dx, dy, shotCollision);
        } else {
            shot->speed = std::max(0.0f, shot->speed + shot->script.acceleration * deltaMs * 0.001f);
            const float distance = shot->speed * shot->script.velocityScale * deltaMs * 0.001f;
            const float dx = std::cos(radians) * distance, dy = std::sin(radians) * distance;
            const float fraction = SegmentFraction(shot->x, shot->y, dx, dy, shotCollision);
            shot->x += dx * fraction; shot->y += dy * fraction;
            if (fraction < 1.0f) { shot->script.Hit(); }
        }
        for (const GunCue &cue : shot->script.TakeCues()) { scene.Cue(cue, shot->x, shot->y, shot->z, shot->direction, shot.get()); }
    }
    std::size_t i = 0;
    while (i < scene.shots.size()) {
        if (scene.shots[i]->script.removed) {
            scene.StopTrail(scene.shots[i].get());
            scene.shots.erase(scene.shots.begin() + i);
        }
        else { ++i; }
    }
    scene.AdvanceParticles(deltaMs);
    scene.audio.Update();
}

void WeaponEffects::Draw(const float *sceneMvp, const float *previewProjection, float meshCameraScale, WeaponDrawPass pass) {
    Impl &scene = *m_impl;
    const EffectProjection projection(previewProjection);
    glDisable(GL_DEPTH_TEST);
    scene.batch.Begin();
    for (auto &shot : scene.shots) {
        // CBullet::GetZOrder (:60280) puts a player's bullet behind its
        // shooter while within 100 world units. Otherwise use world Y + 10.
        // This hides the backward half of long tracers inside the gun mesh.
        const float distance = std::hypot(shot->x - scene.playerX, shot->y - scene.playerY);
        const bool behindPlayer = distance < 100.0f || shot->y + 10.0f < scene.playerY;
        if (pass == WeaponDrawPass::BehindPlayer && !behindPlayer) { continue; }
        if (pass == WeaponDrawPass::InFrontOfPlayer && behindPlayer) { continue; }
        if (!shot->script.visible) { continue; }
        const CGameSpriteGluRef &ref = shot->visual->data.GetSpriteRef();
        VisualAnimation &animation = scene.Animation(ref.packHash, ref.archetype, shot->script.animation);
        const float age = static_cast<float>(shot->script.animationAgeMs);
        const float scale = shot->visual->data.GetSpriteScale() * projection.scale;
        float x = shot->x, y = shot->y;
        projection.Position(x, y, shot->z);
        const float direction = projection.Direction(shot->direction);
        if (shot->beam && (shot->script.flags & 0x400) == 0) {
            // Beam sprites have body / end / source animations in consecutive slots.
            // Corrected from CBullet::Draw: base+1 is source, base+2 is end.
            VisualAnimation &start = scene.Animation(ref.packHash, ref.archetype, shot->script.animation + 1);
            VisualAnimation &end = scene.Animation(ref.packHash, ref.archetype, shot->script.animation + 2);
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
            if (caps && length > startHalf + endHalf) {
                const float startOffset = (startTop + startBottom) * scale * 0.5f;
                const float endOffset = (endTop + endBottom) * scale * 0.5f;
                scene.AddSprite(start, age, x + dx * startOffset, y + dy * startOffset,
                                scale, scale, direction + 90, 1);
                scene.AddSprite(end, age, endX + dx * endOffset, endY + dy * endOffset,
                                scale, scale, direction + 90, 1);
            }
        } else if (!shot->beam) {
            float angle = direction + 90;
            if ((shot->script.flags & 0x80) != 0) { angle = 0; }
            scene.AddSprite(animation, age, x, y, scale, scale, angle, 1);
        }
        if (shot->visual->mesh) {
            PlayerPart &part = *shot->visual->mesh;
            float base[kMatrix4dElements];
            const float meshScale = shot->visual->data.GetMeshScale() * part.mesh.GetBounds().inverseExtent * projection.scale * meshCameraScale;
            BuildPlayerGameMatrix(sceneMvp, x, y, meshScale, direction + 90, base);
            part.buffer.Draw(scene.program, base, part.texture);
        }
    }
    if (pass == WeaponDrawPass::BehindPlayer) {
        scene.batch.Upload();
        scene.batch.Draw(scene.program, sceneMvp);
        return;
    }
    for (const Particle &particle : scene.particles) {
        const ParticleEmitterTemplate &emitter = particle.data->GetEmitters()[particle.emitter];
        VisualAnimation *animation = &scene.Animation(particle.data->GetSpritePackHash(), emitter.archetype, particle.animation);
        const float uniformScale = ParticleValue(emitter, particle, 2, 1) * projection.scale;
        float angle = particle.angle + ParticleValue(emitter, particle, 4, 0);
        if (emitter.alignToVelocity) {
            angle += std::atan2(particle.velocityY, particle.velocityX) / kRadians + 90 - particle.angle;
        }
        angle = projection.Direction(angle - 90) + 90;
        float x = particle.x, y = particle.y;
        projection.Position(x, y, particle.z);
        scene.AddSprite(*animation, particle.ageMs, x, y,
            ParticleValue(emitter, particle, 0, 1) * uniformScale,
            ParticleValue(emitter, particle, 1, 1) * uniformScale, angle,
            std::clamp(ParticleValue(emitter, particle, 3, 1), 0.0f, 1.0f));
    }
    scene.batch.Upload();
    scene.batch.Draw(scene.program, sceneMvp);
}
