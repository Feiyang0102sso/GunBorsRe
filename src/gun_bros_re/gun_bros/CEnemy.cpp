/**
 * @file CEnemy.cpp
 * @brief An enemy's part table, and the script functions that build it.
 */

#include "gun_bros/CEnemy.h"
#include "gun_bros/CLevel.h"

#include <cmath>
#include <cstdio>

namespace {

// The export SpawnForUI runs (:72858). Named here because the number on its
// own says nothing.
constexpr std::uint8_t kExportSpawnForUI = 3;

// The export both CEnemy::Spawn overloads run (:73239, :73284) -- the one a
// level uses.
constexpr std::uint8_t kExportSpawn = 0;

}  // namespace

EnemyPart::EnemyPart()
    : boneIndex(kEnemyNoBoneIndex),
      extraAngleDegrees(0.0f),
      extraAngleDegreesPerSecond(0.0f),
      extraAxisX(0.0f),
      // Bind seeds the axis pointing along y, so a script that sets an angle
      // without setting an axis still turns about something.
      extraAxisY(1.0f),
      extraAxisZ(0.0f),
      radius(0.0f) {}

CEnemy::CEnemy()
    : m_moveSet(nullptr),
      m_partCount(1),
      m_bodyMoveLocked(false),
      m_variableScratch(0) {
    for (std::size_t i = 0; i < 256; ++i) {
        m_reportedFunction[i] = false;
        m_reportedVariable[i] = false;
    }
}

void CEnemy::Bind(const CScript &script, const CMoveSetMesh &moveSet,
                    const std::vector<const CMesh *> &configMeshes) {
    stun.ClearStunned();
    m_moveSet = &moveSet;
    m_configMeshes = configMeshes;
    // Bind :73421 seeds the health-bar flag; Spawn must preserve script state.
    combat.variables[15] = 1;

    // One part until the script says otherwise. This is the line that makes
    // the script, not the template, the authority on an enemy's shape.
    m_partCount = 1;

    for (std::size_t i = 0; i < kEnemyPartSlots; ++i) {
        m_parts[i] = EnemyPart();
        m_parts[i].controller.SetMoveSet(&moveSet, configMeshes);

        // Part 0 does not loop, every other part does. Copied from Bind, where
        // the flag is written as `!(offset == 0)`.
        m_parts[i].controller.GetAnimation().SetLooped(i != 0);
    }

    m_interpreter.SetScript(script, *this);
}

bool CEnemy::SpawnForUI() {
    if (!m_interpreter.HasScript()) {
        std::printf("[enemy] no script; the model stays as bound\n");
        return false;
    }

    // Export 3 and nothing else. CMenuMeshEnemy::Bind (:169128) -- the whole
    // of how the game puts an enemy in front of the player outside a level --
    // is CEnemy::Bind followed by CEnemy::SpawnForUI, with no state entered
    // first. Entering state 0 anyway would run enter code the menu never runs.
    return m_interpreter.CallExportFunction(kExportSpawnForUI);
}

bool CEnemy::Spawn() {
    if (!m_interpreter.HasScript()) {
        std::printf("[enemy] no script; the model stays as bound\n");
        return false;
    }
    // Spawn seeds ten health before the script supplies the real value.
    // Reference: :73239. Keep the template's collision radius and ownership.
    combat.health = 10;
    combat.dead = false;
    combat.removed = false;
    combat.variables[6] = 100;
    const bool ran = m_interpreter.CallExportFunction(kExportSpawn);
    combat.maxHealth = combat.health;
    return ran;
}

bool CEnemy::SetState(std::uint8_t stateId) {
    m_bodyMoveLocked = false;
    return m_interpreter.SetState(stateId);
}

void CEnemy::Update(std::int32_t deltaMs) {
    if (combat.enabled && !combat.dead && GetLevelContext() != nullptr && deltaMs > 0) {
        // Original Update :67783 scales the whole actor clock, rounded to ms.
        deltaMs = static_cast<int>(std::round(deltaMs * GetLevelContext()->GetEnemyMultiplier(combat.templateRef, 4)));
        if (deltaMs < 1) { deltaMs = 1; }
    }
    if (combat.enabled && stun.IsActive()) {
        // UpdateStun (:68399) keeps script timers but freezes movement and all
        // part animation clocks. Original CEnemy stun callbacks are bx lr stubs
        // at 0x397a0 / 0x397a8, so expiry emits no invented script event.
        combat.previousX = combat.x;
        combat.previousY = combat.y;
        combat.hitFlash = std::fmax(0.0f, combat.hitFlash - deltaMs * 0.004f);
        UpdateCombatTimers(deltaMs);
        stun.Update(deltaMs);
        m_interpreter.Refresh();
        return;
    }
    if (combat.enabled) {
        UpdateCombatBeforeAnimation(deltaMs);
    }
    const float deltaSeconds = static_cast<float>(deltaMs) * 0.001f;

    for (std::uint32_t i = 0; i < m_partCount && i < kEnemyPartSlots; ++i) {
        m_parts[i].hitFlash = std::fmax(0.0f, m_parts[i].hitFlash - deltaSeconds * 4);
        m_parts[i].controller.Update(deltaMs);

        // The spin. Every part with a non-zero speed turns forever; the
        // original guards on the speed exactly like this rather than on a
        // separate flag.
        if (m_parts[i].extraAngleDegreesPerSecond != 0.0f) {
            m_parts[i].extraAngleDegrees +=
                m_parts[i].extraAngleDegreesPerSecond * deltaSeconds;
        }
    }

    // After the clocks, not before: Refresh asks whether the current move has
    // finished, and it is this update that decides.
    m_interpreter.Refresh();
    if (combat.enabled) {
        UpdateCombatAfterAnimation(deltaMs);
    }
}

void CEnemy::SetScriptSequenceFrame(std::uint8_t frame) {
    if (m_bodyMoveLocked) {
        return;
    }
    std::size_t part = kEnemyScriptedPart;
    if (combat.enabled && combat.variables[14] >= 0 && combat.variables[14] < kEnemyPartSlots) {
        part = static_cast<std::size_t>(combat.variables[14]);
    }
    m_parts[part].controller.SetMove(frame);
}

bool CEnemy::IsScriptSequenceFrameFinished() {
    std::size_t part = kEnemyScriptedPart;
    if (combat.enabled && combat.variables[14] >= 0 && combat.variables[14] < kEnemyPartSlots) {
        part = static_cast<std::size_t>(combat.variables[14]);
    }
    return m_parts[part].controller.GetAnimation().IsFinished();
}

std::int16_t CEnemy::FunctionResolver(std::uint8_t function,
                                      const std::int16_t *arguments,
                                      std::uint8_t argumentCount) {
    if (function == kEnemyScriptSetPartCount && argumentCount >= 1) {
        std::uint32_t count = static_cast<std::uint32_t>(arguments[0]);
        if (count > kEnemyPartSlots) {
            std::printf("[enemy] script asked for %u parts, only %zu slots\n",
                        count, kEnemyPartSlots);
            count = kEnemyPartSlots;
        }
        m_partCount = count;
        return 0;
    }

    if (function == kEnemyScriptSetPart && argumentCount >= 2) {
        const std::int16_t partIndex = arguments[0];
        if (partIndex < 0 || static_cast<std::size_t>(partIndex) >= kEnemyPartSlots) {
            std::printf("[enemy] script set part %d, out of range\n", partIndex);
            return 0;
        }

        EnemyPart &part = m_parts[partIndex];
        part.controller.SetMove(arguments[1]);

        // Three arguments attach the part to a bone; two leave it free. The
        // original writes -1 in the two-argument case rather than leaving the
        // previous bone in place, so a part can be detached by setting it
        // again with one argument fewer.
        if (argumentCount >= 3) {
            part.boneIndex = arguments[2];
        } else {
            part.boneIndex = kEnemyNoBoneIndex;
        }

        // The original also clears the part's tint here (offset 164, the hit
        // flash). Nothing draws a tint yet, so there is nothing to clear.
        // Arena now consumes this tint; preserve the original reset as well.
        part.hitFlash = 0;
        return 0;
    }

    if (function == kEnemyScriptSetPartRadius && argumentCount >= 2) {
        const std::int16_t partIndex = arguments[0];
        if (partIndex >= 0 && static_cast<std::size_t>(partIndex) < kEnemyPartSlots) {
            m_parts[partIndex].radius = static_cast<float>(arguments[1]);
        }
        return 0;
    }

    if (function == kEnemyScriptSetPartDirection && argumentCount >= 5) {
        const std::int16_t partIndex = arguments[0];
        if (partIndex < 0 || static_cast<std::size_t>(partIndex) >= kEnemyPartSlots) {
            return 0;
        }

        // Argument 1 is the SPEED, in degrees per second: the original writes
        // it to offset 136, and CEnemy::Update (:67820) adds `speed * seconds`
        // to the angle at offset 140 every frame. It is not an angle, which is
        // why setting it once makes a blade turn forever.
        EnemyPart &part = m_parts[partIndex];
        part.extraAngleDegreesPerSecond = static_cast<float>(arguments[1]);

        // The axis is normalised on the way in, and a zero-length one leaves
        // the axis Bind seeded.
        const float x = static_cast<float>(arguments[2]);
        const float y = static_cast<float>(arguments[3]);
        const float z = static_cast<float>(arguments[4]);
        const float length = std::sqrt(x * x + y * y + z * z);
        if (length > 0.0f) {
            part.extraAxisX = x / length;
            part.extraAxisY = y / length;
            part.extraAxisZ = z / length;
        }
        return 0;
    }

    std::int16_t result = 0;
    if (ResolveCombatFunction(function, arguments, argumentCount, result)) {
        return result;
    }

    // Once per id, not once per call: a running script calls the same few
    // handlers many times a second, and a line each buries everything else.
    // One line per id is the list of what to build next, which is why these
    // are logged at all.
    if (!m_reportedFunction[function]) {
        m_reportedFunction[function] = true;
        std::printf("[enemy] function %u, %u args:", function, argumentCount);
        for (std::uint8_t i = 0; i < argumentCount; ++i) {
            std::printf(" %d", arguments[i]);
        }
        std::printf(" -- not implemented\n");
    }
    return 0;
}

std::int16_t *CEnemy::VariableResolver(std::uint8_t variable) {
    if (variable < combat.variables.size()) {
        if (variable == 8) {
            combat.variables[8] = static_cast<std::int16_t>(combat.facing);
        }
        if (variable == 20 && GetLevelContext() != nullptr) {
            combat.variables[20] = static_cast<std::int16_t>(GetLevelContext()->GetRealWave());
        }
        return &combat.variables[variable];
    }
    if (!m_reportedVariable[variable]) {
        m_reportedVariable[variable] = true;
        std::printf("[enemy] variable %u -- not implemented, reading scratch\n",
                    variable);
    }
    m_variableScratch = 0;
    return &m_variableScratch;
}
