/** Built on demand in Debug; reuses the same command routing and implementations. */
#include "engine/platform/ZStartup.h"
#include "TestApplication.h"
#include "Checks.h"
#include "engine/platform/ZAudioPlayer.h"
#include <string>
#include "gameplay/CampaignDoorChecks.h"
#include "gameplay/DebugMapChecks.h"
#include "TestOutput.h"
int CheckDebugInput();
int RunDeathmatchDataCheck(const std::string &bigDirectory);
int CheckViewerControls();
int RunCoverScaleStudy(const std::string &bigDirectory);
int RunMapTurretChecks(const std::string &bigDirectory);
int RunMapResourceChecks(const std::string &bigDirectory);
int RunBigVersionCheck();
int RunGameplayOwnershipCheck();
int RunHavenCollisionCheck(const std::string &bigDirectory, bool artillery, bool disableFlock = false);
int RunHavenMuzzleCheck(const std::string &bigDirectory);
int RunOriginalAssetSampleCheck(const std::string &bigDirectory);
int wmain(int argc, wchar_t **argv) {
    auto sampleBigDirectory = Paths::Root() / Paths::BigDirectory;
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::wstring(argv[index]) == L"--fixtures") { TestOutput::fixtureDirectory = argv[index + 1]; }
        if (std::wstring(argv[index]) == L"--test-output") { TestOutput::Configure(argv[index + 1]); }
        if (std::wstring(argv[index]) == L"--big") { sampleBigDirectory = Paths::Resolve(argv[index + 1]); }
    }
    for (int index = 1; index < argc; ++index) {
        if (std::wstring(argv[index]) == L"--haven-collision-check" || std::wstring(argv[index]) == L"--haven-artillery-check") {
            ZAudioPlayer::SetMuted(true);
            return RunHavenCollisionCheck(sampleBigDirectory.u8string(), std::wstring(argv[index]) == L"--haven-artillery-check");
        }
        if (std::wstring(argv[index]) == L"--haven-muzzle-check") {
            ZAudioPlayer::SetMuted(true);
            return RunHavenMuzzleCheck(sampleBigDirectory.u8string());
        }
        if (std::wstring(argv[index]) == L"--haven-artillery-no-flock-check") {
            ZAudioPlayer::SetMuted(true);
            return RunHavenCollisionCheck(sampleBigDirectory.u8string(), true, true);
        }
        if (std::wstring(argv[index]) == L"--gameplay-ownership-check") {
            ZAudioPlayer::SetMuted(true);
            return RunGameplayOwnershipCheck();
        }
        if (std::wstring(argv[index]) == L"--resource-loading-check") {
            ZAudioPlayer::SetMuted(true);
            return RunResourceLoadingCheck(sampleBigDirectory.u8string());
        }
        if (std::wstring(argv[index]) == L"--deathmatch-data-check") {
            ZAudioPlayer::SetMuted(true);
            return RunDeathmatchDataCheck(sampleBigDirectory.u8string());
        }
        if (std::wstring(argv[index]) == L"--deathmatch-check" || std::wstring(argv[index]) == L"--deathmatch-feedback-check") {
            int RunDeathmatchCombatCheck(const std::string &, bool);
            ZAudioPlayer::SetMuted(true);
            return RunDeathmatchCombatCheck(sampleBigDirectory.u8string(), std::wstring(argv[index]) == L"--deathmatch-feedback-check");
        }
        if (std::wstring(argv[index]) == L"--map-resources-check") {
            ZAudioPlayer::SetMuted(true);
            return RunMapResourceChecks(sampleBigDirectory.u8string());
        }
        if (std::wstring(argv[index]) == L"--map-turret-check") {
            ZAudioPlayer::SetMuted(true);
            return RunMapTurretChecks(sampleBigDirectory.u8string());
        }
        if (std::wstring(argv[index]) == L"--cover-scale-study") {
            ZAudioPlayer::SetMuted(true);
            return RunCoverScaleStudy(sampleBigDirectory.u8string());
        }
        if (std::wstring(argv[index]) == L"--viewer-controls-check") {
            ZAudioPlayer::SetMuted(true);
            return CheckViewerControls();
        }
        if (std::wstring(argv[index]) == L"--big-version-check") {
            ZAudioPlayer::SetMuted(true);
            return RunBigVersionCheck();
        }
        if (std::wstring(argv[index]) == L"--asset-sample-check") {
            ZAudioPlayer::SetMuted(true);
            return RunOriginalAssetSampleCheck(sampleBigDirectory.u8string());
        }
        if (std::wstring(argv[index]) == L"--debug-map-profile-check") {
            ZAudioPlayer::SetMuted(true);
            return RunDebugMapProfileCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-content-check") {
            ZAudioPlayer::SetMuted(true);
            return RunCampaignContentCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-door-check") {
            ZAudioPlayer::SetMuted(true);
            return RunCampaignDoorCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-target-check") {
            ZAudioPlayer::SetMuted(true);
            return RunCampaignTargetCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-progression-check") {
            ZAudioPlayer::SetMuted(true);
            return RunCampaignProgressionCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-rescue-check") {
            ZAudioPlayer::SetMuted(true);
            return RunCampaignRescueCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-portal-check") {
            ZAudioPlayer::SetMuted(true);
            return RunCampaignPortalCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-cache-check") {
            ZAudioPlayer::SetMuted(true);
            return RunCampaignCacheCheck();
        }
        if (std::wstring(argv[index]) == L"--campaign-lava2-check") {
            ZAudioPlayer::SetMuted(true);
            return RunCampaignLava2Check();
        }
        if (std::wstring(argv[index]) == L"--debug-input-check") {
            ZAudioPlayer::SetMuted(true);
            return CheckDebugInput();
        }
    }
    ZUtf8Arguments arguments(argc, argv);
    return RunTestApplication(arguments.Count(), arguments.Data());
}
