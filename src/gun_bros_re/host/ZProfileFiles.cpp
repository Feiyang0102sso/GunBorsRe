#define NOMINMAX
#include "gun_bros_re/host/ZProfileFiles.h"
#include <Windows.h>
#include <fstream>
#include <iterator>
#include <cstdio>
namespace ZProfileFiles {
bool Read(const std::filesystem::path &path, std::vector<std::uint8_t> &bytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input) { return false; }
    bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return !input.bad();
}
bool Replace(const std::filesystem::path &path, const std::vector<std::uint8_t> &bytes) {
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) { return false; }
    output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    output.close();
    if (!output || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { return false; }
    return true;
}
bool AllowsDestination(const std::filesystem::path &destination, const std::filesystem::path &source) {
    // Explicitly imported originals remain read-only, even in a copied account.
    const auto relative = destination.lexically_relative(source);
    if (!source.empty() && (destination == source || (!relative.empty() && *relative.begin() != ".."))) {
        std::printf("[native-profile] source archive is read-only: %s\n", destination.string().c_str());
        return false;
    }
    return true;
}
}
