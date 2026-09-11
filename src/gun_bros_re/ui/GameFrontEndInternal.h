#pragma once
#include "gun_bros_re/ui/GameFrontEnd.h"
int RunGameMenuSession(const std::string &bigDirectory, const std::string &screenshotPath = "", unsigned page = 0,
    bool originalProfile = false, const std::string &profilePath = "", CWindow *sharedWindow = nullptr);
