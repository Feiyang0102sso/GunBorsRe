# ZWeaponEffects 职责核对与拆分方案

2026-09-17。状态：调研完成，方案待进入实施；本次未修改运行代码。

## 结论与范围

`ZWeaponEffects.cpp` 当前 1300 行、头文件 98 行，同时处理弹体实例、碰撞、粒子实例、资源缓存、Sprite 展开、拖尾、电弧绘制、声音和角色事件。源码与测试共 48 个文件引用该名称（含自身）。菜单仅播放粒子，却需创建整个组合对象，其 `AdvanceAmbientEffects` 还更新音频。

原版已有对应职责，不能把整个组合改名为一个新的 C 类。沿现有原类逐步迁移，最后删除组合入口；必要的 Windows 绘制、投影和音频适配保留明确身份。此次研究使用 `codebase-design` 的小接口与职责归属原则，不新增泛化管理框架。

## 原版与当前职责

以下行号指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`，并非恢复的原 cpp 行号。

| 职责 | 原版依据 | 当前落点与目标 |
|---|---|---|
| 粒子参数、发射和寿命 | `entries/particle_effect.bt`；`CParticleEffect::Init` 130974、`CParticleEffectPlayer::Update` 131499 | 已有 `CParticleEffect`、`CParticle`、`CParticleEffectPlayer`、`CParticlePool`，继续共用 |
| 单个效果绘制 | `CParticleEffectPlayer::Draw` 131724 | 当前位于组合类的粒子绘制循环；归回播放器的绘制职责，复用现有 Sprite/绘制能力 |
| 地图临时效果实例与回收 | `CParticleSystem::AddEffect` 133878、`Update` 133863、`QueueParticles` 133841、构造 133944 | 当前 `activeEffects` 混合地图、角色、弹体和 UI 实例；恢复 `CParticleSystem`，与地图效果层区分 |
| UI 粒子 | 模式按钮绘制 250179/250182；结算卡片 249808；`CTransferEffect::Draw` 174292 | `ZMenuSurface.cpp` 借用组合类；菜单直接持有粒子播放器，继续使用菜单共享池 |
| Powerup 屏幕粒子 | `CPowerup::Draw` 188627–188635 | `CPowerupPresentation.cpp` 借用组合类；归同一 `CPowerup` 持有的播放器，保留 100 槽池 |
| 弹体资源、状态、移动和碰撞 | `entries/bullet_template.bt`；`CBullet::Bind` 63647、`Fire` 62212、`Draw` 62998；`CLevel::FindOldestBullet` 114447 | `CBullet` 已执行 Flow，但 `ZShot` 仍拥有大量弹体状态；逐项收回 `CBullet`，关卡管理对象生命周期 |
| 附属效果与带状拖尾 | `EffectContainer` 294598–294761；`TrailEffectHolder::Update` 294891；`CRibbonTrailEffect::Draw` 243694 | 当前 `ZRibbonInstance`、`AdvanceRibbons`、`DrawRibbon` 混在组合类；按原职责恢复 |
| 电弧 | `CBullet::SetLightning` 60480、`CLightningArc::Generate/Draw` | `CLightningArc` 已有独立几何实现；剩余绘制适配从组合中移出，不再复制生成算法 |
| 声音事件与 Windows 播放 | `CSoundQueue` 原符号对应 `soundQueue.cpp`；当前 `PlaySound/PlayWav` 与 `ZAudioPlayer` | 区分原版事件调度和桌面合音策略；菜单粒子不应依赖音频实例 |

## 粒子、图片与动画帧

术语统一见 [特效术语](../CONTEXT.md)。**粒子数量表示独立存活的小对象数量，不表示图片文件数、图集切片数或动画帧数。**

| 层次 | 含义 | 数量关系 |
|---|---|---|
| 图片／图集 | 存放烟雾、火花、碎片等外观 | 多个粒子可以共用同一图片资源 |
| Sprite 动画 | 按帧数据选择图集区域，必要时组合多个图块 | 一个粒子播放多帧动画，仍是一个粒子；不是运行时任意等分图片 |
| 粒子 | 独立记录出生时间、位置、速度、大小、透明度和寿命 | 每个存活粒子占一个粒子槽，不为每个粒子复制一份图片资源 |
| 发射器 | 按资源参数生成粒子 | 一个发射器可以先后或同时生成多个粒子 |
| 效果实例 | 一次粒子效果播放，推进资源中的多个发射器并持有存活粒子 | 在地图 `CParticleSystem` 中占一个效果实例槽；发射器不各占一个效果实例槽 |

示例仅用于解释数量关系，**15 个粒子、8 帧动画不是任何原版资源的已核实配置**：一张图集包含 8 帧烟雾，一个烟团依次播放这 8 帧，只占 1 个粒子槽；若同一效果同时生成 15 个烟团，则占 1 个效果实例槽和 15 个粒子槽。15 个烟团共用外观资源，各自推进年龄、位置和透明度；出生时间不同，显示的帧也可能不同。10 个这样的效果同时播放，且粒子全部存活时，占 10 个效果实例槽和 150 个粒子槽。

已核实的玩家死亡资源含 **12 个发射器**，这不等于 12 个粒子或 12 个效果实例。实际存活粒子数取决于发射间隔、播放时间、粒子寿命和池是否还有空位。当前绘制入口 `ZWeaponEffects::Draw` 使用每个粒子的 `animation` 和 `ageMs` 选择 Sprite 帧，外观资源与粒子状态分别保存。

## 地图系统的范围与原版执行流程

地图上的闪光不能仅凭外观归类：场景物逐帧装饰可以是 Sprite；预先摆放的粒子对象由 `CParticleEffectProp` 持有播放器；运行时生成的地图粒子可进入 `CParticleSystem`；临时文字、Sprite 和粒子还有独立的 `CEffectLayer`；受击变亮可能仅改变绘制状态。不能把地图效果层解释成所有背景循环动画，也不能把所有效果统一塞入 Movie。

敌人爆裂可以组合模型动作、粒子和脚本收尾。原 `CEnemy::FunctionResolver` case 8（71952 起）读取粒子资源并调用地图 `CParticleSystem::AddEffect`；节点与跟随效果另见 `SpawnParticleEffectNode` 70623、`StartLinkedEffect` 70563。具体敌人是否调用这些入口、使用哪个资源，仍由各自 ENEMY Flow 决定。

原版主流程如下，行号均指反编译文件：

1. **预分配。** `CParticleSystem` 构造（133944）准备 20 个效果播放器及 200 槽共享粒子池。按 `CParticlePool::Allocate` 92882 与 `UpdateEmitters` 131393 的空闲栈语义，0 号位置不被领取，实际可分配 199 个粒子。20 和 200 是原程序容量设置，效果外观与发射参数来自 BIG。
2. **申请效果实例。** `AddEffect` 133878 顺序查找已完成的播放器，绑定效果资源、共享池和位置，初始化为非循环播放。20 个实例全部占用时返回空：本次调用不排队、不扩容，也不替换旧效果。调用者可按需要绑定角色锚点。
3. **推进已有粒子。** 系统 `Update` 133863 遍历未完成的播放器；播放器 `Update` 131499 更新锚点和时钟，推进存活粒子的属性与运动，将结束的粒子归还池。原版通过链表记录每个播放器持有的粒子。
4. **按资源发射。** `UpdateEmitters` 131393 按发射窗口和间隔尝试领取粒子，成功后初始化并加入存活链表。**粒子池满时跳过本次生成，但仍消费发射间隔；之后有空位也不补发这次遗漏。** 后续正常发射可再次申请。间隔取整为零时，一次更新只进行该轮的一次生成尝试，避免无限循环。
5. **绘制与回收。** `QueueParticles` 133841 将活动播放器的存活粒子加入渲染队列。有限效果通常在发射结束后继续更新残余粒子，全部结束才释放效果实例槽；循环效果按周期继续，零时长等特殊分支按资源导出的寿命收尾，不能用统一固定延时替代。

| 操作 | 原版含义 |
|---|---|
| `StopSpawning` 131289 | 停止发射，已有粒子继续更新，按播放器收尾规则结束 |
| `Stop` 131311 | 立即释放已有粒子并标记播放器完成 |
| `Start` 131337 | 清理旧粒子，重置时钟、停止标记和发射间隔状态，重新播放 |

这组容量仅对应地图 `CParticleSystem`；UI、角色强化、屏幕道具和地图放置效果按各自原持有者管理。当前共享粒子核心已接入，但外层地图实例管理尚未全部按原版归位，详见下方差异与实施顺序。

## 玩家死亡血液／残肢爆裂的资源链

本节确认的是 Deathmatch 玩家死亡爆裂，不代表所有敌人的血雾都使用同一资源。

1. PLAYER 样本：`_prep/big_360_out/pack0_core_xga/0xf4e02223/16_PLAYER/pack0_core_xga_0037_0x6726.bin`。其导出 2 在 DM 条件成立时，于文件偏移 `0x59A` 调用 `CBrother.native_0x060B(4)`，随后隐藏角色并进入状态 7。依据为对应 Flow 解码清单的 `script.functions[2]`。
2. 参数 4 是脚本资源表下标。原始字节 `0x38/0x3C/0x3D` 分别为包 hash `0x58595522`、类型 11、类型内序号 18。按 `CStringToKey` 算法复核 hash 对应 `pack0_core`；类型 11 对应归档 Section 12，不能把类型号当 Section 号。
3. 粒子样本：`_prep/big_360_out/pack0_core_xga/0xf4e02223/12_PARTICLEEFFECT/pack0_core_xga_0032_0x63f9.bin`，1690 字节。目录记录 logical ID 287、handle `0x0300011F`；这些值与类型内序号 18 不可混用。
4. 文件 `0x00` 的 Sprite 包 hash 为 `0x58595522`，`0x04` 发射器数为 12。全部发射器引用 Sprite 原型 34，以各自 animationMask 选择动画。喷射、位移、大小、透明度及寿命参数来自该粒子资源，不来自 Movie。
5. 原生入口：`CBrother::FunctionResolver` case 11（138963 起）从脚本取资源，调用地图 `CParticleSystem::AddEffect`，并 `SetAnchor`。`GetParticleEffectAnchor` 134152 提供角色位置及零旋转；播放器 131499 调用锚点更新位置。
6. 当前入口：`CBrother.cpp` case 11 → `ZGunCue::Effect` → `ZWeaponEffects::EmitBrother/Emit` → `CParticleEffectPlayer`。`burstActor` 用于死亡收尾查询，不能替代锚点或粒子池归属。

已查看既有 `tests/out/Core/deathmatch-feedback/deathmatch-final-burst.png`，其中可见血液与残肢爆裂，与上述 DM 死亡触发链对应。不是本次新运行的截图；每个发射器的单独视觉名称尚未逐一标定。普通敌人的受伤／死亡效果须继续沿各自 ENEMY Flow 引用核对。

## 拆分时必须处理的差异

- **地图效果实例槽与粒子槽不同。** 原 `CParticleSystem` 固定 20 个播放器，另分配 200 槽粒子池；满 20 个实例时 `AddEffect` 返回空。当前 `activeEffects` 是增长向量，已有粒子池并不等于恢复了实例容量限制。该限制只属于这个原系统，不能套给全部 UI 播放器。
- **兄弟普通效果的池和锚点未归位。** 当前 case 11 不指定池，落入 `ZWeaponEffects` 默认池；正式循环传入的是 `loaded.particlePool`（地图效果层），而原调用明确使用地图 `CParticleSystem` 的池。当前普通 Effect 仅记录 `burstActor`、保持生成位置；原入口另设置角色锚点。恢复时须覆盖受力移动、重生和收尾，保留死亡归属查询。
- **R03 光束启发式仍存在。** `IsBeamSourceAnimation/BeamBodyAnimation` 根据画面 bounds 将动画序号减一；源码注释明确承认偏离原字节。原 Bind 使用资源动画及后续 `+1/+2`，没有该修补。单独核对异常光束的 BIG、Flow 与原消费者，再移除启发式，不能在拆文件时把它认作资源规则。
- **速度常量仍待证据。** `kShotSpeed=450` 与手雷调用参数 430 已定位，但本轮未完成原 `CGun/CBrother/CBullet` 单位链核对；不得仅因出现字面量就判定错误或改数值。
- **声音合并属于现存桌面适配。** 按 WAV 合并、动作声音等待和循环去重应明确保留或另行核对，不能在职责迁移时悄悄改变听感。
- **Viewer 转场表是独立遗留项。** `ZMapParticles::StartTransitionParticlesForProp` 仍按 Cover/Barrel/Spire 和状态挑脚本资源下标；本次查到调用方在 `MapPreview.cpp`。沿用路线图 R08，不把它混入正式 Flow，也不把复制表改名当作修复。

## 实施顺序与验收

1. **先解除 UI 对武器组合的依赖。** 完善粒子资源呈现与 `CParticleEffectPlayer` 绘制职责，迁移模式按钮、结算卡片、炼油厂、Powerup 屏幕粒子。验收：这些纯粒子持有者不再构造 `ZWeaponEffects`；保留共享池、循环、立即停止／停止发射、原绘制顺序和界面坐标。
2. **恢复地图 `CParticleSystem`。** 管理临时效果实例、锚点、容量与回收，逐项归位兄弟爆裂、敌人效果、场景物和拾取物。验收：20 个效果实例槽与 200 个粒子槽分别测试；不同角色不串归属；重生／删除角色不残留错误锚点；最后一次击杀仍等粒子结束后结算。
3. **归回弹体和附属效果。** 将 `ZShot` 的弹体状态与行为收入 `CBullet`，关卡管理实例；恢复 `EffectContainer/TrailEffectHolder/CRibbonTrailEffect` 已核实职责。验收：首帧更新、枪口跟随、碰撞、反弹、子弹计数、脱离后的尾迹消散和绘制次序保持原行为。
4. **收拢音频与宿主绘制，删除组合类。** 正式关卡和永久研究入口共用同一实现，替换全部调用后删除 `ZWeaponEffects.h/.cpp`，不留下同功能转发壳。R03 与速度核对各留独立变更、证据和回归，不混成一次大改。

按每阶段影响选现有检查：UI 使用 `postgame-presentation,refinery-menu,postgame-menu,powerup-play`；地图与角色使用 `pickups,prop-combat,enemies,player-death,deathmatch-feedback`；弹体与音频使用 `weapons,weapon-effects,mines,audio-transitions`，并覆盖涉及的 Viewer 入口。新增检查只针对迁移造成的实际风险，不另造按资源 ID 的测试豁免。

## 本次验证范围

- 只读核对 PLAYER 的资源引用原始字节、粒子资源的 26 个引用／计数字段及末字段边界；退出码 0，记录 `obj/weapon-effects-research-check.log`。
- 查阅粒子、弹体 BT、原消费者、原符号目录、当前正式／Viewer／测试调用方，并检查既有死亡截图。
- 本次只新增研究方案和路线图入口；未修改运行代码，未重新构建或运行游戏回归。上述验收项是后续实施要求，不是本次已通过的测试。
