#include "engine/platform/ZWindow.h"
#include "engine/graphics/ZShaderProgram.h"
#include "engine/platform/ZGLLoader.h"
#include "engine/graphics/CMeshCamera.h"
#include "engine/core/ZPaths.h"
/** @file ZArmorCatalog.cpp
 * @brief Verify every armour script, model and texture from original archives.
 */
#include "TestOutput.h"
#include "gun_bros_re/gameplay/armor/CArmor.h"
#include "engine/graphics/ZPNG.h"
#include "engine/graphics/CMesh.h"
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "engine/core/ZMatrix4d.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include "Checks.h"

int RunArmorCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) {
        return 1;
    }
    CGunBros tables(toc);
    std::vector<CArmor::Entry> catalog;
    if (!CArmor::LoadEntries(toc, tables, catalog)) {
        return 1;
    }
    std::filesystem::create_directories(TestOutput::Path(""));
    std::ofstream report(TestOutput::Path("armor-check.csv"));
    if (!report) {
        return 1;
    }
    report << "index,owner,slot,mesh0,mesh1,node0,node1,attribute0,attribute1,speed,attribute3,xplodium,failures\n";
    unsigned failures = 0;
    unsigned meshCount = 0;
    for (std::size_t index = 0; index < catalog.size(); ++index) {
        const CArmor::Entry &entry = catalog[index];
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
                if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Mesh, ref.assetId, payload)) {
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
                ZPNGImage decoded;
                if (!tables.ReadSectionResource(image.packHash, ZGameSection::Png, image.assetId, payload) ||
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
    CGunBros tables(toc);
    std::vector<CArmor::Entry> catalog;
    std::vector<CGun::Entry> weapons;
    CBrother::Template data;
    if (!CArmor::LoadEntries(toc, tables, catalog) || !CGun::LoadEntries(toc, tables, weapons) ||
        !data.Load(toc, tables)) {
        return 1;
    }
    ZWindow window;
    if (!window.Open("Armor rendering regression", 640, 480)) {
        return 1;
    }
    ZShaderProgram program;
    if (!program.Load(Paths::Shaders(), "ogles_vs_mvp_tex0", "ogles_ps_tex0")) {
        return 1;
    }
    CBrother player;
    if (!player.BuildBody(tables, data.GetMoveSet())) {
        return 1;
    }
    unsigned checks = 0;
    unsigned failures = 0;
    // Distinct original torso configurations: pistols, beam, heavy weapon.
    const std::size_t weaponIndices[] = {0, 47, 75};
    for (std::size_t weapon : weaponIndices) {
        if (weapon >= weapons.size() || !player.EquipWeapon(tables, data.GetScript(), weapons[weapon].data, weapons[weapon].owner) || !player.CreateBuffers(program)) {
            return 1;
        }
        for (std::size_t index = 0; index < catalog.size(); ++index) {
            if (!window.PumpEvents()) {
                return 1;
            }
            player.ClearArmor();
            if (!player.EquipArmor(tables, catalog[index].data, program)) {
                ++failures;
                continue;
            }
            player.SetInput(true, false);
            player.Update(160);
            const std::uint32_t slot = catalog[index].data.GetSlot();
            const CArmor &armor = *player.armor[slot];
            for (std::uint32_t part = 0; part < kArmorVariantCount; ++part) {
                if (armor.GetTemplate().HasMesh(part)) {
                    ZMeshBoneTransform attachment;
                    if (!player.GetTorso().GetAnimation().GetNodeAt(
                        armor.GetTemplate().GetAttachmentNode(part), attachment)) {
                        ++failures;
                        std::printf("[armor-render] missing node: %s part=%u weapon=%zu\n",
                            catalog[index].owner.c_str(), part, weapon);
                    }
                }
            }
            float projection[16];
            float matrix[16];
            Matrix4dOrthoTopLeft(640, 480, 1000, projection);
            MeshCameraBuildGameMatrix(projection, 320, 300, player.GetWorldScale(data.GetGameScale(), 2), 0, matrix);
            glViewport(0, 0, 640, 480);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            player.Draw(program, matrix);
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
