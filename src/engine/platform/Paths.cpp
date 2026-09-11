#define NOMINMAX
#include <Windows.h>
#include "engine/core/Paths.h"
#include <stdexcept>
#include <vector>

namespace Paths {
const std::filesystem::path &Root() {
    static const std::filesystem::path directory = [] {
        // Windows extended paths may exceed MAX_PATH, including Unicode names.
        std::vector<wchar_t> buffer(32768);
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size()) {
            throw std::runtime_error("Cannot resolve executable directory");
        }
        return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
    }();
    return directory;
}

std::filesystem::path Resolve(const std::filesystem::path &path) {
    if (path.is_absolute()) { return path.lexically_normal(); }
    return (Root() / path).lexically_normal();
}

const std::string &Shaders() {
    static const std::string directory = (Root() / ShaderDirectory).u8string();
    return directory;
}
}
