# 下一阶段主线：运行行为与原版职责对齐

日期：2026-09-17。

最新进展：夜间迁移已删除 `ZWeaponEffects.h/.cpp`，UI、地图粒子、弹体、附属效果与音频适配完成职责拆分；R03 已移除，R10 的枪械／手雷速度链已核实并修正。详细依据、阶段验收及边界见 [夜间迁移交接](weapon-effects-night-migration.md)。下文旧批次记录保留历史语境。

状态：用户已授权依次修复；首批统一 R09 粒子核心，第二批恢复播放器持有粒子、共享池及停止语义。用户指出 Z 组合文件仍未归位，第三批完成敌人与关卡的文件归属，第四批将 Powerup 播放职责收归同一原对象，第五批统一 R01 道具执行链，第六批删除 `ZPowerupScene` 并归位本地道具策略。具体结果与验证见本文末尾，其余事项以各条目记录为准。上一轮文件拆分与命名对齐的结果见 [source-alignment-result.md](source-alignment-result.md)。

## 目标与判断原则

当前游戏已经能够运行，下一阶段优先消除替代原行为的编号白名单、手写状态、启发式资源修补和重复执行逻辑，再恢复原对象职责、整理文件归属。不能以成功构建或现有回归通过代替原版行为核对。

- BIG 已有的资源事实继续由 BIG 读取；Flow 已表达的行为交给解释器和对应原生对象执行。
- 通用脚本、Movie、Sprite、绘制能力属于引擎；商品规则、道具效果、角色移动与关卡生命周期属于游戏对象。不能把所有硬编码一律搬到 engine。
- 原程序明确实现的资源身份判断、协议编号和算法常量允许保留，注明原函数依据；不能把它们误删为自建白名单。
- 用户明确要求的本地机器人难度、桌面输入和本地服务规则允许保留，但应集中在对应 Z 适配或策略中，不能散落为通用玩法限制。
- “根据资源推断”不自动等于正确的数据驱动。根据图像边界、引用是否存在、脚本状态数量推断玩法，再据此过滤或改写数据，仍需原消费者证据。
- 不把表挪到 JSON、CSV、配置或生成脚本后宣称解决；不靠统一改前缀完成职责拆分。

## 调研范围与证据等级

初始静态检查覆盖 `src/engine`、`src/gun_bros_re`，对有关路径追到 Viewer 和 Tests。抽查原符号清单、反编译代码以及 POWERUP、PROP、PARTICLE BT；登记时没有修改运行时代码、构建或运行游戏。下列清单保留发现时的情况，不是全仓库逐方法验证结果；后续修复另列实施记录。

分类：

- **已确认**：源码能够直接证明存在该实现或差异；不等于已复现每种运行后果。
- **待核对**：已定位具体规则，尚未证明其违背原版，实施前必须回查原函数、BT 和必要原始字节。
- **保留并归位**：已有原生证据或用户授权，不改变需求，仅纠正归属与重复表达。
- **研究路径**：明确限定于旧预览、导入或显式研究入口，不能描述为正式游戏全部采用该路径。

原反编译位置均指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`，不是恢复后的原 `.cpp` 行号。

## 一、白名单、替代算法及隐含过滤台账

### R01：Powerup 按编号分流执行生命周期

- 原位置：已删除的 `ZPowerupScene.cpp::Use`。当前入口见 [CBrotherPowerups.cpp](../src/gun_bros_re/gameplay/brother/CBrotherPowerups.cpp)、[CLevelPowerups.cpp](../src/gun_bros_re/gameplay/level/CLevelPowerups.cpp)。
- 初始发现：`field112 != 0 || itemIndex == 0 || itemIndex == 10 || itemIndex == 11` 决定是否进入持续播放路径；其余路径创建局部 `CPowerup` 并立即消费一批 action。这是手工列举哪些道具需要持续运行。
- 当前实现：第五批删除上述分流及宿主的原生效果执行循环；所有实际使用进入持久 `CPowerup`，角色 native 与实时查询由原对象负责。`field112` 仅保留原死亡状态使用条件，未改成另一份 Movie 依赖白名单。验证结果见第十节。
- 影响：正式道具使用路径。未来或尚未覆盖的脚本即使能产生 Movie、延迟事件，也不会自动获得相应生命周期。当前逐道具效果是否错误需专项验证。
- 原证据：`CPowerup::Equip` 188694、`Use` 188704 通过脚本出口执行；`Update` 188652 更新 Movie、粒子和计时事件；`CLevel::UsePowerup` 116346 登记活动对象。`entries/powerup_template.bt` 描述脚本与引用，没有此三项 Movie 白名单字段。
- 目标：以原 `CPowerup` 生命周期和 native 请求驱动持续执行；移除编号分流，不以扫描到 Movie 引用替代真实执行语义。

### R02：用固定 300ms 模拟 UI 完成回调

本轮实施方案（已获用户授权）：在 `CMovie` 内补充独立于共享资源缓存的播放状态，按原版章节边界、方向和循环规则推进；选择器依据主 Movie／列表 Movie 完成状态通知道具；InputPad 依据自身状态序列完成通知。移除 Powerup 的独立 UI 倒计时，保留 Flow native 8 定时器。验收覆盖边界跨越、循环、暂停、取消、重复通知、同资源多实例，以及真实 BIG 道具流程。原版 InputPad 底栏还有原生淡入逻辑，不能把所有完成条件都替换成 Movie 时长。

- 位置：[CPowerupPresentation.cpp](../src/gun_bros_re/gameplay/powerup/CPowerupPresentation.cpp)，`ApplyPresentationActions`、`UpdatePresentation`；原 `ZPowerupMoviePlayer` 的逻辑已在第四批收归原对象。
- 原问题：native 2/4/5/13 先设置固定 300ms，倒计时归零后直接 `HandleEvent`；即使选择器关闭改用章节时长，仍存在两套播放进度。
- 本轮实现：通用 `CMovie::Playback` 维护实例进度；`CPowerUpSelector` 根据列表反向播放／主框章节完成发送事件；`CInputPad` 根据底栏与外围 UI 恢复状态发送事件。Powerup 不再持有 UI 完成倒计时，native 8 的 Flow 定时器保留。
- 原版纠正：native 2 是 `Hide`，没有与 native 5 相同的 `OnSelectorHidden` 通知；native 4 通知事件 1，native 5 通知事件 2，native 13 通知事件 4。InputPad 已就绪时允许立即回调，不能强行补一个延时。
- 验证状态及范围见第十四节；其他菜单的手动 Movie 时钟仍需按消费者逐步接入，不把本轮记作全部动画系统迁移完成。

### R03：光束动画的启发式资源修补（已完成）

- 2026-09-17：删除 `IsBeamSourceAnimation/BeamBodyAnimation`。真实 pack5 BULLET 逻辑 104 是物理文件 108，资源绑定主体动画 1、端帽 2/3；Flow 后续换主体不改变 Bind 固定的端帽。当前执行位于 `CBullet.cpp`、`CBulletDrawing.cpp`，真实 BIG 断言及武器／特效回归通过。以下保留原问题记录，旧文件已删除。

- 原位置：已删除的 `ZWeaponEffects.cpp`，`IsBeamSourceAnimation`、`Impl::BeamBodyAnimation` 及 `Draw` 调用。
- 已确认：根据第一帧边界 `bottom > 0` 判断动画是源端帽，再将原动画编号减一。注释明确称其为有意偏离原始字节，针对线索为 `pack5 BULLET104`，但规则实际应用于光束绘制。
- 影响：实际绘制路径；把按 ID 的补丁改成外观判定仍然是补丁，不能据此证明原语义。
- 目标：回查 `CBullet::Bind` 63647、`Draw` 62998、Sprite 映射与原样本，区分读取/槽位解释错误和原版真实行为；明确根因后替换，不能仅因当前图像好看而保留。

### R04：武器 visualOnly 推断参与商店过滤和测试豁免

- 位置：[ZWeaponCatalog.cpp](../src/gun_bros_re/data/ZWeaponCatalog.cpp)，`LoadWeaponCatalog`；[ZStoreMenu.cpp](../src/gun_bros_re/ui/ZStoreMenu.cpp)，`MatchesEquipmentSlot`。
- 已确认：以“脚本出口为空或找不到默认/脚本 BULLET 引用”推断 `visualOnly`，商店要求 `!visualOnly && hasStoreEntry`。这不是显式编号表，但会把实现侧推断变成可购买内容规则。
- 待核对：不能仅据此断言当前某件商品已经被误隐藏。需按原 `CStoreAggregator` 过滤和 `CGun` 消费链逐项验证。
- 测试风险：`ArenaChecks.cpp`、`M35MeshChecks.cpp`、`StoreChecks.cpp` 也使用该字段跳过部分断言或样本。若分类错误，测试可能一起漏查。
- 目标：分开“资源可解析”“当前渲染支持”“原商品允许展示/购买”；测试样本和预期使用原证据，不能继续仅沿用同一个启发式排除条件。

### R05：以零价格推断奖励商品并隐藏

- 位置：[ZStoreMenu.cpp](../src/gun_bros_re/ui/ZStoreMenu.cpp)，`MatchesEquipmentSlot` 的消耗品分支。
- 已确认：两种价格均为零时直接排除，注释推断其属于奖励载荷。
- 待核对：价格为零本身不证明不能展示/购买；必须核对 STORE 类别、标志、原 `CStoreAggregator` 条件和奖励消费链。尚未证明当前样本中存在误过滤。
- 目标：还原原筛选条件；保留未知，不通过“免费就是奖励”的概括补造规则。

### R06：DM 机器人白名单重复散落

- 原位置：已删除 `ZPowerupScene` 的匹配、预算和选号，以及选择器远端商店过滤。
- 初始发现：多处分别写 `pack5` 和 `1/8/9/13`；购买顺序另写 `9/8/1/13`。选择器过滤只检查 localIndex，其他路径还检查包，表达方式不一致。
- 当前归属：[ZLocalPVPBot.cpp](../src/gun_bros_re/gameplay/brother/bot/ZLocalPVPBot.cpp) 集中允许集合、预算分类、选号和购买偏好；UI 与运行时共用 `IsHealthPowerup/IsGrenadePowerup`，同时核对包与编号。购买顺序继续作为本地策略保留。
- **保留并归位**：用户明确要求 Easy/Normal 限定标准手雷与血包，Normal 无限供应，Hard 扩展可用范围。依据见 [DM 难度文档](deathmatch-ending-and-difficulty.md)。这不是已经删除的通用 `IsPlayablePowerup` 白名单，不应擅自扩大机器人允许集合。
- 目标：把允许集合、预算分类和购买偏好集中在本地机器人策略，由 UI 与运行时消费同一结果；原 STORE 模式限制、Flow CanUse、冷却和玩家库存规则仍独立生效。

### R07：把 Movie 依赖等同于空袭的机器人分类

- 原位置：已删除 `ZPowerupScene::Init` 的 `BotUseRules`；当前在 [ZLocalCoopBot.cpp](../src/gun_bros_re/gameplay/brother/bot/ZLocalCoopBot.cpp) 的 `CanUseSelectedPowerup`。
- 已确认：存在 `sectionOrType == 253` 的 Movie 依赖且 `field112 == 0` 就分类为空袭，影响 `CanBotUseSelected` 的敌人数阈值。
- 待核对：这是本地战术启发式，不是原资源中的空袭类型标志。阈值本身有用户策略背景，见 [Live 机器人策略](live-cheats-and-bot-policy.md)；不能误当作原道具可用性。
- 目标：将策略推断移出通用道具生命周期；记录适用资源及分类依据，不以依赖存在证明效果一定发生。

### R08：地图道具的旧手写状态与转场粒子表

- 位置：[ZMapWorld.cpp](../src/gun_bros_re/gameplay/ZMapWorld.cpp) 的 `InteractiveKindFor`、`BuildInteractiveStates`；[ZMapParticles.cpp](../src/gun_bros_re/gameplay/ZMapParticles.cpp) 的 `StartTransitionParticlesForProp`；常量位于 [ZMapResources.h](../src/gun_bros_re/gameplay/ZMapResources.h)。
- 已确认：按包和编号识别掩体、桶、尖塔，手写状态数、动画编号、碰撞开关、脚本资源下标和 z 组。
- 路径边界：正式关卡通过 `ZMapPropWorld` 为有脚本的道具创建 `CProp`；绘制和碰撞存在优先使用 runtime 的分支。旧 Viewer 手动转场仍调用手写表，共享加载也构建这些状态，不能把两条路径混称为一个已完成的 Flow 实现。
- 原依据：`entries/prop_template.bt`、`CProp::Bind` 124863、`FunctionResolver` 及实际 Flow。目标是预览与游戏共用原运行对象，保留研究入口而替换其行为来源。

### R09：粒子重复实现与凭空寿命

- 实施进度：共享粒子核心、地图系统 20 槽／199 可领取粒子、独立 `CEffectLayer` 粒子 20 槽、角色强化持有关系、相对／世界坐标和锚点失效收尾已恢复；地图动态粒子逐粒进入绘制队列。UI、强化和预放置效果没有合并进地图系统。预放置 `CParticleEffectProp` 的完整类归位与独立池、原全局随机序列以及全部粒子参数仍待逐项核对，不能将本条整体关闭。

- 原位置：`ZMapParticles.cpp::SpawnParticle` 与已删除的 `ZWeaponEffects.cpp::Impl::SpawnParticle`，现共用 `CParticleEffectPlayer/CParticle`。
- 已确认：两套出生、随机、运动和生命周期实现。地图版对非正寿命补 750ms；武器版直接不生成。地图版还设有每效果 2048 粒子、每发射器每帧 64 次生成上限，来源待核对。
- 影响：地图预览与战斗/菜单可能对相同资源得出不同结果；尚未统计当前 BIG 是否实际触发所有回退和上限。
- 目标：核对 `CParticle`、`CParticleEmitter`、`CParticleEffectPlayer` 及 `entries/particle_effect.bt`，恢复一套共享行为。通用机制是否进入 engine 依原职责和依赖决定，不能直接把整个 `ZWeaponEffects` 搬入引擎。

### R10：固定移动速度与弹速倍率

- 已确认：[CPlayer.cpp](../src/gun_bros_re/gameplay/brother/CPlayer.cpp) 的基础速度为 220，输入归一化；与原 `CPlayer::UpdateMovement` 101384 消费摇杆幅度的分段计算不同。Bot 也复制了 220 的速度假设。
- 弹速已核对并修正（2026-09-17）：原 `CGun::FireBullet` 128140–128145 与 `CBrother::ThrowGrenade` 138729–138735 使用 IEEE float ±430；枪械另乘 Flow 参数 4 的 8.8 倍率，`CBullet::UpdateDirect` 按毫秒 ×0.001 积分。现由 CGun／CBrother 产生绝对初速，CLevel 不再持有 450。真实枪械倍率 0/128/256/512 的 100ms 位移为 0/21.5/43/86，相关回归通过。玩家／Bot 移动速度仍待处理。
- 目标：输入适配仅提供方向和幅度，玩法计算归 `CPlayer`；核对速度单位和倍率消费链后消除无依据的宿主缩放。

### R11：目录位置被当成默认资源、缺引用可能退到首项

- 位置：[CPowerUpSelector.h](../src/gun_bros_re/ui/CPowerUpSelector.h)，`m_selected = 13`（原位于已删除的 `ZPowerupScene.h`）；[ZSurvivalLoop.cpp](../src/gun_bros_re/gameplay/ZSurvivalLoop.cpp)，`brotherWeaponSlot = 0` 后查找伙伴武器。初始选择默认值仍须按本条核对，文件归位未解决该问题。
- 已确认：默认选择写死为目录位置；伙伴武器查找没有与玩家武器路径相同的 `found` 检查，未命中时仍保留首项。上游验证可能阻止正式路径触发，需检查可达性，不能直接声称现有存档已被换枪。
- 目标：区分目录位置与资源引用；原默认值从对应配置/Flow 出口取得。必需引用缺失时记录来源并返回失败，不能静默换成第一件资源。

### R12：用脚本复杂度识别生存星球

- 位置：[ZProfileImport.cpp](../src/gun_bros_re/data/ZProfileImport.cpp)，`ImportProfile`。
- 已确认：以 `waveLimit == 500 && script.GetStates().size() > 100` 筛选，再使用固定 `pack2/7/9/12` 映射星球进度。
- 路径边界：属于独立导入函数；默认游戏的编号存档走 `LoadProfile`，不能说正式加载都使用该判断。
- 目标：按 Planet、LEVEL 引用及存档客户端语义还原进度映射。500 波需求不能作为“脚本必须超过 100 个状态”的依据。

### R13：未实现 native 返回零或临时变量

- 位置：[CEnemy.cpp](../src/gun_bros_re/gameplay/CEnemy.cpp) 的函数/变量解析；[CLevel.cpp](../src/gun_bros_re/gameplay/CLevel.cpp) 的 `FunctionResolver`；[ZGameScriptObject.cpp](../src/gun_bros_re/gameplay/ZGameScriptObject.cpp) 的分派。
- 已确认：部分未知函数日志后返回零，未知变量使用 scratch；CGame 分派除随机函数外存在直接返回零的路径。
- 目标：统计实际 BIG 的调用覆盖，将原版规定的空实现、未覆盖能力、缺上下文分别记录；只实现经证据确认的行为。不能笼统把所有 return 0 都判错，也不能用默认值掩盖未实现。

## 二、应保留或需要防止误判的内容

| 项目 | 结论与依据 |
|---|---|
| 旧 `IsPlayablePowerup` | 当前 src 已无该符号，`ZPowerupCatalog` 遍历 BIG 条目。此前删除结果仍有效；R01、R06 是不同层次的剩余问题 |
| 炮塔 `pack5/19` 判断 | 原 `CBrother::OnPowerupButton` 约138000行存在相同包 hash/编号判断；保留语义，归回原玩法职责 |
| 新存档默认装备表 | `ZProfileStorage::CreateProfileArchive` 指向原 `CPlayerConfiguration::Reset` 171865；不能仅因数组包含编号而删除。迁移时继续核对原函数，属性仍从 BIG 读取 |
| 菜单 `.inc` 常量 | `ZMenuData.inc` 等保留原可执行文件提取来源和哈希；逐表核实消费者，不等同手填资源数据 |
| `kPlanetMaps` | `ZGameFrontEnd` 仅在显式旧研究存档分支使用；原生存档路径读取 LEVEL 的 mapRef。按研究入口隔离，不混淆正式主线 |
| 无命名枪口时保留原点 | `GetPlayerMuzzle` 注释指向原 `CGun::FireBullet` 的零初始化行为；属于待原函数复核的合法默认候选，不能统一当作造数 |
| 本地 bot 决策、桌面窗口与音频/GL 缓存 | 自建职责允许保留 Z；不要通过冒名原类掩盖平台适配和用户扩展 |

## 三、职责与文件组织台账

以下行数仅为2026-09-16静态快照，大小用于定位，不能单凭行数判断设计好坏。

| 当前组合 | 主要问题 | 目标职责 |
|---|---|---|
| `ZWeaponEffects.cpp`，1387行 | 弹体实例、命中、粒子、拖尾、声音和缓存混合 | `CBullet`、粒子运行对象、对象池及必要渲染/音频适配分别负责 |
| `ZMovieRenderer` / `ZMovieKeyFrame` | 插值、父子锚点、区域计算、类型分派与 GL 绘制集中 | 对照 `CMovie*` 原对象职责，后端只负责绘制；不要求一次恢复所有继承层级 |
| `ZMenuInternal.h`，1082行；`ZStoreMenu.cpp`，1396行 | 页面分文件但共享状态仍很宽，通用菜单承载多页输入和特效 | `CMenuSystem`、菜单控件、`CMenuStore/Option/Group` 及各页面拥有自身状态 |
| `CLevelObjectPool` | 主要恢复敌人路径，Bullet/Prop/Pickup/Platform 的所有权尚未完整归位 | 逐类核对原生成、更新、回收与同帧执行顺序 |
| `ZSurvivalLoop.cpp`，1361行 | 资源装配、模式分支、开发选项和逐帧调度集中 | 先明确 `CGame/CLevel` 唯一状态所有者，再缩小宿主入口 |
| `ZEnemyCombat.cpp`、`ZLiveCombat.cpp`、`ZDeathmatchCombat.cpp` | 实际包含大量 `CEnemy::` 或 `CLevel::` 成员实现，文件名表达不准确 | 随职责归位调整文件和工程引用，保留原注释含义 |

关于 `CLevelxxx`：`CLevelActors/Combat/Projectiles/Runtime/World.cpp` 是 `CLevel` 的分文件实现，不存在对应同名自建类。`CLevelObjectPool`、`CLevelIndicator` 则在原版符号中存在。可按 `gameplay/level/` 聚合关卡文件，但目录分组、原类身份和行为拆分分别判断，不为恢复一个 `level.cpp` 合成巨型文件。

## 四、实施顺序与阶段验收

本计划确定工作主线，不表示允许跳过每阶段原证据调研。顺序为：调研与具体方案 → 范围审核 → 分解任务 → 实现 → 针对性验证 → 记录结果；用户已授权的具体范围不重复请求批准。

1. **补齐证据与可达性清单。** 先处理 R01–R05、R09 的来源核对，列明正式/Viewer/研究调用方、原函数、BT、资源定位及未确认项。验收：待核对项不被提前写成已证实缺陷，测试过滤条件也纳入检查。
2. **统一粒子运行实现。** 优先完成 R09，从地图、武器和菜单共同调用入手；核对零寿命、随机、生成时序、循环停止与已有粒子回收。验收：同一资源不因调用入口不同使用另一套算法；保留原样本与研究入口。
3. **恢复道具与武器的真实执行链。** 先 R08，再 R01/R02/R03；其中 Powerup UI 事件依赖的最小 Movie 能力在本阶段补齐。验收：不靠资源编号选择执行器，不靠图像外观改动画引用，不靠猜定时模拟完成；使用失败不错误扣库存，暂停、死亡和重开可收尾。
4. **核对玩法与内容筛选。** R04/R05/R10/R11/R12/R13；R06/R07 同期收拢机器人策略，保留用户难度要求。验收：商品规则由原消费者决定，错误引用不换首项，实际触发的未实现 native 可定位，策略不改变道具固有能力。
5. **恢复 Movie、菜单和对象所有权。** 先公共 Movie/控件，再各菜单状态；对象池按类别分批，最后缩小 `RunSurvivalSession`。验收：同一对象状态只有一个所有者，Game/Viewer/Tests 调用共享行为，更新与回收顺序对应原版。
6. **随批整理命名，最后做性能优化。** 更新文件映射及 vcxproj 引用，保留单工程三产物。只有实际测量表明热点时才优化；不以缓存、预展开或固定容量改变资源语义。

每阶段根据影响选择现有检查，所有自动运行显式 `--mute`。从 `tests/run.ps1` 核对实际用例再执行，保留命令、退出码和必要截图，不重复全量截图基线。源码阶段需要相应构建与运行验证；仅文档修改不冒称完成游戏回归。

## 五、初始审计交付

- 已登记前轮八类职责问题及本轮13项行为/过滤候选，明确原生规则、用户策略、旧研究路径与未证实项。
- 未删除白名单、未修改玩法、未改 BIG 或存档原件、未调整测试断言。
- 后续每项完成后在对应 R 编号下补充：原资源定位与原函数、最终代码归属、去除的替代规则、受影响路径、验证命令/退出码及剩余限制。台账条目不能仅以“改名完成”关闭。

## 六、实施记录：R09 共享粒子核心（2026-09-16）

### 改动与证据

- 新增 `gameplay/CParticle.*`：接管出生位置、动画位选择、速度、通道插值、寿命及旋转。原符号来自 `particle.cpp`；核对 `Spawn` 133114、`IsDone` 133278、`RefreshInterpolator` 133375、`Update` 133703，以及 `entries/particle_effect.bt`。
- 新增 `gameplay/CParticleEffectPlayer.*`：地图和 `ZWeaponEffects` 共用发射时钟；菜单经 `ZWeaponEffects` 使用同一实现。依据 `UpdateEmitters` 131393、`Update` 131499 核对随机间隔的整数毫秒截断、零间隔每次更新只生成一次，以及先更新存量粒子、再生成新粒子的顺序。
- 删除地图补造的 750ms 寿命、2048 粒子与每帧 64 次生成限制。零寿命粒子保留出生帧，下一次更新按 `IsDone` 回收；不补时长，也不因入口不同采用另一套出生规则。
- 修正每个插值端点独立随机、进入后续关键帧重新取值、`keepPreviousStart` 保留前一段起始值、单次更新跨多段时选最后一个跨越关键帧。寿命按每个通道**最后一项**的结束时间计算。
- 修正 Line 出生图案：原 `CParticleSpawnPatternLine::GetPosition` 132066 返回端点差乘随机比例，不额外加第一个端点。加速度保留发射器局部坐标，位移再按朝向旋转。
- 地图与武器仍分别拥有绘制缓存、坐标投影和实例元数据；没有把游戏粒子模板依赖搬进 engine。旧注释随职责迁移保留。

### 验证

- 新增 `tests/checks/ParticleRuntimeChecks.*`，接入已有 `weapon-effects`：覆盖零寿命、出生年龄、Line 非零起点、最后关键帧寿命、延迟关键帧、保留起始值、跨段更新、独立随机端点与旋转加速度。
- 修改前基线：`pwsh -File tests/run.ps1 -Configuration Debug -Case weapon-effects,pickup-render,deathmatch-feedback -NoBuild`，退出码 0，3/3 通过。
- 首轮实现：同上命令，退出码 0，3/3 通过，受保护存档变化 0。
- 零寿命出生帧修正后：`pwsh -File tests/run.ps1 -Configuration Debug -Case weapon-effects,pickup-render,refinery-menu,postgame-menu -NoBuild`，退出码 0，4/4 通过，受保护存档变化 0。
- 最终边界检查：`pwsh -File tests/run.ps1 -Configuration Debug -Case weapon-effects -NoBuild`，退出码 0；日志明确包含 `[particle-check] lifecycle, key transitions and local acceleration passed`。
- 地图路径：`bin/Debug/GunBrosTests.exe --cover-scale-study --big E:/coding_projects/c_projects/gun_bro_re/big --test-output E:/coding_projects/c_projects/gun_bro_re/obj/runtime-particle-cover --mute`，退出码 0。当前样本 17 个粒子、世界半径 50.396，三种视口使用同一份粒子状态；已检查生成画面。旧研究记录的数值保留为修改前历史结果，不作为新实现必须凑出的目标。
- 最终 Debug Game、Viewer、Tests 均构建成功，`git diff --check` 通过。构建宿主曾遇到重复 `Path/PATH` 环境项及沙盒 FileTracker 访问失败；在子进程规范化环境并通过允许的构建权限后成功，没有修改工程配置或系统环境。未执行 Release 或全量截图回归。
- 本地过程日志：`obj/runtime-alignment-particles-{baseline,after,final,boundary,cover}.log`；地图截图在 `obj/runtime-particle-cover/`。这些为忽略的验证产物，不是新的运行数据源。

### 首批结束时保留的差异与下一步

- `CParticleEffectPlayer` 本批只恢复共享发射职责；原 `CParticlePool` 的按所有者分配、耗尽行为、完整停止语义及逐分支发射窗口尚未恢复。移除无依据的宿主上限不等于原版没有容量限制。
- 确定性随机流仍为桌面实现，不声称随机序列与 iOS 全局 `Utility` 一致。共享机制保证入口采用同一规则，不要求不同宿主随机种子生成相同图像。
- R08 的 Viewer 手写转场表仍在；本批只替换其粒子执行器，不将其误写为已改成 Flow。接下来继续处理 R09 剩余生命周期证据，再进入 R08，随后 R01/R02/R03。

## 七、实施记录：R09 粒子池与停止语义（2026-09-16）

### 最终职责

- `CParticleEffectPlayer` 持有活动粒子的池索引，负责出生、更新、回收和完成状态。删除地图、战斗各自的粒子容器及回收循环；宿主只更新锚点、读取粒子并绘制。
- `CParticlePool` 预分配实际存储并复用槽位，多个播放器共享。按 `Allocate` 92882 与 `UpdateEmitters` 131393 的空闲栈语义，名义分配 N 个槽时槽 0 保留、可用 N−1 个。池耗尽仍消费本次发射间隔，不积压为稍后的补发，不扩容冒充原池。
- `StopSpawning` 停止发射，存量粒子继续更新直到结束；`Stop`、重新 `Start`、重绑及析构释放已有粒子。移动播放器转移所有权，不复制活动槽。依据 131289、131311、131340、131378；CBullet 移除等待拖尾依据 62381。
- 发射窗口使用原 `UpdateEmitters` 的前一时刻判定；仅跨起点不会提前出生，完整跨窗会按窗口时长发射。保留间隔余数、循环换周、零间隔每更新一次、先更新旧粒子再生成新粒子的顺序。
- 区分两个寿命概念：单粒子 `IsDone` 看每通道末项；Emitter 的预分配和无发射周期效果的截止时间看**所有关键帧**的最大结束时间。`GetParticleCount` 按最大寿命/最小发射间隔取整，任一间隔为零返回 1，依据 131811、131970。

### 已接入的所有者

| 所有者 | 名义分配 | 接入位置与依据 |
|---|---:|---|
| 地图效果层 | 200 | `ZLoadedMap::particlePool`；正式 `ZSurvivalLoop` 的 `ZWeaponEffects` 使用同一池；`CMap` 91849 |
| 地图内的 CParticleSystem | 200 | 与上一池独立；拾取物和 PROP native 16 共享 `particleSystemPool`；133966、CPickup::Spawn 99880、124680 |
| 菜单系统 | 200 | 模式、结算卡片、炼油厂共享 `ZMenuInternal` 的池；97381、CMenuDataProvider::CreateContentParticle 149326 附近 |
| 兄弟角色强化 | 25 | `CBrother` 提供池与立即停止请求，宿主不再根据槽号或资源编号推断；139099、137300–137363 |
| Powerup 屏幕效果 | 100 | 同一次 Powerup 的五个播放器共享；188745 |

拾取物 `OnRemove/Collect` 和炼油厂转移效果明确调用 `StopSpawning`，依据 99723、99818、174361。`ZWeaponEffects::StopEffect` 现在表示立即停止；原调用方按原消费者选择接口。`GetEffectCount` 包含仍在回收粒子的播放器，避免仅因发射停止就提前复用其名额。

PROP native 16 保留 `CParticleSystem::AddEffect` 的非循环属性；设置附着锚点不代表循环播放。循环与停止策略由调用方传入，避免宿主根据“有锚点”统一猜测。

### 验证与剩余边界

- 新增 `ParticlePlayerChecks`：共享池耗尽及复用、满池错过出生不补发、暂停、停止发射与立即停止的区别、重启、移动析构、发射窗口、出生年龄、有限效果收尾、循环余数和多个零间隔发射器。通过已有 `weapon-effects` 入口运行。
- 首轮 `weapon-effects,pickup-render,refinery-menu,postgame-menu,deathmatch-feedback` 共 5 项通过，退出码 0、受保护存档变化 0。
- 所有者接入后：`pwsh -File tests/run.ps1 -Configuration Debug -Case weapon-effects,pickup-render,refinery-menu,postgame-menu,deathmatch-feedback,powerup-play,prop-combat,horde-first -NoBuild`，8/8 通过，退出码 0，已知数据问题与受保护存档变化均为 0；日志为 `obj/runtime-particle-player-final.log`。
- 最终循环标志修正及满池间隔测试加入后，Debug Game、Viewer、Tests 重新构建成功；`pwsh -File tests/run.ps1 -Configuration Debug -Case weapon-effects,prop-combat,powerup-play -NoBuild`，3/3 通过，退出码 0，受保护存档变化 0；日志为 `obj/runtime-particle-player-boundary.log`。
- 地图路径：`bin/Debug/GunBrosTests.exe --cover-scale-study --big E:/coding_projects/c_projects/gun_bro_re/big --test-output E:/coding_projects/c_projects/gun_bro_re/obj/runtime-particle-pool-cover --mute`，退出码 0，已查看生成画面。三个视口共用同一状态：15 个粒子、世界半径 50.396；发射窗口恢复后不以首批的 17 个粒子为必须凑出的目标。日志为 `obj/runtime-particle-pool-cover.log`，截图保存在对应输出目录。
- `git diff --check` 通过；本批未运行 Release 或全量截图基线，未修改 BIG 和存档原件。
- 拾取物测试改为检查回收期内播放器和粒子均保留、最终两者均归零；没有只放宽数量上限来消除失败。
- 未恢复 `CEffectLayer` / `CParticleSystem` 的各 20 个播放器槽位分配及渲染入队结构；此处入队指绘制队列，不是满槽后的待播放队列，`CParticleSystem::AddEffect` 满槽直接返回空。当前限制的是**粒子槽位**，不能宣称所有效果对象数量也已与原版一致。`CParticleEffectProp` 根据资源预分配独立池的接入随放置对象归位处理，现已恢复其容量计算方法。
- 当前宿主的世界坐标粒子和部分附着效果还未完整表达原 `CParticleEffectPlayer` 的局部坐标标志；本批不把锚点丢失后的现有收尾方式声称为全部原生锚点行为。桌面确定性随机流仍为已知差异。
- R08 手写转场表和 R01/R02 Powerup 分流/假回调保持待办，下一批先恢复地图预览的 Flow 执行链。

## 八、实施记录：敌人与关卡文件归位（2026-09-16）

### 本批范围与验收

用户指出前两批虽抽出原版粒子核心，却没有移除 Z 组合文件。调整顺序，先落实已经明确的所有权与目录归属，再继续 R08；不以新增原版类等同于完成旧组合层清理。

本批核对 `source_tree.md` 的 `enemy.cpp`、`level.cpp`，原 `CEnemy::Update/Spawn/Damage/ResolveFunctionLocally` 与 `CLevel::InitDeathMatch/RespawnPlayerForDeathMatch`，以及 `enemy_template.bt`、`entries/level_template.bt`。现有三个 Z 实现文件的方法实际均属于 `CEnemy` 或 `CLevel`。验收为：旧模块及类型引用清零、不留转发头或别名、字段和执行逻辑不变、三产物构建及相关行为回归通过。

### 任务与归属

1. 删除 `ZEnemyCombat.h`，将敌人部件、运行状态、延迟动作归入 `CEnemy::Part/CombatState/Action`；保留字段注释和默认值。它们是宿主对原对象成员的组织方式，不声称恢复了同名原版嵌套符号。
2. `ZEnemyCombat.cpp` 归为 `CEnemyCombat.cpp`，继续实现同一个 `CEnemy`，不新增 `CEnemyCombat` 类。
3. `ZLiveCombat.cpp`、`ZDeathmatchCombat.cpp` 归为 `level/CLevelCoop.cpp`、`level/CLevelDeathmatch.cpp`；`CLevel` 的头文件、各实现、对象池及指示器共 10 个现有文件移入 `gameplay/level/`，保留分文件实现。
4. 自建桌面寻路 `ZDeathmatchNavigation.cpp` 一并移入 `level/`，保留 Z 和原有宿主说明；本地 Bot 策略不会因调用 `CLevel` 就被伪装成原版算法。
5. 更新 Game、Viewer、Tests 的 include 和类型引用；工程使用递归源码列表，无须增加工程或手写重复编译项。

### 验证

- 14 个迁移文件逐一比对：除 include 路径、类型限定名及 `@file` 更新外，逻辑与注释一致；`ZEnemyCombat/ZEnemyAction/ZEnemyPart` 和旧 `gameplay/CLevel*` 源码引用均为 0。没有兼容头文件或类型别名。
- `src` 下以 Z 开头的源码文件从本批前的 146 个减少为 142 个；另有 3 个顶层 Z 类型收归 `CEnemy`。目录移动与实际移除分别统计，不把移动 `ZDeathmatchNavigation.cpp` 算成消除 Z 模块。
- Debug Game、Viewer、Tests 构建成功，退出码 0；仍有现存数值转换警告，本批未修改相应算术表达式。
- `pwsh -File tests/run.ps1 -Configuration Debug -Case enemies,level-flow,local-live,deathmatch-feedback,flock,horde-first -NoBuild`：6/6 通过，退出码 0，已知数据问题及受保护文件变化均为 0。日志：`obj/runtime-owner-alignment-checks.log`。
- `git diff --check` 通过。未运行 Release 或全量截图基线。

本批不改资源读取、战斗公式或脚本分派，也不将尚未拆完的 `ZWeaponEffects`、`ZMapParticles`、菜单与 Movie 组合层标为完成。后续各批需明确记录旧文件是否真正移除，不能仅报告抽出新的原版类。

## 九、实施记录：Powerup 播放职责收归原对象（2026-09-16）

核对原 `CPowerup::Draw/DrawForeground/Update/Use/Exit`（188138、188590–188704 附近）、`entries/powerup_template.bt`、`ui_movie.bt` 和 Flow 结构。原对象自己持有解释器、Movie 和五个屏幕粒子播放器；当前 `ZPowerupMoviePlayer` 在外部重新组合这些职责，并嵌入另一个 `CPowerup`。

本批任务：

1. 将 Movie、粒子、完成事件和播放生命周期归入同一个 `CPowerup`；图形缓存作为它的私有桌面呈现数据，避免把渲染依赖扩散到模板查询接口。
2. 删除 `ZPowerupMoviePlayer.h/.cpp`，由 `CPowerupPresentation.cpp` 实现 `CPowerup` 的呈现方法；不创建同名替代类或兼容别名。
3. `ZPowerupScene` 直接拥有执行中的 `CPowerup`，调用方和测试从该原对象读取状态；无图形上下文的装备/可用性查询继续可用。
4. 验证无图形查询、使用、Movie 完成事件、暂停、重开与屏幕特效；检查 Game、Viewer、Tests 构建。

验收：旧类型、包含路径和二次脚本成员清零；只保留一个脚本对象及一份计时状态，旧注释保留。R01 的道具分流、R02 的部分 300ms 模拟回调仍须后续恢复真实 UI 完成事件；本批不把移入原类等同于修正这些已知行为差异。

### 实现结果与验证

- `CPowerup` 直接绑定自己的解释器并接收 Movie 完成事件；私有 `Presentation` 仅保存桌面资源、播放状态和绘制缓存，不再内嵌第二个 `CPowerup`。默认构造的脚本查询不创建呈现数据或 GL 资源。
- `Reset` 同时清除脚本计时、待处理动作与呈现状态，停止后不残留延迟回调；现有空袭重开检查增加断言，确认原对象的 `IsDone` 已结束。
- `ZPowerupScene` 直接拥有 `CPowerup`，通过 `GetPresentedPowerup` 暴露只读验证状态。`ZPowerupMoviePlayer`、旧包含路径和旧 getter 源码引用均为 0，无兼容别名。
- 实际移除两个 Z 文件，`src` 的 Z 文件数由 142 降至 140。`CPowerupPresentation.cpp` 是同一个 `CPowerup` 的分文件实现，不新增替代顶层类。
- Debug Game、Viewer、Tests 构建成功；补充取消断言后 Tests 增量构建成功，退出码均为 0。
- `pwsh -File tests/run.ps1 -Configuration Debug -Case powerups,powerup-play,powerup-selector,player-death,local-live -NoBuild`：5/5 通过，退出码 0，已知数据问题和受保护文件变化均为 0。日志：`obj/runtime-powerup-owner-checks.log`。
- 空袭检查覆盖普通入口及选择器入口、播放期间冻结、结束后恢复移动、实际伤害和库存、取消后不再触发。已查看 `tests/out/Core/powerup-play/airstrike-check-11-selector.png`；无图形 `powerups` 查询也通过。
- `git diff --check` 通过。未运行 Release 或全量截图基线；原 BIG 和存档未修改。

## 十、实施记录：R01 统一道具执行链（2026-09-16）

用户同意优先消除按 ID 分流。核对 `CPowerup::FunctionResolver` 188262–188550、`Use/Update` 188652–188704、`CBrother::OnPowerupButton`，以及 POWERUP BT 和实际解码的回血、护盾、手雷 Flow：原生行为由脚本调用，普通使用与选择器使用分别是出口 6/7，投掷装备在出口 5、请求在出口 6，库存必须等实际生成后扣除。

任务：删除 `field112/itemIndex` 选择执行器的条件；全部使用进入同一个持久 `CPowerup`。将角色原生动作和实时查询归回 `CPowerup`，库存、冷却与本地 Bot 政策仍由宿主处理。区分脚本仍在运行与正在占用画面，避免普通计时道具冻结世界。保留原版有依据的炮塔重复使用检查。

验收：普通与 Movie 路径共享脚本生命周期；延迟 native 可执行、即时退出可再次使用；失败不扣库存；投掷实际生成和取消路径均正确；无 ID 白名单替代持续运行。使用真实 BIG 回归，并增加独立于原物品编号的计时脚本测试。R02 的部分模拟 UI 回调另行处理，不随本批迁移宣称完成。

### 实现结果

- `ZPowerupScene::Use` 只负责使用条件、库存与模式政策，统一调用 `CPowerup::Start`。删除按 `0/10/11/field112` 选择执行器的条件及局部脚本的一次性效果循环。
- `CPowerupActions.cpp` 实现同一原类的回血、护盾、狂暴、自动瞄准、投掷和实时状态查询。角色 native 在 Flow 调用时立即执行，后续查询能够观察变化；无图形研究查询仍支持状态快照。
- 正确区分出口 6/7：普通使用与选择器使用由实际入口决定。`CPowerup` 持续接收计时与完成事件，脚本的 `Exit` 结束本次使用；即时结束不会占用下一次使用。
- 脚本活动与画面占用分开：无 Movie 的计时 Flow 可以继续运行而不冻结世界；取消只释放本次 Flow 获取的暂停，不解除已有外部暂停。
- 投掷引用由 `CPowerup` 保持到实际生成时返回宿主扣库存；插入其他即时道具、重复请求、死亡或换武器取消，均不应扣错或多扣。原炮塔 `pack5:19` 重复保护有原函数依据，保留；本地 Bot 白名单属于 R06，尚未归并。
- 本批新增原类分文件实现，继续缩减 `ZPowerupScene` 职责；没有额外删除 Z 文件，仍为 140 个。不将减少条件分支虚报为消除整个宿主模块。
- 新增 `PowerupRuntimeChecks`：测试专用 Flow 验证无 Movie 计时、实时查询、出口差异、取消与未知 native 拒绝。它不进入游戏资源加载，也不替代 BIG 数据。
- 首轮实际使用回归发现：选择器空袭 `pack5:10` 的 Movie 结束后仍有 Flow 计时步骤，按“当前 Movie/回调是否活动”判断画面占用会提前放行。改为首次进入呈现时记录占用，直到 Flow `Exit` 才释放；纯计时 Flow 不获取该占用。保留原伤害、冻结及恢复断言，没有放宽测试。

### 验证与剩余边界

- Debug Game、Viewer、Tests 首轮及修复后均构建成功，退出码 0。
- 首轮命令：`pwsh -File tests/run.ps1 -Configuration Debug -Case powerups,powerup-play,powerup-selector,player-death,local-live,deathmatch,deathmatch-feedback -NoBuild`。`player-death/local-live/deathmatch/deathmatch-feedback/powerups` 五项通过；`powerup-play` 暴露上述空袭呈现占用问题后停止，整体退出码 1，选择器检查尚未执行。日志：`obj/runtime-powerup-unified-checks.log`。
- 修复并补充投掷间插血包、护盾选择器延迟断言后：`pwsh -File tests/run.ps1 -Configuration Debug -Case powerup-play,powerup-selector -NoBuild`，2/2 通过，退出码 0。日志：`obj/runtime-powerup-unified-boundary.log`。
- 新计时脚本检查失败数 0；实际手雷 13/14/15 各只生成一枚、只扣一份；投掷期间血包独立扣除；护盾选择器延迟生效。空袭两种入口的伤害、画面占用、恢复移动和重开取消全部通过，包括 `pack5:10` 选择器路线的 500 点伤害。
- 两轮验证已知数据问题及受保护文件变化均为 0；`git diff --check` 通过。没有运行 Release 或全量截图基线。
- R01 的编号执行器分流已消除；R02 的部分 300ms UI 模拟回调仍未恢复真实完成通知。下一步处理 R02，再处理 R08 地图预览的手写状态/转场表；R06/R07 本地 Bot 政策归位另行推进。

## 十一、实施记录：删除 Powerup 宿主组合层（2026-09-16）

用户要求先实际删除 `ZPowerupScene.h/.cpp`。原证据：`CPowerUpSelector::Bind/SetupPowerUps/OptionUse` 186808、186087、184707 持有选项、冷却与执行对象；`CBrother::OnPowerupButton` 137932 检查角色和装备入口；`CLevel::UsePowerup/Update` 116346、121255 登记并推进活动对象。已核对 POWERUP、STORE 和 UI Movie BT；本批不改变磁盘格式及原资源。纠正此前文档对原函数名的误记：本工程新增的 `CBrother::UsePowerup` 是共享这些检查的适配入口，不是原版同名符号。

任务：选择、装备槽和库存视图归现有 `CPowerUpSelector`，正式游戏复用 InputPad 的选择器；角色使用条件归 `CBrother`；活动执行、取消与投掷结果提交由 `CLevel` 调度；库存仍调用 `CProfileManager`。机器人白名单、选号与战术判断放回已有桌面策略，保留用户指定的难度行为。删除旧类型和全部引用，不增加同职责替代类、别名或转发文件。

验收：Game/Viewer/Tests 构建通过；道具双入口、投掷库存、重开、选择器、合作和 DM 策略回归通过；原资源及存档无修改；明确统计实际删除文件及剩余边界。R02 顺延，不将 UI 假回调迁移描述为完成。

### 实际归属

| 职责 | 当前所有者 |
|---|---|
| STORE 引用验证、目录、当前选择、装备槽和库存视图 | `CPowerUpSelector`，实现位于 `CPowerUpSelectorInventory.cpp`；正式玩家复用 InputPad 内已有选择器及 `ZHudResources` 的目录 |
| 出生/死亡状态、已有强化、炮塔重复保护、Flow 可用性查询 | `CBrother::UsePowerup`，实现位于 `CBrotherPowerups.cpp` |
| 道具执行、呈现与原生效果 | 选择器持有的同一个 `CPowerup`；无中间播放器或宿主对象 |
| 更新、取消、重开清理、实际投掷提交、冷却及挑战通知 | `CLevelPowerups.cpp`；关卡直接登记 `CPowerup*`，库存读写仍使用 `CProfileManager` |
| DM 允许集合、预算分类、虚拟库存、血包/手雷选择、购买偏好 | 现有 `ZDeathmatchBot`；UI 复用相同的包与编号判断 |
| 合作 Bot 使用时机、随机尝试、空袭/手雷战术条件 | 现有 `ZLocalCoopBot`；随机状态随桌面角色保存 |

- 删除 `ZPowerupScene.h/.cpp` 和全部源码/测试引用；未新增替代管理类、别名或转发文件。上述三个 C 文件都是现有原类的分文件实现。Z 文件数从 140 降为 138，没有新增 Z 文件抵消删除。
- 更新正式循环、作弊入口、研究场景和测试：选择器只处理选择与装备，手工研究推进显式调用 `CLevel::UpdatePowerup`，画面查询与绘制直接调用 `CPowerup`。
- 无图形 peer/研究选择器可以单独绑定目录与角色，不依赖打开商店；正式玩家 UI 与使用路径共享目录，不再各自加载一套。
- 合作 Bot 的弹体引用分类改为策略请求时读取，不再由通用道具初始化预加载；规则与资源解析仍使用原 BULLET 标志。Movie 依赖分类仍是明确的桌面战术推断，不宣称恢复了原版空袭类别字段。
- R02 的固定回调尚未改动；本批不恢复网络远端玩家或原版双槽 Powerup 全部内存布局。

### 验证

- Debug Game、Viewer、Tests 全部构建成功，退出码 0。首次编译发现新拆出的 `CBrotherPowerups.cpp` 缺少 `CMPMatch` 定义头文件，补齐后重新构建通过；没有用前向声明或跳过目标掩盖错误。
- `pwsh -File tests/run.ps1 -Configuration Debug -Case powerup-play,powerup-selector,original-hud -NoBuild`：3/3 通过，退出码 0，日志 `obj/runtime-powerup-scene-removal-core.log`；道具详细日志保留于 `obj/runtime-powerup-scene-removal-play.log`。
- `pwsh -File tests/run.ps1 -Configuration Debug -Case powerups,player-death,local-live,deathmatch,deathmatch-feedback -NoBuild`：5/5 通过，退出码 0，日志 `obj/runtime-powerup-scene-removal-gameplay.log`。
- 覆盖普通/选择器双入口、无 Movie 计时、护盾延迟、空袭冻结与恢复、投掷期间使用血包、实际生成扣库存、取消/重开、原生装备存档回读、合作角色归属及倒地复活。DM 检查覆盖 Easy 预算、Normal/Hard 虚拟库存、冷却、禁止商店与结算重开。
- 两轮均为已知数据问题 0、受保护文件变化 0；`git diff --check` 通过。`ZPowerupScene`、`GetPresentedPowerup`、`SetPowerups/SetPeerPowerups` 源码及测试引用为 0，无兼容定义；Z 文件数 138。
- 没有运行 Release 或全量截图基线。R02 和 R10 的默认选择值仍按台账待办，不因本批旧文件删除而标为完成。

## 十二、实施记录：Powerup 本体目录归并（2026-09-16）

用户确认将 `CPowerup.h/.cpp`、`CPowerupActions.cpp`、`CPowerupPresentation.cpp` 四个文件移入 `gameplay/powerup/`。本批仅移动文件并更新 include 与文档路径；角色入口、关卡调度、选择器、Catalog 和测试保留各自职责目录。

验收：四文件除 include 路径外内容一致；源码旧路径引用清零；单工程递归收集新路径，三产物构建和相关道具回归通过，不新增转发文件或类型。

结果：四文件已移动，逐文件比对确认仅 include 路径变化，逻辑与注释保留；全部旧路径引用清零。更新文件映射及路线链接，工程现有递归源码列表自动收集新目录，无需修改编译项。

- Debug Game、Viewer、Tests 构建通过，退出码 0。
- `pwsh -File tests/run.ps1 -Configuration Debug -Case powerups,powerup-play,powerup-selector -NoBuild`：3/3 通过，退出码 0；已知数据问题及受保护文件变化均为 0，日志 `obj/runtime-powerup-directory-checks.log`。
- `git diff --check` 通过；未运行 Release 或全量截图基线。本批只归并目录，Z 文件数量及既有待办状态不变。

## 十三、实施记录：Brother 核心目录归并（2026-09-16）

用户确认保留独立的 `CBrotherPowerups.cpp`，将角色本体、玩家控制、模型组装、默认伙伴 AI 及两个自建 Bot 共 13 个文件移入 `gameplay/brother/`。包括 `CBrother.h/.cpp`、`CBrotherPowerups.cpp`、`CPlayer.h/.cpp`、`ZPlayerModel.h/.cpp`、`CBrotherAI.h/.cpp`、`ZLocalCoopBot.h/.cpp`、`ZDeathmatchBot.h/.cpp`。

任务与验收：只移动上述文件、更新引用与文档；逐文件确认除路径外内容不变，保留全部注释；旧路径引用清零；单工程三产物构建及玩家、默认 AI、合作与 DM 相关回归通过。数据、关卡、共享战斗能力和测试继续保留现有归属，不合并角色实现文件。

结果：13 个文件已移入新目录，包括独立的 `CBrotherPowerups.cpp`；逐文件比对确认仅路径替换，逻辑和注释均保留。更新源码、测试的 include 与文档链接；旧路径引用为 0，未增加转发头。工程继续使用原递归源码列表，Debug Game、Viewer、Tests 构建成功，退出码 0。

- `pwsh -File tests/run.ps1 -Configuration Debug -Case brother,player-death,local-live,deathmatch-feedback,powerup-play -NoBuild`：5/5 通过，退出码 0；已知数据问题及受保护文件变化均为 0。日志：`obj/runtime-brother-directory-checks.log`。
- 原位置目标文件数 0，新目录文件数 13；`git diff --check` 通过。
- 未运行 Release 或全量截图基线；本批不改变类名、继承关系、AI/Bot 策略及已有行为待办。

## 十四、实施记录：Movie 播放状态与道具 UI 完成通知（2026-09-16）

依据：`ui_movie.bt` 与 `CMovie::SetChapter` 108873、`Update` 109057；选择器 `SetState` 185697、`UpdateHideItemsPause` 185533、`Update` 186581；列表 `CMenuMovieControl::ChangeMode` 141357；InputPad `Base::Update` 88366、`PeripheralHUD::SetState` 88097、序列消费 91232；Powerup native 13 188373。

- `CMovie::Playback` 是共享资源上的独立播放状态，不复制资源，不往 `ZMovieRenderer` 缓存塞全局进度。支持章节／全片、跨章节后循环、正反向、暂停、取消、单次完成通知。按原版严格跨越包含端点的范围，循环取模使用原章节长度；循环一圈不视为整个动作结束。
- 道具演出 Movie 使用该状态；选择器保管主框与商品列表的实例，以实际播放进度同时驱动绘制和通知。列表反播章节 0；关闭框播放章节 3。普通 Hide 与脚本等待的 HideSelector 区分，显式重置取消后不再通知。
- InputPad 恢复请求进入实际 Base／Peripheral 状态：底栏采用原版 alpha 变化速率，外围采用资源章节 5。已有可用操作界面时直接通知；无 InputPad 的伙伴／独立研究入口也不模拟不存在的界面延时。
- 修复同步回调的动作消费：立即完成可能在原动作批次内生成末尾伤害和 Exit，必须继续消费新动作，不能因脚本已 Exit 遗失伤害。原始 pack5 Powerup10 Flow 的状态 5→6 明确包含这一路径。
- 修复即时退出的 UI 收尾：只有已请求的列表／主框关闭动画可以在 Flow Exit 后继续播放；即时回血等未请求动画的导出直接释放呈现状态。DM 普通／困难 Bot 的连续回血回归发现并覆盖了此边界，未添加道具编号分支。
- 首轮三产物构建通过；`movies`、`original-hud`、`powerup-selector` 通过。`powerup-play` 首轮暴露同步回调后伤害遗漏，已定位到上述动作消费顺序，修复后重跑。

验证结果（2026-09-17）：

- `pwsh -File obj/build-runtime.ps1 -Product Game`：Debug Game、Viewer、Tests 三产物构建成功，退出码 0。
- `pwsh -File tests/run.ps1 -Configuration Debug -Case movies,powerups,powerup-play,powerup-selector,original-hud,player-death,local-live,deathmatch-feedback -NoBuild`：8/8 通过，退出码 0；已知数据问题 0、受保护文件变化 0。汇总日志 `obj/runtime-movie-final-checks.log`。
- 新增 `movie-playback` 边界／反向／循环／暂停／取消／替换／同资源多实例测试，以及 BIG 选择器列表／关闭框、即时退出、关闭商店、InputPad 恢复与取消测试，失败数均为 0。Powerup10 两条入口均结算实际 500 伤害，DM 普通／困难 Bot 连续回血和后续使用正常。
- 已查看 `tests/out/OriginalUI/powerup-selector/selector-closing-completion.png` 与 `tests/out/Core/powerup-play/airstrike-frame-closing.png`，关闭阶段主框正常保留；`git diff --check` 通过。
- 未运行 Release 或全量截图基线。此次不迁移骨骼、Sprite、粒子播放器；其他菜单既有手动时间采样、嵌入 Movie／声音轨事件等也不等同于已全面恢复原版。

## 十五、ZWeaponEffects 职责与血液爆裂调研（2026-09-17）

- 具体证据、差异和阶段验收见 [ZWeaponEffects 职责核对与拆分方案](weapon-effects-alignment-plan.md)。本次为调研与方案记录，未修改运行代码。
- 确认 DM 玩家死亡爆裂由 PLAYER 导出 2 → native 11 → `pack0_core` 类型 11、序号 18 粒子资源触发；1690 字节、12 个发射器、Sprite 原型 34。原始引用字节及粒子目录字段已只读核对，既有死亡截图可对应血液与残肢；不把这一资源泛称为全部敌人血雾。
- 后续顺序：UI／屏幕粒子解除武器组合依赖 → 恢复地图 `CParticleSystem` 和归属 → 弹体／附属效果归回原类 → 分离音频和必要绘制适配后删除 `ZWeaponEffects`。
- 新增待修差异：原地图系统 20 个效果实例槽尚未恢复；兄弟普通粒子的默认池与锚点尚未按原调用归位。R03 光束启发式、速度单位和 Viewer 转场表继续分别跟踪。
- 补充说明见方案中的“粒子、图片与动画帧”和“地图系统的范围与原版执行流程”，统一术语见 [特效术语](../CONTEXT.md)。多个粒子可共用同一图片／动画；发射器数、粒子数和帧数不能混用。示例的 15 个粒子与 8 帧仅作解释，不是原资源配置。效果槽满不排队，粒子池满不积压补发；200 是分配容量，按原空闲栈语义可分配 199 个粒子。

## 十六、实施记录：删除武器特效组合层（2026-09-17）

- 用户授权夜间实施，实际工作 06:32–08:38 UTC。`ZWeaponEffects.h/.cpp` 已删除，无别名／转发壳；`ZShot` 并入 `CBullet`，实例归 `CLevel`，四槽附属效果归原持有者。完整文件表见 [源码映射](source-name-map.md#夜间特效组合层迁移)。
- UI 直接使用共享粒子播放器；地图 `CParticleSystem` 20 槽与 199 个可领取粒子分别限制，兄弟普通爆裂恢复地图池、零角度锚点及死亡归属；强化播放器归 `CBrother` 并保留相对坐标。独立效果层、预放置效果、UI 与强化未混入地图临时系统。
- 恢复链接效果的组、朝向、缩放参数和当前部件锚点；地图动态粒子逐粒进入组／Y 队列。Ribbon 按 native 顺序竞争四槽，满槽不自动重试；停止发射、立即停止、非光束拖尾排空和光束即时清理分别处理。
- R03 删除 bounds 减一启发式；真实 BULLET104 绑定主体 1／端帽 2、3。枪械初速经原 IEEE 常量、Flow 8.8 倍率及毫秒积分核实，450 修正为 430；手雷由 CBrother 产生 430，CLevel 不再硬写初速。
- Debug／Release 三产物构建均退出码 0；Debug 集中 21/21、最终补验 2/2；Release 七项功能回归与修正后的正式菜单冒烟均通过，独立 Viewer 开火及既有 Release 资源／作弊码／保存验证均通过。修复 Release 冒烟参数与日志缓冲问题，未恢复 Release 截图。命令、退出码、失败诊断和截图见 [夜间交接](weapon-effects-night-migration.md#最终验证与交接)。
- 本轮没有关闭全部 R09/R10：预放置效果完整类归位、全局随机序列、玩家移动速度、弹体最终移除回调与完整绘制队列仍需核对；R08 Viewer 手写转场表保持为独立待办。未提交 Git，原始资源只读。
