/** @file SurvivalInputDriver.h
 * @brief Optional external input driver for repeatable runs.
 * The game installs no driver; the test executable owns the pilot implementation.
 */
#ifndef GUN_BROS_RE_SURVIVALINPUTDRIVER_H
#define GUN_BROS_RE_SURVIVALINPUTDRIVER_H
#include <memory>
class CombatScene;
struct MapRectangle;

class ISurvivalInputDriver {
public:
    virtual ~ISurvivalInputDriver() = default;
    virtual void Update(int deltaMs, float &moveX, float &moveY) = 0;
    virtual void Report() const = 0;
};
using SurvivalInputFactory = std::unique_ptr<ISurvivalInputDriver> (*)(CombatScene &, const MapRectangle &);
void SetSurvivalInputFactory(SurvivalInputFactory factory);
std::unique_ptr<ISurvivalInputDriver> CreateSurvivalInputDriver(CombatScene &scene, const MapRectangle &bounds);
#endif
