#include "ui/GameMenuStudy.h"
#include "gun_bros_re/ui/host/ZGameFrontEndInternal.h"

int RunGameMenuStudy(const std::string &bigDirectory, const std::string &screenshotPath, unsigned page,
    bool originalProfile, const std::string &profilePath, ZWindow *sharedWindow) {
    return RunGameMenuSession(bigDirectory, screenshotPath, page, originalProfile, profilePath, sharedWindow);
}
