/** @file MissionObjective.h
 * @brief Exact original mission objective wire reader (:175995).
 */
#ifndef GUN_BROS_RE_MISSIONOBJECTIVE_H
#define GUN_BROS_RE_MISSIONOBJECTIVE_H
#include "gun_bros_re/data/objects/CGameAssetRef.h"

class MissionObjective {
public:
    bool Init(CArrayInputStream &stream) {
        title.Init(stream);
        description.Init(stream);
        type = stream.ReadUInt8();
        value32 = stream.ReadUInt16();
        value36 = stream.ReadUInt16();
        return !stream.Overran();
    }
    CGameAssetRef title, description;
    unsigned type = 0;
    unsigned value32 = 0;
    unsigned value36 = 0;
};
#endif
