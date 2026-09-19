#pragma once
/** @file ZProfileImportInternal.h
 * @brief Native storage envelope and proven data layouts; source files stay read-only.
 */
#include "gun_bros_re/data/ZProfileImport.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "engine/core/CCrc32.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/game/CGameSession.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iterator>

namespace ProfileImportDetail {

std::uint64_t ReadUInt64(CArrayInputStream &stream);

GameObjectRef ReadMemoryRef(CArrayInputStream &stream);

bool ReportReference(ZPackTables &tables, CResTOCManager &toc, const GameObjectRef &ref,
    unsigned type, std::ofstream &report);

// Exercise malformed copies only. A failed envelope read must not replace the
// caller's last valid record; checksum failure remains visible to the inspector.

unsigned CheckStorageBoundaries(const std::filesystem::path &source, const std::filesystem::path &output);

}
