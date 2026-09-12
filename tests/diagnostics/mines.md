# 地雷诊断与修复（2026-09-12）

> 原版一致性待复核：用户随后反馈，3.6.0 的 Load Dropper 与 Deuce Dropper X90 都可以无限制丢地雷。下面的6/5留场数量是本地重建及所读脚本的结果，不能当作已由原版实测确认的上限；此前“最终验收”仅说明本地测试通过。连续发射与同时留场数量需要分别核对，不据此直接改写原资源或删除native13。

再次核对：本地CGun脚本确实包含计数比较与native13；原CGun::FunctionResolver调用FindOldestBullet(CGun*)和ForceRemoval，CBullet::GetLevelObjectType（64057）返回5，符合其筛选条件；CallExportFunction（107394）不重新初始化脚本局部变量。尚未找到能解释与用户实测差异的证据。本次复核未再修改游戏逻辑。

首次诊断后，经用户确认实施修复。复现使用 BIG 武器脚本、真实玩家动作和 WeaponEffects；边界取自 pack2 MAP7，经原关卡脚本选择的碰撞层4。固定16毫秒步长，以测试世界接收真实爆炸伤害，隔离敌人调度、输入和存档干扰。

## 运行

Debug 构建 GunBrosTests.vcxproj 后，在项目根目录的 PowerShell 执行：

```powershell
pwsh -File tests/run.ps1 -Case mines -NoBuild
```

预期退出码0。永久测试位于 `tests/checks/MineChecks.cpp`，也可用 `GunBrosTests.exe --mine-check --mute --test-output <隔离目录>` 单独运行。旧环境变量诊断入口已移除。

## 修复前结果

| 武器 | 10秒累计生成 | 20秒累计生成 | 距边界12单位：线外弹体帧数 | 距边界120单位 |
| --- | ---: | ---: | ---: | --- |
| Load Dropper | 7 | 7 | 81 | 无越界存活 |
| Deuce Dropper X90 | 6 | 6 | 75 | 无越界存活 |
| Eggsecutioner | 25 | 49 | 102 | 无越界存活 |

边界两端为(280,1638)和(273,1313)。线外帧数按每帧每个弹体累计，不是独立地雷数量。边界场景各运行960毫秒。修复前连射场景20秒时前两把的地雷已全部消失，枪仍不能开火。旧复现退出码1，多次结果一致。

## 根因与原版证据

原反编译行号均对应 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`，脚本偏移对应原bin。研究输入只读，复现运行不依赖 `_prep`。

1. **销毁回调遗漏。** 原CBullet释放时（62312）调用来源枪的CGun::OnBulletRemoved（128523），执行脚本export2。修复前CGun只有装备export0和开火export1；WeaponEffects移除弹体时没有执行export2。Load Dropper GUN29的export2在bin偏移0x164递减local[0]；Deuce GUN41在0x155做同样操作。缺失回调后两把分别累积到7和6，不再恢复发射资格。
2. **最旧弹体淘汰流程也不完整。** 原native13（128341）按具体CGun实例寻找最旧弹体（114474），立即ForceRemoval（61876），经释放回调更新计数，并据是否找到返回0/1。修复前native13只排队并固定返回1；战斗适配按玩家owner筛选，独立场景直接删除队首。修复不能只在某一处减少计数，也不能只按玩家找地雷。
3. **出生线段检测遗漏。** 原CBullet::Fire（62212—62243）按0x20选择普通地图碰撞层，再检查owner位置到弹体出生位置。修复前WeaponEffects直接从枪口生成，只检查后续移动线段。因此枪口越过边界后会漏检。Load Dropper脚本0x141发射速度倍率为0，更无法在后续移动时补检。terrain选层此前已实现，不是完全没有0x20逻辑。
4. **Egg不是同一种计数策略。** Egg GUN68的射击门控变量由计时事件0xAC—0xBF递减并恢复；export2为空，所以未受缺失销毁回调的连射卡死影响，但出生越界同样存在。

另发现寿命偏差：修复前CBullet.h默认lifetimeMs=3000，Update强制到期Hit；Load/Deuce原弹体脚本native4安排7680/256=30秒后的函数。原Update（63502起）没有统一3秒寿命，现已移除该宿主超时，让碰撞、视野剔除及原脚本控制消亡，不另写武器寿命表。

## 已实施范围

- CGun追踪本实例弹体，native13同步强制移除最旧活弹体。CBullet::OnRemove先断开来源再回调export2，防止重复通知；枪销毁或重新Bind时断开残留弹体的反向引用。
- WeaponEffects补齐owner到枪口的出生检查；0x20选择terrain（调试橙色），其他弹体仍选择walls（黄色）。出生碰撞沿既有event2、伤害和粒子通路处理，不把所有弹体都改成撞橙线。
- 去掉统一3秒超时以及已废弃的枪械RemoveBullet延迟cue。敌人按owner淘汰弹体的独立接口保留。
- 旧全武器测试的“松手4秒所有弹体清零”断言改为停止持续发射及停止光束；地雷留场寿命由专项测试验证。

## 修复后专项结果

- 20秒持续开火：Load Dropper累计33颗、场上6颗；Deuce累计28颗、场上5颗；Egg累计49颗，均继续发射。
- 三把地雷武器近边界测试均无线外存活弹体，触发真实Splash并生成粒子；Load/Deuce各2次爆炸、总伤害370，Egg为3次、总伤害186（基础装备测试场景值，不是另设的武器配置）。
- Load/Deuce单发后停止开火：29秒仍有1颗，31秒已消失且恰好爆炸1次。
- 两把同类型、同owner ID的枪：一把连续发射并回收，不影响另一把留下的雷。主动回收自己的6颗只触发6次爆炸；空集合native13返回0。
- 换枪时旧弹体保持存在，但不会通知新枪；两次Clear后重新连续射击仍保留正确6颗。
- ER97E Elite作为不带0x20的对照，验证普通子弹仍可越过这条橙色边界。

## 最终验收

- Debug测试程序、Debug/Release游戏和Viewer均构建成功，退出码0；编译器仍报告既有数值转换告警。
- `pwsh -File tests/run.ps1 -Case mines,weapons,weapon-effects,enemies,props,player-death -NoBuild`：退出码0，完成6/6。
- weapons、weapon-effects、mines、enemies、player-death五项通过；props仍为测试清单原有的KnownDataIssue（pack9:43模板解析，预期退出码1），未当作本次修复成功项。
- 保护文件检查变更数0，原资源与存档未修改。详情见 `tests/out/summary.json`、`results.json` 和各项logs。
- 已移除临时调试标签和环境变量入口；永久入口为 `--mine-check`。
