#include "gun_bros_re/data/ZProfileImportInternal.h"

namespace ProfileImportDetail {
unsigned CheckStorageBoundaries(const std::filesystem::path &source, const std::filesystem::path &output) {
    std::ifstream input(source, std::ios::binary);
    if (!input) {
        std::printf("[original-save-check] missing boundary fixture: %s\n", source.string().c_str());
        return 1;
    }
    const std::vector<std::uint8_t> original{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (original.size() < 536) { return 1; }
    unsigned failures = 0;
    for (unsigned test = 0; test < 5; ++test) {
        std::vector<std::uint8_t> bytes = original;
        if (test == 0) { bytes.resize(23); }
        if (test == 1) { bytes.pop_back(); }
        if (test == 2) { bytes[4] = 1; }
        if (test == 3) {
            for (unsigned index = 8; index < 12; ++index) { bytes[index] = 255; }
        }
        if (test == 4) { bytes.back() ^= 1; }
        const auto path = output / ("boundary-" + std::to_string(test) + ".dat");
        std::ofstream fixture(path, std::ios::binary);
        fixture.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        fixture.close();
        if (!fixture) { ++failures; continue; }
        ZImportedDataStore record;
        record.version = 123;
        const bool loaded = ReadDataStore(path, record);
        if (test < 4 && (loaded || record.version != 123)) { ++failures; }
        if (test == 4 && (!loaded || record.crcMatches)) { ++failures; }
    }
    std::printf("[original-save-check] malformed-copies=5 failures=%u\n", failures);
    return failures;
}
}
