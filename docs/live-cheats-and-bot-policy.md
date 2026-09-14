# Live 作弊码与 bot 决策修正

2026-09-14。用户补充：故障表现为窗口短暂无响应、画面停止，随后恢复，没有闪退。

## 范围与验收

本阶段按已授权范围处理作弊码阻塞、波次事件交接和 Live bot 决策。任务为：建立组合回归 → 核对原版消费者 → 修复阻塞与事件传递 → 接入商店及道具条件 → 定向测试与双配置构建。

- `stboss` 在主循环逐帧快进，窗口仍能处理输入；暂停、救援或任一角色的道具 Movie 进行中不启动。
- Live bot 每个绝对波次最多一次普通商店请求，玩家手动购物不受 bot 次数限制。`bros` 不占用次数；复活道具选择属于死亡处理，不占普通购物次数。一人倒地等待救援时禁止开店，包括救援接近阶段；已经排队或显示的商店也会关闭。
- bot 自动用空袭时，存活敌人数必须大于 10；手雷类必须有至少 2 个存活敌人在自身 250 世界单位内。250 是本次 Windows bot 策略参数，不是原版资源值。
- bot 不在救援时自动用道具，不在启动道具 Movie 的同一帧请求商店。手动道具输入与 `brop` 保留原来的使用路径。

## 依据与实现

`entries/powerup_template.bt`、`entries/bullet_template.bt`、`entries/common.bt`、`flow_bytecode.bt` 与 `ui_movie.bt` 已核对。原程序路径为 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`。

- `CPowerup::FunctionResolver` native 1（约188267行）从脚本引用加载 Movie；本批原 POWERUP 的 Movie 依赖类型为253。当前可用道具中的非 afterDeath Movie 道具对应三个空袭。`PowerupScene::Init` 从 BIG 依赖计算分类，不维护道具 ID 表。
- `CBullet::IsGrenade`（60370行）检查 flags bit 4。`PowerupScene::Init` 读取原 POWERUP 引用的 BULLET 模板，按该位确认手雷；测试使用三种原手雷资源核对。
- `CLevel::Update`（121325–121394行）在 Live 波间等待期间继续更新对象，只暂停关卡脚本。原重建的等待分支在 `scene.Update` 后直接返回，跳过死亡、传送和关卡事件交接，以及道具库存、拾取和场景物件更新；下一帧场景清空事件容器，导致事件丢失。现改为共用完整更新路径，只扣住 LEVEL 更新及波间释放回调。
- `BossCheat.cpp` 保留原 LEVEL 的强制 Boss 标志和实际敌人死亡回调。每帧最多32步、约4毫秒预算，单步不可中断；总墙钟上限15秒，原600000毫秒模拟上限保留。每批结束恢复角色无敌标记和声音状态，结束或取消恢复原标志。
- `ApplyCombatCheat` 原先在任何战斗作弊码后都保存账户。现在只有修改账户数据的命令即时保存；`brok/stsuicide/stboss` 等运行时操作由正常波次／结束保存边界保存游戏进度。

## 复现与验证记录

命令均由 `tests/run.ps1` 自动加上 `--mute`，原始 BIG 和存档样本只读。

1. `pwsh -File tests/run.ps1 -Case local-live -TimeoutSeconds 90`：新增账户写入断言在旧保存路径退出1，日志出现 `runtime cheat unexpectedly saved account`；规则边界检查已通过。见 `obj/live-runtime-save-red.log`。
2. 恢复旧波间提前返回分支运行相同专项：退出1，`wait-death-delivered=0`。见 `obj/live-wait-event-red.log` 与 `obj/live-wait-event-red-stdout.log`。该夹具在真实敌人尚有待交接死亡时设置 Live 覆盖层，专门验证事件不能丢失，不声称重放了用户当时的所有输入。
3. 修正后首轮专项退出0：两名角色分别倒地、对照组均推进两波；空袭10/11个敌人、手雷距离与数量、库存不误扣、作弊开店免计次通过。Boss快进35批，峰值约10毫秒。见 `obj/live-policy-green.log`。

最初新增测试还误跑了同一入口的单人分支；单人死亡后停止波次是预期行为，已将该组合检查限定为 Live。这条早期失败没有被当作 Live 根因。

当前测试存档未重现用户原现场的全部短暂失响应；已验证不必要的账户写入和波间事件丢失，并覆盖快进分帧及相关状态互斥。实际机器的单次资源加载／磁盘延迟仍可能影响帧耗时，4毫秒预算不是单帧耗时的硬上限。

## 最终结果

相关9项全部通过：`debug-input,player-death,local-live,deathmatch,boss,powerups,progress,powerup-play,powerup-selector`。其中前5项记录在 `obj/live-final-validation.log`，最终 Live 与其余4项记录在 `obj/live-final-followup-validation.log`（5/5、退出0）。两轮均报告 `protected-changes=0`。

首轮 `powerups` 暴露既有 Shock G. 断言遗漏：原 `pack5_xga_0097_0x61ab.bin` 在 `0x97` 产生三次定时脉冲，随后 function 2 在 `0xB4` 再爆炸一次并移除，共四次。已将测试预期从3改为4，并保留首爆时间、轨迹事件和移除断言；没有修改手雷伤害行为。原失败报告保留在 `obj/live-powerups-old-expectation.txt`。

最终游戏 Debug／Release 构建退出0，输出为 `bin/Debug/GunBrosRe.exe` 和 `bin/Release/GunBrosRe.exe`，日志为 `obj/live-final-Debug-build.log`、`obj/live-final-Release-build.log`。使用 `GunBrosRe.vcxproj /p:Configuration=<配置> /p:Platform=x64 /p:SkipAutoTests=true /m`；对应检查已通过运行器单独执行，未重复全量截图基线。`git -c core.safecrlf=false diff --check` 通过。
