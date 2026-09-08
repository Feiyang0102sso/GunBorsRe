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
        value80 = stream.ReadUInt16();
        thumbnail.Init(stream);
        largeImage.Init(stream);
        image44.Init(stream);
        missions.resize(stream.ReadUInt8());
        for (GameObjectRef &mission : missions) { mission.Init(stream); }
        object12.Init(stream);
        value82 = stream.ReadUInt16();
        return !stream.Overran();
    }
    CGameAssetRef name;
    CGameAssetRef description;
    unsigned value80 = 0;
    CGameSpriteGluRef thumbnail;
    CGameSpriteGluRef largeImage;
    CGameSpriteGluRef image44;
    std::vector<GameObjectRef> missions;
    GameObjectRef object12;
    unsigned value82 = 0;
};
#endif
