#include "gun_bros_re/gameplay/brother/CBrother.h"
#include <cstdio>

/**
 * The one player template in the archives, with the scale it draws at.
 *
 * `gameScale` is template offset 112: CBrother::Bind (:135608) copies it to
 * this[495] and CBrother::Draw multiplies it into the draw scale, exactly
 * where an enemy's template word 66 goes.
 */
bool CBrother::Template::Load(CResTOCManager &tocManager, ZPackTables &tables) {
    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        CResPackTOC *pack = tocManager.GetPack(static_cast<int>(i));
        CGameObjectPack &objectPack = tables.GetObjectPack(static_cast<int>(i));
        const std::uint32_t count = objectPack.GetObjectCount(ZGameSection::Player);

        for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
            std::vector<std::uint8_t> payload;
            if (!pack->GetResource(objectPack.GetHandle(ZGameSection::Player, ordinal),
                                   payload)) {
                continue;
            }

            CArrayInputStream stream(payload);
            if (!Init(stream)) {
                continue;
            }

            char label[128];
            std::snprintf(label, sizeof(label), "%s player %u",
                          pack->GetShortName().c_str(), ordinal);

            m_owner = label;
            return true;
        }
    }
    return false;
}

