# 拾取物组合层拆分

目录归并授权：建立 `src/gun_bros_re/gameplay/pickup/`，集中 `CPickup.h/.cpp`、`CPickupPresentation.cpp`、`CLevelPickups.cpp` 四个专属源码文件。只调整路径和引用，保留类职责、实现及注释；共享类中的对象池/奖励接口保留原归属，测试继续位于 `tests/`。迁移后核对内容一致、旧路径引用清零，并构建三产物、运行拾取物及关卡检查。

目录层后续授权：删除 `data/ZPickupCatalog.h/.cpp`、`ZPickupEntry` 与全局目录加载函数。单条 BIG 读取及完整性检查归 `CPickup::Template::Load`，关卡按包 hash 和局部序号持有模板；实例直接绑定模板和资源引用。测试只枚举引用并调用同一模板读取接口，展示名称和报告留在测试。验证沿用真实九模板、渲染截图、20 槽边界及实际关卡流程。

后续授权：彻底移除 `ZPickupResources` 及 `ZPickupVisual`。计划把通用帧展开/提交放回 `CSpritePlayer::Init/Draw`，由 `CPickup` 持有播放器；`CLevel` 直接管理拾取模板、Sprite 包及绘制批次。保持既有时钟和采集行为，先删除组合缓存，再构建三产物并验证拾取、实际关卡与共用 Sprite 播放。依据为 `CPickup::Bind/Draw` 99914/99677、`CSpritePlayer::Draw` 59035 及 `pickup_template.bt`、`sprite_archetype.bt`。

2026-09-17。用户批准优先拆分 `ZPickupScene`；本轮只处理拾取物及其调用方。

## 依据与方案

- 磁盘结构：`_prep/_Big_tool/binary template/big_assets/entries/pickup_template.bt`；原 `CPickup::Template::Init` 99591，资源继续来自 BIG，不改变解析格式。
- 原 `CPickup::Bind/Spawn/Update/Draw/Collect/OnRemove` 99914、99880、99736、99677、99805、99714：拾取物持有位置、Sprite 播放器及粒子引用；收集或移除停止发射，不立即删除存量粒子。
- 原 `CLevelObjectPool::GetPickup` 145625：20 个拾取物名额，满时返回空；`CLevel::OnPickupCollected` 118288 处理关卡通知。
- 原 `CPickup::FunctionResolver` 99742 调用玩家奖励入口，`CPlayer::CollectItem` 101094 调用商品获取。当前本地双人/PvP 的接触者奖励归属继续保留，不能冒充原版联网路径。

拆分后的持有关系：`CLevelObjectPool` 持有 `CPickup`；`CPickup` 持有位置、动画及效果句柄；`CLevel` 负责生成、更新、接触者选择、关卡通知及统计；`CPlayer` 负责商品奖励入口，库存继续调用 `CProfileManager`。最初保留的 `ZPickupResources` 已按后续授权彻底移除，模板、Sprite 包和批次归关卡，帧展开及绘制归 `CSpritePlayer`。

## 任务与验收

1. 提取资源缓存，恢复拾取物运行状态及对象池分配/回收；删除旧组合类，不留兼容壳。
2. 正式循环、研究入口和测试直接使用同一关卡/拾取物实现，删除重复的拾取物宿主参数。
3. 验证 20 槽满、回收后再分配、重复拾取、粒子停止排空与旧句柄不影响新效果。
4. 构建 Debug 三产物，按影响运行 `pickups,pickup-render,actor-feedback,deathmatch,deathmatch-feedback,local-live,tutorial,prop-combat`；测试显式静音，保留日志与必要截图，不跑全量截图基线。

## 保留边界

本轮不改已有接触算法、双人奖励规则和商品发放规则，也不恢复完整原联网/对象 UID 协议。拾取物池的 20 个名额与地图粒子系统的 20 个效果名额相互独立。

商品入口已核对 `entries/store_entry.bt` 与原 `CPlayer::CollectItem`，本轮保留既有 `Grant/AddPowerup` 行为，未恢复完整 `CStoreAggregator::AcquireItem` 的等级/容量等策略。

生成粒子改为在 `CPickup::Spawn` 立即申请，与原函数一致；粒子池满时不重试。对象移除仅停止发射，代数句柄防止旧拾取物停止已复用槽位中的新效果。

## 实现结果

- 删除 `gameplay/ZPickupScene.h/.cpp`，源码与测试不保留旧类引用或转发壳。
- `CPickupPresentation.cpp` 承担位置、Sprite 播放、绘制及粒子生命周期；模板读取和脚本出口仍在 `CPickup.cpp`。
- `pickup/CLevelPickups.cpp` 承担生成、接触者选择、奖励分派及关卡通知；实例由 `CLevelObjectPool` 持有。奖励先完成，再按既有顺序执行本帧 LEVEL 回调。
- `CPlayer::CollectItem` 接收商品奖励请求；`ZPickupResources` 和 `ZPickupVisual` 已删除。`CSpritePlayer::Init/Draw` 负责 BIG 动画展开和批次提交；每个拾取物在绑定时准备帧，纹理仍由关卡共享的 `CSpriteGlu` 持有。关卡初始化继续预先验证全部动画。
- 正式循环、研究入口和测试统一使用关卡接口，删除重复的拾取物宿主参数。单工程现有通配规则直接收集新增源码，无需新建工程。

## 验证记录

Debug 三产物构建：`pwsh -File obj/build-runtime.ps1 -Product Game`，退出码 0。日志：`obj/runtime-Game-build.log`。沙箱内 MSBuild FileTracker 无法运行，已使用获准的沙箱外构建。首轮发现的两处显式头文件缺失已修复。

分批执行以下检查；`tests/run.ps1` 统一添加 `--mute`，每批受保护的 BIG、存档样本及用户配置变更数均为 0：

```powershell
pwsh -File tests/run.ps1 -Configuration Debug -Case pickups,pickup-render,actor-feedback,deathmatch,deathmatch-feedback,local-live,tutorial,prop-combat,campaign-cache,horde-first -NoBuild
pwsh -File tests/run.ps1 -Configuration Debug -Case pickup-render,tutorial,campaign-cache,horde-first -NoBuild
pwsh -File tests/run.ps1 -Configuration Debug -Case campaign-cache,horde-first -NoBuild
```

运行器遇到失败立即停止，因此后两批执行剩余检查。三批运行器退出码分别为 1、1、0。

| 检查 | 结果 |
|---|---|
| pickups | 退出码 0；9 个原模板、13 个脚本动作；20 槽满、重复领取、回收再分配 |
| pickup-render | 退出码 0；9 个模板绘制，5 个持续效果，容量/回收/旧句柄/停止后排空均通过 |
| local-live、deathmatch、deathmatch-feedback | 均退出码 0；奖励归属、补给、复活及通知回归通过 |
| prop-combat | 退出码 0 |
| horde-first | 退出码 0；真实战斗及重开通过，生命/经验/矿石/手雷/单次存档检查通过 |
| campaign-cache | 退出码 0；实际关卡领取 3 个拾取物，failures=0 |
| actor-feedback | 退出码 1；5 处既有碰撞断言失败，拾取子检查为 9 个玩家领取、0 个 AI 领取 |
| tutorial | 300 秒超时；三条通关路径、奖励/存档及调试退出检查通过，停在最后的菜单 Shift-T 检查 |

首批证据保存在 `obj/pickup-validation-initial/`；渲染/教程证据在 `obj/pickup-validation-render-tutorial/`；该阶段最后一批已归档到 `obj/pickup-validation-levels/`。已查看 `obj/pickup-validation-render-tutorial/Core/pickup-render/pickup-render-check.png`，九种原拾取物显示完整。未运行全量截图基线。

为核对角色失败，将重构前 `cc8d9dc` 的源码独立导出到 `obj/pickup-baseline-source/` 并构建 Tests，构建退出码 0。基线使用 `--mute --big E:/coding_projects/c_projects/gun_bro_re/big --actor-feedback-check`，退出码同为 1；所有 `[actor-*-check]` 诊断与当前版本完全一致，包括 `failures=5`。日志：`obj/pickup-baseline-actor.log`。本轮未改动这些碰撞行为。

教程基线也使用该独立构建，以 `--mute --big E:/coding_projects/c_projects/gun_bro_re/big --tutorial-check` 运行；由 `obj/run-baseline-tutorial.ps1` 限制为 120 秒，超时后终止。日志 `obj/pickup-baseline-tutorial.log` 同样确认三条通关路径、奖励/存档及 `debug escape=1` 已通过，最终 `menu shift-T=1` 未出现。当前版本与基线均在末尾菜单检查未完成；这证明该用例在基线也无法通过，不代表已定位菜单挂起的具体原因。两项既有问题保留为独立待办，本轮不修改碰撞或菜单输入。

静态检查：`git diff --check` 通过；`src/`、`tests/` 与工程文件中无 `ZPickupScene` / `ZPickupCollection` 残留引用。

## 移除过渡缓存后的验证

已删除 `ZPickupResources.h/.cpp` 及 `ZPickupVisual`，没有别名或转发壳。通用帧展开和批次提交实现于 `engine/glu/sprite/CSpritePlayerDrawing.cpp`；原有纯时钟调用继续使用外部时长表。播放器拥有不可变帧数据，复制播放器时保持时长引用有效；纹理仍由关卡的 Sprite 包拥有，销毁/重新初始化时先清空拾取实例，再释放模板和包。

```powershell
pwsh -File obj/build-runtime.ps1 -Product Game
pwsh -File tests/run.ps1 -Configuration Debug -Case pickups,pickup-render,map-occlusion,movies,horde-first,campaign-cache -NoBuild
```

两条命令退出码均为 0，Debug Game/Viewer/Tests 均构建成功；6/6 检查通过，受保护文件变更数为 0。该阶段测试输出已归档至 `obj/pickup-validation-resources/`，运行日志为 `obj/pickup-resources-removal-tests.log`。本轮未重跑上文已确认的两项基线问题。

`tests/out/Core/pickup-render/pickup-render-check.png` 与前阶段保留截图的 SHA-256 完全一致：`A4C563F928B052B7E78EA02458C929CCA6A6260DD456B590004F7F72E650696B`。`git diff --check` 通过；源码和测试中不再出现 `ZPickupResources`、`ZPickupVisual`、`m_pickupResources`。

## 删除 data 目录包装后的结果

`data/ZPickupCatalog.h/.cpp`、`ZPickupEntry`、`LoadPickupCatalog` 已删除。`CPickup::Template::Load` 通过既有包读取器取得单条原始负载，调用原 `Init`，检查越界和尾部剩余字节；异常日志包含包 hash 和局部序号。磁盘字段顺序不变。

关卡使用按包 hash 分组的模板数组，数组下标就是该包的拾取物局部序号，不再另建条目包装。实例绑定 `Template` 与 `GameObjectRef`，不依赖展示名称。测试侧 `GetPickupCheckReferences` 只枚举原引用，解析仍调用生产接口；名称解码、标签及报告由对应测试生成。

```powershell
pwsh -File obj/build-runtime.ps1 -Product Game
pwsh -File tests/run.ps1 -Configuration Debug -Case pickups,pickup-render,horde-first,campaign-cache -NoBuild
```

Debug 三产物构建成功，补齐测试公共头文件的显式依赖后最终退出码为 0。4/4 检查通过，运行器退出码 0、受保护文件变更数 0；日志为 `obj/pickup-catalog-removal-tests.log`，该阶段详细结果已归档到 `obj/pickup-validation-catalog/`。两项此前已复现的基线问题未重跑。

与 `obj/pickup-validation-resources/` 对照，拾取物解析报告 SHA-256 均为 `D3AA9C6DFE193A2DB0729729CB05B0DFF7FD7DEB649067E7A7FE681E1BB9DAC0`，渲染截图仍为上节记录的相同 SHA-256。`git diff --check` 通过；源码与测试中无旧目录类型、加载函数或 `GetOwnerName` 引用。

## pickup 目录归并结果

专属运行时代码集中到 `src/gun_bros_re/gameplay/pickup/`：`CPickup.h`、`CPickup.cpp`、`CPickupPresentation.cpp`、`CLevelPickups.cpp`。迁移脚本逐文件比较迁移前后内容，仅允许 `CPickup.h` 的 include 路径替换；四个文件全部一致，旧路径无转发头。对象池、玩家奖励和通用 Sprite 类保留各自所属目录；专属测试仍在 `tests/checks/PickupCatalogChecks.cpp`，跨功能检查仍在对应测试中。

单工程的既有递归源码规则自动纳入新目录。以下命令均退出 0：

```powershell
pwsh -File obj/build-runtime.ps1 -Product Game
pwsh -File tests/run.ps1 -Configuration Debug -Case pickups,pickup-render,horde-first,campaign-cache -NoBuild
```

Debug 三产物构建成功，4/4 检查通过、受保护文件变更数 0。日志为 `obj/runtime-Game-build.log`、`obj/pickup-package-tests.log`，详细结果在 `tests/out/`。渲染截图与迁移前的 SHA-256 完全一致。`git diff --check` 通过，源码/测试/工程中无旧 `gameplay/CPickup.h` 或 `level/CLevelPickups.cpp` 路径引用。
