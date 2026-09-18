/** Original: src/gunbros/enemy.cpp Template::Init :67174; enemy_template.bt.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
/**
 * @file CEnemyTemplate.cpp
 * @brief One enemy's models, assembled by its script and ready to draw.
 */

#include "gun_bros_re/gameplay/enemy/CEnemy.h"

#include "engine/resources/CArrayInputStream.h"
#include "gun_bros_re/data/CGameObjectPack.h"

#include <cstdio>

CEnemy::Template::Template()
    : packHash(0),
      ordinal(0),
      gameScale(0.0f),
      uiScalePercent(0.0f),
      experienceReward(0),
      xplodiumReward(0),
      flag117(0),
      radius116(0) {}

bool CEnemy::Template::Load(ZPackTables &tables, std::uint32_t packHash,
    std::uint32_t ordinal, const std::string &owner) {
    std::vector<std::uint8_t> payload;
    if (!tables.ReadSectionResource(packHash, ZGameSection::Enemy, ordinal,
                                    payload)) {
        return false;
    }

    this->packHash = packHash;
    this->ordinal = ordinal;
    this->owner = owner;

    CArrayInputStream stream(payload);
    return Init(stream);
}

bool CEnemy::Template::Init(CArrayInputStream &stream) {
    stream.ReadUInt8();

    name.Init(stream);

    script.Load(stream);
    if (!moveSet.Init(stream)) {
        std::printf("[enemy] %s: move set unreadable\n", owner.c_str());
        return false;
    }

    objectRef104.Init(stream);
    experienceReward = stream.ReadUInt16();
    xplodiumReward = stream.ReadUInt16();
    flag117 = stream.ReadUInt8();
    radius116 = stream.ReadUInt8();
    gameScale = static_cast<float>(stream.ReadUInt16());
    uiScalePercent = static_cast<float>(stream.ReadUInt16());
    // M5 consumes the final field which the assembly-only reader skipped.
    if (!collision.Load(stream)) {
        return false;
    }
    if (stream.Overran() || stream.Available() != 0) {
        std::printf("[enemy] %s: template has invalid length or trailing bytes\n",
                    owner.c_str());
        return false;
    }

    // A move set names one pack for every model in it, and that is the pack an
    // enemy's parts come out of -- not necessarily the one the template is in.
    // Keep the template's pack identity. Individual mesh configs are addressed
    // through the move set's pack, which can differ from the template's pack.
    return true;
}

bool CEnemy::Template::LoadCatalog(CResTOCManager &toc, ZPackTables &tables,
    std::vector<CEnemy::Template> &entries) {
    entries.clear();
    bool complete = true;
    for (std::uint32_t i = 0; i < toc.GetPackCount(); ++i) {
        CResPackTOC *pack = toc.GetPack(static_cast<int>(i));
        const std::uint32_t count = tables.GetObjectPack(static_cast<int>(i)).GetObjectCount(ZGameSection::Enemy);
        for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
            CEnemy::Template entry;
            const std::string label = pack->GetShortName() + " enemy " + std::to_string(ordinal);
            if (!entry.Load(tables, pack->GetPackHash(), ordinal, label)) {
                complete = false;
            }
            entries.push_back(std::move(entry));
        }
    }
    return complete;
}
