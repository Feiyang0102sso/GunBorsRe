#pragma once
/** Concrete typed object storage, original CGameObjectPack::InitGameObject.
 * Maps provide stable addresses; reinitializing/destroying the pack releases them.
 * These containers describe host storage, not the original ARM object layout.
 */
#include "gun_bros_re/data/objects/CGameObjectPack.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "gun_bros_re/gameplay/armor/CArmor.h"
#include "gun_bros_re/gameplay/powerup/CPowerup.h"
#include "gun_bros_re/data/store/CStoreItem.h"
#include "gun_bros_re/data/mission/Mission.h"
#include "gun_bros_re/data/mission/Planet.h"
#include <map>
struct CGameObjectPack::Objects {
    std::map<unsigned, CGun::Template> guns;
    std::map<unsigned, CArmor::Template> armor;
    std::map<unsigned, CPowerup::Template> powerups;
    std::map<unsigned, CStoreItem> store;
    std::map<unsigned, Mission> missions;
    std::map<unsigned, Planet> planets;
    std::map<unsigned, std::string> strings;
};
