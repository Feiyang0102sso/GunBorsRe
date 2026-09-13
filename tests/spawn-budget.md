# 生存关卡刷怪容量与批次节奏修复

## 问题与范围

2026-09-13，用户确认帧率和敌人间距已恢复，但反馈敌人涌入过多、像几批合并。截图为 pack2 / map7，第 50 波、状态 106、存活 49、击杀 51。调查发现默认敌人池容量错误，以及死亡对象过早释放刷怪名额。本轮恢复这两项原生限制，不修改 BIG 波次配置、敌人组合、总量、出生节点或间隔。

## 原版依据

`_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`：

- `CLevelObjectPool::CLevelObjectPool :145400` 将 `pool+275584` 的 u16 最大数量设为 **20**，后面的 u16 为已占用数量。池底层存储 100 个实例不等于普通刷怪容量。
- `Clear :145780` 清零占用数量，保留容量；本轮移除重建版在 Bind 时无条件写入 50 的操作。
- `CEnemySpawner::GetNumFreeEnemies :147230` 普通入口返回容量减占用数；`CLevelObjectPool::GetEnemy :145509` 同样检查容量并在分配后增加占用。
- `CLevelObjectPool::Release :145426` 在移除敌人对象时减少占用；死亡不等于立即释放。`CLevel::RemoveObject :116783` 调用 Release。
- `CLevel::FunctionResolver` native 59（:118044）允许脚本设置容量，上限 50。这是显式修改的上限，不能当默认值。当前解码清单中 pack11 的关卡确有设置 50，pack12 生存关卡设置 20；所查 pack2 生存脚本没有此调用。
- `CEnemySpawner::GetEnemyCount(resource) :146595` 遍历未移除的对应资源对象，用于每条规则的数量限制；无参数版本 :146257 对敌人列表检查存活，用于刷怪器总存活上限。两者计数口径不同。

同时核对 `entries/level_template.bt`、`entries/common.bt`、`maps/map.bt` 及 Flow 模板。本次没有新增磁盘字段。

## 第 50 波为何仍有混合敌人

依据原 BIG 对应的 `pack2_xga_0014_0x19f3.bin`（LEVEL ordinal 6）及其 Flow 清单：

- wave 索引 49 在 `@0x3351` 进入状态 105。
- 状态 105 的 `@0x237A—0x239A` 同时启用两条规则，资源槽 1 / 4；各累计 25，总数 50；各自同时存在上限 20 / 15。
- 状态 105 的击杀事件计数到 50（`@0x2363`）才进入状态 106。
- 状态 106 的 `@0x23C7—0x23FB` 启用资源槽 1 / 4 / 12，共 20 / 20 / 10，总数仍为 50；这是用户截图中的三种混合敌人。
- 上述规则还同时受原生默认池容量 20 限制。因此每批总数 50 应陆续补出，并不表示默认允许同时存在 50 个敌人。没有依据把这些原有混合规则改成单一兵种。

## 修改

- `CLevel` 默认容量 50 改为原版 20，保留 native 59 显式覆盖，Bind 保留已配置容量。
- 新增 `IEnemySpawnWorld::CountEnemySlots`；`SurvivalSession` 按未移除对象计数，包括死亡对象。普通存活计数不改。
- `CEnemySpawner` 的池容量和每资源规则最大值改用占用计数；刷怪器全局存活上限继续用存活计数。
- 现有性能检查改为验证达到实际池容量且 CPU p95 不超过 16.667 ms。之前要求至少 30 个敌人的条件基于错误的默认 50，不能作为原版默认玩法的验收标准；早先 35 敌人的性能数据仅作为历史压力测试证据保留。

## 回归

最小复现读取真实 BIG 第 50 波脚本，发送原版开场完成事件，不发送死亡回调，更新 3 秒：

| 条件 | 修复前 | 修复后 |
| --- | ---: | ---: |
| 默认容量 | 50 | 20 |
| 状态 | 105 | 105 |
| 存活 | 35 | 20 |
| 测试退出码 | 1 | 0 |

显式调用 native 59 将容量改为 50 后，修复版恢复补怪并达到两条规则合计上限 35，证明没有把整个游戏硬锁到 20。另以测试世界模拟 20 个未回收槽位，确认存活数为 0 时仍不补怪；回收后恢复生成 20 个。

命令：

```powershell
pwsh -File tests/run.ps1 -Case level-flow,flock,spawn-performance-realtime,final-pack2 -NoBuild
```

原件只读，自动验证静音。修复前日志 `obj/spawn-budget-before.log`；回归总日志 `obj/spawn-budget-regression.log`。

- level-flow：通过，包含默认容量、脚本覆盖与未回收槽位阻塞检查。
- flock：通过，分离与眩晕/到点检查无回退。
- pack2 第 50 波真实补算：通过，1200 个逻辑步、峰值存活 20，CPU p95 3.646 ms。
- final-pack2：走到波次 500，636 出生、618 击杀、0 存活、invalid=0；退出码 1，唯一累计失败仍为已知电雷眩晕到期断言，未修改手雷。protected-changes=0。回归证据已复制到 `obj/spawn-budget-evidence/`。
- Debug / Release 构建日志：`obj/spawn-budget-game-debug-build.log`、`obj/spawn-budget-game-release-build.log`。

恢复的默认上限是原程序算法常量，不是根据玩家体感手调的难度值。预选阵容和出生编组仍不作为既定事实。
