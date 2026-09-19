# gameplay 全目录归位与 Z 文件消除：调研及阶段方案

日期：2026-09-19。调研基线：`2fb4c8e`。状态：用户授权范围内的目录和职责拆分、构建与回归已完成；Boss 串珠仍有明确证据缺口。下文保留调研时的证据与阶段验收要求，实施结论另见结果记录。

实施结果与当前路径：[gameplay 全目录优化结果](gameplay-optimization-result.md)。下文“当前差异”“待验证方向”描述的是实施前调研状态，不能覆盖结果记录中的实际结论。

## 目标与范围

用户要求先调研，再完整优化 `src/gun_bros_re/gameplay/`：所有根目录文件进入职责目录；消除可归回原版对象的 Z 包装；multiplayer 为例外；同时处理玩家弹体被地图遮挡、Haven 机械 Boss 激光恢复串珠状两个问题。行为与命名以 iOS 原版、BIG 原始数据及消费代码为依据。

本地盘点共 169 个源码文件，根目录 32 个，其中递归 Z 文件 23 个。此次为目录及 Z 文件的完整处置方案，并非已逐函数审计全部 169 个文件。自建类型不能机械改为 C；删除文件必须消除包装、合并所有权，或将真正的桌面/Viewer 适配迁回适当位置。

用户最新补充：保留 `gameplay/multiplayer/`，`ZLiveShopSession` 也归入其中。Bot 行为、档案、名单和配置统一进入 `multiplayer/bot/`；双人统计、比赛规则与本地商店同步在 multiplayer；原默认伙伴 `CBrotherAI` 仍归 brother。这覆盖此前“不专门新增 multiplayer 目录”的暂定方案。自建多人功能保留 Z 身份。

## 目标目录

目录均相对 `src/gun_bros_re/`，继续一个 vcxproj、Game/Viewer/Tests 三产物，不新增内部静态库。

| 目标 | 职责和输入文件 |
|---|---|
| `gameplay/weapon/`（新增） | `CGun.h/.cpp`、`CGunDrawing.h/.cpp`；`CBullet.h/.cpp`、`CBulletDrawing.cpp`、`CBulletEffects.cpp`、`CBulletProjectile.cpp`；归位后的模板/资源加载实现 |
| `gameplay/armor/`（新增） | `CArmor.h/.cpp`、`CArmorDrawing.h/.cpp`；盔甲模板、脚本与绘制，不混入枪弹目录 |
| `gameplay/collision/`（新增） | `CCollisionData.h/.cpp`；核对后恢复 `Collision` 几何算法。地图层查询仍由 `map/CLayerCollision` 负责 |
| `gameplay/script/`（新增） | 游戏 native 分派及 Mission 脚本消费；解释器保留在 `engine/glu/script/`。`CMissionScriptContext` 也要核对 `Mission` 的真实归属，不能因已有 C 前缀跳过 |
| `gameplay/audio/`（新增） | `CBGM.h/.cpp`，按证据恢复的 `SoundEffect`、`CSoundQueue` 职责；底层 Windows 播放器仍归 `engine/platform/` |
| `gameplay/multiplayer/`（新增） | `CMPMatch.h/.cpp`、`ZMultiplayerStatistics.h`、本地合作/PvP Bot 与档案、`ZLiveShopSession.h`；统计仍由关卡计算，目录归属不改变所有权 |
| 现有 `game/level/map/enemy/brother/powerup/pickup/` | 保留各原对象职责；`CBrotherAI` 是原默认伙伴，留在 brother，不随自建 Bot 移走 |
| `ui/`（已有） | 根目录的 `CInputPadMeter.h` 归 UI，与使用者一致 |
| `host/`（必要时新增） | Windows 键位、共享会话的调用方适配；保持 Z 命名，不承接原版战斗规则 |
| `../gun_bros_viewer/scenes/` | Viewer 专属地图选择、预览输入及研究调度；生产对象仍由 gameplay 提供 |

根目录 32 个文件均有归属。碰撞、声音或类型目录不能变成新的杂项容器；`CRenderQueue` 继续归 map，除非进一步原工程证据要求调整。

### script 目录的消费关系

`gameplay/script/` 仅管理游戏特有的原生调用分派与宿主接入，不集中搬走各对象的 Flow 行为，也不重复实现引擎解释器。

- 通用字节码读取/执行、状态与事件机制在 `engine/glu/script/`。
- BIG 中各 ENEMY、PROP、LEVEL、GUN、BULLET 等模板持有各自脚本；当前由 CEnemy、CProp、CLevel、CGun、CBullet 等对象的解释器实例执行。
- 游戏分派层把脚本 class/slot 编号转交对应对象的 `FunctionResolver/VariableResolver`，同时接入游戏/关卡变量与动画回调。
- 当前 `ZGameScriptObject` 的直接派生消费者共 10 个：CEnemy、CProp、CLevel、CBrother、CGun、CBullet、CArmor、CPickup、CPowerup、CMissionScriptContext。当前 CPlayer 是借用 CBrother 的控制对象，不是它的派生类；玩家脚本由 CBrother 接入。Spawner native 经关卡上下文转交。
- 地图中的 PROP 脚本由 CProp 消费，关卡流程由 CLevel 消费，不能笼统认为 CMap 已经拥有一套完整通用脚本系统。
- 示例：弹体 Flow 的 `0x0908` → 引擎解释器解码 → 游戏分派层 → 当前 CBullet 的 native 8。具体转向行为仍归 CBullet。
- CMissionScriptContext 当前另由 `data/ZPlanetCatalog.h` 使用：执行 Mission 导出以取得进入条件。它不是敌人/地图公用的业务脚本，最终归位仍需核对原 Mission 对象。

因此原文件中的通用接线可以归 script；CEnemyScript.cpp、CProp 的 native、CLevel 的刷怪/波次、CGun/CBullet 的行为仍归各自目录。游戏分派不能通过引擎直接 include 全部游戏类来制造反向依赖。

## 23 个 Z 文件逐项处置

以下路径相对 `gameplay/`。配对文件按实际文件数计。

| 文件 | 数量 | 处置目标与边界 |
|---|---:|---|
| `ZBulletResources.h/.cpp` | 2 | 删除独立组合层；读取/模型资源归 `CBullet::Template` 相关实现，共享缓存仍由明确的资源所有者管理。保留一次加载多弹体共享、模板地址稳定、GPU 生命周期；不能每发重新加载 |
| `ZCombatAudio.h/.cpp` | 2 | 原事件/资源队列归原音频对象，Windows 去重/循环播放差异归适配层；消除目前一个类同时承担原语义和宿主策略的混合 |
| `ZCombatGeometry.h` | 1 | 按 `Collision::*` 证据归位；宿主补充算法明确注明依据，不用标准公式替换原版特殊判定 |
| `ZProjectileGeometry.h` | 1 | 枪口/骨骼坐标归 `CBrother` 的节点/方向查询；场景线段/射线查询归碰撞层；删除混合工具头 |
| `ZCombatTypes.h` | 1 | 生命状态归角色、武器成长归成长消费对象；命中数据和对象引用沿原 `CBullet/ILevelObject/CLevel` 调用链梳理。观察用值类型可作为明确的嵌套宿主类型；避免直接制造第二份状态或按文件名改 C |
| `ZProjectileTypes.h` | 1 | 删除玩家前/后绘制阶段枚举；原裁剪归 `CBullet::CanBeCulled`；碰撞层引用归地图；观察快照归调用方观察接口。原始资源引用和观测能力保留 |
| `ZGameScriptObject.h/.cpp` | 2 | 分解游戏随机/模式上下文、native 分派和动画宿主回调。原名 `ScriptResolver` 有证据；完整的当前基类无一对一原名证据，不能凭空改成 CScriptObject。保留引擎与游戏依赖方向 |
| `ZMultiplayerStatistics.h` | 1 | 移至 multiplayer；本地合作/PvP 的双方统计值，计算由关卡负责，消费包括 HUD、结算与档案；不是网络包实现 |
| `brother/bot/ZLocalCoopBot.h/.cpp` | 2 | 自建合作 Bot，移至 multiplayer；保留 Z，不伪装 CRemotePlayer |
| `brother/bot/ZLocalPVPBot.h/.cpp` | 2 | 自建 PvP 策略，移至 multiplayer；原 CMPMatch 规则仍与 Bot 决策区分 |
| `brother/bot/ZLocalPVPBotNavigation.cpp` | 1 | 同一 PvP Bot 的寻路实现，跟随所有者 |
| `brother/bot/ZLocalBotFriend.h/.cpp` | 2 | Windows Bot 档案/身份适配，移至 multiplayer；不制造原 NGS 身份 |
| `brother/bot/ZLocalBotRoster.cpp` | 1 | 本地名单管理，随 Bot 档案归位 |
| `game/ZLiveShopSession.h` | 1 | 本地合作商店同步，移至 multiplayer；原请求延迟规则与 Bot 每波限购策略拆清 |
| `game/ZGameKeys.h` | 1 | Windows 游戏键位归 host 输入适配；保持集中配置，不复制到测试、Viewer 或各 gameplay 类 |
| `game/ZGameObserver.h` | 1 | 合并/缩小共享调用方接口后归 host，或直接归入 CGame 会话的嵌套接口；不能删除确定性输入、生命周期、测试截图能力，不能仅改名扩大 CGame |
| `map/ZMapViewer.h` | 1 | Viewer 选择、预览和研究输入迁出 gameplay。测试通过已有 Viewer 场景编译入口复用，正式 Game 不调用 Viewer；恢复原行为的函数继续归原对象 |

暂定结果：10 个根目录 Z 文件通过职责分解/归位消除，3 个非多人适配文件迁归 host/Viewer，10 个本地多人文件按现有对象职责管理。此处“文件消除”不意味着抹掉所有 Z 类型：引擎与 Windows 适配仍需要准确标识。

## 渲染问题一：弹体被地图遮住

### 已确认的实现差异

- `game/CGameDrawing.cpp:21,32,33` 顺序为弹体 BehindPlayer → 道具/角色/粒子共同队列 → 弹体 InFrontOfPlayer。
- `level/CLevelEffects.cpp:529` 的两次弹体循环均按相对主玩家的位置判定前后；没有使用弹体 `zOrderGroup`。这不是与其他地图对象逐个比较的排序值。
- `CBullet.cpp:288` 的 native 9 确实写入 `zOrderGroup`，`CBullet.h:202` 保存它，但当前弹体绘制没有消费者。Boss BULLET104 的真实 Flow 在负载偏移 `0xAA` 调用 native 9 设置组 5。
- 原 `CBullet::GetZOrder`（60280）针对特定 owner type，近距离时使用实际发射者的排序值减一；其余使用弹体 Y+10。当前把全部弹体都与主玩家比较，丢失发射者条件。
- 原 `CBullet::GetZOrderGroup`（64005）返回 native 9 写入的值。原 `CLevel::Draw`（120591）向共同队列加入关卡对象；`Compare`（145029）按 group 再按 ZOrder；`CRenderQueue::Draw`（145123）分别执行背景、主体、前景三个阶段。

这些是源码可确认的对齐缺口；尚未用自动化重现用户截图中的精确站位与朝向，因此不把其中某一项宣称为该画面唯一根因。

### 验收缺口与建议

现有 `tests/gameplay/MapChecks.cpp:23` 已覆盖 Haven `pack7/MAP6`，但只测试角色与建筑的前后遮挡，未生成弹体。其通过不能证明子弹层次正确。

实施前补充实际 `CGame::Session::Draw` 路径的 Haven 样本：保留真实地图、Infinity Laser、固定站位/朝向/时间；覆盖朝向两侧、近远枪口、不同发射者和脚本设置的 group。记录弹体与建筑的排序值、被覆盖的像素及背景/主体/前景阶段。

目标是恢复原版共同排序与三个绘制阶段，地表先画、弹体按原规则叠加，保留原版应当存在的建筑前景遮挡。不能用“所有子弹最后画”代替原版规则。

## 渲染问题二：Haven 机械 Boss 光束串珠

### 已复现的症状及历史

- 当前 `weapon-effects` 真实资源入口复现 `pack5/BULLET104` 串珠状绿色底图，截图保存在本轮证据目录；日志为主体 1、source 2、end 请求 3、100 个 beam quads、56 个 arc quads，亮度低谷为峰值的 3%。
- `066840d`（2026-09-10）曾按帧包围盒判断枪口槽，将动画 1 减为 0；旧文档明确声明它有意偏离原始字节，且当时没有 iOS 同场景视频证明。
- `cc8d9dc`（2026-09-17 effect 重构）移除该启发式，同时移除测试中 `troughPercent < 25` 的失败条件，改为只要求有亮像素并检查引用槽。
- 因此“之前修过、effect 重构后再次断裂”的历史链成立；但直接恢复旧补丁不满足当前原版优先要求。

### 本轮一手资源核对

按当前 `CBigFileReader` 的记录封装读取 `big/pack5_xga.big` 在 `0x6743` 的压缩块：原长度 235，zlib 解压后长度 235，与只读解包样本 `pack5_xga_0108_0x6743.bin` 全字节一致。

解压后的前七字节为 `85 75 26 00 8B 00 01`，即 Sprite 包 `0x00267585`、原型 139、action 0、animation 1。`0x6743` 指块头，不是解压负载起点；直接把压缩块头和样本相比没有意义。

原 `CBullet::Bind`（63647–63669）确实请求资源动画及 +1/+2；`Draw`（62965–63038）消费三个 SpritePlayer。Flow 清单没有 Sprite 序列，包含电弧、组号和状态控制。因此不能把“原始引用 1”直接改写成“正确引用必为 0”。

### 待验证的三个方向

1. **主体与端点的变换/步长**：保持原资源不变，逐式核对整数截断、GetBounds、source 的矩阵偏移、body 铺贴原点及末端长度；若变换有误，对应最小修正应改善画面而不改变引用。
2. **Sprite 消费语义**：核对 action、动画越界钳制、帧同步、图块展开和 blend。已读原 `CSpritePlayer::SetAnimation`（58861）会钳制越界编号；当前 `ZSpriteRenderer::Animation` 对越界返回空动画，两者存在一般性差异。尚未核实原型 139 的动画总数，不断言此次末端必定越界。该差异也不能单独解释主体串珠。
3. **真实 Boss 发射链**：使用实际 ENEMY/LEVEL Flow 对照独立 BULLET 样本，核对发射时后续 native 是否改变渲染状态，确认资源版本与真实 iOS 表现。

若完整消费链均符合原版而实际 iOS 画面仍未知，应记录证据冲突，不把已知视觉现象默认为“原版本来有 bug”。需要原版实机样本时再给出精确星球/敌人/攻击状态的核对请求；当前不要求用户补材料来代替可做的本地调研。

## 其余对齐工作一并纳入

- 弹体追踪 native 8：当前将转向速率当作搜索半径并瞬时转向，模板相关字段未接入 Bind；按原 `UpdateSeeking` 的目标持有、重选标志和角速度恢复。
- 弹体碰撞：Fire 的初始射线/线段查询、0x20 路径、反弹、道具伤害、ForceRemoval/OnRemove/枪械出口时序；不能只合并重复初始化。
- 游戏脚本：原 `CGame` 随机、模式和教程变量的所有权；当前逐宿主 ZRandom 不是 iOS 随机实现。native 分派中的合法零返回与未实现分支分别核对。
- 音频：原 SoundEffect/CSoundQueue 与 Windows 去重、忙窗口和循环归属分别管理。`loopSound` 当前只写不读是清理项，不能用它代替行为核对。
- 单人/多人统计：`GetTotalKills`、`m_kills`、挑战击杀、武器经验、PvP 比赛分数分别确认口径；本地统计不能直接声称是 StatisticPacket 的磁盘/网络布局。
- 头文件与依赖：删除不必要的整类 include；防止迁移引入 engine→gameplay、Game→Tests/Viewer 的反向依赖；现有测试与 Viewer 编译入口保留。

## 阶段任务与验收

下列阶段已经授权。复合验收项只在全部通过后勾选，未勾选不表示其所有工作均未开始。

1. [x] 固定两个渲染问题的自动化样本。Boss 保留资源绑定与视觉测量两种独立判据；地图新增实际弹体场景，重放旧顺序确认测试能够检测问题。
2. [x] 归位 weapon/armor，共享模板缓存／Bind，恢复追踪并验证生命周期；通过 `weapons,weapon-effects,mines,boss` 和实际 Haven 发射链。两类发射的不同准备步骤保留，理由见结果记录。
3. [x] 恢复弹体 group/ZOrder 与地图共同排序，删除两阶段弹体枚举；角色／弹体遮挡与三阶段验证通过。Boss 串珠依旧存在，原版证据边界见结果记录，未伪报修复。
4. [x] 归位 collision，核对出生碰撞、射线、反弹、爆炸与道具消费；通过 `map-occlusion,prop-combat,enemies,mines`，补齐原直接命中击退与四地图定向检查。未声称逐一穷尽所有历史碰撞分支。
5. [x] 分解 script 与共享状态，核对原 resolver 和随机/模式来源；通过 `level-flow,tutorial,missions,powerups,gameplay-ownership`。
6. [x] 归位 audio、统计与比赛规则；通过声音生命周期、本地合作/PvP、双方档案、商店／结算与单人进度相关检查。SoundEffect 解析归原对象，宿主播放策略明确保留，不建立 CSoundQueue 空壳。
7. [x] Viewer/host 适配迁出，根目录其余文件全部归位；更新 include、原名映射和文档，保留用户原注释与研究入口。
8. [x] Debug/Release 三产物构建与最终增量构建均通过；按影响完成进入/移动射击/受伤死亡/重开/结算相关检查，四地图各 500 波长程实战通过，最后 Debug 7 项与 Release 5 项复验通过。每阶段有定向验证，不重跑无关全量截图基线；详细命令及验收边界见结果记录。

完成标准：gameplay 根目录无游离源码；每个原 Z 文件有实施后的具体归属；multiplayer 以外不遗留无处置说明的 Z 文件；无转发空壳；无按资源 ID 修画面或假默认值；两个用户现象有明确复现/验证结论；与原版仍不确定的分支逐项列出，不以构建通过代替完成。

## 本轮验证与证据

命令：`pwsh -File tests/run.ps1 -Configuration Debug -Phase Core -Case weapon-effects,map-occlusion -NoBuild`。

- 两项现有测试通过，整体退出 0，受保护文件变化 0；运行器自动传入 `--mute`。
- 使用现有 Debug Tests（文件时间 2026-09-18 19:55:50），本轮未重新构建，因此不将结果声称为新构建验证。
- 另对本轮 `weapon-effects` stdout 重放历史连续性判据：提取 `trough=(\d+)%`，低于 25 时退出 1；实测 3，退出 1。该判据只检测已报告视觉回退，不是原版正确性的证明。
- `map-occlusion` 的 Haven 样本确实存在，但无弹体；精确截图站位尚未自动复现，不报告已完成地图问题的动态定位。
- 日志、截图、summary 与 169 文件清单已复制到 `obj/gameplay-z-research-20260919/`，避免下一轮测试覆盖 `tests/out/`。

参考：`docs/weapon-effects-night-migration.md`、`docs/boss-powerup-fixes-2026-09-10.md`、`docs/game-responsibility-migration.md`；BT `entries/bullet_template.bt`、`entries/common.bt`、`sprite_archetype.bt`、`maps/map.bt`；原函数行号均指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`，不冒充原 cpp 行号。
