#pragma once
/** Load and equip CBrother instances at original object-layer spawn points. */
#include "gun_bros_re/gameplay/map/CMapResources.h"
struct ZWeaponEntry;
namespace MapDetail {
void LoadPlacedPlayers(CResTOCManager &tocManager, const ZShaderProgram &program,
                       CMap &loaded);
bool EquipControlledPlayer(ZPackTables &tables, CMap &loaded,
    const ZShaderProgram &program, const ZWeaponEntry &weapon);
}
