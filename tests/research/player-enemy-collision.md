# 玩家与敌人的移动碰撞回归

2026-09-13。用户批准按原版补齐普通阻挡与近战击退后的穿敌保护。

## 原版依据与实现

- `CPlayer::Move`，`_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:100623`：玩家候选位置先受地图外框限制，再按 `CEnemy::CanCollide` 筛选敌人。第 0 部件原始半径乘原生系数 0.8，圆心使用模型旋转偏移；朝敌人移动时按原圆碰撞参数截断，随后处理地图边缘。现位于 `CombatScene::ResolvePlayerMovement`。
- `CBrother` 构造函数 `:139098` 设半径 22。它是敌人接触圆的半径；墙体处理另取半径的一半及边缘余量，不能将现有 11.5 的墙体参数用于敌人阻挡和近战接触。
- `Collision::CircleCircle :65444` 的非标准判别式区间和位移长度判断已核对 ARM，按原算法保留在 `CombatGeometry::CircleCircle`，不替换为普通扫掠算法。弹体原有的 `CircleFraction` 保留。
- `CBrother::SetForce :137709` 调用 PLAYER export 4。原 BIG 的 PLAYER Flow state 10 在 bin 偏移 `0x354` 写 `variable[3]=1400`。运行时继续执行原脚本，未复制此时长到游戏代码。`CanPassEnemies()` 直接读取该变量，与红色强度、护盾及作弊免伤独立。
- **补充修正前轮调研**：`CBrother::Update :135184–135235` 在 force/stun 状态绕过 `UpdateNormal`。因此 1400 ms 计时在击退动作期间暂停，恢复普通更新后递减；并非自接受击退起固定 1.4 秒墙钟时间。`CBrother::Update` 已按此修正相关计时。
- 原 `Draw :134741` 与 `UpdateNormal :138258` 的保护闪烁、独立红色反馈衰减已接入；死亡角色不被保护闪烁隐藏。红色强度按每毫秒 0.002 衰减，正常更新中从 1 到 0 为约 500 ms。

原资源结构交叉依据：`_prep/_Big_tool/binary template/big_assets/enemy_template.bt`、`entries/player_template.bt`、`entries/common.bt`。更详细的只读调查在 `_prep/docs/player-enemy-collision.md`。

## 验证入口

```powershell
pwsh -File tests/run.ps1 -Case actor-feedback,enemies,prop-combat,player-death
```

运行器自动传入 `--mute`，原 BIG、存档与真实账户保持只读。新增检查复用永久 `actor-feedback` 入口，执行实际 `CombatScene::Update` 和 BIG 脚本。

覆盖内容：正常向内阻挡、向外脱离、碰撞模式过滤、死亡敌人不阻挡、普通变红不授予穿透、作弊免伤及护盾不自动穿透；实际敌人近战触发保护、击退时计时暂停、连续穿过三个敌人、计时结束恢复阻挡，以及保护期间仍被真实 BIG 地图边缘阻挡。另保留原圆碰撞数值分支用例。

修复前已运行新增阻挡检查，退出码 1：`blocked=0 escaped=0 inward=3.520 outward=0.000`。第一轮修复后退出码 0：`blocked=1 escaped=1 inward=0.000 outward=-3.520`；真实近战穿敌距离 `165.440`，`blocked-again=1`。

## 最终结果

| 验证 | 结果 |
| --- | --- |
| `actor-feedback` | 通过，退出 0；向内位移 0，向外 -3.520，穿过三个敌人后到期恢复阻挡。 |
| 保护期间真实地图碰撞 | 通过，`immune-wall separation=11.500 immune=1`。 |
| `enemies`、`prop-combat`、`player-death` | 全部通过，退出 0。 |
| `pwsh -File tests/run.ps1 -Case final-pack2` | 通过，退出 0；最后一波及结算完成，原始资源与存档变化 0。 |
| Debug / Release `GunBrosRe.vcxproj` | 构建通过，退出 0；Debug 工程规定的 `progress` 自动测试通过。 |
| `git diff --check` | 通过。 |

正式关卡首次检查同时暴露了旧手雷测试的截止时间假设：电击伤害及 750 ms 眩晕参数正确，但固定 4 秒时效果仍余 62 ms。`SurvivalWavesChecks.cpp` 现保留伤害/时长检查，再通过实际敌人更新验证剩余计时最后 1 ms 前仍生效、最后 1 ms 后解除；未修改手雷游戏逻辑。

本地构建日志：`_prep/out/player-enemy-collision-debug-build.log`、`_prep/out/player-enemy-collision-release-build.log`；正式关卡日志：`_prep/out/player-enemy-collision-final-pack2.log`。可执行文件：`bin/Debug/GunBrosRe.exe`、`bin/Release/GunBrosRe.exe`。

边界：验证覆盖当前 BIG 脚本及上述流程，未逐帧对齐 iOS 实机，也未穷举所有敌人所有脚本状态；未运行全量截图基线。
