/**
 * @file CBullet.cpp
 * @brief The projectile template: a sprite, a model, and a pile of scalars.
 */

#include "gun_bros/CBullet.h"

#include <cstdio>
#include <limits>

namespace {

// The same 16.16 fixed point every template scalar uses.
constexpr float kFixedPointScale = 1.0f / 65536.0f;

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
    animation = data.GetSpriteRef().animation;
    flags = data.GetFlags();
    acceleration = data.GetAcceleration();
    // UpdateBeam is attached to the gun; only direct projectiles time out.
    if ((data.GetFlags() & 0x100) != 0) { lifetimeMs = std::numeric_limits<int>::max(); }
    m_interpreter.SetScript(data.GetScript(), *this);
    if (data.GetScript().IsPresent()) { m_interpreter.CallExportFunction(0, alternate); }
}

void CBullet::SetScriptSequenceFrame(std::uint8_t frame) {
    animation = frame;
    animationAgeMs = 0;
    animationFinished = false;
}

void CBullet::Update(int deltaMs, int animationDurationMs) {
    ageMs += deltaMs;
    // CBullet::Update advances CSpritePlayer both before and after seeking.
    animationAgeMs += deltaMs * 2;
    // Script progress must not depend on whether this frame gets rendered.
    animationFinished = animationAgeMs >= animationDurationMs;
    if (m_timer > 0) {
        m_timer -= deltaMs;
        if (m_timer <= 0) { m_interpreter.CallFunctionDirect(m_timerFunction); }
    }
    m_interpreter.Refresh();
    if (ageMs >= lifetimeMs && !removed) { Hit(); }
}

void CBullet::Hit() {
    // Class 8 event 0 is an enemy hit. Map contact / forced expiry is 2.
    m_interpreter.HandleEvent(8, 2);
    if ((flags & 0x100) == 0) { removed = true; }
}

std::int16_t *CBullet::VariableResolver(std::uint8_t variable) {
    if (variable < 2) { return &m_variables[variable]; }
    return nullptr;
}

std::vector<GunCue> CBullet::TakeCues() {
    std::vector<GunCue> result;
    result.swap(m_cues);
    return result;
}

std::int16_t CBullet::FunctionResolver(std::uint8_t function,
    const std::int16_t *arguments, std::uint8_t argumentCount) {
    GunCue cue;
    switch (function) {
    case 1:
    case 2:
    case 6:
    case 17: {
        cue.kind = GunCue::Kind::Effect;
        if (function == 2) { cue.kind = GunCue::Kind::Trail; }
        cue.alignEffect = argumentCount > 1 && arguments[1] != 0;
        if (function == 6) { cue.kind = GunCue::Kind::Sound; }
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
        cue.kind = GunCue::Kind::StopTrail;
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
        lifetimeMs = ageMs + arguments[0];
        break;
    case 19:
        visible = true;
        break;
    case 20:
        visible = false;
        break;
    case 15:
        // TODO: CLightningArc geometry; the original beam sprite is drawn now.
        break;
    case 0: case 7: case 8: case 9: case 16:
    case 12: case 13: case 21: case 22: case 23:
        // Combat, homing and ribbon geometry are outside this visual host.
        break;
    default:
        std::printf("[bullet] unsupported native %u\n", function);
        break;
    }
    return 0;
}
