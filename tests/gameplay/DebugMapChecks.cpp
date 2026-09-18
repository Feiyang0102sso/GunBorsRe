/** @file DebugMapChecks.cpp
 * @brief Verify saved equipment at the real debug launch seam and inspect BIG map content.
 */
#include "gameplay/SurvivalCheckScenario.h"
#include "gameplay/DebugMapChecks.h"
#include "gameplay/SurvivalStudy.h"
#include "TestOutput.h"
#include "gun_bros_re/debug/DebugMaps.h"
#include "gun_bros_re/gameplay/ZSurvivalRuntime.h"
#include "gun_bros_re/gameplay/ZMapWorldInternal.h"

static int CheckWallWeaponResources(CResTOCManager &toc, ZPackTables &tables) {
    // Enumerate actual gun-to-bullet dependencies; do not infer attributes
    // from weapon category, damage amount, or the projectile's appearance.
    std::vector<ZWeaponEntry> weapons;
    if (!LoadWeaponCatalog(toc, tables, weapons)) { return 1; }
    for (const auto &weapon : weapons) {
        for (const auto &resource : weapon.data.GetScript().GetResources()) {
            if (resource.sectionOrType != 3) { continue; }
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(resource.packHash, ZGameSection::Bullet, resource.resourceId, bytes)) { return 1; }
            CArrayInputStream input(bytes);
            CBullet::Template bullet;
            if (!bullet.Init(input)) { return 1; }
            if ((bullet.GetFlags() & 1) == 0) { continue; }
            std::printf("[campaign-wall-weapon] gun=%s:%u name=%s bullet=%s:%u flags=%x\n",
                tables.GetPackName(weapon.packHash).c_str(), weapon.ordinal, weapon.name.c_str(),
                tables.GetPackName(resource.packHash).c_str(), resource.resourceId, bullet.GetFlags());
        }
    }
    return 0;
}

int RunDebugMapProfileCheck() {
    const std::string big = (Paths::Root() / Paths::BigDirectory).u8string();
    CResTOCManager toc;
    if (!toc.Init(big, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    CProfileManager original;
    const std::filesystem::path copiedSave = TestOutput::Path("profile");
    if (!LoadProfile(toc, tables, original, copiedSave, TestOutput::Fixtures())) { return 1; }
    std::vector<ZMissionEntry> missions;
    if (!LoadMissionCatalog(toc, tables, missions)) { return 1; }
    for (const auto &mission : missions) {
        if (tables.GetPackName(mission.resource.packHash) != "pack2" || mission.resource.localIndex != 14) { continue; }
        DebugMapSelection selected;
        selected.pack = "pack2";
        selected.map = 3;
        selected.level = mission.data.level;
        selected.mission = mission;
        selected.hasMission = true;
        // Both starting slots must select the corresponding saved gun.
        for (unsigned slot = 0; slot < 2; ++slot) {
            CProfileManager preview = original;
            preview.activeWeaponSlot = slot;
            ZSurvivalGameContext context{preview, copiedSave / "must-not-exist"};
            auto launch = MakeDebugMapLaunch(big, selected, context);
            if (launch.gameContext != &context || context.persistProgress ||
                launch.withBrother != original.brotherEnabled) { return 1; }
            SurvivalDevelopment development;
            DevelopmentBinding binding(launch, development);
            development.debugMapProfileCheck = true;
            if (RunSurvivalSession(launch) != 0) { return 1; }
            preview.coins += 1;
            if (!context.SaveProfile() || std::filesystem::exists(context.savePath) ||
                preview.coins == original.coins) { return 1; }
        }
        std::printf("[debug-map-profile-check] both-slots=1 isolated-save=1 failures=0\n");
        return 0;
    }
    return 1;
}

int CheckDebugMapProfile(ZPackTables &tables, const CBrother &player,
    const CPlayerProgress &progress, ZSurvivalGameContext &context, const CLevel &scene, const CLevel &level) {
    const auto &profile = context.profile;
    const auto &gun = profile.configuration.guns[profile.activeWeaponSlot];
    if (player.gunResource.packHash != gun.packHash || player.gunResource.localIndex != gun.localIndex ||
        progress.GetExperience() != profile.experience || player.brotherIndex != profile.playerBrother ||
        player.masteryExperience != profile.GetWeaponExperience(gun)) { return 1; }
    unsigned checkedArmor = 0;
    for (const auto &ref : profile.configuration.armor) {
        if (ref.IsNull()) { continue; }
        std::vector<std::uint8_t> bytes;
        if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Armor, ref.localIndex, bytes)) { return 1; }
        CArrayInputStream input(bytes);
        CArmor::Template expected;
        if (!expected.Init(input)) { return 1; }
        const auto &actual = player.armor[expected.GetSlot()];
        if (!actual) { return 1; }
        for (unsigned variant = 0; variant < kArmorVariantCount; ++variant) {
            if (actual->GetTemplate().GetMeshRef(variant).packHash != expected.GetMeshRef(variant).packHash ||
                actual->GetTemplate().GetMeshRef(variant).assetId != expected.GetMeshRef(variant).assetId ||
                actual->GetTemplate().GetImageRef(variant).assetId != expected.GetImageRef(variant).assetId) { return 1; }
        }
        ++checkedArmor;
    }
    std::uint64_t accountedXplodium = 0;
    if (!SaveSurvivalProgress(&context, progress, scene, level, accountedXplodium) ||
        std::filesystem::exists(context.savePath)) { return 1; }
    std::printf("[debug-map-profile-check] slot=%u gun=%08x:%u armor=%u xp=%llu mastery=%u failures=0\n",
        profile.activeWeaponSlot, gun.packHash, gun.localIndex, checkedArmor,
        static_cast<unsigned long long>(profile.experience), player.masteryExperience);
    return 0;
}

int RunCampaignContentCheck() {
    const std::string big = (Paths::Root() / Paths::BigDirectory).u8string();
    CResTOCManager toc;
    if (!toc.Init(big, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    if (CheckWallWeaponResources(toc, tables) != 0) { return 1; }
    std::vector<ZMissionEntry> missions;
    if (!LoadMissionCatalog(toc, tables, missions)) { return 1; }
    for (const auto &mission : missions) {
        if (tables.GetPackName(mission.data.level.packHash) != "pack2") { continue; }
        std::printf("[campaign-mission] %s:%u -> LEVEL pack2:%u title=%s\n",
            tables.GetPackName(mission.resource.packHash).c_str(), mission.resource.localIndex, mission.data.level.localIndex, mission.title.c_str());
    }
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const auto &pack = *toc.GetPack(packIndex);
        const auto &objects = tables.GetObjectPack(packIndex);
        for (unsigned index = 0; index < objects.GetObjectCount(ZGameSection::Level); ++index) {
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(pack.GetPackHash(), ZGameSection::Level, index, bytes)) { return 1; }
            CArrayInputStream input(bytes);
            CLevel::Template level;
            if (!level.Init(input) || input.Available() != 0) { return 1; }
            std::printf("[campaign-content] %s LEVEL %u -> MAP %s:%u script=%d\n", pack.GetShortName().c_str(), index,
                tables.GetPackName(level.mapRef.packHash).c_str(), level.mapRef.localIndex, level.script.IsPresent());
        }
        for (unsigned index = 0; index < objects.GetObjectCount(ZGameSection::TileLayer); ++index) {
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(pack.GetPackHash(), ZGameSection::TileLayer, index, bytes)) { return 1; }
            CArrayInputStream input(bytes);
            CMap map;
            if (!map.Init(input) || input.Available() != 0) { return 1; }
            if (pack.GetShortName() == "pack2" && index == 6) {
                for (const auto &dependency : map.GetRequirements().objects) {
                    std::printf("[campaign-map6-dependency] type=%u ref=%s:%u\n", dependency.objectType,
                        tables.GetPackName(dependency.object.packHash).c_str(), dependency.object.localIndex);
                }
            }
            unsigned players = 0;
            for (unsigned layerIndex = 0; layerIndex < map.GetObjectLayerCount(); ++layerIndex) {
                const auto &layer = map.GetObjectLayer(layerIndex);
                unsigned objectId = 0;
                for (const auto &object : layer.GetObjects()) {
                    if (object.objectType == static_cast<unsigned>(ZPlacedObjectType::Player)) { ++players; }
                    if (pack.GetShortName() == "pack2" && (index == 0 || index == 2 || index == 4 || index == 5 || index == 6)) {
                        std::printf("[campaign-object] MAP %u layer=%u id=%u type=%u ref=%s:%u xy=%d,%d tag=%u\n",
                            index, layer.GetLayerIndex(), objectId, object.objectType,
                            tables.GetPackName(object.packHash).c_str(), object.localIndex, object.x, object.y, object.spawnTag);
                        if (object.objectType == static_cast<unsigned>(ZPlacedObjectType::Prop)) {
                            if (!tables.ReadSectionResource(object.packHash, ZGameSection::Prop, object.localIndex, bytes)) { return 1; }
                            CArrayInputStream propInput(bytes);
                            CProp::Template data;
                            if (!data.Init(propInput)) { return 1; }
                            CProp prop;
                            prop.Bind(data);
                            std::printf("[campaign-prop] id=%u hp=%.0f body=%zu bullet=%zu script=%d sprite=%u state=%u entry=%d\n",
                                objectId, prop.GetHealth(), data.GetCollision().GetEdges().size(), data.GetBulletCollision().GetEdges().size(),
                                data.GetScript().IsPresent(), data.GetSpriteRef().archetype, prop.GetStateId(), prop.ChecksEntry());
                        }
                    }
                    ++objectId;
                }
            }
            std::printf("[campaign-content] %s MAP %u players=%u\n", pack.GetShortName().c_str(), index, players);
            if (pack.GetShortName() == "pack2" && index == 2) {
                for (unsigned cameraIndex = 0; cameraIndex < map.GetCameraLayerCount(); ++cameraIndex) {
                    const auto &camera = map.GetCameraLayer(cameraIndex);
                    const auto &bounds = camera.GetSecondaryBounds();
                    std::printf("[campaign-camera] MAP 2 layer=%u xywh=%d,%d,%d,%d\n",
                        camera.GetLayerIndex(), bounds.x, bounds.y, bounds.width, bounds.height);
                }
            }
        }
    }
    return 0;
}
