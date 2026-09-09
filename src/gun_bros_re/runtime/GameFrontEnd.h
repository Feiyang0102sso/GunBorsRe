/** @file GameFrontEnd.h
 * @brief Desktop mission, equipment and refinery entry points.
 */
#ifndef GUN_BROS_RE_GAMEFRONTEND_H
#define GUN_BROS_RE_GAMEFRONTEND_H
#include <string>
int RunProfilePlayCheck(const std::string &bigDirectory);
int RunTutorialPlayCheck(const std::string &bigDirectory);
int RunGameMenuCheck(const std::string &bigDirectory);
/** Isolated store template playback, hit testing and screenshot acceptance. */
int RunStoreTemplateCheck(const std::string &bigDirectory, bool cardsOnly = false);
int RunGameFrontEnd(const std::string &bigDirectory, const std::string &screenshotPath = "", unsigned page = 0,
    bool originalProfile = false, const std::string &profilePath = "");
#endif
