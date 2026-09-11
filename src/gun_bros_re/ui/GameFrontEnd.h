/** @file GameFrontEnd.h
 * @brief Desktop mission, equipment and refinery entry points.
 */
#ifndef GUN_BROS_RE_GAMEFRONTEND_H
#define GUN_BROS_RE_GAMEFRONTEND_H
#include <string>
class CWindow;
/** The production entry accepts only resources, accounts, and a shared window. */
int RunGameFrontEnd(const std::string &bigDirectory, bool originalProfile = false,
    const std::string &profilePath = "", CWindow *sharedWindow = nullptr);
#endif

