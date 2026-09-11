/** @file MovieStudy.h
 * @brief Permanent access to the original UI movie data and rendering.
 */
#ifndef GUN_BROS_RE_MOVIESTUDY_H
#define GUN_BROS_RE_MOVIESTUDY_H
#include <string>
int RunMovieStudy(const std::string &bigDirectory, unsigned ordinal, const std::string &screenshotPath, unsigned advanceMs,
    bool gallery = false, bool regionOverlay = false);
#endif
