#include "gun_bros_re/gameplay/map/CLayerObjectPlayers.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "gun_bros_re/data/objects/CGunBros.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace MapDetail {

/**
 * Load a model for every enemy the object layer places.
 *
 * **These are leftovers, not how the shipped game works.** Every shipped map
 * is survival: enemies arrive from off screen and close in, spawned by the
 * level rather than placed on it. The object layer's enemies are what was left
 * of a campaign mode, which is why most maps have none and the ones that do
 * cannot be checked against anything -- except pack9's two, which are turrets
 * and stand where they are placed.
 */
/**
 * Stand a player on every spawn point the object layer names.
 *
 * The default model and nothing else: no weapon, no armour. Which gun a
 * player carries is a loadout question and the loadout is not read yet, so
 * putting one in his hand here would be inventing data.
 */
void LoadPlacedPlayers(CResTOCManager &tocManager, const ZShaderProgram &program,
                       CMap &loaded) {
    CGunBros tables(tocManager);

    for (std::uint32_t layer = 0; layer < loaded.GetObjectLayerCount();
         ++layer) {
        const std::vector<CLayerObject::Object> &objects =
            loaded.GetObjectLayer(layer).GetObjects();
        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (objects[i].objectType !=
                static_cast<std::uint8_t>(CLayerObject::ObjectType::Player)) {
                continue;
            }

            // Looked up on the first spawn point rather than up front, so a
            // map with none never pays for the walk over every pack.
            if (loaded.GetResources().playerTemplate == nullptr) {
                loaded.GetResources().playerTemplate.reset(new CBrother::Template());
                if (!loaded.GetResources().playerTemplate->Load(tocManager, tables)) {
                    std::printf("[m3] no player template in the archives\n");
                    loaded.GetResources().playerTemplate.reset();
                    return;
                }
            }

            CMap::Resources::Player placed;
            placed.x = static_cast<float>(objects[i].x);
            placed.y = static_cast<float>(objects[i].y);
            // CBrother::Spawn :135887 stores the PLAYER object's extra uint16 in
            // the angle member +1984. Maps without that field leave the original
            // reading uninitialised memory; this port keeps 0.
            placed.facingDegrees = static_cast<float>(objects[i].playerSpawnFacing);
            placed.model.reset(new CBrother());
            if (!placed.model->BuildBody(tables, loaded.GetResources().playerTemplate->GetMoveSet()) ||
                !placed.model->CreateBuffers(program)) {
                std::printf("[m3] player at %d %d could not be built\n",
                            objects[i].x, objects[i].y);
                continue;
            }

            // BuildBody already selects the first authored move for each part.
            std::printf("[m3] %s at %d %d -- scale %.0f facing %.0f authored=%d\n",
                        loaded.GetResources().playerTemplate->GetOwner().c_str(), objects[i].x,
                        objects[i].y, loaded.GetResources().playerTemplate->GetGameScale(),
                        placed.facingDegrees, objects[i].hasPlayerSpawnFacing);
            loaded.GetResources().players.push_back(std::move(placed));
        }
    }
}

/**
 * Drive the first player with WASD and resolve the requested movement.
 *
 * Maps contain one real player spawn. A few abandoned campaign maps contain
 * none; those remain valid viewers and simply ignore movement input.
 */
/** Swap equipment only after every referenced asset has loaded. */
bool EquipControlledPlayer(CGunBros &tables, CMap &loaded,
    const ZShaderProgram &program, const CGun::Entry &weapon) {
    if (loaded.GetResources().players.empty()) { return true; }
    // CombatScene retains this actor's address. CBrother::EquipWeapon stages the
    // weapon atomically and preserves the body, vitals pointer and armour.
    CBrother &player = *loaded.GetResources().players[0].model;
    player.gunResource.packHash = weapon.packHash;
    player.gunResource.localIndex = static_cast<std::uint8_t>(weapon.ordinal);
    const std::uint64_t key = (static_cast<std::uint64_t>(weapon.packHash) << 8) | weapon.ordinal;
    player.masteryExperience = 0;
    const auto mastery = player.masteryByWeapon.find(key);
    if (mastery != player.masteryByWeapon.end()) { player.masteryExperience = mastery->second; }
    if (!player.EquipWeapon(tables, loaded.GetResources().playerTemplate->GetScript(), weapon.data, weapon.owner) ||
        !player.CreateBuffers(program)) { return false; }
    return true;
}
}
