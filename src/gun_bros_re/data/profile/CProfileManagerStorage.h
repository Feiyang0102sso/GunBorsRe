#pragma once
/** Internal native serialization helpers; original CProfileManager and clients.
 * Disk evidence: saves/GB_save_profile.bt and save_payloads.bt.
 * Windows file operations are deliberately absent from this interface.
 */
#include "gun_bros_re/data/profile/CProfileManager.h"
#include <random>
namespace ProfileStorageDetail {

constexpr unsigned kFirstStore = 1000;
constexpr unsigned kLastStore = 1018;
constexpr unsigned kUnregisteredStore = 1015;
constexpr unsigned kBlockSize = 512;
constexpr unsigned kWrapperSize = 24;
constexpr unsigned kStatisticCount = 47; // CPlayerStatistics::ResetData :221159, 0xBC bytes.
constexpr unsigned kChallengeSlots = 8; // CChallengeProgressData::Save :298860.
constexpr unsigned kFriendSlots = 30;

void Put16(std::vector<std::uint8_t> &bytes, std::size_t offset, unsigned value);

void Put32(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint32_t value);


/** CContentTracker::UserData::Init :225636, PerPackData::Serialize :225374. */
bool InitializeContentSeen(CProfileManager::Archive &archive);

/** Set only the authored object's seen bit; preserve unrelated packs and bits. */
bool MarkContentSeen(CProfileManager::Archive &archive, const GameObjectTypeRef &object);

/** Determine the real end before padding; odd payloads cannot use a size guess. */
bool PayloadSize(unsigned id, const std::vector<std::uint8_t> &bytes, std::size_t &size);

/** Native random fill consists of GetRand(0x7FFF) words and zero remainder. */
void FillPadding(std::vector<std::uint8_t> &bytes, std::size_t offset, std::size_t length, std::mt19937 &random);

std::vector<std::uint8_t> EncodeRecord(const CProfileManager::Record &record);


std::uint64_t Get64(CArrayInputStream &input);

void Put64(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint64_t value);

std::uint32_t Get32(const std::vector<std::uint8_t> &bytes, std::size_t offset);

GameObjectRef MemoryRef(const std::vector<std::uint8_t> &bytes, unsigned offset);

bool SameRef(const GameObjectRef &first, const GameObjectRef &second);

GameObjectRef CollectionRef(const std::vector<std::uint8_t> &bytes, std::size_t offset);

bool CheckRef(const CProfileManager::Archive &archive, const GameObjectRef &ref, unsigned type);

/** Only changed references are reconciled. Untouched native alignment survives. */
bool WriteMemoryRef(CProfileManager::Archive &archive, std::vector<std::uint8_t> &bytes, unsigned offset, const GameObjectRef &ref);

std::size_t FindRecord(const std::vector<std::uint8_t> &bytes, unsigned stride, unsigned type, const GameObjectRef &ref);

std::size_t EnsureRecord(std::vector<std::uint8_t> &bytes, unsigned stride, unsigned type, const GameObjectRef &ref);

/** Decode proven fields without replacing unknown values or unsupported records. */
bool ApplyArchive(CProfileManager &profile, CProfileManager::Archive archive);

bool ReadArchive(const std::filesystem::path &directory, CProfileManager::Archive &archive);

}
