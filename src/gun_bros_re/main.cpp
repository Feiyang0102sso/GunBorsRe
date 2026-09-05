/**
 * @file main.cpp
 * @brief M1 milestone harness: open a .big archive and dump its contents.
 *
 * Usage: gun_bros_re [archive_name]
 *        archive_name defaults to pack0_core_xga.big and is looked up under
 *        ASSET_ROOT/big/.
 */

#include "engine/CBigFileReader.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

// How many resources to list in full before collapsing into the summary.
constexpr int kMaxListedResources = 40;

// How many resources to decompress and magic-check.
constexpr int kSampleCount = 20;

/** Build table2 index -> logical ID by walking the table1 runs. */
std::map<std::uint32_t, std::uint32_t> BuildIndexToIdMap(const CBigFileReader &reader) {
    std::map<std::uint32_t, std::uint32_t> indexToId;
    for (const BigTable1Range &range : reader.GetTable1()) {
        for (std::uint16_t offset = 0; offset < range.rangeLength; ++offset) {
            const std::uint32_t index = range.table2StartIndex + offset;
            const std::uint32_t id = range.baseResourceId + offset;
            indexToId[index] = id;
        }
    }
    return indexToId;
}

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
    if (data[0] == 0x03) {
        return "mesh? (magic 03)";
    }
    return "raw";
}

}  // namespace

int main(int argc, char **argv) {
    const std::string archiveName = (argc > 1) ? argv[1] : "pack0_core_xga.big";
    const std::string path = std::string(ASSET_ROOT) + "/big/" + archiveName;

    CBigFileReader reader;
    if (!reader.Open(path)) {
        return 1;
    }

    const std::map<std::uint32_t, std::uint32_t> indexToId = BuildIndexToIdMap(reader);

    std::printf("\ntable1 ranges: %zu\n", reader.GetTable1().size());
    for (const BigTable1Range &range : reader.GetTable1()) {
        std::printf("  ID 0x%X..0x%X -> table2 %u..%u  (%u entries)\n",
                    range.baseResourceId,
                    range.baseResourceId + range.rangeLength - 1,
                    range.table2StartIndex,
                    range.table2StartIndex + range.rangeLength - 1,
                    range.rangeLength);
    }

    // --- listing ---
    std::printf("\n%-6s %-8s %-12s %-10s %s\n", "idx", "id", "group", "blockSize", "compressed");
    const std::uint32_t total = reader.GetResourceCount();
    for (std::uint32_t i = 0; i < total && i < kMaxListedResources; ++i) {
        const std::uint32_t groupHash = reader.GetGroupHash(i);
        const std::uint32_t blockSize = reader.GetBlockSize(i);

        std::string idText = "-";
        const auto found = indexToId.find(i);
        if (found != indexToId.end()) {
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "0x%X", found->second);
            idText = buffer;
        }

        std::printf("%-6u %-8s %-12s %-10u\n",
                    i, idText.c_str(), BigGroupName(groupHash), blockSize);
    }
    if (total > kMaxListedResources) {
        std::printf("... %u more\n", total - kMaxListedResources);
    }

    // --- group census ---
    std::map<std::uint32_t, std::uint32_t> groupCounts;
    for (std::uint32_t i = 0; i < total; ++i) {
        groupCounts[reader.GetGroupHash(i)]++;
    }
    std::printf("\ngroup census:\n");
    for (const auto &pair : groupCounts) {
        std::printf("  %-12s 0x%08X  %u\n",
                    BigGroupName(pair.first), pair.first, pair.second);
    }

    // --- sampled decompression ---
    std::printf("\nsampling %d resources:\n", kSampleCount);
    const std::uint32_t stride = (total > kSampleCount) ? (total / kSampleCount) : 1;
    int okCount = 0;
    int failCount = 0;
    for (std::uint32_t i = 0; i < total; i += stride) {
        std::vector<std::uint8_t> payload;
        if (!reader.GetResourceByIndex(i, payload)) {
            std::printf("  [%4u] FAILED\n", i);
            failCount++;
            continue;
        }
        okCount++;
        std::printf("  [%4u] %-12s %8zu bytes  %s\n",
                    i, BigGroupName(reader.GetGroupHash(i)),
                    payload.size(), DetectMagic(payload));
    }

    std::printf("\nsample result: %d ok, %d failed\n", okCount, failCount);

    // --- string pack must sit at logical ID 1 / table2[0] ---
    std::uint32_t stringPackIndex = 0;
    if (reader.ResolveResourceId(kStringPackResourceId, stringPackIndex)) {
        std::printf("string pack: ID 1 -> table2[%u], group=%s\n",
                    stringPackIndex, BigGroupName(reader.GetGroupHash(stringPackIndex)));
    } else {
        std::printf("string pack: ID 1 did not resolve\n");
    }

    return failCount == 0 ? 0 : 1;
}
