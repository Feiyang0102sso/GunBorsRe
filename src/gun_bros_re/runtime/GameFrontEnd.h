/** @file GameFrontEnd.h
 * @brief Desktop mission, equipment and refinery entry points.
 */
#ifndef GUN_BROS_RE_GAMEFRONTEND_H
#define GUN_BROS_RE_GAMEFRONTEND_H
#include <string>
int RunProfilePlayCheck(const std::string &bigDirectory);
int RunTutorialPlayCheck(const std::string &bigDirectory);
int RunGameMenuCheck(const std::string &bigDirectory);
int RunPackagePurchaseCheck(const std::string &bigDirectory);
/** Isolated store template playback, hit testing and screenshot acceptance. */
int RunStoreTemplateCheck(const std::string &bigDirectory, bool cardsOnly = false, bool bankOnly = false, bool feedbackOnly = false);
/** Original upgrade phases, resource mutations and isolated purchase/reload. */
int RunUpgradePopupCheck(const std::string &bigDirectory);
int RunOptionsCheck(const std::string &bigDirectory);
int RunSocialOfflineCheck(const std::string &bigDirectory);
int RunPlanetMenuCheck(const std::string &bigDirectory);
int RunMissionMenuCheck(const std::string &bigDirectory);
int RunNavigationBarCheck(const std::string &bigDirectory);
int RunRefineryMenuCheck(const std::string &bigDirectory);
int RunGreetingCheck(const std::string &bigDirectory);
int RunPlayerSelectCheck(const std::string &bigDirectory);
int RunPostGameMenuCheck(const std::string &bigDirectory);
int RunGameFrontEnd(const std::string &bigDirectory, const std::string &screenshotPath = "", unsigned page = 0,
    bool originalProfile = false, const std::string &profilePath = "");
#endif
