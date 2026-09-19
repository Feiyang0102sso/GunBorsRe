# 游戏会话职责归位

## 范围与方案

2026-09-18 用户要求：本批会话代码统一放入 `gameplay/game/`，消除可归位的 Z 包装，仅保留必要宿主适配；测试实现迁到 `tests/`。同时集中游戏键位，移除 G、N/M 游戏绑定和游戏中的 Viewer 武器目录选择。

1. `CGame`、启动入口、会话运行、输入和绘制统一归入 game；按职责拆分长函数，保留资源寿命及调用顺序。
2. `CGameFlow` 承接跨局进度和结算状态；当前宿主扩展明确标注，不冒充原 ARM 内存布局。
3. 测试配置、自动驾驶、性能实验、截图和断言全部归 tests。共享循环仅保留中性资源、生命周期和逐帧输入观察入口，不包含测试项名称。
4. 游戏键位集中为 `ZGameKeys.h`；按钮直接提交动作，消除 F/R/G 伪键码。Viewer、调试和作弊键保留各自职责。
5. 左侧 info 栏仍由 debug/SurvivalDebug 与 DebugConfig 负责。

## 原版依据

- `game.cpp`：CGame::Bind 76803、Load 76850 附近、Init 76863、Update 76355、Draw 75544；负责关卡/HUD 绑定和会话组织。
- `gameFlow.cpp`：CGameFlow::Init 77286、OnMissionStart 77337、OnMissionOver 77345、ConfigureBrother 77430、UpdatePlayerProgress 77476。
- 清波与结算：CGame::OnWaveCleared 76192、UpdateKillStats 75714、UpdatePostGameStats 75868、OnMissionTerminate 76266。
- `level.cpp`：CLevel::Update 121697、Bind 121717；`inputPad.cpp`：CInputPad::UpdateInput 87716、Draw 87955。
- BT：entries/level_template.bt、entries/mission_entry.bt、ui_movie.bt、saves/save_payloads.bt。此次不改变磁盘结构和资源数值。
- CGame 的嵌套会话存储及观察接口是宿主实现细节；进度保存仍使用既有原生存档读取器，不复制资源表。

## 任务与验收

- [x] 目录、依赖及状态归位，删除旧转发文件。
- [x] 输入动作与键位统一；正式游戏只保留配置的绑定，Viewer 切枪不受影响。
- [x] 测试实现迁出，保留研究入口、确定性输入和性能报告能力。
- [x] 构建 Debug/Release 三产物；按影响检查 profile-play、教程、死亡/重开、合作/DM、终轮与 Viewer。
- [x] 更新实际映射和结果；静音运行，保留日志，不重复全量截图基线。

## 当前目录与边界

生产会话文件为 CGame、CGameFlow 及 Loading／Session／Input／Update／Drawing／Hud／Shop 各实现文件。原 1,355 行循环按职责拆开；资源仍由启动栈持有，嵌套会话借用资源，不改变析构次序。

本批只保留三个 Z 文件：`ZGameKeys.h` 管理 Windows 游戏键位；`ZGameObserver.h` 合并此前重复的调用方接入口；`ZLiveShopSession.h` 管理本地多人商店同步。前两者不是原版类，不更名为 C。观察入口不包含测试名称、配置选择、自动驾驶工厂、截图或性能统计。

键位入口为 `src/gun_bros_re/gameplay/game/ZGameKeys.h`：WASD 移动、1 打开商店、2 切换已装备的两把枪、Q/E 左右道具、Space/Esc 暂停或返回。F/R 保持不绑定，G 和 N/M 从游戏输入移除。鼠标重开、道具选择直接提交动作，不再伪装成键盘事件。Viewer 的分类／目录选枪实现移到 ViewerControls；Shift+M 等调试入口仍归 DebugKeys。

左侧 info 栏数据由 `debug/SurvivalDebug.cpp` 的 PopulateSurvivalDebugInfo／PopulateDebugBuffs 汇总、DrawSurvivalDebugInfo 绘制；样式与位置归 `debug/DebugConfig.h`，CInputPad::Draw 调用，不随测试代码迁移。

## 原注释保留

业务、原版依据和行为注释随实现迁移。以下旧包装注释因描述了已移除的文件／字段，原文存档在此；它们仅表示迁移前状态，不代表当前架构：

```text
src/gun_bros_re/debug/FlockMetrics.h: /** @file FlockMetrics.h * @brief Crowd-spacing measurements for performance reports. * * Pure measurement with no assertions and no gameplay effect; the thresholds * here are diagnostic only. Lives in src/ because the session loop writes * these numbers into its own performance CSV. */
src/gun_bros_re/debug/SurvivalDevelopment.h: /** @file SurvivalDevelopment.h * @brief Development-run configuration for a survival session. * * Pure configuration: which experiment to run, where to write evidence, how * far to advance. It carries no assertions, so the session loop can read it * without reaching into tests/. Production leaves SurvivalLaunch::development * null and every field below stays at its default. */
src/gun_bros_re/gameplay/CGameSession.h: // Development-only check configuration, defined in tests/. Production never
src/gun_bros_re/gameplay/CGameSession.h: // sets it, so an incomplete type is all this header needs.
src/gun_bros_re/gameplay/CGameSession.h: // Always null on the production path. Keep this last so the existing
src/gun_bros_re/gameplay/CGameSession.h: // aggregate initialisations in tests/ stay valid.
src/gun_bros_re/gameplay/CGameSession.h: // Development hook set; production leaves it null and the loop skips every hook.
src/gun_bros_re/gameplay/ZSurvivalFrame.h: /** Borrowed state valid only for the current RunSurvival invocation. */
src/gun_bros_re/gameplay/ZSurvivalFrame.h: // -1 continues normally; -2 presents and requests another frame; >=0 exits.
src/gun_bros_re/gameplay/ZSurvivalGameContext.h: /** @file ZSurvivalGameContext.h * @brief Persistent game session settings; absent in permanent research harnesses. */
src/gun_bros_re/gameplay/ZSurvivalGameContext.h: // Optional borrowed input/observation driver supplied by the caller.
src/gun_bros_re/gameplay/ZSurvivalInputDriver.cpp: /** @file ZSurvivalInputDriver.cpp * @brief Optional research hook; ordinary gameplay uses platform input. */
src/gun_bros_re/gameplay/ZSurvivalInputDriver.h: /** @file ZSurvivalInputDriver.h * @brief Optional external input driver for repeatable runs. * The game installs no driver; the test executable owns the pilot implementation. */
src/gun_bros_re/gameplay/ZSurvivalRuntime.h: // One signature in every configuration. Checks travel in launch.development.
src/gun_bros_re/gameplay/ZSurvivalScenario.h: /** Borrowed runtime views. Observers never own the level, actors or resources. */
```

## 验证记录（2026-09-18）

构建使用单个 `GunBrosRe.vcxproj`，指定 `GbProduct=Game`、`Platform=x64`、`SkipAutoTests=true`，由工程继续构建 Viewer 和 Tests；Debug、Release 最终均退出 0。日志：`obj/game-migration/final2-debug-build.log`、`final2-release-build.log`。完整编译中的已有数值转换警告保留，未扩大本次修改范围。

迁移期间的编译错误已修复：运行状态缺少 progressData 引用、音频测试误传输入驱动、网格检查缺少迁移后的 ViewerControls 声明。旧失败日志保留用于追溯，最终日志为上述 final2 文件。

教程最初在菜单 Shift+T 入口超时：测试临时配置为 DebugMode=0，现有测试没有显式开启调试入口。独立 `debug-input` 检查用真实 SDL 按键验证开关两种状态，0.6 秒通过。测试现仅在该菜单断言期间启用并恢复 debugMode；完整教程复测 25.6 秒通过，包含原生存档恢复、调试重放、Esc 返回和 Shift+T。生产菜单不作修改。

所有自动运行均静音，不重复全量截图基线。旧批次结果保留在 `obj/game-migration/stage3-results/`，独立输入检查在 `debug-input-results/`。阶段初次检查中的 player-death、local-live、deathmatch 均退出 0；正式键位检查包含 F/R/G/N/M 无游戏动作，以及保留的 1/2/Q/E 和鼠标控件。

最终回归分批执行，均使用 `-NoBuild`，运行器统一添加 `--mute` 并校验 BIG、存档样本及用户配置未变：

| 批次 | 命令入口及用例 | 结果与日志 |
|---|---|---|
| Debug 初批 | `tests/run.ps1 -Configuration Debug -Case player-death,local-live,deathmatch,profile-play,tutorial` | 前四项通过；教程问题按上文修复并复测；`obj/game-migration/stage3-tests.log` |
| 调试输入 | `tests/run.ps1 -Configuration Debug -Case debug-input` | 1/1，通过；`obj/game-migration/debug-input-check.log` |
| Debug 最终 | `tests/run.ps1 -Configuration Debug -Case tutorial,profile-play,viewer-controls,debug-map-profile,native-profile-play,horde-first,progress,weapons` | 8/8，通过，退出 0；`obj/game-migration/final-debug-tests.log`，完整结果 `final-debug-results/` |
| Release 最终 | `tests/run.ps1 -Configuration Release -Case performance,spawn-performance,final-pack2,final-pack7,final-pack9,final-pack12` | 6/6，通过，退出 0；`obj/game-migration/final-release-tests.log`，完整结果 `final-release-results/` |

两个性能入口均实际完成 1,200 帧并生成 CSV。普通实验 CPU p95=1.471 ms；刷怪实验 p95=0.988 ms，峰值存活数=20，资源对象池限制=20，满足既有 16.667 ms 验收条件。这只验证迁移后实验及当前场景可运行，不宣称重构带来性能提升。

去重共 18 项回归通过。源代码检查：src/tests 中无旧 ZSurvival 符号引用；game 下无实验选择、截图路径、自动驾驶或测试断言；`git diff --check` 通过。工程已有递归源文件收集规则，无需新增工程或转发文件。

Viewer：`pwsh -NoProfile -File tests/viewer-smoke.ps1` 退出 0，所有路由、配置及场景寿命检查通过。日志 `obj/game-migration/final-viewer-smoke.log`，产物保留于 `tests/out/viewer-smoke/` 和 `obj/game-migration/final-viewer-results/`。
