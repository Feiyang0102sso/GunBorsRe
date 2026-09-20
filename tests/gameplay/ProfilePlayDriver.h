#pragma once
#include "gun_bros_re/host/ZGameObserver.h"

/** Deterministic pointer/shortcut and animation assertions for profile-play. */
class ProfilePlayDriver : public ZGameObserver {
public:
    explicit ProfilePlayDriver(bool mouseInputOnly = false, int mouseMode = 2)
        : mouseOnly(mouseInputOnly), mouseControlMode(mouseMode) {}
    int OnFrame(ZGameObserver::FramePhase phase, ZGameObserver::Frame &frame) override;
    ~ProfilePlayDriver() override;
private:
    int CheckMouseFire(ZGameObserver::FramePhase phase, ZGameObserver::Frame &frame);
    int CheckScreenFire(ZGameObserver::FramePhase phase, ZGameObserver::Frame &frame);
    unsigned mouseFrame = 0;
    int previousControl = 1;
    bool mouseCheckStarted = false;
    bool mouseOnly = false;
    int mouseControlMode = 2;
    unsigned screenInitialSlot = 0, screenLeftCount = 0, screenRightCount = 0;
    std::size_t screenShots = 0;
    std::size_t mouseShotsBefore = 0;
    float mouseExpectedFacing = 0;
    unsigned controlFrame = 0, checkFailures = 0, combatSwapEvents = 0;
    unsigned controlClickPhase = 0;
    float controlClickX = 0, controlClickY = 0;
    std::uint64_t controlsInitialBucks = 0;
    bool controlsInitialSound = false;
    unsigned controlsInitialGrenades = 0, controlsBeforeKeys = 0, controlsLeftBeforeKeys = 0;
    GameObjectRef controlsInitialLeft;
    int controlsWaveBeforeInput = 0;
    std::size_t shotsBeforeSwap = 0;
    bool checkSwapFiring = false;
    const CMesh *outgoingMesh = nullptr;
    int outgoingTime = 0;
};
