/** Native DataStore responsibilities; see saves/GB_save_profile.bt and save_payloads.bt. */
#include "gun_bros_re/data/profile/CProfileManagerStorage.h"
#include "gun_bros_re/data/profile/CDailyBonusTracking.h"
using namespace ProfileStorageDetail;

/** Native client 1009, CDailyBonusTracking::CommitBonus :209606. */
void CDailyBonusTracking::ReadProfileData(CProfileManager &candidate, const std::vector<std::uint8_t> &bytes) {
    candidate.dailyLastLaunchSeconds = Get32(bytes, 0);
    candidate.dailyConsecutiveSeconds = Get32(bytes, 4);
    candidate.dailyLastCommit = Get32(bytes, 8);
    candidate.dailyConsecutiveDays = candidate.dailyConsecutiveSeconds / 86400 + 1;
    candidate.dailyLastClaimDay = -1; // Never reinterpret the native seconds as a host date.
}

void CDailyBonusTracking::WriteProfileData(const CProfileManager &profile, std::vector<std::uint8_t> &bytes) {
    Put32(bytes, 0, profile.dailyLastLaunchSeconds);
    Put32(bytes, 4, profile.dailyConsecutiveSeconds);
    Put32(bytes, 8, profile.dailyLastCommit);
}
