/**
 * @file CBrotherBody.cpp
 * @brief Assemble the player's body models from BIG templates, meshes and atlases.
 */

#include "gun_bros_re/gameplay/brother/CBrotherDrawing.h"


#include "gun_bros_re/data/ZMeshAssets.h"
#include <cstdio>

bool CBrother::BuildBody(ZPackTables &tables, const CMoveSetMesh &moves) {
    ClearScript();
    ClearArmor();
    uiActiveWeapon = nullptr;
    uiOtherWeapon.reset();
    m_cachedWeapons.clear();
    weapon.reset();
    m_drawing->parts.clear();
    moveSet = moves;

    if (moveSet.GetMeshConfigs().size() <= kPlayerLegsConfigIndex) {
        std::printf("[player] the move set has %zu configs, expected two\n",
                    moveSet.GetMeshConfigs().size());
        return false;
    }

    // Part 0 is the torso, and it is the parent: every attachment is read off
    // ITS mesh at ITS animation time.
    // Build each animated body mesh from its move-set config and original atlas.
    const char *names[] = {"torso", "legs"};
    for (unsigned index = 0; index <= kPlayerLegsConfigIndex; ++index) {
        auto part = std::make_unique<Drawing::BodyMesh>();
        const auto &config = moveSet.GetMeshConfigs()[index];
        if (!LoadMeshAndAtlas(tables, names[index], moveSet.GetPackHash(), config.meshOrdinal,
            moveSet.GetPackHash(), config.imageOrdinal, part->mesh, part->texture, &moveSet)) { return false; }
        m_drawing->parts.push_back(std::move(part));
    }
    m_bodyMeshes.assign(moveSet.GetMeshConfigs().size(), nullptr);
    for (std::size_t index = 0; index < m_drawing->parts.size(); ++index) {
        m_bodyMeshes[index] = &m_drawing->parts[index]->mesh;
    }
    CMoveSetMeshController *controllers[] = {&m_torso, &m_legs};
    for (unsigned config = 0; config < 2; ++config) {
        controllers[config]->SetMoveSet(&moveSet, m_bodyMeshes);
        for (std::size_t move = 0; move < moveSet.GetMoves().size(); ++move) {
            if (moveSet.GetMoves()[move].meshConfigIndex != config) { continue; }
            controllers[config]->SetMove(static_cast<std::int32_t>(move));
            break;
        }
    }
    return true;
}
