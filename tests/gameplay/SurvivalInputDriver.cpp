#if GB_ENABLE_TESTS
/** @file SurvivalInputDriver.cpp
 * @brief Optional research hook; ordinary gameplay uses platform input.
 */
#include "gameplay/SurvivalInputDriver.h"
#include <cstdio>
namespace {
SurvivalInputFactory inputFactory = nullptr;
}
void SetSurvivalInputFactory(SurvivalInputFactory factory) { inputFactory = factory; }
std::unique_ptr<ISurvivalInputDriver> CreateSurvivalInputDriver(CombatScene &scene, const MapRectangle &bounds) {
    if (inputFactory == nullptr) {
        std::printf("[research] input driver is available in GunBrosViewer.exe\n");
        return nullptr;
    }
    return inputFactory(scene, bounds);
}

#endif
