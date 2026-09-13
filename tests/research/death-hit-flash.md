# 死亡实体持续变红回归

2026-09-13。用户授权参考原版修复玩家与兄弟死亡后持续变红。

## 方案与验收

先在既有四地图死亡检查中补充尸体颜色断言，复现后核对原版更新分派；恢复死亡期间正常衰减，再检查逐帧慢动作衰减、兄弟复活和现有碰撞保护。运行器统一传入 `--mute`。

## 依据与根因

- `1d67b05` 在 `CBrother::Update` 的 `normalUpdate` 条件中加入 `dead`，致命伤害设置的 `flash=1` 因而永久停止衰减。
- iOS `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:135156–135235`：启用对象在非特殊复活动作、非 force/stun 分支调用 `UpdateNormal`，没有按死亡排除。`:138246–138258` 在 `mem+1988` 按每毫秒 0.002 衰减并钳制为零。
- `HandleDamage :136779` 经 export 2 进入原死亡 Flow。资源结构交叉核对 `entries/player_template.bt`、`entries/common.bt`；原 PLAYER 来源为 core 包 hash `0x58595522`、type 15 / Section 16 / ordinal 0。对应只读反汇编 `pack0_core_xga_0037_0x6726.flow.txt` 的 `0x268`、`0x26C` 选择死亡动作，`0x2A5` 设置慢动作，`0x2BA` 等待动作完成。
- `ReceiveDamage` 已拒绝死亡目标，`DrawPlayer` 每帧直接读取 `vitals.flash`，排除尸体重复受击及绘制缓存残留。

修复只移除普通更新条件中额外的死亡限制；保留 force/stun 暂停计时、BIG 动作和原衰减速率，未添加死亡时直接清色的补丁。

## 回归记录

修复前执行 `pwsh -File tests/run.ps1 -Case player-death`，退出 1。四图普通致死后玩家、兄弟均为 `corpse-flash=1.000`，作弊直接死亡没有受击染色，为 `0.000`。每图两处失败，其余原死亡时序检查通过。日志归档 `_prep/out/death-flash/before.log`，截图 `before.png`。

永久检查位于 `tests/gameplay/SurvivalDeathChecks.cpp`：真实致命伤害仍先变红、重复攻击尸体被忽略、按缩放后的世界时间逐帧衰减、死亡动画结束时归零；兄弟更新 250ms 时为半强度，随后归零并能复活。

最终验证：

| 检查 | 结果 |
| --- | --- |
| `pwsh -File tests/run.ps1 -Case player-death,actor-feedback,enemies,prop-combat` | 4/4 通过，退出 0；原资源、存档及账户变化 0。 |
| 四地图死亡颜色 | 玩家、兄弟均为 `corpse-flash=0.000`；慢动作逐帧衰减及兄弟半强度检查通过。 |
| Debug / Release `GunBrosRe.vcxproj` | 均构建成功，退出 0；Debug 自动 `progress` 检查通过。 |
| `git diff --check` | 通过。 |

运行与构建日志归档 `_prep/out/death-flash/`，包含 `verification.log`、`after.log`、`summary.json`、`debug-build.log`、`release-build.log`。目视对比 `before.png` 和 `after.png`，确认原死亡姿态下纯红覆盖消失、身体贴图恢复。Debug / Release 游戏 EXE 已更新。

验证限于上述死亡、碰撞及战斗流程；未逐帧对齐 iOS 实机，未运行全量截图基线。
