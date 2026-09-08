/** @file StartupSequence.h
 * @brief Glu video startup and a permanent original-media validation harness.
 */
#ifndef GUN_BROS_RE_STARTUPSEQUENCE_H
#define GUN_BROS_RE_STARTUPSEQUENCE_H
#include <string>
int RunStartupSequence(const std::string &screenshotPath = "", unsigned advanceMs = 0);
int RunMediaCheck();
#endif
