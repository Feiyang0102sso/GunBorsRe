#pragma once
/** Load and equip CBrother instances at original object-layer spawn points. */
#include "gun_bros_re/gameplay/map/CMapResources.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
namespace MapDetail {
void LoadPlacedPlayers(CResTOCManager &tocManager, const ZShaderProgram &program,
                       CMap &loaded);
bool EquipControlledPlayer(CGunBros &tables, CMap &loaded,
    const ZShaderProgram &program, const CGun::Entry &weapon);
}
