#pragma once
/** Windows file access only; native DataStore payloads belong to their clients. */
#include <filesystem>
#include <vector>
#include <cstdint>
namespace ZProfileFiles {
bool Read(const std::filesystem::path &path, std::vector<std::uint8_t> &bytes);
bool Replace(const std::filesystem::path &path, const std::vector<std::uint8_t> &bytes);
bool AllowsDestination(const std::filesystem::path &destination, const std::filesystem::path &source);
}
