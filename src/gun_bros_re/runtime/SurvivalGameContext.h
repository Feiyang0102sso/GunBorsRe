/** @file SurvivalGameContext.h
 * @brief Persistent game session settings; absent in permanent research harnesses.
 */
#ifndef GUN_BROS_RE_SURVIVALGAMECONTEXT_H
#define GUN_BROS_RE_SURVIVALGAMECONTEXT_H
#include "gun_bros/CProfileManager.h"

struct SurvivalGameContext {
    CProfileManager &profile;
    std::filesystem::path savePath;
    unsigned planet = 0;
};
#endif
