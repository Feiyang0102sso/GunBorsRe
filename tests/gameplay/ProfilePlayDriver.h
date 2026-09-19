#pragma once
#include "gun_bros_re/gameplay/game/ZGameObserver.h"

/** Deterministic pointer/shortcut and animation assertions for profile-play. */
class ProfilePlayDriver : public ZGameObserver {
public:
    int OnFrame(ZGameObserver::FramePhase phase, ZGameObserver::Frame &frame) override;
private:
    unsigned controlFrame = 0, checkFailures = 0, combatSwapEvents = 0;
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
