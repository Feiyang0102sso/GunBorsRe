#pragma once
/** Desktop caller adapter for resource observation and ordinary frame input.
 * No test selection, assertion, screenshot path or performance policy lives here.
 * These nested views are borrowed host data, not recovered original classes.
 */
#include "gun_bros_re/gameplay/game/CGameSession.h"
#include "gun_bros_re/ui/hud/ZHudState.h"
#include "engine/platform/ZWindow.h"
#include <optional>
class CBGM;
class ZShaderProgram;

class ZGameObserver {
  public:
    struct Options {
        bool realtime = true;
        bool mouseAim = true;
        bool exitOnCompletion = true;
        bool showCollisions = false;
        bool seedLevel = true;
        std::optional<std::uint32_t> levelSeed;
        std::optional<unsigned> matchSeed;
    };
    // Lifecycle points are independent of individual test cases.
    enum class Stage { Bound, Ready, LoopStarting, WorldDrawn };
    struct Resources {
        CResTOCManager &toc;
        ZPackTables &tables;
        std::vector<CEnemy::Template> &enemies;
        CBrother::Vitals &vitals;
        CPlayerProgress::Template &progressData;
        ZWindow &window;
        CInputPad &survivalHud;
        ZShaderProgram &program;
        CMap &loaded;
        CBrother &player;
        CLevel &scene;
    };

    /** Optional caller control at real input/update/render boundaries. */
    enum class FramePhase {
        Begin,
        Pointer,
        Keys,
        AfterKeys,
        BeforeSimulation,
        AfterSimulation,
        BeforeWeaponSwap,
        AfterWeaponSwap,
        Drawn,
        Starting,
        Updated,
        GeometryDrawn,
        WorldDrawn,
        Presented
    };

    /** Borrowed state valid only for the current CGame::Run invocation. */
    struct Frame {
        CProfileManager *profile;
        CInputPad &hud;
        CGame &session;
        CLevel &scene;
        CBrother &player;
        CBrother::Vitals &vitals;
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
        bool suppressFire = false;
        float moveX = 0, moveY = 0;
        unsigned updateSteps = 0;
        std::vector<ZInputPadAction> actions;
    };

    virtual ~ZGameObserver() = default;
    virtual void Configure(const CGame::Launch &, Options &) {}
    virtual int OnResources(Resources) { return -1; }
    virtual int OnInventory(CResTOCManager &, CProfileManager &) { return -1; }
    virtual int OnStage(Stage, CGame::Session &) { return -1; }
    // -1 continues; -2 after Drawn presents and requests another frame; >=0 exits.
    virtual int OnFrame(FramePhase, Frame &) { return -1; }
};
