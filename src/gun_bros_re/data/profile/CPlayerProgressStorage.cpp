/** Native DataStore responsibilities; see saves/GB_save_profile.bt and save_payloads.bt. */
#include "gun_bros_re/data/profile/CProfileManagerStorage.h"
using namespace ProfileStorageDetail;

void CPlayerProgress::ReadProfileData(CProfileManager &candidate, const std::vector<std::uint8_t> &bytes) {
    CArrayInputStream progress(bytes);
    // EnterShell :79673 selects first-game flow from this original flag.
    candidate.firstLaunch = progress.ReadUInt8() != 0;
    candidate.pushChallenges = bytes[46] != 0;
    candidate.tutorialCompleted = false;
    candidate.tutorialSteps = 0; // Host trace, not the 22 original menu-tip flags.
    progress.Skip(3);
    candidate.xplodium = Get64(progress);
    candidate.coins = Get64(progress);
    candidate.warbucks = progress.ReadUInt32();
    candidate.experience = Get64(progress);
}

void CPlayerProgress::WriteProfileData(const CProfileManager &profile, std::vector<std::uint8_t> &progress, const Template &progression, std::uint64_t loadedExperience) {
    progress[0] = profile.firstLaunch;
    progress[46] = profile.pushChallenges;
    Put64(progress, 4, profile.xplodium);
    Put64(progress, 12, profile.coins);
    Put32(progress, 20, static_cast<unsigned>(profile.warbucks));
    Put64(progress, 24, profile.experience);
    if (profile.experience != loadedExperience) {
        CPlayerProgress value;
        value.Bind(progression);
        value.SetExperience(profile.experience);
        Put16(progress, 32, value.GetLevel());
    }
}
