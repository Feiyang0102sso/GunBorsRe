/** @file ArmorCatalog.cpp
 * @brief Verify every armour script, model and texture from original archives.
 */
#include "runtime/ArmorCatalog.h"
#include "engine/CPNG.h"
#include "gun_bros/CMesh.h"
#include "runtime/PlayerModel.h"
#include "runtime/WeaponCatalog.h"
#include "engine/CMatrix4d.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

bool LoadArmorCatalog(CResTOCManager &toc, PackTables &tables,
    std::vector<ArmorEntry> &armor) {
    armor.clear();
    for (std::uint32_t packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC *pack = toc.GetPack(packIndex);
        const std::uint32_t count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::Armor);
        for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack->GetPackHash(), GameSection::Armor, ordinal, payload)) {
                return false;
            }
            CArrayInputStream stream(payload);
            ArmorEntry entry;
            entry.packHash = pack->GetPackHash();
            entry.ordinal = ordinal;
            entry.owner = pack->GetShortName() + " armor " + std::to_string(ordinal);
            if (!entry.data.Init(stream) || stream.Available() != 0 || entry.data.GetSlot() >= kArmorSlotCount) {
                std::printf("[armor] invalid template: %s\n", entry.owner.c_str());
                return false;
            }
            armor.push_back(entry);
        }
    }
    return !armor.empty();
}

int RunArmorCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) {
        return 1;
    }
    PackTables tables(toc);
    std::vector<ArmorEntry> catalog;
    if (!LoadArmorCatalog(toc, tables, catalog)) {
        return 1;
    }
    std::filesystem::create_directories("out");
    std::ofstream report("out/armor-check.csv");
    if (!report) {
        return 1;
    }
    report << "index,owner,slot,mesh0,mesh1,node0,node1,attribute0,attribute1,speed,attribute3,xplodium,failures\n";
    unsigned failures = 0;
    unsigned meshCount = 0;
    for (std::size_t index = 0; index < catalog.size(); ++index) {
        const ArmorEntry &entry = catalog[index];
        unsigned entryFailures = 0;
        CArmor equipped;
        equipped.Bind(entry.data);
        equipped.Equip();
        if (entry.data.GetScript().GetExportFunctions().empty()) {
            ++entryFailures;
        }
        // Two instances must never share writable script attributes.
        CArmor independent;
        independent.Bind(entry.data);
        independent.Equip();
        const std::int16_t original = independent.GetAttribute(0);
        *equipped.VariableResolver(0) = static_cast<std::int16_t>(original + 1);
        if (independent.GetAttribute(0) != original) {
            ++entryFailures;
        }
        equipped.Bind(entry.data);
        equipped.Equip();
        for (std::uint32_t part = 0; part < kArmorVariantCount; ++part) {
            std::vector<std::uint8_t> payload;
            if (entry.data.HasMesh(part)) {
                const CGameAssetRef &ref = entry.data.GetMeshRef(part);
                if (!tables.ReadSectionResource(ref.packHash, GameSection::Mesh, ref.assetId, payload)) {
                    ++entryFailures;
                } else {
                    CArrayInputStream stream(payload);
                    CMesh mesh;
                    if (!mesh.Init(stream) || stream.Available() != 0) {
                        ++entryFailures;
                    }
                    ++meshCount;
                }
            }
            const CGameAssetRef &image = entry.data.GetLoadedImageRef(part);
            if (image.assetId >= 0 && !image.IsNull()) {
                PNGImage decoded;
                if (!tables.ReadSectionResource(image.packHash, GameSection::Png, image.assetId, payload) ||
                    !PNGDecode(payload, decoded)) {
                    ++entryFailures;
                }
            }
        }
        report << index << ',' << entry.owner << ',' << unsigned(entry.data.GetSlot());
        for (std::uint32_t part = 0; part < kArmorVariantCount; ++part) {
            report << ',' << entry.data.GetMeshRef(part).assetId;
        }
        for (std::uint32_t part = 0; part < kArmorVariantCount; ++part) {
            report << ',' << unsigned(entry.data.GetAttachmentNode(part));
        }
        for (std::uint32_t attribute = 0; attribute < kArmorAttributeCount; ++attribute) {
            report << ',' << equipped.GetAttribute(attribute);
        }
        report << ',' << entryFailures << '\n';
        failures += entryFailures;
        std::printf("[armor-check] %zu %s slot=%u values=%d/%d/%d/%d/%d failures=%u\n",
            index, entry.owner.c_str(), entry.data.GetSlot(), equipped.GetAttribute(0),
            equipped.GetAttribute(1), equipped.GetAttribute(2), equipped.GetAttribute(3),
            equipped.GetAttribute(4), entryFailures);
    }
    std::printf("[armor-check] templates=%zu meshes=%u failures=%u\n", catalog.size(), meshCount, failures);
    if (failures != 0 || !report) {
        return 1;
    }
    return 0;
}

int RunArmorRenderCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) {
        return 1;
    }
    PackTables tables(toc);
    std::vector<ArmorEntry> catalog;
    std::vector<WeaponEntry> weapons;
    PlayerTemplateData data;
    if (!LoadArmorCatalog(toc, tables, catalog) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !FindPlayerTemplate(toc, tables, data)) {
        return 1;
    }
    CWindow window;
    if (!window.Open("Armor rendering regression", 640, 480)) {
        return 1;
    }
    CShaderProgram program;
    if (!program.Load(std::string(ASSET_ROOT) + "/src/gun_bros_re/shaders", "ogles_vs_mvp_tex0", "ogles_ps_tex0")) {
        return 1;
    }
    PlayerModel player;
    if (!BuildPlayerBody(tables, data.moveSet, player)) {
        return 1;
    }
    unsigned checks = 0;
    unsigned failures = 0;
    // Distinct original torso configurations: pistols, beam, heavy weapon.
    const std::size_t weaponIndices[] = {0, 47, 75};
    for (std::size_t weapon : weaponIndices) {
        if (weapon >= weapons.size() || !EquipPlayerWeapon(tables, data.script, weapons[weapon].data,
            weapons[weapon].owner, player) || !CreatePlayerBuffers(player, program)) {
            return 1;
        }
        for (std::size_t index = 0; index < catalog.size(); ++index) {
            if (!window.PumpEvents()) {
                return 1;
            }
            ClearPlayerArmor(player);
            if (!EquipPlayerArmor(tables, catalog[index].data, program, player)) {
                ++failures;
                continue;
            }
            SetPlayerInput(player, true, false);
            AdvancePlayer(player, 160);
            PosePlayer(player);
            const std::uint32_t slot = catalog[index].data.GetSlot();
            const PlayerArmorState &armor = *player.armor[slot];
            for (std::uint32_t part = 0; part < kArmorVariantCount; ++part) {
                if (armor.parts[part]) {
                    MeshBoneTransform attachment;
                    if (!player.weapon->brother.GetTorso().GetAnimation().GetNodeAt(
                        armor.parts[part]->boneIndex, attachment)) {
                        ++failures;
                        std::printf("[armor-render] missing node: %s part=%u weapon=%zu\n",
                            catalog[index].owner.c_str(), part, weapon);
                    }
                }
            }
            float projection[16];
            float matrix[16];
            Matrix4dOrthoTopLeft(640, 480, 1000, projection);
            BuildPlayerGameMatrix(projection, 320, 300, PlayerModelWorldScale(player, data.gameScale, 2), 0, matrix);
            glViewport(0, 0, 640, 480);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            DrawPlayer(player, program, matrix);
            if (glGetError() != GL_NO_ERROR) {
                ++failures;
            }
            window.Present();
            ++checks;
        }
    }
    std::printf("[armor-render] combinations=%u failures=%u\n", checks, failures);
    if (failures != 0) {
        return 1;
    }
    return 0;
}
