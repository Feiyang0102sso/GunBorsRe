/** @file Mission.h
 * @brief Original Mission record (:164402), including archived campaign data.
 */
#ifndef GUN_BROS_RE_MISSION_H
#define GUN_BROS_RE_MISSION_H
#include "gun_bros_re/data/CGameAssetRef.h"
#include "engine/glu/script/CScript.h"
#include <vector>

class Mission {
public:
    bool Init(CArrayInputStream &stream) {
        title.Init(stream);
        description.Init(stream);
        requirements.Init(stream);
        overview.Init(stream);
        level.Init(stream);
        objectives.resize(stream.ReadUInt8());
        for (GameObjectRef &objective : objectives) { objective.Init(stream); }
        script.Load(stream);
        type = stream.ReadUInt8();
        value64 = stream.ReadUInt16();
        value66 = stream.ReadUInt8();
        return !stream.Overran();
    }
    CGameAssetRef title, description, requirements, overview;
    GameObjectRef level;
    std::vector<GameObjectRef> objectives;
    CScript script;
    unsigned type = 0;
    unsigned value64 = 0;
    unsigned value66 = 0;
};
#endif
