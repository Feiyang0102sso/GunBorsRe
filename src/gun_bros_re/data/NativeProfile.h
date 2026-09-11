/** @file NativeProfile.h
 * @brief Windows storage adapter for the original numbered DataStores.
 * Payloads retain all fields, including those without a restored consumer.
 */
#ifndef GUN_BROS_RE_NATIVEPROFILE_H
#define GUN_BROS_RE_NATIVEPROFILE_H
#include "gun_bros_re/data/CPlayerProgress.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "gun_bros_re/data/COptionsMgr.h"
#include <array>
#include <filesystem>
#include <vector>

class CResTOCManager;
class PackTables;
class CProfileManager;

struct NativeProfileRecord {
    unsigned version = 0;
    int owner = -1;
    std::vector<std::uint8_t> payload;
    // Preserve wrapper/padding byte-for-byte when a record has not changed.
    std::vector<std::uint8_t> originalPayload;
    std::vector<std::uint8_t> originalFile;
};

struct NativeProfileArchive {
    // Explicitly imported sources are read-only; this protection persists after copying into an account.
    std::filesystem::path importDirectory;
    // Non-owning runtime binding, like the original SaveRestore clients. Both
    // resource managers outlive the active profile and all of its checkpoints.
    CResTOCManager *toc = nullptr;
    PackTables *tables = nullptr;
    // CGunBros::Init :80407 registers IDs 1000..1018 except 1015.
    std::array<NativeProfileRecord, 19> records;
    std::array<std::uint8_t, 19> status{};
    COptionsMgr options;
    CPlayerProgress::Template progression;
    // Host's four retail slots follow mapSlot order, filtered by Mission.type=1.
    std::array<GameObjectRef, 4> survivalLevels;
    std::uint64_t loadedExperience = 0;
};

bool ReadNativeProfileRecord(const std::filesystem::path &path, unsigned id, NativeProfileRecord &record);
bool WriteNativeProfileArchive(const std::filesystem::path &directory, const NativeProfileArchive &archive);
bool CreateNativeProfileArchive(CResTOCManager &toc, PackTables &tables, NativeProfileArchive &archive);
bool LoadNativeProfile(CResTOCManager &toc, PackTables &tables, CProfileManager &profile,
    const std::filesystem::path &directory, const std::filesystem::path &sourceDirectory = {});
bool ReloadNativeProfile(CProfileManager &profile, const std::filesystem::path &directory);
bool SaveNativeProfile(const CProfileManager &profile, const std::filesystem::path &directory);
bool RecordNativeMissionWaves(CProfileManager &profile, const GameObjectRef &level,
    unsigned waveProgress, const std::vector<bool> &perfectResults);
bool RecordNativeMissionScore(CProfileManager &profile, const GameObjectRef &mission, unsigned score);
#endif

