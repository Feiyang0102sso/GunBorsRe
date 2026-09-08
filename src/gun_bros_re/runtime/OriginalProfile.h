/** @file OriginalProfile.h
 * @brief Read-only archive adapter for original CProfileManager data stores.
 */
#ifndef GUN_BROS_RE_ORIGINALPROFILE_H
#define GUN_BROS_RE_ORIGINALPROFILE_H
#include "gun_bros/CPlayerConfiguration.h"
#include <filesystem>
#include <vector>
#include <string>

class CResTOCManager;
class PackTables;
class CProfileManager;

struct OriginalDataStore {
    unsigned version = 0;
    unsigned padding = 0;
    unsigned minimumSize = 0;
    int clientId = 0;
    bool crcMatches = false;
    std::vector<std::uint8_t> data;
};

bool ReadOriginalDataStore(const std::filesystem::path &path, OriginalDataStore &record);
int RunOriginalProfileCheck(const std::string &bigDirectory);
bool ImportOriginalProfile(CResTOCManager &toc, PackTables &tables, CProfileManager &profile);
int RunOriginalProfilePlayCheck(const std::string &bigDirectory);
#endif
