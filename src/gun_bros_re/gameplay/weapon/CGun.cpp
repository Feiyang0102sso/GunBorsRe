/**
 * @file CGun.cpp
 * @brief The weapon template: stat tables, a script, and a model set.
 */

#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "gun_bros_re/gameplay/weapon/CBullet.h"

#include <cstdio>
#include "gun_bros_re/gameplay/weapon/CGunDrawing.h"
#include <algorithm>

namespace {

// Same 16.16 fixed point the move set speeds use.
constexpr float kFixedPointScale = 1.0f / 65536.0f;
// FireBullet :128140-128145 uses IEEE-754 0x43D70000 (430 world units/s).
constexpr float kBulletLaunchSpeed = 430.0f;

/** One stat table: a uint16 count followed by that many uint32 values. */
void ReadStatTable(CArrayInputStream &stream, std::vector<std::uint32_t> &values) {
    const std::uint16_t count = stream.ReadUInt16();
    values.resize(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        values[i] = stream.ReadUInt32();
    }
}

}  // namespace

CGun::Template::Template()
    : m_flag104(0),
      m_value140(0),
      m_scalar144(0.0f),
      m_value148(0),
      m_scalar152(0.0f),
      m_flag256(0) {}

bool CGun::Template::Init(CArrayInputStream &stream) {
    m_flag104 = stream.ReadUInt8();
    m_meshRef.Init(stream);
    m_imageRef.Init(stream);
    m_objectRef132.Init(stream);
    m_value140 = stream.ReadUInt16();
    m_scalar144 = static_cast<float>(stream.ReadInt32()) * kFixedPointScale;
    m_value148 = stream.ReadUInt16();
    m_scalar152 = static_cast<float>(stream.ReadInt32()) * kFixedPointScale;
    m_flag256 = stream.ReadUInt8();

    m_script.Load(stream);

    for (std::uint32_t i = 0; i < kGunStatTableCount; ++i) {
        ReadStatTable(stream, m_statTables[i]);
        unsigned minimum = 0;
        if (i == 2 || i == 4) { minimum = 100; }
        // CGun::Template::Init :127798 / :127835 keeps three values and
        // clamps movement/damage percentages to 100. Retail rifles store zero.
        m_statTables[i].resize(3, minimum);
        for (unsigned &value : m_statTables[i]) { value = std::max(value, minimum); }
    }

    if (!m_moveSet.Init(stream)) {
        return false;
    }

    if (stream.Overran()) {
        std::printf("[gun] template truncated\n");
        return false;
    }

    return true;
}

unsigned CGun::Template::GetMasteryLevel(unsigned experience) const {
    for (unsigned level = 0; level < 3; ++level) {
        if (GetMasteryThreshold(level) > experience) { return level; }
    }
    return 3;
}

unsigned CGun::Template::GetMasteryThreshold(unsigned index) const {
    if (index >= m_statTables[0].size()) { return 0; }
    return m_statTables[0][index];
}

unsigned CGun::Template::GetMasteryLimit() const { return GetMasteryThreshold(2); }

unsigned CGun::Template::GetMasteryModifier(unsigned table, unsigned level, unsigned base) const {
    if (level == 0 || table >= kGunStatTableCount || level > m_statTables[table].size()) { return base; }
    return m_statTables[table][level - 1];
}

void CGun::SetMasteryExperience(unsigned experience) {
    m_mastery = static_cast<std::int16_t>(m_template->GetMasteryLevel(experience));
}

unsigned CGun::GetFireRateMs() const {
    return m_template->GetMasteryModifier(5, m_mastery, m_template->GetFireIntervalMs());
}

float CGun::GetMasteryDamageMultiplier(float randomUnit, bool *critical) const {
    // CGun::GetMasteryDamageMultiplier :128469, normal damage table at +200.
    float multiplier = m_template->GetMasteryModifier(4, m_mastery, 100) * 0.01f;
    const unsigned range = m_template->GetMasteryModifier(1, m_mastery, 0);
    // Utility::Random(0, range) is inclusive; zero is the critical outcome.
    const bool rolledCritical = range != 0 && randomUnit < 1.0f / (range + 1.0f);
    if (critical != nullptr) { *critical = rolledCritical; }
    if (rolledCritical) { multiplier *= m_template->GetCriticalDamageScale(); }
    return multiplier;
}

unsigned CGun::GetMasterySpeedMod() const {
    // CPlayer::UpdateMovement :101437 multiplies walking speed, not bullets.
    return m_template->GetMasteryModifier(2, m_mastery, 100);
}

CGun::CGun() : m_template(nullptr), m_ammo(1), m_mastery(0),
    m_functionTimer(0), m_timerFunction(0), m_eventTimer(0), m_fireMode(0),
    m_shooting(false), m_beam(false), m_heatIntensity(0.0f), m_targetHeat(0.0f) {}

CGun::~CGun() { DetachBullets(); }

void CGun::DetachBullets() {
    // Host equipment replacement can destroy a gun before its world bullets.
    // Detach rather than calling into an expired or newly rebound interpreter.
    for (CBullet *bullet : m_bullets) { bullet->m_sourceGun = nullptr; }
    m_bullets.clear();
}

void CGun::AddBullet(CBullet &bullet) {
    bullet.OnRemove();
    bullet.m_sourceGun = this;
    m_bullets.push_back(&bullet);
}

void CGun::OnBulletRemoved(CBullet &bullet) {
    const auto entry = std::find(m_bullets.begin(), m_bullets.end(), &bullet);
    if (entry == m_bullets.end()) { return; }
    m_bullets.erase(entry);
    // CGun::OnBulletRemoved :128523 calls export 2, not a guessed ammo increment.
    m_interpreter.CallExportFunction(2);
}

void CGun::Bind(const Template &data, const CMesh *mesh, bool beam) {
    DetachBullets();
    m_beam = beam;
    m_seekAngle = data.GetSeekAngle();
    m_templateData = data;
    m_template = &m_templateData;
    m_overrides.assign(11, -1);
    m_cues.clear();
    m_ammo = 1;
    m_mastery = 0;
    m_functionTimer = 0;
    m_eventTimer = 0;
    m_fireMode = 0;
    m_shooting = false;
    m_heatIntensity = 0.0f;
    m_targetHeat = 0.0f;
    m_animation.SetMesh(mesh);
    m_interpreter.SetScript(m_templateData.GetScript(), *this);
}

void CGun::OnEquip() {
    if (!m_template->GetScript().GetExportFunctions().empty()) { m_interpreter.CallExportFunction(0); }
}

void CGun::SetShooting(bool shooting) {
    if (m_shooting == shooting) {
        return;
    }
    m_shooting = shooting;
    // CGun::OnShootStart / OnShootStop (:128541): class 7 events 0 / 1.
    if (shooting) {
        m_interpreter.HandleEvent(7, 0);
    } else {
        m_interpreter.HandleEvent(7, 1);
    }
}

void CGun::Fire() {
    if (m_template->GetScript().GetExportFunctions().size() > 1) { m_interpreter.CallExportFunction(1); }
}

void CGun::OnScriptStateEntered() { m_eventTimer = 0; }

void CGun::Update(std::int32_t deltaMs) {
    if (deltaMs <= 0) {
        return;
    }
    // Keep the order in CGun::Update: delayed call, recoil, timer event, mesh.
    // Offset 224 was traced through CBrother::Draw: this channel is red heat
    // intensity, not a transform (MeshPart tint starts at float index 13).
    if (m_functionTimer > 0) {
        m_functionTimer -= deltaMs;
        if (m_functionTimer <= 0) {
            m_functionTimer = 0;
            m_interpreter.CallFunctionDirect(m_timerFunction);
        }
    }
    const float step = static_cast<float>(deltaMs) * 0.0025f;
    if (m_heatIntensity < m_targetHeat) {
        m_heatIntensity = std::min(m_heatIntensity + step, m_targetHeat);
    } else {
        m_heatIntensity = std::max(m_heatIntensity - step, m_targetHeat);
    }
    if (m_eventTimer > 0) {
        m_eventTimer -= deltaMs;
        if (m_eventTimer <= 0) {
            m_eventTimer = 0;
            m_interpreter.HandleEvent(7, 2);
        }
    }
    m_animation.Update(deltaMs);
}

std::int16_t *CGun::VariableResolver(std::uint8_t variable) {
    if (variable == 0) { return &m_ammo; }
    if (variable == 1) { return &m_mastery; }
    return nullptr;
}

std::vector<ZGunCue> CGun::TakeCues() {
    std::vector<ZGunCue> result;
    result.swap(m_cues);
    return result;
}

std::int16_t CGun::FunctionResolver(std::uint8_t function,
    const std::int16_t *arguments, std::uint8_t argumentCount) {
    ZGunCue cue;
    // Native ordinals come from CGun::FunctionResolver (:128186).
    switch (function) {
    case 0:
    case 1:
    case 2:
        if (argumentCount < 5) { return 0; }
        cue.kind = ZGunCue::Kind::Bullet;
        cue.resource = m_template->GetBulletRef();
        if (arguments[0] != 0) {
            std::uint32_t ordinal = 0;
            m_interpreter.GetResource(arguments[0], cue.resource.packHash, ordinal);
            cue.resource.localIndex = static_cast<std::uint8_t>(ordinal);
        }
        cue.hand = m_template->GetHandedness();
        if (function == 1) { cue.hand = 0; }
        if (function == 2) { cue.hand = 1; }
        cue.node = arguments[1];
        cue.minimumAngle = static_cast<float>(arguments[2]);
        cue.maximumAngle = static_cast<float>(arguments[3]);
        cue.speed = kBulletLaunchSpeed * static_cast<float>(arguments[4]) / 256.0f;
        if (argumentCount > 5) { cue.alternate = arguments[5] != 0; }
        m_cues.push_back(cue);
        return 0;
    case 4:
        m_functionTimer = static_cast<int>(arguments[0] * (1000.0f / 256.0f));
        m_timerFunction = static_cast<std::uint8_t>(arguments[1]);
        return 0;
    case 5:
        m_eventTimer = static_cast<int>(arguments[0] * (1000.0f / 256.0f));
        return 0;
    case 6:
        if (arguments[0] >= 0 && arguments[0] < 11) {
            m_overrides[arguments[0]] = arguments[1];
        }
        return 0;
    case 7:
        m_fireMode = arguments[0];
        return 0;
    case 8:
        m_targetHeat = static_cast<float>(std::clamp<int>(arguments[0], 0, 5)) / 5.0f;
        return 0;
    case 9:
    case 10: {
        cue.kind = ZGunCue::Kind::Sound;
        if (function == 10) { cue.kind = ZGunCue::Kind::LoopSound; }
        std::uint32_t ordinal = 0;
        if (m_interpreter.GetResource(arguments[0], cue.resource.packHash, ordinal)) {
            cue.resource.localIndex = static_cast<std::uint8_t>(ordinal);
            m_cues.push_back(cue);
        }
        return 0;
    }
    case 11:
        cue.kind = ZGunCue::Kind::StopSound;
        m_cues.push_back(cue);
        return 0;
    case 12: {
        cue.kind = ZGunCue::Kind::Effect;
        cue.alignEffect = true;
        cue.hand = arguments[0];
        cue.node = arguments[1];
        std::uint32_t ordinal = 0;
        if (m_interpreter.GetResource(arguments[2], cue.resource.packHash, ordinal)) {
            cue.resource.localIndex = static_cast<std::uint8_t>(ordinal);
            m_cues.push_back(cue);
        }
        return 0;
    }
    case 13:
        // FindOldestBullet(CGun*) :114474 and ForceRemoval :61876 are synchronous.
        // The removal export must update script locals before Fire continues.
        for (CBullet *bullet : m_bullets) {
            if (bullet->removed) { continue; }
            bullet->ForceRemoval();
            return 1;
        }
        return 0;
    case 14:
        // Seek distance is gameplay targeting; it does not change the model.
        // Correction: native 14 :128352 supplies a cone angle, not a distance.
        m_seekAngle = arguments[0];
        return 0;
    case 15:
        m_eventTimer = arguments[0];
        return 0;
    default:
        std::printf("[gun] unsupported native %u\n", function);
        return 0;
    }
}
