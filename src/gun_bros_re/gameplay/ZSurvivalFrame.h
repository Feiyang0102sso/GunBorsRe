#pragma once
#include "gun_bros_re/ui/ZHudState.h"
#include "engine/platform/ZWindow.h"

class CInputPad;
class CBGM;
class ZLevelHost;

/** Optional caller control at real input/update/render boundaries. */
enum class ZSurvivalFramePhase {
    Begin, Pointer, Keys, AfterKeys, BeforeSimulation, AfterSimulation,
    BeforeWeaponSwap, AfterWeaponSwap, Drawn
};

/** Borrowed state valid only for the current RunSurvival invocation. */
struct ZSurvivalFrame {
    CProfileManager *profile;
    CInputPad &hud;
    ZLevelHost &session;
    ZCombatWorld &scene;
    ZPlayerModel &player;
    ZPlayerVitals &vitals;
    ZWeaponEffects &effects;
    ZWindow &window;
    CBGM &music;
    GameObjectRef &leftPowerup;
    GameObjectRef &rightPowerup;
    bool &paused;
    bool &shopOpen;
    unsigned &equippedWeaponSlot;
    int &accumulator;
    ZInputPadState inputState;
    std::vector<ZKeyCode> inputs;
    float inputX = -1, inputY = -1;
    bool pointerDown = false;
    bool forceFire = false;
};

class ZSurvivalFrameDriver {
public:
    virtual ~ZSurvivalFrameDriver() = default;
    // -1 continues normally; -2 presents and requests another frame; >=0 exits.
    virtual int OnFrame(ZSurvivalFramePhase phase, ZSurvivalFrame &frame) = 0;
};
