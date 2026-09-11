/** @file ResearchLauncher.cpp
 * @brief Starts optional companion tools without linking milestone code into the game.
 */
#define NOMINMAX
#include "gun_bros_re/ViewerLauncher.h"
#include "engine/core/Paths.h"
#include <Windows.h>
#include <cstdio>
#include <filesystem>
#include <string>

int LaunchViewer(const wchar_t *arguments, bool interactive) {
    const auto tool = Paths::Root() / Paths::ExecutableFilename(Paths::ViewerName);
    if (!std::filesystem::exists(tool)) {
        std::fprintf(stderr, "[research] missing companion: %s\n", tool.u8string().c_str());
        if (interactive) { MessageBoxW(nullptr, L"The viewer is not installed. Build GunBrosViewer.vcxproj.", L"Gun Bros research tools", MB_OK | MB_ICONINFORMATION); }
        return 1;
    }
    std::wstring command = L"\"" + tool.wstring() + L"\" " + arguments;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    DWORD flags = CREATE_NEW_CONSOLE;
    if (!interactive) {
        flags = CREATE_NO_WINDOW;
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        SetHandleInformation(startup.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        SetHandleInformation(startup.hStdError, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    }
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(tool.c_str(), command.data(), nullptr, nullptr, !interactive, flags, nullptr, nullptr, &startup, &process)) {
        std::fprintf(stderr, "[research] launch failed error=%lu\n", GetLastError());
        return 1;
    }
    std::printf("[research] started process=%lu interactive=%u\n", process.dwProcessId, interactive);
    CloseHandle(process.hThread);
    DWORD code = 0;
    if (!interactive) {
        WaitForSingleObject(process.hProcess, INFINITE);
        GetExitCodeProcess(process.hProcess, &code);
    }
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
}
