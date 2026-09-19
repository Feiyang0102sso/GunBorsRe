#pragma once
/** @file ZProfileStorageInternal.h
 * @brief Original serialized layouts; Windows file replacement is the host boundary.
 * Sources: saves/GB_save_profile.bt, save_payloads.bt and CProfileManager :202880/:203163.
 */
#define NOMINMAX
#include "gun_bros_re/data/ZProfileStorage.h"
#include "gun_bros_re/data/ZProfileImport.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/gameplay/game/CGameSession.h"
#include "gun_bros_re/data/ZPlanetCatalog.h"
#include "gun_bros_re/data/ZMissionCatalog.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/data/Planet.h"
#include "gun_bros_re/data/Mission.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/gameplay/CGun.h"
#include "gun_bros_re/data/CDailyBonusTracking.h"
#include "engine/core/CCrc32.h"
#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <chrono>
#include <cmath>
#include <cstring>
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

bool ReadBytes(const std::filesystem::path &path, std::vector<std::uint8_t> &bytes);

/** CContentTracker::UserData::Init :225636, PerPackData::Serialize :225374. */
bool InitializeContentSeen(ZProfileArchive &archive);

/** Set only the authored object's seen bit; preserve unrelated packs and bits. */
bool MarkContentSeen(ZProfileArchive &archive, const GameObjectTypeRef &object);

/** Determine the real end before padding; odd payloads cannot use a size guess. */
bool PayloadSize(unsigned id, const std::vector<std::uint8_t> &bytes, std::size_t &size);

/** Native random fill consists of GetRand(0x7FFF) words and zero remainder. */
void FillPadding(std::vector<std::uint8_t> &bytes, std::size_t offset, std::size_t length, std::mt19937 &random);

std::vector<std::uint8_t> EncodeRecord(const ZProfileRecord &record);

bool ReplaceFile(const std::filesystem::path &path, const std::vector<std::uint8_t> &bytes);

std::uint64_t Get64(CArrayInputStream &input);

void Put64(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint64_t value);

std::uint32_t Get32(const std::vector<std::uint8_t> &bytes, std::size_t offset);

GameObjectRef MemoryRef(const std::vector<std::uint8_t> &bytes, unsigned offset);

bool SameRef(const GameObjectRef &first, const GameObjectRef &second);

GameObjectRef CollectionRef(const std::vector<std::uint8_t> &bytes, std::size_t offset);

bool CheckRef(const ZProfileArchive &archive, const GameObjectRef &ref, unsigned type);

/** Only changed references are reconciled. Untouched native alignment survives. */
bool WriteMemoryRef(ZProfileArchive &archive, std::vector<std::uint8_t> &bytes, unsigned offset, const GameObjectRef &ref);

std::size_t FindRecord(const std::vector<std::uint8_t> &bytes, unsigned stride, unsigned type, const GameObjectRef &ref);

std::size_t EnsureRecord(std::vector<std::uint8_t> &bytes, unsigned stride, unsigned type, const GameObjectRef &ref);

/** Decode proven fields without replacing unknown values or unsupported records. */
bool ApplyArchive(CProfileManager &profile, ZProfileArchive archive);

bool ReadArchive(const std::filesystem::path &directory, ZProfileArchive &archive);

}
