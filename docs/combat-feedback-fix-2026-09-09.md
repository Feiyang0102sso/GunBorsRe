# 战斗反馈修复与验收（2026-09-09）

> 后续纠错：本记录中的血条尺寸验收不充分，曾将 LEVEL 变量 4 错当轮次，并混用镜头/屏幕倍率。已重新核对 BIG 脚本并修复，显隐检查也改用原敌人状态，见[血条与音效复核](audio-health-fix-2026-09-09.md)。本页保留当时的检查记录，不作为这些错误判断的依据。

本阶段按用户反馈修复尖塔层级、技能计时、战斗切枪、快捷键、波次冻结、敌人血条和已复现的无伤害路径。运行时资源仍直接读取 BIG；没有改动原资源、原存档或正式账户。

## 方案、任务与原版依据

执行顺序为核对 BT 和 iOS 消费函数、复现失败、修复实际运行链路、专项验证、跨地图回归。以下源码行号均指 `_IDA_OUT/gunbros_3.6.0_IOS.c`。

- [x] **尖塔层级**：磁盘字段原顺序正确，错误在 CProp 原生动画槽映射。`CProp::Template::Init`（123346）、`Bind`（124863）、`DrawBackground/DrawForeground`（123549/123555）和 `FunctionResolver`（124470）确认槽 0 为背景、1 为主体、2 为前景。修正 `gun_bros/CProp.cpp` 与 `runtime/MapScene.cpp`，补充 `entries/prop_template.bt` 注释，不交换磁盘字段。
- [x] **持续和冷却**：pack7 PROP 33，样本 `big_360_out/pack7_xga/0xf4e02223/20_PROP/pack7_xga_0071_0x3dba.bin`，Flow 清单 `out/binary-research/flow-disassembly/pack7_xga_0071_0x3dba.flow.txt`。原脚本设置 4 秒对象计时、51/256 时间缩放，结束后恢复正常时间并冷却 120 秒。按 `CLevel::TransformObjectElapseMS`（114280）恢复机关对象的缩放计时；不修改脚本参数。
- [x] **战斗切枪**：普通战斗和商店共用双枪资源与原角色状态机，等待 `OnSwapGun` 的 native 3 事件再换枪，保留躯干动画和时间。依据 `CPlayer::OnSwapGun`（101048）、`CBrother` 原生换枪分支（138868）及 BIG Player/MoveSet。`PlayerModel`、`CombatScene`、`WeaponEffects` 统一读取当前枪，避免换枪后发射、枪口或移动属性仍使用旧枪。
- [x] **按钮与快捷键**：2 键驱动原蓝色切枪按钮的按下/松开帧，依据 `CInputPad::Base::UpdateInput`（88480），core Sprite 1 的动画 35/36。取消战斗 F/R 键输入，保留鼠标重试按钮；Q/E 道具、1 紧急购买、2 切枪继续有效。
- [x] **波次结算**：移除 `SurvivalSession::Update` 在 HUD 过场期间提前返回的逻辑，Movie 和游戏对象继续更新；慢动作仍由原关卡脚本控制。依据 `CGame::Update`（76581）、`CLevel::UpdateNormal`（121150/121333）。
- [x] **敌人血条**：按 `CLevel::DrawEnemyHealthBars`（120454）、`CEnemy::GetBoundsInternal/GetBounds`（67314/67485）及命中闪光（71563）恢复位置、尺寸、血量填充、受击亮度和脚本变量 15 的显隐。模型包围盒、gameScale 来自 BIG；30×4 等尺寸属于原生绘制算法常量。实现于 `EnemyCombat`、`CombatScene` 和 `SurvivalHud`。
- [x] **无伤害复现**：正常可受伤敌人的玩家、队友连续同帧命中均累计扣血。另复现原穿透弹遇到待处理命中后停止继续碰撞的问题。原单人碰撞没有联网 `ApplyCollision`（71676）才使用的弹体暂停；移除 `WeaponEffects` 对 `pendingHit` 的错误碰撞阻断，保留延迟回调本身。没有绕过原脚本的护盾、待机和免伤状态。

## 失败与修复验证

新增永久研究入口 72 / `--combat-feedback-check`，并加入 `test-muted.ps1` 的 OriginalUI 检查。测试对象、位置和伤害只存在于检查分支，使用 BIG 模板和独立账户副本。

| 检查 | 修复前 | 修复后 |
|---|---|---|
| HUD 结算期间移动 | 被提前返回阻断，移动 0 | 实际 wave-clear Movie 仍播放时，20 个 16ms 更新移动 70.398 世界单位 |
| 尖塔持续和冷却 | 持续仅 4000ms 墙钟时间 | 16ms 固定步下持续 21344ms、冷却 120000ms，与原缩放及逐步取整计算一致 |
| 原穿透弹穿过待处理敌人后命中后方敌人 | 后方伤害 0，检查退出 1 | 使用 pack5 BULLET 80，后方伤害 5，检查退出 0 |
| 同帧双人伤害 | 核对原脚本状态，未发现通用覆盖冲突 | 10 种正常可受伤模板均累计两次伤害 |
| 血条及脚本显隐 | 未绘制 | 3 条可见，关闭变量 15 后对应血条隐藏 |

固定步下的 21344ms 是缩放后每步取整的检查结果，不是另设的技能时长。塔动画及血条最终截图见 [修复截图](../out/combat-feedback-spire-healthbar.png)。

## 命令与结果

以下研究命令均使用 `bin/x64/Release/gun_bros_research.exe`，均退出 0。

| 命令参数 | 范围 | 日志 |
|---|---|---|
| `--combat-feedback-check --mute` | 塔层级/计时、结算继续移动、真实穿透弹、双人伤害、血条 | `out/combat-feedback-verified.log` |
| `--profile-play-check --mute` | 鼠标和 2 键双向切枪、原动画事件、换枪后真实开火、Q/E/1、F/R 无动作 | `out/combat-feedback-controls-final.log` |
| `--original-hud-check --mute` | 键盘按下与鼠标按下帧缓冲一致，松开恢复 | `out/combat-feedback-hud.log` |
| `--survival-check --map pack9 0 --weapon 65 --check-waves 2 --mute` | 两组机关可达、两波调度 | `out/combat-feedback-pack9.log` |
| `--native-profile-play-check --mute` | 四个正式星球各两波、Horde 两波，原装备及进度保存重载 | `out/combat-feedback-native-play.log` |

Release 构建命令：`MSBuild.exe gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /m /v:minimal`，退出 0，日志 `out/combat-feedback-release.log`。正式及研究 EXE 均已更新。

原存档与正式账户共 60 个受保护文件，前后 SHA-256 无变化，见 `out/combat-feedback-protected-result.json`。失败依据保留在 `out/combat-feedback-before.log`、`out/combat-feedback-piercing-before.log`。

## 验证边界

本轮修复了可复现的无伤害路径，不能据此断言用户遇到的所有偶发无伤害均为同一原因。原脚本的待机、护盾等状态继续遵守原行为。跨星球回归为各两波，不代表已经完成全部 50 波 × 10 轮验收；前轮记录中的持续卡顿问题也不因本轮功能回归而标为全部解决。
