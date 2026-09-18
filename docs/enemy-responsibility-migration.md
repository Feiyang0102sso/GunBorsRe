# 敌人职责归位与目录统一

开始时间：2026-09-18 06:08 UTC；截止时间：11:08 UTC。用户授权本批独立完成，完成并测试后停止，不推进其他主题。

## 范围与验收

用户要求优先于旧命名约定：本批敌人专属源码统一放入 `src/gun_bros_re/gameplay/enemy/`，文件采用 C 前缀，原有 I 接口保留 I 前缀。文件头注明原文件、原函数与宿主适配范围。共享图形、地图、脚本和关卡不因被敌人调用而整体改名。

原版依据：`_prep/_IDA_OUT/source_tree.md` 的 `enemy.cpp`、`enemySpawner.cpp`、`flock.cpp`、`linkPathFinder.cpp`、`meshPathFinder.cpp`、`levelObjectPool.cpp`、`menuMeshEnemy.cpp`；ENEMY 磁盘字段依据 `enemy_template.bt`，同时核对 iOS 反编译消费者。

实施顺序：

1. 敌人模板、模型资源和绘制归回 CEnemy，移除 ZEnemyModel；对象池直接持有 CEnemy，移除 ZCombatEnemy。
2. 敌人辅助代码目录归并；敌人菜单展示按 CMenuMeshEnemy 归位，地图预放置不再单独包装敌人状态。共享关卡只保留调度职责。
3. 核对出生出口、部件动画、碰撞和绘制，删除无原版依据的自动选状态／首动作回退；必要研究操作由 Viewer 明确请求。
4. 网络刷怪保留 TODO。核查本地合作和 PvP Bot 的调用关系，不引入网络实现。
5. Debug / Release 的 Game、Viewer、Tests 构建；按影响运行敌人、炮塔、Boss、碰撞、刷怪性能、群体／寻路、合作、PvP、死亡重开及结算显示检查，所有自动运行传入 --mute。

验收要求：无敌人 Z 文件或 Z 敌人包装类型；无旧路径和转发别名；实例状态和脚本只有一份；原资源与存档只读；保留既有注释和研究入口；记录真实测试结果及未验证的原版差异，不把构建成功当作全量原版行为证明。

## 网络边界

调用核查：本地 `ZLocalCoopBot`、`ZLocalPVPBot`、`ZLocalBotFriend` 没有调用 CNetworkEnemySpawner。当前敌人生成通过本地 CEnemySpawner / CLevel / CLevelObjectPool。合作、PvP 与 PvP 反馈回归均已通过。原网络生成器、SpawnPacket 和敌人网络状态同步暂不恢复，TODO 位于 `CEnemySpawner.h`。

## 实施与验证结果

### 所有权与目录

- `enemy/` 当前 36 个生产源码文件，全部 C／I 前缀。已删除 `ZEnemyModel`、`ZEnemyTemplateData`、`ZCombatEnemy`、`ZPlacedEnemy`，无兼容别名或旧路径转发头。
- `CEnemy::Template` 负责 ENEMY 读取；`CEnemy` 直接持有脚本、部件、动画和战斗状态。资源缓存只共享模型／纹理／上传缓冲，不共享实例脚本或动画控制器。
- `Bind` 只绑定，调用方显式选择 `Spawn` 或 `SpawnForUI`。删除菜单自动进入状态 0／首动作的无依据回退；`CMenuMeshEnemy` 使用原 UI 生命周期。
- `CLevelObjectPool` 直接持有 `unique_ptr<CEnemy>`，仍保留原 100 个敌人槽和独立 20 个拾取物槽；静态地图直接持有稳定模板与敌人，模板晚于实例析构。
- `CEnemyWorld`、`CEnemyCasualty` 的 C 前缀遵循本轮用户要求；两者文件头明确为宿主契约／统计展示值，不声称恢复了完整原版接口继承或 `CStatisticEnemy`。
- 共享地图层、碰撞、脚本、图形、弹体与主循环保留原职责目录；Viewer 和测试仍分别编入对应产物。

### 原版行为核对与修复

| 内容 | 原版依据与结果 |
|---|---|
| ENEMY 模板、资源绑定 | `enemy_template.bt`；Template::Init :67174、Load :68895、Bind :73381。检查截断／尾部多余字节；CPU 调研不会污染 GPU 缓存，资源失败不会发布半成品缓存 |
| 导航目的点 | `meshPathFinder.cpp` :168398，ARM 0xe7754–0xe7978；从下一格公共边取最近点并推进一单位，保留原有符号 epsilon 判断，去掉旧 240ms／多节点前瞻启发式 |
| 直达判定 | `CLayerPathMesh::CanMoveDirect` ARM 0xe71a0–0xe71d4 确认不同格时实际传入两个零端点，未调用 GetSharedSide；保留该 iOS 行为，不将其擅自修成另一套视线算法 |
| 距离图与群聚 | `flock.cpp` :170259、:170441，`layerPathMesh.cpp` :167884、:167932、:168066。按目标共享距离图；锁／同尺寸重载使缓存失效；原递归松弛改为堆松弛，邻接选择保留原次序 |
| 格子定位 | `CLayerPathMesh::GetCellForLocation` :167744，恢复射线交点规则与最近未锁格子，支持凹四边形 |
| 直线冲刺边界 | `CEnemy::SetBehaviourMoveAngle` :69625／ARM 0x3c874，`CLayerPathMesh::CastRay` :168257、GetConnectedCell :167987。沿连接格子查射线出口，扣除部件 0 半径后限制移动；负距离按 ARM 0x3c908 变为 1，最终非正距离使用原常量 0.1 |
| native 23 视线 | `enemy.cpp` :72152 → `layerCollision.cpp` :125270 → `collision.cpp` :65660，查询当前地图层和活动道具弹体边，不再始终返回真；平行／共线与端点规则按原版 |
| native 54 子敌人 | `enemy.cpp` :72587，ARM 0x403b4／0x40444；读 LEVEL 资源表，在当前部件节点位置生成，截断坐标、保留可选对象 ID，不强制占槽、不继承玩家召唤者 |
| 节点位置 | `mesh.bt`、`CEnemy::GetNodeLocationChunk` :70263／ARM 0x3d424；按当前部件边界中心、挂点四元数、原倾斜和 gameScale 计算；不混入 viewport 或受击圆缩放 |
| native 59 | :72640：前三参数为 Q8，最后一参数为整数转浮点；恢复四值存储，后续消费者尚未证实，不编造演出效果 |
| UI 模型 | `menuMeshEnemy.cpp` :169038／:169098／:169128；`CEnemy::UpdateUI` :68332、DrawUI :68451，脚本先刷新，再推进 UI 部件动画 |

本次还修复了 Arena 换枪重置生命的测试暴露问题：已装备角色切换缓存武器，首次装备才执行初始化。保留原生命断言。测试入口补齐输出目录，避免性能检查对空路径调用 `create_directories`；临时异常诊断已删除。群聚检查分离控制段与真实地图寻路，保留位移与间距断言；长跑驾驶器扩大交战距离、优先处理近身威胁，保留敌人眩晕和全部通关断言。

### 验证记录

命令日志和逐例截图备份位于 `obj/enemy-rework/`（忽略目录，不作为运行依赖）。所有自动执行静音，受保护资源与存档检查变化数为 0。没有重复全量截图基线。

- `build-debug-final.log`：Debug Game／Viewer／Tests 统一构建，默认 3 项验证与 Viewer 全部冒烟检查通过。
- `check-native.log`、`validation-native/`：敌人、寻路缓存、群聚全部通过。78 个 ENEMY 条目中 76 个有脚本、2 个无脚本；本次覆盖中未知 native 计数为 0。新增资源隔离、实例独立、模板边界、LEVEL 子敌人生成和 native 59 单位检查均通过。
- `check-gameplay.log`、`validation-gameplay-first/`：死亡重开、本地合作、PvP、PvP 反馈、Boss、道具战斗、角色反馈、LEVEL Flow、Horde 首／末波通过；此批旧群聚夹具失败已在后续修复回归。
- `validation-navigation-first/`：敌人、地图炮塔、结算敌人展示通过，已人工查看结算模型截图。
- `check-boundaries-fixed.log`、`validation-boundaries/`：敌人、刷怪性能、实时刷怪性能、寻路、群聚和 pack2 最后两波通过；pack7 曾被围堵后持续眩晕而超时。
- `check-boundaries-other.log`、`validation-boundaries-other/`：pack9／pack12 最后两波通过。
- `check-pilot-fixed.log`、`validation-pilot-fixed/`：驾驶器修复后 pack7 最后两波通过，最终 wave=500、alive=0；通关断言未放宽。
- 性能初测：1200 次更新、峰值 20 敌人、CPU P95 3.173ms；实时模式 P95 2.895ms，均通过 16.667ms 门槛。此为测试机和该场景测量，不代表所有地图的最坏耗时。
- 失败证据保留：`check-sight-red.log` → `check-sight-green.log`；`check-performance-diagnostic.log` 记录空测试路径异常，后续两项性能检查均通过。

- `build-release-final.log`：Release Game／Viewer／Tests 统一构建通过。`check-release-final.log`、`validation-release-final/` 的 21 项回归全部通过，包含上述战斗、UI、性能及四地图末轮场景；保护文件变化数 0。Release 性能 P95 分别为 1.709ms／1.458ms。
- 收尾复查修复了 LEVEL native 33 在尸体回收后使用旧群聚成员地址的风险：从对象池重新收集有效成员后立即刷新距离图。`build-debug-membership.log`、`build-release-membership.log`：两配置三产物增量构建通过；`check-membership-debug.log` 及最终 Release 的 LEVEL Flow／寻路／群聚三项回归通过。
- Debug 的 pack2 长跑在已清除 47 波后主动停止，改用 Release 执行完整 2,000 波验收，以留出五小时上限内的修复余量。`check-longrun-debug.log` 中 exit=-1 是主动终止，不计作长跑通过；部分证据保存在 `validation-debug-longrun-partial/`。
- `static-audit.json`：36 个文件，非 C/I 名称、缺原版文件头、缺失 include、旧包装类型均为 0。`git diff --check` 通过；原注释随职责迁移保留，补充历史说明与已确认的纠正。

首轮 Release 长跑中 pack2 500 波通过（127.7 秒）；pack7 在 wave=444 留下一只处于直线移动的敌人，位置 x=-261.6，无法被玩家命中。原版核对确认旧代码漏掉 `SetBehaviourMoveAngle → CLayerPathMesh::CastRay`。最小地图回归先复现越界目的地 550／-450，再修复为 190／10，并验证负距离语义、跨格射线及原版不排除已锁邻格的规则。红／绿证据分别为 `check-move-angle-red.log`、`check-move-angle-green.log`；首轮完整日志保留在 `validation-longrun-first/`。

最终源码的 `build-debug-ray-final.log`、`build-release-ray-final.log` 均退出 0，两配置各自的 Game／Viewer／Tests 构建通过。Debug 再跑敌人资源、寻路和群聚三项，全部退出 0；日志为 `check-debug-ray-arena.log`、`check-debug-ray-path-cache.log`、`check-debug-ray-flock.log`，截图与输出位于 `validation-debug-ray-final/`。

修复后 `check-longrun-ray-fixed.log` 的六项检查全部通过（敌人、群聚与四张地图完整长跑），runner 退出 0，保护文件变化数 0。完整输出已备份至 `validation-longrun-final/`。

| 地图 | 已清波数 | 生成数 | 击杀数 | 结束时活动敌人数 | 耗时 | 结果 |
|---|---:|---:|---:|---:|---:|---|
| pack2 | 500 | 28,837 | 28,827 | 0 | 218.1 秒 | 通过 |
| pack7 | 500 | 28,723 | 28,722 | 0 | 146.7 秒 | 通过 |
| pack9 | 500 | 28,601 | 28,592 | 2 | 140.5 秒 | 通过 |
| pack12 | 500 | 28,791 | 28,720 | 1 | 93.0 秒 | 通过 |

长跑使用测试驾驶器、实际弹体与敌人死亡脚本，并开启玩家无敌；验证 500 波清除计数及关卡 `IsCleared()`，不等同于真人难度测试。pack9／pack12 结束时仍有活动对象，已保留真实计数，不将关卡通关表述为对象池全部清空。四图合计完成 2,000 波，未放宽通关断言。

完成时间：2026-09-18 08:01 UTC，约用时 1 小时 53 分，低于五小时上限。完成本批后停止，不进入下一项任务。

### 验证边界

网络生成／同步按用户要求未实现。本地 Bot 无网络依赖。空 Arena 没有 LEVEL 上下文，native 54 和相机 native 68 仍显式计入 deferred，不伪造资源或关卡。native 59 的存储已恢复，但其后续消费者未知。Windows 图形／音频、延迟动作队列和宿主随机数序列仍是移植适配；这些检查证明覆盖流程可运行，不能证明与 iOS 每一帧、每个未走分支完全等价。
