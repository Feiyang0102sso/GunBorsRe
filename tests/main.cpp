/** Built on demand in Debug; reuses the same command routing and implementations. */
#include "engine/platform/Startup.h"
#include "gun_bros_viewer/ViewerApplication.h"
#include "engine/platform/CAudioPlayer.h"
#include <string>
#include "gameplay/CampaignDoorChecks.h"
#include "gameplay/DebugMapChecks.h"
#include "TestOutput.h"
int CheckDebugInput();
int wmain(int argc, wchar_t **argv) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::wstring(argv[index]) == L"--fixtures") { TestOutput::fixtureDirectory = argv[index + 1]; }
        if (std::wstring(argv[index]) == L"--test-output") { TestOutput::Configure(argv[index + 1]); }
    }
    for (int index = 1; index < argc; ++index) {
        if (std::wstring(argv[index]) == L"--debug-map-profile-check") {
            CAudioPlayer::SetMuted(true);
            return RunDebugMapProfileCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-content-check") {
            CAudioPlayer::SetMuted(true);
            return RunCampaignContentCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-door-check") {
            CAudioPlayer::SetMuted(true);
            return RunCampaignDoorCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-target-check") {
            CAudioPlayer::SetMuted(true);
            return RunCampaignTargetCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-progression-check") {
            CAudioPlayer::SetMuted(true);
            return RunCampaignProgressionCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-rescue-check") {
            CAudioPlayer::SetMuted(true);
            return RunCampaignRescueCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-portal-check") {
            CAudioPlayer::SetMuted(true);
            return RunCampaignPortalCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-cache-check") {
            CAudioPlayer::SetMuted(true);
            return RunCampaignCacheCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-lava2-check") {
            CAudioPlayer::SetMuted(true);
            return RunCampaignLava2Check();
        }
        if (std::wstring(argv[index]) == L"--debug-input-check") {
            CAudioPlayer::SetMuted(true);
            return CheckDebugInput();
        }
    }
    Utf8Arguments arguments(argc, argv);
    return RunViewerApplication(arguments.Count(), arguments.Data());
}

