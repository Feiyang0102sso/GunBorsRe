#pragma once
/** @file OriginalProfile.cpp
 * @brief Native storage envelope and proven data layouts; source files stay read-only.
 */
#include "gun_bros_re/data/OriginalProfile.h"
#include "gun_bros_re/data/StoreCatalog.h"
#include "engine/core/CCrc32.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/gameplay/SurvivalGameContext.h"
#include "gun_bros_re/gameplay/MapScene.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iterator>

namespace OriginalProfileDetail {

std::uint64_t ReadUInt64(CArrayInputStream &stream);

GameObjectRef ReadMemoryRef(CArrayInputStream &stream);

bool ReportReference(PackTables &tables, CResTOCManager &toc, const GameObjectRef &ref,
    unsigned type, std::ofstream &report);

// Exercise malformed copies only. A failed envelope read must not replace the
// caller's last valid record; checksum failure remains visible to the inspector.

#if GB_ENABLE_TESTS
unsigned CheckStorageBoundaries(const std::filesystem::path &source, const std::filesystem::path &output);
#endif

}
