/** @file ZGameFrontEnd.h
 * @brief Desktop mission, equipment and refinery entry points.
 */
#ifndef GUN_BROS_RE_ZGAMEFRONTEND_H
#define GUN_BROS_RE_ZGAMEFRONTEND_H
#include <string>
class ZWindow;
/** The production entry accepts only resources, accounts, and a shared window. */
int RunGameFrontEnd(const std::string &bigDirectory, bool originalProfile = false,
    const std::string &profilePath = "", ZWindow *sharedWindow = nullptr);
#endif
