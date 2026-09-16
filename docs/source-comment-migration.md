# 随接口迁移保留的历史注释

日期：2026-09-15。按“不要删除原注释”的约定，以下逐字保留因旧文件或接口移除而无法继续放在原处的说明。其余实现注释随代码移动；当前职责以 [重构结果](source-alignment-result.md) 为准。

## `src/engine/core/BuildFeatures.h`

原头已删除；产品差异现在由工程配置与 debug/Capture 承担。

```cpp
// MSBuild selects capture per product; both game configurations enable cheats.
// Test sources belong exclusively to the Tests executable.
```

## `src/gun_bros_re/gameplay/CombatScene.cpp`

经验处理已迁到 CPlayer.cpp；重新定位后的原函数入口为 101185，旧注释行号保留如下。

```cpp
// CPlayer::AddExperience (:101250) preserves the current health fraction.
```

## `src/gun_bros_re/gameplay/SurvivalGameContext.h`

原 profile 专用注入已替换为调用方持有的通用帧驱动。

```cpp
// Program-local pointer injection for the persistent profile regression.
```

## `src/gun_bros_re/gameplay/SurvivalLoop.cpp`

测试选择现在完全由 tests 的生命周期适配负责。

```cpp
// Which check runs here is a development concern; the loop only offers the
    // finished scene and honours the answer.
```

## `src/gun_bros_re/gameplay/SurvivalScenario.h`

旧版接口说明。用例上下文现已迁入 tests/SurvivalFixtures，生产接口改为共享生命周期；不再适用的路径说明不能继续冒充当前设计。

```cpp
/** @file SurvivalScenario.h
 * @brief Hooks a development scenario can use to take over a survival session.
 *
 * The session loop owns the scene; a scenario only observes it or takes it
 * over. Every hook returns -1 to let the session continue, or >= 0 to end it
 * with that exit code. Production installs no scenario, so all of this is
 * inert unless SurvivalLaunch::scenario is set.
 *
 * The fixtures below are views onto session-local state. They live here, in
 * src/, so the loop never has to include anything out of tests/.
 */
```
