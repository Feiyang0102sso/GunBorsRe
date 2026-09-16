# engine 与 gun_bros_re 重构结果

日期：2026-09-15。依据用户批准的 [对齐计划](source-alignment-plan.md) 实施。

## 结果

以原职责拆分玩家、刷怪、输入面板、选择器、菜单绑定和 Movie 读取；把桌面适配及自建表示统一为 Z 命名。保留一个工程、三种产物和既有研究入口。没有修改 BIG、存档协议或原生脚本编号。

### 职责与依据

下表源码路径相对 `src/`。原函数位置指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`；不是恢复出来的原 `.cpp` 行号。

| 原组合 | 当前归属 | 核对依据与边界 |
|---|---|---|
| `CombatScene` 的玩家状态、经验、矿石、移动、射击、击退 | `gun_bros_re/gameplay/CPlayer.*` | `CPlayer::AddExperience` 101185、`AddXplodium` 101116、`Move` 100623、`UpdateMovement` 101384、`UpdateShooting` 101314；玩家 BT。玩家状态只有一份，模型及生命状态为非拥有引用 |
| `SurvivalSession::ChooseSpawnNode` | `CEnemySpawner::GetSpawnPoint`，`CLayerPathLink/Mesh::GetSpawnLocation` | 146098、146112、166819、168115；LEVEL 与 MAP BT。保留节点锁定、最近五点、屏外边界和随机调用顺序 |
| 宿主击杀计数 | `CLevel::OnEnemyKilled` / `GetKills` | 119306；只在交付死亡事件时计数，Bind 重置，宿主读取同一计数 |
| 旧战斗组合层的敌人存储、预加载、UID、延迟生成与回收 | `gameplay/CLevelObjectPool.*` | 原 `levelObjectPool.cpp`：构造 145287、`GetEnemy` 145509、`Release` 145426、`Clear` 145701；保留 100 个敌人槽、活动上限以及尸体到实际回收前仍占槽的语义 |
| 旧战斗组合层的击杀奖励、分数、连杀、波次奖励和关卡事件队列 | `CLevel::RewardEnemy`、`ResolveWaveReward` 及 `CLevel` 事件入口 | `CLevel::OnEnemyKilled` 119306–119912、`OnWaveCleared` 116897；奖励及统计状态随关卡，死亡确认后直接进入关卡原生链，不再通过宿主公开容器二次轮询 |
| Powerup 编号白名单 | `ZPowerupScene` 的 STORE 引用、模式标志与 `CPowerup` Flow 查询 | POWERUP 2/3/4 没有专用 STORE 条目且 Flow 不提供装备入口；删除 `IsPlayablePowerup`，不再用 `pack5` 编号重复表达资源事实 |
| `ZLevelHost` 的组合职责 | `gameplay/CGame.*`、`CLevel.*` | 原 `CGame::Update` 76355–76663、`CGame::Bind` 76803；`CLevel::UpdateNormal` 121150、`UpdateAfterDeath` 121047、`Update` 121697。`CGame` 引用活动 `CLevel` 并处理 HUD、对话、转场和结果；生成、对象查询、拾取、触发、相机及世界更新归 `CLevel` |
| `SurvivalHud` 与两个 `Original*Selector` | `ui/CInputPad.*`、`CPowerUpSelector.*` | `powerUpSelector.cpp` 183797–187670；Movie BT。选择、购买提示、命中区域、滚动和动画归选择器 |
| 面板与选择器的资源缓存 | `ui/ZHudResources.*`，状态输入 `ZHudState.h` | 桌面共享缓存；两消费者使用同一缓存，不复制 BIG 数据。删除只写不读的 `originalUi` |
| `OriginalPromotionPopup` 的两套绑定 | `ui/CMenuInviteFriends.*`、`CMenuIncentives.*` | 248330–248861、292917–293260；文字、图标、区域和动作映射归各菜单，`ZPromotionPopup` 保留共同的章节播放和点击生命周期 |
| `OriginalLoadingSplash`、`OriginalDialogPopup` | `CMenuSplash.h`、`CDialogPopup.h` | 文件与已有原类名一致，保留原章节、布局及读取逻辑 |
| `CMovie` 混合对象读取和章节范围 | `engine/glu/movie/CMovieObject.*`、`CMovieChapter.*` | `CMovie::InitResource` 109263、`CMovieChapter::Init` 109532、章节长度 109576；`ui_movie.bt`。`ZMovieKeyFrame` 明示当前合并存储表示 |
| `MapWorldInternal` 的缓存和道具运行对象 | `gameplay/ZMapResources.h`、`ZMapPropWorld.h` | 地图、模板缓存、实例的生命周期不变；原 `CMap`、`CProp`、`CParticleEffect` 继续负责对应数据和行为 |

### 命名

- 文件变更逐项列在 [源码文件名映射](source-name-map.md)。主要宿主为 `ZWindow`、`ZAudioPlayer`、`ZMediaVideo`、`ZShaderProgram`、`ZMeshBuffer`、`ZTexture`、`ZQuadBatch`、`ZMovieRenderer`。
- `CombatScene` 组合层已删除；运行时状态并入原名 `CLevel`，并按运行绑定、地图与对象、角色、投射物、战斗更新拆成独立实现文件。原组合类 `SurvivalSession` 同样已删除，`CGame` 只协调活动 `CLevel`。
- `BroAIDeathmatch` → `ZLocalCoopBot`，PvP 策略为 `ZDeathmatchBot`；实际本地 bot 的 `SetTestBot/HasTestBot` 改为 `SetLocalBot/HasLocalBot`。
- `OriginalProfile` / `NativeProfile` → `ZProfileImport` / `ZProfileStorage`。函数按导入、加载、保存、记录职责命名；磁盘 ID、格式和未知字段保留策略不变。
- `CSpriteGluArchetype` → `ZSpriteArchetype`：算法来自 `CSpriteGlu::LoadArcheType`，当前独立缓存类是自建表示，不能据此捏造一个原类。
- 原 `ILayerPath`、`Mission`、`Planet` 等保持原符号；自建接口使用 Z。没有凭前缀补造 I/IC 接口或继承关系。
- 普通函数、局部变量、嵌套类型，以及确有语义的 `InitialValues` 等不机械改名；`cheats/`、`debug/` 豁免。
- 提取出来的 `.inc` 保留来源与原值，生成器输出文件名同步更新。历史研究注释可能仍提旧名，使用映射表定位；已移除接口的旧说明单列于 [注释迁移记录](source-comment-migration.md)，保留原文及现状说明。

### 测试与依赖

- 删除 `BuildFeatures.h`、恒开的 `GB_ENABLE_CHEATS` 分支与定义、`GB_SAVE_FRAME`；没有 `GB_ENABLE_TESTS`。不再通过工程强制包含游戏的 Capture 头。
- 截图调用显式使用 `Capture::SaveFrame`；`GB_ENABLE_CAPTURE` 只表达真实产品差异。Release Game/Viewer 禁用实现返回 false，Tests 两配置均保留截图。
- `engine` 不显式引用游戏、Viewer 或 tests。Game/Viewer 不编译 tests 实现；一个 `.vcxproj` 的 Game 构建继续自动构建另外两个 EXE。
- `ProfilePlayDriver` 在 tests 内负责自动输入、BGM/快捷键/换枪断言；共享帧驱动仅提供输入与运行时观察。
- 生存用例上下文迁到 `tests/gameplay/SurvivalFixtures.h`；测试选择、组合与断言由 `SurvivalCheckScenario` 负责。原十几项按用例命名的虚回调收敛为资源、库存和阶段 3 个入口。
- `ZSurvivalScenario` 提供 Bound、Ready、Advanced、LoopStarting、WorldDrawn、Captured 阶段。上下文借用正在运行的对象，只有调用期间有效，不复制关卡状态。
- 关卡声音、触发路线、道具路线/伤害检查不再声明为生产类方法；伙伴默认装备/换枪、GL 混合状态以及 Horde 存档/受伤/重开断言均在 tests。
- 保留现有 UI 检查所需的少量 friend 声明；实现只在 tests。未为了移除这些声明而公开整套可写 UI 内部状态。
- Horde 收尾检查发现开发输出目录可为空；改用 `TestOutput::Path`，避免创建空路径时异常。首波用例完成约 6 秒，末段约 19 秒；它们不跑四星球 500 波 LongRun。

## 保留的宿主边界和未声称完成的内容

- 旧战斗组合层及其头文件已经删除，没有保留兼容门面或类型别名。敌人对象生命周期进入 `CLevelObjectPool`，玩家移动进入 `CPlayer`；命中查询、逐帧协调和本地模式状态成为 `CLevel` 的原生职责，并分散在 `CLevelWorld.cpp`、`CLevelActors.cpp`、`CLevelProjectiles.cpp`、`CLevelCombat.cpp` 等实现文件。当前对象池只恢复已消费的敌人路径，尚未纳入原池中的 Bullet、Prop、Pickup、Platform。
- 本轮没有改玩家桌面固定速度为原模拟摇杆加速，也没有把桌面确定性随机流冒充原全局随机发生器。相关差异写在 `CPlayer.cpp` 与 `ZRandom.h`。
- Movie 的分类型解析职责已分离，但未恢复全部原 `CMovie*` 子类继承树；GL 资源缓存和绘制仍是桌面实现。
- `SurvivalDevelopment` 继续承载历史研究配置；共享生命周期视图仍较宽。它们没有测试实现，进一步缩窄需结合运行入口的后续整理，不能仅靠改名宣称完成。
- 不更改资源数值、协议、硬编码旧问题或未实现分支。本次通过构建和回归不等于整个游戏已完成原版复刻。

## 验证

全部通过，命令退出码均为 0：

| 验证 | 结果 | 持久日志 |
|---|---|---|
| Debug Game / Viewer / Tests | 三产物构建成功，默认 3 项检查及 Viewer smoke 全通过 | `obj/refactor-default-build.log` |
| Release Game / Viewer / Tests | 三产物构建成功 | `obj/refactor-release-build.log` |
| Debug 受影响回归 | 24/24，通过；受保护资源变化 0 | `obj/refactor-final-debug-checks.log`、`obj/refactor-final-debug-results.json` |
| Release 受影响回归 | 8/8，通过；受保护资源变化 0 | `obj/refactor-release-checks.log`、`obj/refactor-release-results.json` |
| Release 实际运行 | 资源、其他工作目录启动、菜单、作弊、保存、产品命令边界通过 | `obj/refactor-runtime-check.log` |
| 静态整理 | include 路径无缺失，header guard 无重复，跨包引用检查及 `git diff --check` 通过 | `obj/audit_alignment.py` |

构建命令：`MSBuild GunBrosRe.vcxproj /p:Configuration=Debug /m`，以及对应 `Release`。默认 Debug 构建的测试没有改为长跑。

回归命令：

```powershell
pwsh -File tests/run.ps1 -Configuration Debug -Case progress,big-version,viewer-controls,profile-play,original-saves,original-profile,enemies,brother,movies,original-hud,powerup-selector,pause-menu,promotion,dialog,loading-wipe,play-interaction,path-cache,flock,horde-first,horde-last,player-death,deathmatch-data,deathmatch,local-live -NoBuild
pwsh -File tests/run.ps1 -Configuration Release -Case progress,original-profile,movies,powerup-selector,promotion,profile-play,horde-first,play-interaction -NoBuild
pwsh -File tests/verify-runtime.ps1
```

测试脚本显式传入 `--mute`。Debug 回归覆盖死亡、合作、PvP、玩家移动射击、短波次/末段 Horde、重开、存档样本、菜单与 Movie；未运行四星球 500 波 LongRun，未重跑全量截图基线。必要的 stdout/stderr 副本位于 `obj/refactor-evidence/`。

最终命名和格式收尾之后重新完成两配置三产物构建、默认 Debug 检查、Viewer smoke，以及上述 8 项 Release 回归；24 项 Debug 记录来自其前一批相同逻辑的构建。

更早的 Horde 超时诊断日志也保留；最终首波及末段检查均已通过，临时诊断输出已经移除。
