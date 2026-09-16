#define NOMINMAX
#include <Windows.h>
#include "engine/platform/ZStartup.h"
#include <cstdio>
#include <io.h>
#include <stdexcept>

ZUtf8Arguments::ZUtf8Arguments(int count, wchar_t **values) {
    for (int index = 0; index < count; ++index) {
        const int length = WideCharToMultiByte(CP_UTF8, 0, values[index], -1, nullptr, 0, nullptr, nullptr);
        if (length <= 0) { throw std::runtime_error("Invalid command line encoding"); }
        std::string value(length, '\0');
        WideCharToMultiByte(CP_UTF8, 0, values[index], -1, value.data(), length, nullptr, nullptr);
        value.pop_back();
        m_values.push_back(std::move(value));
    }
    for (auto &value : m_values) { m_pointers.push_back(value.data()); }
}

/** Retain shell redirection; Explorer launches get a file and no console. */
void OpenProductLog(const char *productName) {
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output != nullptr && output != INVALID_HANDLE_VALUE && GetFileType(output) != FILE_TYPE_UNKNOWN) { return; }
    const auto directory = Paths::Root() / Paths::LogDirectory;
    std::filesystem::create_directories(directory);
    const auto file = directory / (std::string(productName) + ".log");
    FILE *opened = nullptr;
    if (_wfreopen_s(&opened, file.c_str(), L"w", stdout) == 0) {
        _dup2(_fileno(stdout), _fileno(stderr));
        SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout))));
        SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr))));
    }
}
