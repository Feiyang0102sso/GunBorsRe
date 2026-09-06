/**
 * @file CEnemy.cpp
 * @brief An enemy's part table, and the script functions that build it.
 */

#include "gun_bros/CEnemy.h"

#include <cmath>
#include <cstdio>

namespace {

// The export SpawnForUI runs (:72858). Named here because the number on its
// own says nothing.
constexpr std::uint8_t kExportSpawnForUI = 3;

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
      m_variableScratch(0) {}

void CEnemy::Bind(const CScript &script, const CMoveSetMesh &moveSet,
                  const std::vector<const CMesh *> &configMeshes) {
    m_moveSet = &moveSet;

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

void CEnemy::Update(std::int32_t deltaMs) {
    const float deltaSeconds = static_cast<float>(deltaMs) * 0.001f;

    for (std::uint32_t i = 0; i < m_partCount && i < kEnemyPartSlots; ++i) {
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
}

void CEnemy::SetScriptSequenceFrame(std::uint8_t frame) {
    if (m_bodyMoveLocked) {
        return;
    }
    m_parts[kEnemyScriptedPart].controller.SetMove(frame);
}

bool CEnemy::IsScriptSequenceFrameFinished() {
    return m_parts[kEnemyScriptedPart].controller.GetAnimation().IsFinished();
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

    std::printf("[enemy] function %u, %u args:", function, argumentCount);
    for (std::uint8_t i = 0; i < argumentCount; ++i) {
        std::printf(" %d", arguments[i]);
    }
    std::printf(" -- not implemented\n");
    return 0;
}

std::int16_t *CEnemy::VariableResolver(std::uint8_t variable) {
    std::printf("[enemy] variable %u -- not implemented, reading scratch\n",
                variable);
    m_variableScratch = 0;
    return &m_variableScratch;
}
