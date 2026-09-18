/** @file ZSurvivalInputDriver.cpp
 * @brief Optional research hook; ordinary gameplay uses platform input.
 */
#include "gun_bros_re/gameplay/ZSurvivalInputDriver.h"
#include <cstdio>
namespace {
ZSurvivalInputFactory inputFactory = nullptr;
}
void SetSurvivalInputFactory(ZSurvivalInputFactory factory) { inputFactory = factory; }
std::unique_ptr<ZSurvivalInputDriver> CreateSurvivalInputDriver(CLevel &scene, const CLayerCamera::Rectangle &bounds) {
    if (inputFactory == nullptr) {
        std::printf("[input] no external driver installed; using platform input\n");
        return nullptr;
    }
    return inputFactory(scene, bounds);
}

namespace MapDetail {
void AppendSurvivalShortcut(std::vector<ZKeyCode> &inputs, ZKeyCode key) {
    // Desktop binding policy: F/R are not gameplay shortcuts. Pointer Retry
    // and NextItem actions are dispatched separately and remain available.
    if (key == ZKeyCode::F || key == ZKeyCode::R) { return; }
    inputs.push_back(key);
}
}
