/** Native DataStore responsibilities; see saves/GB_save_profile.bt and save_payloads.bt. */
#include "gun_bros_re/data/profile/CProfileManagerStorage.h"
#include "engine/core/CCrc32.h"
#include <algorithm>
#include <cstdio>
using namespace ProfileStorageDetail;

namespace ProfileStorageDetail {
void Put16(std::vector<std::uint8_t> &bytes, std::size_t offset, unsigned value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void Put32(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned index = 0; index < 4; ++index) { bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8)); }
}

/** Determine the real end before padding; odd payloads cannot use a size guess. */
bool PayloadSize(unsigned id, const std::vector<std::uint8_t> &bytes, std::size_t &size) {
    CArrayInputStream stream(bytes);
    switch (id) {
    case 1000: stream.Skip(48); break; // CPlayerProgress::LoadFromDisk :194502.
    case 1001: stream.Skip(120); break; // CPlayerConfiguration::LoadFromDisk :170602.
    case 1002: case 1003: case 1004: case 1005: case 1013: case 1016: case 1018: {
        unsigned stride = 14;
        if (id == 1002) { stride = 10; }
        if (id == 1003) { stride = 524; }
        if (id == 1004) { stride = 8; }
        const unsigned count = stream.ReadUInt32();
        if (count > stream.Available() / stride) { return false; }
        stream.Skip(static_cast<std::size_t>(count) * stride);
        break;
    }
    case 1006: {
        stream.ReadUInt16();
        const unsigned count = stream.ReadUInt16();
        if (count > 20) { return false; }
        stream.Skip(count * 8);
        break;
    }
    case 1007: stream.Skip(22); break; // CTutorialManager::SaveToDisk :210615.
    case 1008: stream.Skip(340); break; // CRefinementManager::SaveToDisk :177115.
    case 1009: stream.Skip(12); break;
    case 1010: {
        const unsigned count = stream.ReadUInt32();
        if (count > stream.Available() / 4) { return false; }
        stream.Skip(count * 4);
        break;
    }
    case 1011: stream.Skip(4); break;
    case 1012: stream.Skip(1052); break;
    case 1014: {
        const unsigned count = stream.ReadUInt8();
        for (unsigned pack = 0; pack < count; ++pack) {
            stream.ReadUInt32();
            const unsigned length = stream.ReadUInt32();
            const std::size_t start = stream.Position();
            const unsigned categories = stream.ReadUInt8();
            if (categories > 4) { return false; }
            for (unsigned category = 0; category < categories; ++category) {
                if (stream.ReadUInt8() >= 4) { return false; }
                const unsigned bits = stream.ReadUInt8();
                stream.Skip((bits + 7) / 8);
            }
            if (stream.Position() - start != length) { return false; }
        }
        break;
    }
    case 1017: {
        stream.Skip(6);
        unsigned count = stream.ReadUInt8();
        if (count > kChallengeSlots) { return false; }
        stream.Skip(count);
        count = stream.ReadUInt8();
        if (count > kChallengeSlots) { return false; }
        stream.Skip(count);
        count = stream.ReadUInt8();
        if (count > kChallengeSlots) { return false; }
        for (unsigned group = 0; group < count; ++group) {
            const unsigned friends = stream.ReadUInt8();
            if (friends > kFriendSlots) { return false; }
            stream.Skip(friends * 8);
        }
        count = stream.ReadUInt8();
        if (count > kChallengeSlots) { return false; }
        stream.Skip(count * 20);
        break;
    }
    default: return false;
    }
    size = stream.Position();
    return !stream.Overran();
}

/** Native random fill consists of GetRand(0x7FFF) words and zero remainder. */
void FillPadding(std::vector<std::uint8_t> &bytes, std::size_t offset, std::size_t length, std::mt19937 &random) {
    for (std::size_t index = 0; index + 4 <= length; index += 4) { Put32(bytes, offset + index, random() % 0x7FFF); }
}

std::vector<std::uint8_t> EncodeRecord(const CProfileManager::Record &record) {
    const std::size_t payloadSize = record.payload.size();
    const std::size_t aligned = payloadSize + kBlockSize - payloadSize % kBlockSize;
    const std::size_t prefix = (aligned >> 1) - (payloadSize >> 1);
    std::vector<std::uint8_t> bytes(aligned + kWrapperSize, 0);
    Put32(bytes, 0, record.version);
    Put32(bytes, 8, static_cast<unsigned>(prefix));
    std::random_device seed;
    std::mt19937 random(seed());
    FillPadding(bytes, 12, prefix, random);
    std::copy(record.payload.begin(), record.payload.end(), bytes.begin() + 12 + prefix);
    const std::size_t suffixStart = 12 + prefix + payloadSize;
    FillPadding(bytes, suffixStart, bytes.size() - 8 - suffixStart, random);
    Put32(bytes, bytes.size() - 8, static_cast<std::uint32_t>(record.owner));
    Put32(bytes, bytes.size() - 4, CCrc32::Crc32(bytes.data(), bytes.size() - 4));
    return bytes;
}

std::uint64_t Get64(CArrayInputStream &input) {
    const std::uint64_t low = input.ReadUInt32();
    return low | (static_cast<std::uint64_t>(input.ReadUInt32()) << 32);
}

void Put64(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint64_t value) {
    Put32(bytes, offset, static_cast<std::uint32_t>(value));
    Put32(bytes, offset + 4, static_cast<std::uint32_t>(value >> 32));
}

std::uint32_t Get32(const std::vector<std::uint8_t> &bytes, std::size_t offset) {
    CArrayInputStream input(bytes.data() + offset, bytes.size() - offset);
    return input.ReadUInt32();
}

GameObjectRef MemoryRef(const std::vector<std::uint8_t> &bytes, unsigned offset) {
    GameObjectRef ref;
    ref.packHash = Get32(bytes, offset);
    ref.localIndex = bytes[offset + 6];
    if (ref.localIndex == 255) { return {}; }
    return ref;
}

bool SameRef(const GameObjectRef &first, const GameObjectRef &second) {
    return first.packHash == second.packHash && first.localIndex == second.localIndex;
}

GameObjectRef CollectionRef(const std::vector<std::uint8_t> &bytes, std::size_t offset) {
    GameObjectRef ref;
    ref.packHash = Get32(bytes, offset);
    ref.localIndex = bytes[offset + 4];
    return ref;
}

bool CheckRef(const CProfileManager::Archive &archive, const GameObjectRef &ref, unsigned type) {
    if (ref.IsNull()) { return true; }
    std::vector<std::uint8_t> bytes;
    if (!archive.tables->ReadSectionResource(ref.packHash, static_cast<ZGameSection>(type + 1), ref.localIndex, bytes)) {
        std::printf("[native-profile] unresolved ref hash=%u type=%u ordinal=%u\n", ref.packHash, type, ref.localIndex);
        return false;
    }
    return true;
}

/** Only changed references are reconciled. Untouched native alignment survives. */
bool WriteMemoryRef(CProfileManager::Archive &archive, std::vector<std::uint8_t> &bytes, unsigned offset, const GameObjectRef &ref) {
    if (SameRef(MemoryRef(bytes, offset), ref)) { return true; }
    if (ref.IsNull()) { bytes[offset + 6] = 255; return true; }
    const int pack = archive.toc->GetPackIndexFromHash(ref.packHash);
    if (pack < 0) { return false; }
    Put32(bytes, offset, ref.packHash);
    Put16(bytes, offset + 4, pack);
    bytes[offset + 6] = ref.localIndex;
    return true;
}

std::size_t FindRecord(const std::vector<std::uint8_t> &bytes, unsigned stride, unsigned type, const GameObjectRef &ref) {
    for (std::size_t offset = 4; offset < bytes.size(); offset += stride) {
        if (bytes[offset + 5] == type && SameRef(CollectionRef(bytes, offset), ref)) { return offset; }
    }
    return bytes.size();
}

std::size_t EnsureRecord(std::vector<std::uint8_t> &bytes, unsigned stride, unsigned type, const GameObjectRef &ref) {
    const std::size_t offset = FindRecord(bytes, stride, type, ref);
    if (offset != bytes.size()) { return offset; }
    const unsigned count = Get32(bytes, 0);
    bytes.resize(offset + stride, 0);
    Put32(bytes, 0, count + 1);
    Put32(bytes, offset, ref.packHash);
    bytes[offset + 4] = ref.localIndex;
    bytes[offset + 5] = static_cast<std::uint8_t>(type);
    bytes[offset + 6] = 1; // Collection::WriteSavedData :81309 changes dirty 0 to local 1.
    return offset;
}

}
