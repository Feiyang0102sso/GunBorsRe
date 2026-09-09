/** @file Planet.h
 * @brief Original Planet metadata, including names and menu sprite references.
 */
#ifndef GUN_BROS_RE_PLANET_H
#define GUN_BROS_RE_PLANET_H
#include "gun_bros/CGameAssetRef.h"
#include <vector>

class Planet {
public:
    bool Init(CArrayInputStream &stream) {
        // Planet::Init (:169935), CreateNameString (:170215).
        name.Init(stream);
        description.Init(stream);
        mapSlot = stream.ReadUInt16();
        thumbnail.Init(stream);
        largeImage.Init(stream);
        image44.Init(stream);
        missions.resize(stream.ReadUInt8());
        for (GameObjectRef &mission : missions) { mission.Init(stream); }
        object12.Init(stream);
        requiredLevel = stream.ReadUInt16();
        return !stream.Overran();
    }
    CGameAssetRef name;
    CGameAssetRef description;
    unsigned mapSlot = 0; // CMenuMission::Bind :162800, original mem+80.
    CGameSpriteGluRef thumbnail;
    CGameSpriteGluRef largeImage;
    CGameSpriteGluRef image44;
    std::vector<GameObjectRef> missions;
    GameObjectRef object12;
    unsigned requiredLevel = 0; // LevelReqCallback :161277, original mem+82.
};
#endif
