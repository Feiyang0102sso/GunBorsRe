/** @file ZSurvivalInputDriver.h
 * @brief Optional external input driver for repeatable runs.
 * The game installs no driver; the test executable owns the pilot implementation.
 */
#ifndef GUN_BROS_RE_ZSURVIVALINPUTDRIVER_H
#define GUN_BROS_RE_ZSURVIVALINPUTDRIVER_H
#include <memory>
#include "engine/platform/ZWindow.h"
class ZCombatWorld;
struct ZMapRectangle;

class ZSurvivalInputDriver {
public:
    virtual ~ZSurvivalInputDriver() = default;
    virtual void Update(int deltaMs, float &moveX, float &moveY) = 0;
    virtual void Report() const = 0;
};
using ZSurvivalInputFactory = std::unique_ptr<ZSurvivalInputDriver> (*)(ZCombatWorld &, const ZMapRectangle &);
void SetSurvivalInputFactory(ZSurvivalInputFactory factory);
std::unique_ptr<ZSurvivalInputDriver> CreateSurvivalInputDriver(ZCombatWorld &scene, const ZMapRectangle &bounds);
namespace MapDetail {
void AppendSurvivalShortcut(std::vector<ZKeyCode> &inputs, ZKeyCode key);
}
#endif
