#include "ui/GameMenuStudy.h"
#include "gun_bros_re/ui/GameFrontEndInternal.h"

int RunGameMenuStudy(const std::string &bigDirectory, const std::string &screenshotPath, unsigned page,
    bool originalProfile, const std::string &profilePath, CWindow *sharedWindow) {
    return RunGameMenuSession(bigDirectory, screenshotPath, page, originalProfile, profilePath, sharedWindow);
}
