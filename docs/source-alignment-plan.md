# engine 与 gun_bros_re 原版对齐计划

日期：2026-09-15。状态：用户已授权实施；本文件保留调研时的基线，变更与验证见 [重构结果](source-alignment-result.md)。

## 目标与范围

- 整理 `src/engine` 与 `src/gun_bros_re` 的命名和职责；Viewer 负责调用共享实现，保留现有研究入口。
- 有原版符号和职责依据的代码归回原类；完全自建部分使用 `Z` 前缀，`cheats/` 与 `debug/` 内命名豁免。
- 删除无职责的转发层、过时构建残留；拆开混合职责的组合。拆分的结果应使修改集中、调用简单，而非追求类和文件数量。
- 保持既有玩法、资源读取、存档格式、测试入口及产品能力。已有未实现行为仍记录为未实现，本次不顺带补完原版所有类。

## 调研依据与边界

检查了两个包的文件清单、关键组合类、平台实现、工程配置、测试残留，以及原工程符号清单和相关反编译片段。当前 engine 有 97 个文件，gun_bros_re 有 240 个文件；其中 `Original*` 文件 18 个，`Initial*` 文件 0 个。

原始依据：

- `_prep/_IDA_OUT/source_tree.md`：来自 ARMv7 的 N_SO/N_FUN，说明原文件和方法归属；符号不能恢复全部头文件或纯数据结构。
- `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`：核对原行为和调用顺序。此次重新查看了 `CEnemySpawner::GetSpawnPoint` 及 `GetSpawnPointOffScreen`（146098、146112）；此前已核对 `CLevel::Update` 中 CFlock 与对象更新的顺序。
- `_prep/docs/original-name-map.md`：仅作研究索引。部分旧路径和宿主归属已经过时，不能直接当最终映射表。
- 当前源码的原函数/BT 注释：作为定位线索，实施每项迁移前仍需回查对应原函数及相关 BT。

这是优先级和结构调研，不代表 337 个文件已经逐方法验证。未找到同名符号也不等于确认自创；未知项需继续查证。

## 一、建议补齐的命名规则

| 对象 | 建议规则 |
|---|---|
| 已核实的原版类、枚举和方法 | 使用原符号拼写与职责，包括大小写；原版没有 C 前缀的 `Mission`、`Planet` 等保持原名 |
| 完全自建的顶层类型 | 使用 `Z` 前缀；例如 `CWindow` → `ZWindow`，不用 `ZCWindow` |
| 自建模块文件 | 使用 `Z` 加明确职责；有主类型时与主类型一致 |
| 自建接口 | 同样 Z 开头，建议 `ZWindowOverlay` 等职责名；有原版接口证据则保留原名 |
| 函数、变量、成员 | 按职责命名，不给所有局部标识符机械加 Z；独立工具模块可用 Z 前缀命名空间表达来源 |
| 嵌套实现类型 | 随所属类型表达归属，不机械重复加 Z |
| 原版与宿主混合实现 | 先拆职责，再确定各部分名称；不把整个混合类改成某个原类名 |
| `Original`、`Native` 等前缀 | 按真实用途改名；“原资源导入”“桌面存储”应由职责名区分 |
| `Internal` 后缀 | 私有实现共享头确有用途时保留；清理其中混杂的职责，不机械删除后缀 |
| 生成的 `.inc` 数据 | 按消费者命名，保留生成器、版本和原件哈希；不能仅因文件去掉 Original 就丢弃数据来源 |
| 豁免 | `cheats/`、`debug/`；第三方源码不动；`main.cpp` 等入口沿用常规名称 |

对齐以原符号和职责为主；不要求复制原工程的整个目录树，也不必将 `CBullet.cpp` 改回原来的小写 `bullet.cpp`。不修改 BIG 资源名、脚本出口、持久化字段、存档编号等外部协议名称。

## 二、优先处理的实际问题

### 可以独立进行的命名整理

| 现状 | 依据及拟处理 |
|---|---|
| `ui/OriginalDialogPopup.h` | 实际类型已经是 `CDialogPopup`；原符号清单存在该类，文件对齐为 `CDialogPopup.h` |
| `engine/platform/CWindow.*` | 文件明确说明是 SDL 窗口/GL 适配，未沿用原 Cocoa 类结构；改为 `ZWindow.*` |
| `engine/platform/CMediaDecoder.*` | Windows 媒体解码，文件与主类型 `CMediaVideo` 也不一致；按视频读取与音频解码职责整理为 Z 模块 |
| `HostSettings`、`Utf8Arguments`、路径工具 | 属于桌面宿主设置、参数和路径适配，按职责使用 Z 命名 |
| `BroAIDeathmatch` | 实际包含本地伙伴输入和商店操作策略，另有 `DeathmatchBot`；按真实模式区分命名，避免把合作 bot 叫作死亡竞赛 AI |
| `OriginalProfile` / `NativeProfile` | 分别涉及原件读取导入和桌面编号 DataStore 存储；候选职责名为 `ZProfileImport` / `ZProfileStorage`，共用编解码只保留一份 |

`CAudioPlayer`、`CShaderProgram`、`CQuadBatch` 等需核实适配与原逻辑的范围。原版确实存在 `platform::graphics::CShaderProgram`，但当前实现是独立加载 GLSL 文件的包装；不能仅看同名就判定已对齐。

### 应先拆职责的组合

| 当前组合 | 原职责或建议归属 | 剩余宿主职责 |
|---|---|---|
| `CombatScene` | 玩家移动/射击/经验：`CPlayer`；击杀奖励、波次奖励和对象更新：`CLevel`；敌人导航按 `CEnemy` 等核对 | 场景装配、桌面输入连接、本地模拟扩展 |
| `SurvivalSession` | 刷怪点选择回 `CEnemySpawner`；关卡更新/重开和对象生命周期回 `CLevel`；跨关卡流程核对 `CGame` | Windows 运行装配及必要调用协调 |
| `SurvivalHud`、两个 `Original*Selector.cpp` | 核对并分离 `CInputPad`、`CPowerUpSelector`、暂停和提示相关原类 | 鼠标/键盘到原操作的转换 |
| `OriginalPromotionPopup` | 当前合并 `CMenuInviteFriends`、`CMenuIncentives`；分别核对原菜单绑定与回调 | 本地服务替代通过已有宿主调用接入 |
| `OriginalLoadingSplash` | 核对 `CMenuSplash` 的章节、刷新和加载回调 | 桌面加载进度/窗口呈现 |
| `GameFrontEnd` / `MenuInternal` | 按具体菜单及 `CGame` / `CGunBros` 职责核对；不整体冒名一个原菜单 | 桌面启动、账户选择及本地服务装配 |
| `WeaponEffects`、`EnemyCombat`、`PlayerModel`、`EnemyModel` | 分开原对象行为、资源模板、渲染缓存；已有 `CBullet`、`CGun` 等直接复用 | 必要的共享渲染缓存和宿主绘制适配 |
| `MovieRenderer` | 章节/对象/区域/文本更新与原 `CMovie*` 家族对照 | GL 缓存和绘制后端保留为 Z 模块 |
| `MapWorldInternal` 及相关实现 | 地图加载、对象更新、绘制和正式游戏入口分开；原 `CMap` 等负责对应职责 | 调用这些能力的宿主流程 |
| `PackTables` / `ResourcePacks` | 游戏条目解析与通用包读取保持分离；前者注释提及原 `CGunBros`，需查真实消费者再决定归位范围 | 确有复用价值的加载通知/读取辅助可保留为 Z 模块 |

`CombatScene` 与 `SurvivalSession` 不应被视为最终必须保留的类。目标是迁完职责后删除或缩减为必要的 Z 宿主装配；不能提前承诺删除所有协调代码，再把同样的复杂度分摊给 Game、Viewer 和 Tests。

迁移前为每个对象确定唯一所有者、非拥有引用及销毁顺序。尤其注意 `CLevel::Template` 的地址稳定性、地图/资源缓存生存期、重开清理，以及暂停、CFlock、对象运动、击杀奖励和 HUD 刷新的执行顺序。不能在新旧类中各留一份游戏状态。

## 三、必须一起补上的依赖与测试整理

目标方向：Game / Viewer / Tests 调用 gun_bros_re 和 engine；gun_bros_re 调用 engine；engine 不依赖上层。Viewer 可以保留自身菜单、摄像机操作和研究显示，但游戏算法、解析器和共享渲染实现在所属包内。

目前没有搜到 engine 源码显式 include gun_bros_re、viewer 或 tests；但工程对编译单元强制包含 `gun_bros_re/debug/Capture.h`，存在构建层的隐式上层依赖。

测试也尚未完成职责隔离：

- `SurvivalLoop.cpp` 仍有自动点击/按键、`checkControls`、`checkFailures`、截图和断言。
- `SurvivalScenario.h` 暴露大量测试上下文；`SurvivalSession.h` 还声明 `CheckLevelSounds`、`CheckTriggerRoutes`。
- 迁移这些驱动和判断到 tests。共享游戏模块提供正常的输入、单步更新和必要只读状态；测试通过这些能力驱动真实流程。
- 不把测试专用逻辑简单改名为 `Z...` 后继续留在游戏里，也不以大量测试专用回调替代此次职责拆分。
- 已有只读诊断不因名字包含 Check/State 就一律删除；按是否属于稳定可观察状态判断。

## 四、BuildFeatures 清理方案

`BuildFeatures.h` 目前只有两个宏默认值以及禁用截图时返回 false 的 `GB_SAVE_FRAME` 宏。项目通过 `ForcedIncludeFiles` 隐式注入它和 `Capture.h`。

建议：

1. 移除当前所有产品恒为 1 的 `GB_ENABLE_CHEATS` 条件及工程定义，保留运行时作弊启用/禁用行为。
2. 将 `GB_SAVE_FRAME` 改为显式截图函数调用，调用方显式包含 `Capture.h`；移走夹在游戏更新中的测试截图调度。
3. 截图产品差异先集中到 `debug/Capture` 和必要入口。保留少量确有产品差异的 `GB_ENABLE_CAPTURE` 条件是可接受的，不需要重新造一个 Features 头或通用功能注册框架。
4. 删除 `BuildFeatures.h` 和这套强制包含项，核对所有配置编译分支。Release 禁用实现需明确返回失败，不能把空操作报告为截图成功。

保持当前能力表：

| 产品 | Debug | Release |
|---|---|---|
| Game / Viewer | 作弊可用，截图可用，不编译测试源码 | 作弊可用，截图关闭，不编译测试源码 |
| Tests | 测试及截图可用 | 测试及截图可用 |

继续一个 vcxproj；两种配置构建 Game 均顺带构建 Viewer 和 Tests。是否自动运行、运行哪些测试由测试入口决定，不能将“同时编译 Tests”变成“每次跑 500 波”。

## 五、实施顺序与验收

方案通过后，每阶段先补全该范围的“旧名 → 新归属/新名 → 原证据 → 调用者 → 验证项”清单，再修改。纯重命名和职责迁移分开，便于审阅和定位问题。

1. **清理构建残留**：完成上述头、宏、显式依赖调整。检查 Debug/Release 三产品构建、截图能力及作弊运行时开关。
2. **独立命名整理**：先平台适配、明确的原类文件名、宿主设置和导入/存储命名；同步 include、保护宏、工程项及文档引用。生成数据同步生成器输出目标，保留原件只读。
3. **游戏核心归位**：分批迁移 CPlayer、CEnemySpawner、CLevel 职责，收缩两个场景组合；同时迁出对应测试驱动。验证短波次、刷怪、移动射击、受伤死亡、重开和奖励不重复。
4. **UI 与渲染职责整理**：拆选择器/弹窗/菜单，梳理 Movie、模型、特效和地图。验证相关菜单交互、暂停恢复、切换、资源缓存和必要截图。
5. **收尾**：剩余自建模块按 Z 规则处理，删除无用兼容别名、仅转发包装和过时工程引用；同步命名映射、README 和项目约定。保留用户注释，旧阶段说明补充现状，不直接删除其依据。

验收约束：

- 每阶段选择受影响的现有检查；最终验证 Debug/Release 三产品及短流程。
- 常规回归使用短波次与直达末波边界；LongRun 的四星球 500 波只作显式专项，不放入默认构建或每阶段验收。
- 不重复运行全量截图基线；UI 变更只检查涉及页面，并区分截图生成成功与人工视觉符合原版。
- 自动执行显式 `--mute`，正常 F5 保留声音；资源验证仍执行。
- 不改 BIG、原始存档、持久化格式或脚本协议；验证存档未知字段与未修改记录保持原样。
- 检查跨包显式 include、构建强制 include 和链接来源；Game/Viewer 不包含 tests 源码。
- 保留命令、退出码和相关结果；必要记录放不会被测试清空的目录。

## 原调研的范围选择（已批准）

建议采用上面的规则：Z 用于自建模块文件及顶层类型，局部变量和普通成员不机械加 Z；有意义的 Internal 后缀保留；截图保留当前产品差异。最先实施构建清理和低风险命名，再逐类归位。未核实原归属的类单列，不靠命名伪装成已经复刻完成。
