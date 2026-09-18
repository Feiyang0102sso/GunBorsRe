/**
 * @file CBrotherEquipment.cpp
 * @brief Assemble weapon and armor banks; bind full actors or select another gun.
 */

#include "gun_bros_re/gameplay/brother/CBrotherDrawing.h"

#include <cstdio>

bool CBrother::EquipArmor(ZPackTables &tables, const CArmor::Template &data, const ZShaderProgram &program) {
    if (data.GetSlot() >= kArmorSlotCount) { return false; }
    auto replacement = std::make_unique<CArmor>();
    if (!replacement->Load(tables, data, program)) { return false; }
    armor[data.GetSlot()] = std::move(replacement);
    std::printf("[armor] equipped slot %u defense=%.0f%% damage=%.0f%% speed=%.0f%%\n",
        data.GetSlot(), (GetArmorMultiplier(0) - 1) * 100,
        (GetArmorMultiplier(1) - 1) * 100, (GetArmorMultiplier(2) - 1) * 100);
    return true;
}

void CBrother::ClearArmor() {
    for (auto &slot : armor) {
        slot.reset();
    }
}

float CBrother::GetArmorMultiplier(std::uint32_t attribute) const {
    float result = 1.0f;
    for (const auto &slot : armor) {
        if (slot) {
            result += slot->GetAttribute(attribute) / 100.0f;
        }
    }
    return result;
}

bool CBrother::PrepareSecondaryWeapon(ZPackTables &tables, const CGun::Template &data, const std::string &owner) {
    auto replacement = std::make_unique<CGun>();
    if (!replacement->Load(tables, data, owner)) { return false; }
    uiOtherWeapon = std::move(replacement);
    return true;
}

void CBrother::SelectWeapon(bool primary) {
    CGun *active = uiOtherWeapon.get();
    if (primary) { active = weapon.get(); }
    const auto meshes = active->GetBodyMeshes();
    SetUIGun(*active, meshes);
    uiActiveWeapon = active;
}

bool CBrother::EquipWeapon(ZPackTables &tables, const CScript &playerScript, const CGun::Template &data, const std::string &owner) {
    auto replacement = std::make_unique<CGun>();
    if (!replacement->Load(tables, data, owner)) { return false; }
    const auto meshes = replacement->GetBodyMeshes();
    std::vector<const CMesh *> bodyMeshes;
    for (auto &part : m_drawing->parts) { bodyMeshes.push_back(&part->mesh); }
    replacement->SetDeathmatch(IsDeathmatch());
    replacement->SetMasteryExperience(masteryExperience);
    Bind(playerScript, moveSet, bodyMeshes, *replacement, meshes);
    std::printf("[player] equipped %s: weaponTorso=%d move=%d legs=%d hand=%u state=%d\n",
        owner.c_str(), TorsoUsesWeapon(), GetTorso().GetMoveIndex(),
        GetLegs().GetMoveIndex(), data.GetHandedness(), GetStateId());
    weapon = std::move(replacement);
    uiActiveWeapon = nullptr;
    uiOtherWeapon.reset();
    return true;
}

bool CBrother::SelectCachedWeapon(ZPackTables &tables, const CGun::Template &data,
    const std::string &owner, std::uint64_t key, const ZShaderProgram &program) {
    auto found = m_cachedWeapons.find(key);
    if (found == m_cachedWeapons.end()) {
        auto replacement = std::make_unique<CGun>();
        if (!replacement->Load(tables, data, owner) || !replacement->CreateBuffers(program)) { return false; }
        found = m_cachedWeapons.emplace(key, std::move(replacement)).first;
    }
    CGun &active = *found->second;
    const auto meshes = active.GetBodyMeshes();
    active.SetDeathmatch(IsDeathmatch());
    active.SetLevelContext(GetLevelContext());
    active.SetMasteryExperience(masteryExperience);
    SetUIGun(active, meshes);
    uiActiveWeapon = &active;
    return true;
}
