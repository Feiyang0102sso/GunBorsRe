#pragma once
#include "gun_bros_re/ui/host/ZGameFrontEnd.h"
/** Development-only entry points for page selection and screenshots. */
int RunGameMenuStudy(const std::string &bigDirectory, const std::string &screenshotPath = "", unsigned page = 0,
    bool originalProfile = false, const std::string &profilePath = "", ZWindow *sharedWindow = nullptr);
