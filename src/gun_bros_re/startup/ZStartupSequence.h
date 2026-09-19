/** @file ZStartupSequence.h
 * @brief Glu video startup and a permanent original-media validation harness.
 */
#ifndef GUN_BROS_RE_ZSTARTUPSEQUENCE_H
#define GUN_BROS_RE_ZSTARTUPSEQUENCE_H
#include <string>
class ZWindow;
class ZTexture;
// Original iOS bundle launch image, shared by video handoff and startup loading.
bool LoadStartupSplash(ZTexture &texture);
int RunStartupSequence(const std::string &screenshotPath = "", unsigned advanceMs = 0, ZWindow *sharedWindow = nullptr);
#endif
