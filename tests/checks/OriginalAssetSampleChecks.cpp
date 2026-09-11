/** Original M1 acceptance sample, intentionally tied to the reference archives. */
#include "engine/resources/CResTOCManager.h"
#include <cstdio>
#include <cstring>

namespace {
// A known reference from _Big_tool/refs.md: pack1, one of the level names.
constexpr std::uint32_t kSampleAssetPackHash = 0x00267581u;
constexpr std::uint32_t kSampleAssetHandle = 0x21FF02FEu;
const char *const kSampleAssetExpectedText = "Colony Test";

}

int RunOriginalAssetSampleCheck(const std::string &bigDirectory) {
    CResTOCManager tocManager;
    if (!tocManager.Init(bigDirectory, kArtSetXga) || !tocManager.Bind()) { return 1; }
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
    std::printf("%.*s", static_cast<int>(payload.size()), reinterpret_cast<const char *>(payload.data()));
    std::printf("\"\n");

    const bool matches =
        payload.size() == std::strlen(kSampleAssetExpectedText) + 1 &&
        std::memcmp(payload.data(), kSampleAssetExpectedText,
                    std::strlen(kSampleAssetExpectedText)) == 0;
    std::printf("  expected \"%s\": %s\n",
                kSampleAssetExpectedText, matches ? "MATCH" : "MISMATCH");

    return matches ? 0 : 1;
}
