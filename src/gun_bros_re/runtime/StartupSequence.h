/** @file StartupSequence.h
 * @brief Glu video startup and a permanent original-media validation harness.
 */
#ifndef GUN_BROS_RE_STARTUPSEQUENCE_H
#define GUN_BROS_RE_STARTUPSEQUENCE_H
#include <string>
class CWindow;
class CTexture;
// Original iOS bundle launch image, shared by video handoff and startup loading.
bool LoadStartupSplash(CTexture &texture);
int RunStartupSequence(const std::string &screenshotPath = "", unsigned advanceMs = 0, CWindow *sharedWindow = nullptr);
int RunMediaCheck();
#endif
