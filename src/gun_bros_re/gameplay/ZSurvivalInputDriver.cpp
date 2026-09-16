/** @file ZSurvivalInputDriver.cpp
 * @brief Optional research hook; ordinary gameplay uses platform input.
 */
#include "gun_bros_re/gameplay/ZSurvivalInputDriver.h"
#include <cstdio>
namespace {
ZSurvivalInputFactory inputFactory = nullptr;
}
void SetSurvivalInputFactory(ZSurvivalInputFactory factory) { inputFactory = factory; }
std::unique_ptr<ZSurvivalInputDriver> CreateSurvivalInputDriver(CLevel &scene, const ZMapRectangle &bounds) {
    if (inputFactory == nullptr) {
        std::printf("[input] no external driver installed; using platform input\n");
        return nullptr;
    }
    return inputFactory(scene, bounds);
}
