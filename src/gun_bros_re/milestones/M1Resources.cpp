/**
 * @file M1Resources.cpp
 * @brief M1 milestone harness: walk the whole cross-pack addressing chain.
 *
 * Proves the four foundations are in place:
 *   packTOC_xga.dat  ->  which packs exist and where their TOCs are
 *   CStringToKey     ->  names and pack identities hash correctly
 *   pack TOC         ->  name -> handle
 *   handle           ->  table1 -> table2 -> block -> zlib, or into an aggregate
 */

#include "milestones/M1Resources.h"

#include "engine/CStringToKey.h"
#include "gun_bros/CResTOCManager.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

// A known reference from _Big_tool/refs.md: pack1, one of the level names.
constexpr std::uint32_t kSampleAssetPackHash = 0x00267581u;
constexpr std::uint32_t kSampleAssetHandle = 0x21FF02FEu;
const char *const kSampleAssetExpectedText = "Colony Test";

// How many resources per pack to decompress in the sampling pass.
constexpr int kSamplesPerPack = 20;

/** Identify a payload by its leading bytes. */
const char *DetectMagic(const std::vector<std::uint8_t> &data) {
    if (data.empty()) {
        return "(empty)";
    }
    if (data.size() >= 8 && std::memcmp(data.data(), "\x89PNG\r\n\x1a\n", 8) == 0) {
        return "PNG";
    }
    if (data.size() >= 12 && std::memcmp(data.data(), "RIFF", 4) == 0 &&
        std::memcmp(data.data() + 8, "WAVE", 4) == 0) {
        return "RIFF/WAVE";
    }
    return "raw";
}

/** Print a payload as text when it looks like a NUL-terminated ASCII string. */
bool PrintAsText(const std::vector<std::uint8_t> &data) {
    if (data.empty() || data.size() > 256) {
        return false;
    }
    for (std::size_t i = 0; i + 1 < data.size(); ++i) {
        const std::uint8_t byte = data[i];
        if (byte < 0x20 || byte > 0x7E) {
            return false;
        }
    }
    std::printf("%.*s", static_cast<int>(data.size() - 1),
                reinterpret_cast<const char *>(data.data()));
    return true;
}

/** Verify the hash against values cross-checked against the original binary. */
bool CheckStringToKey() {
    struct HashCase {
        const char *text;
        std::uint32_t expected;
    };
    // pack0/pack1/pack9 are documented in _Big_tool/refs.md; the rest were
    // confirmed against packTOC_xga.dat.
    const HashCase cases[] = {
        {"pack0", 0x00267580u},
        {"pack1", 0x00267581u},
        {"pack9", 0x00267589u},
        {"pack10", 0x01675820u},
        {"pack11", 0x01675821u},
        {"pack12", 0x01675822u},
        {"pack0_core", 0x58595522u},
    };

    bool allPassed = true;
    for (const HashCase &testCase : cases) {
        const std::uint32_t actual = CStringToKey(testCase.text);
        const bool passed = (actual == testCase.expected);
        if (!passed) {
            allPassed = false;
            std::printf("  FAIL %-12s got 0x%08X want 0x%08X\n",
                        testCase.text, actual, testCase.expected);
        }
    }
    std::printf("CStringToKey: %s (%zu cases)\n",
                allPassed ? "ok" : "FAILED", sizeof(cases) / sizeof(cases[0]));
    return allPassed;
}

/** Resolve every handle in a pack and report how many came back with data. */
void SurveyPack(CResPackTOC &pack) {
    const std::vector<PackTOCEntry> &entries = pack.GetEntries();

    int aggregateCount = 0;
    int directCount = 0;
    int nullCount = 0;
    int okCount = 0;
    int failCount = 0;

    std::vector<std::uint32_t> failedHandles;

    std::vector<std::uint8_t> payload;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const std::uint32_t handle = entries[i].handle;

        // A handle whose ID bits are zero is a null reference, not a target.
        if ((handle & kHandleIdMask) == 0) {
            nullCount++;
            continue;
        }

        if ((handle & kHandleAggregateFlag) != 0) {
            aggregateCount++;
        } else {
            directCount++;
        }

        if (pack.GetResource(handle, payload)) {
            okCount++;
        } else {
            failCount++;
            failedHandles.push_back(handle);
        }
    }

    std::printf("  %-16s %4zu entries | %4d aggregate %4d direct %3d null | "
                "resolved %4d, failed %3d\n",
                pack.GetShortName().c_str(), entries.size(),
                aggregateCount, directCount, nullCount, okCount, failCount);

    // The failures are the interesting part -- name them rather than let a
    // count hide what is still unaddressable.
    for (std::size_t i = 0; i < failedHandles.size(); ++i) {
        const std::uint32_t handle = failedHandles[i];
        const bool aggregate = (handle & kHandleAggregateFlag) != 0;
        std::printf("      unresolved 0x%08X  id=%-6u %s\n",
                    handle, handle & kHandleIdMask,
                    aggregate ? "(aggregate)" : "(direct)");
    }
}

/** Decompress a spread of resources and report what they look like. */
void SamplePack(CResPackTOC &pack) {
    CBigFileReader &reader = pack.GetReader();
    const std::uint32_t total = reader.GetResourceCount();
    if (total == 0) {
        return;
    }

    const std::uint32_t stride = (total > kSamplesPerPack) ? (total / kSamplesPerPack) : 1;

    std::map<std::string, int> magicCounts;
    int failCount = 0;
    std::vector<std::uint8_t> payload;
    for (std::uint32_t i = 0; i < total; i += stride) {
        if (!reader.GetResourceByIndex(i, payload)) {
            failCount++;
            continue;
        }
        magicCounts[DetectMagic(payload)]++;
    }

    std::printf("  %-16s sampled:", pack.GetShortName().c_str());
    for (const auto &pair : magicCounts) {
        std::printf(" %s=%d", pair.first.c_str(), pair.second);
    }
    if (failCount > 0) {
        std::printf("  FAILED=%d", failCount);
    }
    std::printf("\n");
}

/**
 * List every resource of one pack: table2 index, logical ID, group, block size
 * and compression flag. Logical IDs come from walking the table1 runs backwards.
 */
void DumpPack(CResPackTOC &pack) {
    CBigFileReader &reader = pack.GetReader();

    // table2 index -> logical ID, built by expanding the run-length table1.
    std::map<std::uint32_t, std::uint32_t> indexToId;
    for (std::size_t i = 0; i < reader.GetTable1().size(); ++i) {
        const BigTable1Range &range = reader.GetTable1()[i];
        for (std::uint16_t offset = 0; offset < range.rangeLength; ++offset) {
            indexToId[range.table2StartIndex + offset] = range.baseResourceId + offset;
        }
    }

    std::printf("\n=== %s: %u resources ===\n",
                pack.GetFullName().c_str(), reader.GetResourceCount());
    std::printf("%-6s %-8s %-12s %-10s %s\n",
                "idx", "id", "group", "blockSize", "compression");

    for (std::uint32_t i = 0; i < reader.GetResourceCount(); ++i) {
        char idText[16] = "-";
        const std::map<std::uint32_t, std::uint32_t>::const_iterator found =
            indexToId.find(i);
        if (found != indexToId.end()) {
            std::snprintf(idText, sizeof(idText), "%u", found->second);
        }

        const std::uint8_t compression = reader.GetCompressionFlag(i);
        std::printf("%-6u %-8s %-12s %-10u %s\n",
                    i, idText, BigGroupName(reader.GetGroupHash(i)),
                    reader.GetBlockSize(i),
                    (compression == kResourceZlib) ? "zlib" : "none");
    }
}

}  // namespace

int RunM1Resources(const std::string &bigDirectory) {
    std::printf("=== M1: resource addressing ===\n\n");

    if (!CheckStringToKey()) {
        return 1;
    }

    CResTOCManager tocManager;
    if (!tocManager.Init(bigDirectory, kArtSetXga)) {
        return 1;
    }

    std::printf("\n--- binding packs ---\n");
    if (!tocManager.Bind()) {
        std::printf("one or more packs failed to bind\n");
        return 1;
    }

    std::printf("\n--- handle survey ---\n");
    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        SurveyPack(*tocManager.GetPack(static_cast<int>(i)));
    }

    std::printf("\n--- sampled decompression ---\n");
    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        SamplePack(*tocManager.GetPack(static_cast<int>(i)));
    }

    // --- the acceptance target ---
    std::printf("\n--- CGameAssetRef 0x%08X 0x%08X ---\n",
                kSampleAssetPackHash, kSampleAssetHandle);

    const int packIndex = tocManager.GetPackIndexFromHash(kSampleAssetPackHash);
    std::printf("  pack hash resolves to %s\n",
                tocManager.GetPack(packIndex)->GetShortName().c_str());

    std::vector<std::uint8_t> payload;
    if (!tocManager.GetAsset(kSampleAssetPackHash, kSampleAssetHandle, payload)) {
        std::printf("  FAILED to fetch\n");
        return 1;
    }

    std::printf("  %zu bytes: \"", payload.size());
    PrintAsText(payload);
    std::printf("\"\n");

    const bool matches =
        payload.size() == std::strlen(kSampleAssetExpectedText) + 1 &&
        std::memcmp(payload.data(), kSampleAssetExpectedText,
                    std::strlen(kSampleAssetExpectedText)) == 0;
    std::printf("  expected \"%s\": %s\n",
                kSampleAssetExpectedText, matches ? "MATCH" : "MISMATCH");

    return matches ? 0 : 1;
}

int RunPackDump(const std::string &bigDirectory, const std::string &packShortName) {
    CResTOCManager tocManager;
    if (!tocManager.Init(bigDirectory, kArtSetXga)) {
        return 1;
    }

    // An unknown hash falls through to pack 0, so confirm we got what we asked
    // for rather than silently dumping the core pack.
    const int packIndex = tocManager.GetPackIndexFromName(packShortName.c_str());
    CResPackTOC *pack = tocManager.GetPack(packIndex);
    if (pack == nullptr || pack->GetShortName() != packShortName) {
        std::printf("no pack named %s\n", packShortName.c_str());
        return 1;
    }

    if (!tocManager.Bind()) {
        return 1;
    }
    DumpPack(*pack);
    return 0;
}
