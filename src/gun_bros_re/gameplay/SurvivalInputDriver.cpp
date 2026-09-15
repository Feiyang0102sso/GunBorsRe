/** @file SurvivalInputDriver.cpp
 * @brief Optional research hook; ordinary gameplay uses platform input.
 */
#include "gun_bros_re/gameplay/SurvivalInputDriver.h"
#include <cstdio>
namespace {
SurvivalInputFactory inputFactory = nullptr;
}
void SetSurvivalInputFactory(SurvivalInputFactory factory) { inputFactory = factory; }
std::unique_ptr<ISurvivalInputDriver> CreateSurvivalInputDriver(CombatScene &scene, const MapRectangle &bounds) {
    if (inputFactory == nullptr) {
        std::printf("[input] no external driver installed; using platform input\n");
        return nullptr;
    }
    return inputFactory(scene, bounds);
}

