/** @file ZProfileImport.h
 * @brief Read-only archive adapter for original CProfileManager data stores.
 */
#ifndef GUN_BROS_RE_ZPROFILEIMPORT_H
#define GUN_BROS_RE_ZPROFILEIMPORT_H
#include "gun_bros_re/data/CPlayerConfiguration.h"
#include <filesystem>
#include <vector>
#include <string>

class CResTOCManager;
class ZPackTables;
class CProfileManager;

struct ZImportedDataStore {
    unsigned version = 0;
    unsigned padding = 0;
    unsigned minimumSize = 0;
    int clientId = 0;
    bool crcMatches = false;
    std::vector<std::uint8_t> data;
};

bool ReadDataStore(const std::filesystem::path &path, ZImportedDataStore &record);
bool ImportProfile(CResTOCManager &toc, ZPackTables &tables, CProfileManager &profile, const std::filesystem::path &source);
#endif
