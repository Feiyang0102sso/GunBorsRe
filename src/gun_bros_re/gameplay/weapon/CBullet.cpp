/**
 * @file CBullet.cpp
 * @brief The projectile template: a sprite, a model, and a pile of scalars.
 */

#include "gun_bros_re/gameplay/weapon/CBullet.h"

#include <cstdio>
#include <cmath>

namespace {

// The same 16.16 fixed point every template scalar uses.
constexpr float kFixedPointScale = 1.0f / 65536.0f;
constexpr std::uint32_t kHavenBeamPackHash = 0x00267585; // pack5.
constexpr unsigned kHavenBeamBulletIndex = 104;

float ReadFixedPoint(CArrayInputStream &stream) {
    return static_cast<float>(stream.ReadInt32()) * kFixedPointScale;
}

}  // namespace

CBullet::Template::Template()
    : m_value16(0),
      m_value18(0),
      m_value20(0),
      m_flag32(0),
      m_scalar28(0.0f),
      m_value128(0),
      m_scalar24(0.0f),
      m_scalar256(0.0f),
      m_scalar116(0.0f),
      m_scalar120(0.0f),
      m_value124(0),
      m_scalar260(0.0f),
      m_value264(0),
      m_flag266(0) {}

bool CBullet::Template::Init(CArrayInputStream &stream) {
    m_mesh.reset();
    m_sprite.Init(stream);
    m_meshRef.Init(stream);
    m_imageRef.Init(stream);

    // One byte the original steps over without reading.
    stream.Skip(1);

    m_value16 = stream.ReadInt16();
    m_value18 = stream.ReadInt16();
    m_value20 = stream.ReadInt16();
    m_flag32 = stream.ReadUInt8();
    m_scalar28 = ReadFixedPoint(stream);

    m_script.Load(stream);

    m_value128 = stream.ReadUInt32();
    m_scalar24 = ReadFixedPoint(stream);
    m_scalar256 = ReadFixedPoint(stream);
    m_scalar116 = ReadFixedPoint(stream);
    m_scalar120 = ReadFixedPoint(stream);
    m_value124 = stream.ReadUInt16();
    m_scalar260 = ReadFixedPoint(stream);
    m_value264 = stream.ReadUInt16();
    m_flag266 = stream.ReadUInt8();

    if (stream.Overran()) {
        std::printf("[bullet] template truncated\n");
        return false;
    }

    return true;
}

void CBullet::Bind(const Template &data, bool alternate) {
    this->data = &data;
    seekingTurnRate = data.GetSeekingTurnRate();
    seekingTarget = Collision::NoObject;
    zOrderGroup = 3;
    maximumBeamLength = 3000; // CBullet::Bind :63673.
    effects.Clear(); // CBullet::Bind :63653 clears old attachments.
    m_trailHandle = 0;
    m_ribbonHandle = 0;
    m_retirementStarted = false;
    ribbon = {};
    lightning = {};
    m_trajectoryHeight = data.GetTrajectoryHeight();
    m_trajectoryDurationMs = data.GetTrajectoryDurationMs();
    m_trajectoryType = data.GetTrajectoryType();
    if (!data.HasMesh()) { m_trajectoryHeight = 0; m_trajectoryDurationMs = 0; }
    animation = data.GetSpriteRef().animation;
    // User-approved compatibility fix (2026-09-19): iOS displays a continuous
    // Haven beam, but our BULLET104 path tiles the muzzle flare. Use the same
    // BIG sprite's body/source/end slots as Kraken, without changing the template.
    // The remaining original-runtime discrepancy is documented in
    // docs/haven-boss-beam-investigation.md; this is not an original Bind rule.
    const auto &sprite = data.GetSpriteRef();
    if (source.resource.packHash == kHavenBeamPackHash && source.resource.localIndex == kHavenBeamBulletIndex &&
        sprite.packHash == kHavenBeamPackHash && sprite.archetype == 139 && sprite.action == 0 &&
        sprite.animation == 1 && (data.GetFlags() & 0x100) != 0) {
        animation = 0;
    }
    beamSourceAnimation = static_cast<std::uint8_t>(animation + 1);
    beamEndAnimation = static_cast<std::uint8_t>(animation + 2);
    m_boundAnimation = -1;
    m_beamSourceFrame = 0;
    m_beamEndFrame = 0;
    flags = data.GetFlags();
    acceleration = data.GetAcceleration();
    m_damage = data.GetBaseDamage();
    // UpdateBeam is attached to the gun; only direct projectiles time out.
    // Correction: CBullet::Update :63502 has script timers, not a host-wide 3-second fuse.
    // Direct bullets and beams both retain their authored removal behavior.
    m_interpreter.SetScript(data.GetScript(), *this);
    if (data.GetScript().IsPresent()) { m_interpreter.CallExportFunction(0, alternate); }
}

void CBullet::SetScriptSequenceFrame(std::uint8_t frame) {
    animation = frame;
    m_boundAnimation = -1;
    animationAgeMs = 0;
    animationFinished = false;
}

void CBullet::Update(int deltaMs, int animationDurationMs) {
    if (removed) { return; }
    // Variable 1 is the authored damage period. The elapsed time is a separate
    // runtime field; overwriting the period makes beams frame-rate dependent.
    m_damageDeltaMs = deltaMs;
    const int previousAge = ageMs;
    ageMs += deltaMs;
    if (m_trajectoryDurationMs > 0 && m_trajectoryHeight > 0) {
        // TRAJECTORY_TYPE_STAGES (:18589); type 0's authored first boundary
        // is 100 * duration, not one duration. Event 3 drives rolling scripts.
        float firstStage = 100;
        if (m_trajectoryType == 1) { firstStage = 0.5f; }
        if (m_trajectoryType == 2) { firstStage = 0.4f; }
        const int boundary = static_cast<int>(m_trajectoryDurationMs * firstStage);
        if (previousAge < boundary && ageMs >= boundary) {
            ++m_trajectoryEvents;
            m_interpreter.HandleEvent(8, 3);
        }
        // Original type 2 starts rolling after its third bounce (Draw :63232).
        // Move this simulation change out of Draw so headless and render agree.
        if (m_trajectoryType == 2 && ageMs > m_trajectoryDurationMs * 0.7f && ageMs <= static_cast<int>(m_trajectoryDurationMs)) {
            acceleration = -150;
        }
    }
    // CBullet::Update advances CSpritePlayer both before and after seeking.
    animationAgeMs += deltaMs * 2;
    // Script progress must not depend on whether this frame gets rendered.
    animationFinished = animationAgeMs >= animationDurationMs;
    if (m_boundAnimation == animation) {
        m_spritePlayer.Update(static_cast<std::uint16_t>(deltaMs));
        m_spritePlayer.Update(static_cast<std::uint16_t>(deltaMs));
        // CSpritePlayer::Update :58844 clears the completion bit each call.
        animationFinished = m_spritePlayer.HasFinished();
    }
    if (m_timer > 0) {
        m_timer -= deltaMs;
        if (m_timer <= 0) { m_interpreter.CallFunctionDirect(m_timerFunction); }
    }
    m_interpreter.Refresh();
}

CBullet::~CBullet() { OnRemove(); }

void CBullet::OnRemove() {
    // CBullet::OnRemove :62312. Clear first so forced removal and destruction
    // cannot notify twice, even when the gun export makes nested script calls.
    CGun *source = m_sourceGun;
    m_sourceGun = nullptr;
    if (source != nullptr) { source->OnBulletRemoved(*this); }
}

void CBullet::ForceRemoval() {
    if (removed) { return; }
    removed = true;
    m_interpreter.HandleEvent(8, 2);
    OnRemove();
}

void CBullet::Hit() {
    // Class 8 event 0 is an enemy hit. Map contact / forced expiry is 2.
    m_interpreter.HandleEvent(8, 2);
    if ((flags & 0x100) == 0) { removed = true; }
}

float CBullet::GetDamage() const {
    if (m_damagePeriodMs >= 1) {
        return m_damage * m_damageDeltaMs / m_damagePeriodMs;
    }
    return m_damage;
}

void CBullet::OnWallCollision() {
    // UpdateLevelCollision preserves reflective or beam bullets before event 2.
    if ((flags & 0x900) == 0) { removed = true; }
    m_interpreter.HandleEvent(8, 2);
}

void CBullet::OnCollision(Collision::HitResult result) {
    if (result == Collision::HitResult::Pending || removed) { return; }
    int event = 0;
    if (result == Collision::HitResult::Killed) { event = 1; }
    if (result == Collision::HitResult::Ignored) { event = 2; }
    m_interpreter.HandleEvent(8, static_cast<std::uint8_t>(event));
    // Enemy reflection is flag 0x1000; native 9 counts terrain ricochets.
    // Correction: native 9 is GetZOrderGroup's field +448 (:64007), not a count.
    // Ignored contacts remove ordinary bullets even when they can penetrate.
    if (result == Collision::HitResult::Ignored) {
        if ((flags & 0x100) == 0) { removed = true; }
    } else if ((flags & 0x1140) == 0) { removed = true; }
}

std::int16_t *CBullet::VariableResolver(std::uint8_t variable) {
    if (variable == 0) { return &m_masteryLevel; }
    if (variable == 1) { return &m_damagePeriodMs; }
    return nullptr;
}

std::vector<ZGunCue> CBullet::TakeCues() {
    std::vector<ZGunCue> result;
    result.swap(m_cues);
    return result;
}

std::int16_t CBullet::FunctionResolver(std::uint8_t function,
    const std::int16_t *arguments, std::uint8_t argumentCount) {
    ZGunCue cue;
    switch (function) {
    case 1:
    case 2:
    case 6:
    case 17: {
        cue.kind = ZGunCue::Kind::Effect;
        if (function == 2) { cue.kind = ZGunCue::Kind::Trail; }
        cue.alignEffect = argumentCount > 1 && arguments[1] != 0;
        if (function == 6) { cue.kind = ZGunCue::Kind::Sound; }
        std::uint32_t ordinal = 0;
        if (m_interpreter.GetResource(arguments[0], cue.resource.packHash, ordinal)) {
            cue.resource.localIndex = static_cast<std::uint8_t>(ordinal);
            m_cues.push_back(cue);
        }
        break;
    }
    case 4:
        m_timer = static_cast<int>(arguments[0] * (1000.0f / 256.0f));
        m_timerFunction = static_cast<std::uint8_t>(arguments[1]);
        break;
    case 3:
        cue.kind = ZGunCue::Kind::StopTrail;
        m_cues.push_back(cue);
        break;
    case 14:
        m_timer = arguments[0];
        m_timerFunction = static_cast<std::uint8_t>(arguments[1]);
        break;
    case 5:
        removed = true;
        break;
    case 11:
        velocityScale *= static_cast<float>(arguments[0]) / 256.0f;
        break;
    case 10:
        acceleration = static_cast<float>(arguments[0]);
        break;
    case 18:
        // UpdateBeam :62074 uses mem+372 as ray length, never as a timer.
        maximumBeamLength = arguments[0];
        break;
    case 19:
        collisionEnabled = true;
        break;
    case 20:
        collisionEnabled = false;
        break;
    case 15:
        // TODO: CLightningArc geometry; the original beam sprite is drawn now.
        // Restored: SetLightning :60480 scales native arguments before Init.
        lightning.displacement = arguments[0] * 0.005f;
        lightning.halfWidth = arguments[1] * 0.5f;
        lightning.length = arguments[2];
        lightning.pointCount = static_cast<std::uint16_t>(arguments[3]);
        lightning.frameCount = static_cast<std::uint16_t>(arguments[4]);
        ++lightning.revision;
        break;
    case 0: case 7: case 23:
        cue.kind = ZGunCue::Kind::Splash;
        cue.percentDamage = function == 23;
        cue.damage = arguments[0];
        cue.radius = arguments[1];
        if (function == 7) { cue.cone = arguments[2]; }
        else if (argumentCount > 2) {
            cue.force = arguments[2] / 256.0f;
            if (argumentCount > 3) { cue.forceMs = arguments[3]; }
        }
        m_cues.push_back(cue);
        break;
    case 22: {
        cue.kind = ZGunCue::Kind::SpawnEnemy;
        // :61351 accepts resource[, object ID[, force pool allocation]].
        if (argumentCount >= 2) { cue.spawnObjectId = arguments[1]; }
        if (argumentCount >= 3) { cue.forceSpawn = arguments[2] != 0; }
        std::uint32_t ordinal = 0;
        if (m_interpreter.GetResource(arguments[0], cue.resource.packHash, ordinal)) {
            cue.resource.localIndex = static_cast<std::uint8_t>(ordinal);
            m_cues.push_back(cue);
        }
        break;
    }
    case 8:
        seekingTurnRate = arguments[0];
        break;
    case 9:
        zOrderGroup = arguments[0];
        break;
    case 21:
        m_collisionHeightThreshold = arguments[0] / 256.0f;
        break;
    case 12:
        // SetRibbonTrail :60520 creates at most one trail per projectile.
        if (arguments[0] > 0) {
            ribbon.capacity = arguments[0];
            ribbon.width = arguments[1];
            ribbon.intervalMs = static_cast<std::uint16_t>(arguments[2]);
            cue.kind = ZGunCue::Kind::RibbonTrail;
            cue.ribbon = ribbon;
            m_cues.push_back(cue);
        }
        break;
    case 13:
        for (unsigned channel = 0; channel < ribbon.color.size(); ++channel) {
            ribbon.color[channel] = static_cast<std::uint16_t>(arguments[channel]);
        }
        cue.kind = ZGunCue::Kind::RibbonColor;
        cue.ribbon = ribbon;
        m_cues.push_back(cue);
        break;
    case 16:
        // Combat, homing and ribbon geometry are outside this visual host.
        // Ribbon natives now expose their original parameters to WeaponEffects.
        // Correction: iOS FunctionResolver :61101 has no case 16; it returns 0.
        // Retain the original white arc color rather than invent a color native.
        break;
    default:
        std::printf("[bullet] unsupported native %u\n", function);
        break;
    }
    return 0;
}

float CBullet::GetTrajectoryPhaseScale() const {
    if (m_trajectoryHeight <= 0 || m_trajectoryDurationMs == 0) { return 0; }
    const float time = ageMs / static_cast<float>(m_trajectoryDurationMs);
    if (time >= 1) { return 0; }
    if (m_trajectoryType == 1) {
        if (time > 0.8f) { return 0.25f; }
        if (time > 0.5f) { return 0.5f; }
    }
    if (m_trajectoryType == 2) {
        if (time > 0.7f) { return 0; }
        if (time > 0.6f) { return 0.25f; }
        if (time > 0.4f) { return 0.5f; }
    }
    return 1;
}

float CBullet::GetTrajectoryFraction() const {
    if (GetTrajectoryPhaseScale() == 0) { return 0; }
    float time = ageMs / static_cast<float>(m_trajectoryDurationMs);
    float duration = 1;
    if (m_trajectoryType == 1) {
        duration = 0.5f;
        if (time > 0.8f) { time -= 0.8f; duration = 0.2f; }
        else if (time > 0.5f) { time -= 0.5f; duration = 0.3f; }
    }
    if (m_trajectoryType == 2) {
        duration = 0.4f;
        if (time > 0.6f) { time -= 0.6f; duration = 0.1f; }
        else if (time > 0.4f) { time -= 0.4f; duration = 0.2f; }
    }
    return std::sin(time / duration * 3.14159265f);
}
