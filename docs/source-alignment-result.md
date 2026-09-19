# engine 与 gun_bros_re 重构结果

日期：2026-09-15。依据用户批准的 [对齐计划](source-alignment-plan.md) 实施。

2026-09-19 gameplay 全目录更新：根目录 32 个源码文件全部归位；原 23 个 Z 文件中 6 个合并删除、7 个迁至 host／Viewer、10 个留在 multiplayer，新增 `multiplayer/bot/ZBotSettings.h` 统一 Bot 策略配置。弹体恢复共同 group／Y 排序、追踪角速度和直接命中击退，脚本随机恢复原算法并共享关卡上下文。Haven 玩家激光遮挡有动态修复证据；Quadcaptain 串珠已核对真实发射链，原始动画 1 仍产生串珠，不冒充已修复。完整归属、统计口径、测试及边界见 [本轮结果](gameplay-optimization-result.md)。本段覆盖下文旧目录和包装状态。

2026-09-18 游戏会话更新：本批源码归入 `gameplay/game/`，八个旧 `ZSurvival*` 文件全部移除。会话资源与各阶段归 `CGame`，跨局进度归 `CGameFlow`；只保留键位、统一宿主观察入口、本地商店同步三个必要 Z 文件。测试配置、自动驾驶、性能实验、CSV 与截图编排归 `tests/`。游戏输入采用动作分派，G、N/M 不再是游戏快捷键，Viewer 目录选枪留在 Viewer。证据与验收见 [游戏会话职责归位](game-responsibility-migration.md)，本段覆盖下文旧会话文件状态。

2026-09-18 地图更新：地图相关实现集中在 `gameplay/map/`，移除原九个 `ZMap*` 文件及 `ZPropWorld.h`，只保留一个 Viewer 专属 `ZMapViewer.h`。`CMap` 直接持有资源，`CProp` 统一脚本与三播放器，道具调度归 `CLevel::Props`，绘制归 `CMap`／`CLayerTile`／`CProp`／`CRenderQueue`。正式加载不再遍历匹配关卡脚本；Viewer 状态预览执行原 Flow。Movie 图层保留引用和位置，当前样本没有此层，绘制仍为明确的未实现分支。映射与验收见 [地图职责归位](map-responsibility-migration.md)；本段覆盖后文旧地图包装与路径。

2026-09-18 敌人更新：敌人专属生产源码归入 `gameplay/enemy/`，按本轮用户要求使用 C 文件前缀、保留 I 前缀，文件头标出原版对应与宿主差异。`ZEnemyModel`、`ZCombatEnemy`、`ZPlacedEnemy` 组合层已删除，模板、资源、脚本和动画归同一个 `CEnemy`；菜单归 `CMenuMeshEnemy`；对象池直接持有敌人实例。恢复 `CMeshPathFinder` 与 `CFlock` 目标距离图、地图／道具视线查询、原骨骼位置查询和 native 54 的 LEVEL 资源生成链。网络保留 TODO，本地合作／PvP Bot 不依赖网络生成器。完整范围、证据、验收与未验证边界见 [敌人职责归位](enemy-responsibility-migration.md)。本段覆盖后文旧敌人路径与旧 Z 包装状态。

2026-09-17 夜间更新：`ZWeaponEffects.h/.cpp` 已删除，未留下别名或转发壳。UI 直接播放粒子；地图临时实例、弹体、附属效果、带状拖尾分别归 `CParticleSystem`、`CBullet/CLevel`、`EffectContainer`、`TrailEffectHolder/CRibbonTrailEffect`；兄弟强化播放器归 `CBrotherParticles.cpp`。`CBrotherPowerups.cpp` 保持独立。原版依据、各阶段验证与保留的宿主适配见 [夜间交接](weapon-effects-night-migration.md)，具体路径见 [文件名映射](source-name-map.md)。以下批次描述中的旧状态保留为历史记录。

后续主线（2026-09-16）：用户指定继续处理运行行为硬编码、重复执行逻辑与原版职责归位，具体问题、优先级及验收见 [运行行为与原版职责对齐路线](runtime-alignment-roadmap.md)。本页保留上一阶段实际结果，不将下一阶段待办计为已完成。

当前路径补充：`CLevel*` 文件已统一移入 `gameplay/level/`；`ZEnemyCombat.h` 已并入 `CEnemy.h`，原敌人、协作与死亡竞赛三个 Z 实现文件已归回 `CEnemy`、`CLevel` 的分文件实现。详细映射见 [文件名映射](source-name-map.md)，验证见路线第八节。下文历史路径按映射定位。

第四批补充：`ZPowerupMoviePlayer.*` 已删除，其脚本和播放职责归同一个 `CPowerup`，呈现方法位于 `CPowerupPresentation.cpp`，详见路线第九节。

第五批补充：`ZPowerupScene` 不再按道具编号选择执行器；角色原生调用归 `CPowerupActions.cpp`，所有使用共享持久脚本生命周期，详见路线第十节。R02 的部分模拟 UI 回调仍为待办。

第六批补充：`ZPowerupScene.h/.cpp` 实际删除，职责分归现有 `CPowerUpSelector`、`CBrother`、`CLevel` 与本地 Bot；正式游戏不再另建一份玩家道具宿主和目录。Z 文件从 140 个降至 138 个，详细归属与验证见路线第十一节。

目录归并：`CPowerup` 本体四个文件统一移入 `gameplay/powerup/`，仅更新路径，不改逻辑或注释。角色、关卡、UI 和数据读取文件继续按所属对象放置，验证见路线第十二节。

角色目录归并：玩家、共用 Brother、默认伙伴 AI、两个自建 Bot 及模型组装共 13 个文件统一移入 `gameplay/brother/`。保留 `CBrotherPowerups.cpp`，不合并实现；仅变更路径，验证见路线第十三节。

## 结果

Bot 归并增补（2026-09-17）：本地合作、PvP、寻路及本地好友／名单配置集中在 `brother/bot/`，策略名统一为 `ZLocalCoopBot`、`ZLocalPVPBot`。寻路及巡逻／掩体选择从 `CLevel` 归回 PvP Bot，原默认伙伴 `CBrotherAI` 留在 `brother/`。详见 [Bot 归并记录](bot-directory-migration.md)。

角色所有权增补（2026-09-17）：过渡 `ZPlayerModel`、`ZPlayerActor`、`ZPlayerEquipment`、`ZPlayerPart` 与资源／绘制拆分文件已全部删除。游戏、菜单和测试直接使用 `CBrother`；角色持有 PLAYER 脚本、强化状态和枪槽，`CGun`／`CArmor` 持有自身模板与资源。`ZBrotherRenderer.h/.cpp` 随后也已删除，角色绘制归 `CBrother`，枪械、盔甲和弹体各自持有绘制资源；`brother/` 根目录已无 Z 文件。变更与验证见 [Brother 所有权归并](brother-ownership-migration.md) 和 [绘制职责归并](brother-renderer-removal.md)。

拾取物目录归并：`CPickup.h/.cpp`、`CPickupPresentation.cpp` 和 `CLevelPickups.cpp` 集中在 `gameplay/pickup/`。逐文件核对仅头文件引用路径变化，原有实现和注释保留；测试独立放在 `tests/`，仍只使用一个工程。

拾取物目录层增补：`data/ZPickupCatalog.*` 与 `ZPickupEntry` 已删除。模板读取归 `CPickup::Template::Load`，关卡直接持有模板，展示标签留在测试；运行时不再依赖目录条目包装。

2026-09-17 增补：`ZPickupScene` 及过渡缓存 `ZPickupResources` / `ZPickupVisual` 已拆分并删除。职责回到 `CPickup`、`CSpritePlayer`、`CLevelObjectPool`、`CLevel` 和 `CPlayer::CollectItem`。`CLevelObjectPool` 现同时持有敌人和拾取物，后文仅持有敌人的描述属于此前阶段。详见 [拾取物组合层拆分](pickup-responsibility-migration.md)。

2026-09-17 目录归并：UI 与战斗共享的 28 个特效源码文件已从 `gameplay/` 移到 `effects/`。仅更新路径与引用，逐文件内容核对保留原实现和注释；各原对象仍负责自己的实例与生命周期，工程继续通配收集到三种产物。详见 [归并与验证记录](effects-directory-migration.md)。

以原职责拆分玩家、刷怪、输入面板、选择器、菜单绑定和 Movie 读取；把桌面适配及自建表示统一为 Z 命名。保留一个工程、三种产物和既有研究入口。没有修改 BIG、存档协议或原生脚本编号。

### 职责与依据

下表源码路径相对 `src/`。原函数位置指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`；不是恢复出来的原 `.cpp` 行号。

| 原组合 | 当前归属 | 核对依据与边界 |
|---|---|---|
| `CombatScene` 的玩家状态、经验、矿石、移动、射击、击退 | `gun_bros_re/gameplay/brother/CPlayer.*` | `CPlayer::AddExperience` 101185、`AddXplodium` 101116、`Move` 100623、`UpdateMovement` 101384、`UpdateShooting` 101314；玩家 BT。玩家状态只有一份，模型及生命状态为非拥有引用 |
| `SurvivalSession::ChooseSpawnNode` | `CEnemySpawner::GetSpawnPoint`，`CLayerPathLink/Mesh::GetSpawnLocation` | 146098、146112、166819、168115；LEVEL 与 MAP BT。保留节点锁定、最近五点、屏外边界和随机调用顺序 |
| 宿主击杀计数 | `CLevel::OnEnemyKilled` / `GetKills` | 119306；只在交付死亡事件时计数，Bind 重置，宿主读取同一计数 |
| 旧战斗组合层的敌人存储、预加载、UID、延迟生成与回收 | `gameplay/CLevelObjectPool.*` | 原 `levelObjectPool.cpp`：构造 145287、`GetEnemy` 145509、`Release` 145426、`Clear` 145701；保留 100 个敌人槽、活动上限以及尸体到实际回收前仍占槽的语义 |
| 旧战斗组合层的击杀奖励、分数、连杀、波次奖励和关卡事件队列 | `CLevel::RewardEnemy`、`ResolveWaveReward` 及 `CLevel` 事件入口 | `CLevel::OnEnemyKilled` 119306–119912、`OnWaveCleared` 116897；奖励及统计状态随关卡，死亡确认后直接进入关卡原生链，不再通过宿主公开容器二次轮询 |
| Powerup 编号白名单 | `CPowerUpSelector` 的 STORE 引用、模式标志与 `CPowerup` Flow 查询 | POWERUP 2/3/4 没有专用 STORE 条目且 Flow 不提供装备入口；删除 `IsPlayablePowerup`，不再用 `pack5` 编号重复表达资源事实 |
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
- Movie 的分类型解析职责已分离；2026-09-16 补充 `CMovie::Playback` 实例状态，并用于 Powerup Movie、选择器关闭和 InputPad 恢复通知，移除固定 300ms 回调。未恢复全部原 `CMovie*` 子类继承树或迁移所有菜单时钟；GL 资源缓存和绘制仍是桌面实现。实现与验证见 [运行时路线第十四节](runtime-alignment-roadmap.md)。
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
