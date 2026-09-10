# 玩家死亡演出与 stsuicide（2026-09-10）

## 阶段方案与验收

用户已授权修复死亡直接结算、核对慢动作并新增 `stsuicide`。先以真实伤害复现，再恢复 BIG 死亡出口、时间倍率和完成通知；不另设死亡秒数或按状态编号强制跳转。

- [x] 复现：`--survival-check --map pack9 0 --check-waves 1 --mute`，`out/death-before.log`，退出 1：`postgame opens on fatal hit before death animation`。正式离场条件与检查共用 `SurvivalSession::IsDeathComplete`。
- [x] 核对 PLAYER BT、共用 MoveSet/Flow 结构、字节码与 iOS 消费函数。
- [x] 恢复死亡完成通知与原关卡全局时间缩放，动画完成后才离场。
- [x] `stsuicide` 经 SDL 连续按键触发共用死亡出口，死亡期间重复输入无效。
- [x] 检查真实致死、作弊致死、慢动作、完成时机、重开、兄弟死亡与结算界面；构建并记录结果。

## 原版证据

- `entries/player_template.bt`、`entries/common.bt`、`flow_bytecode.bt`、`flow_native_sources.bt`；未修改读取器或原件。
- core 包 hash `0x58595522`，PLAYER 类型 15 / Section 16 / ordinal 0，样本 `big_360_out/pack0_core_xga/0xf4e02223/16_PLAYER/pack0_core_xga_0037_0x6726.bin`，清单 `out/binary-research/flow-disassembly/pack0_core_xga_0037_0x6726.flow.txt`。
- 原 bin `0x590` export 2 进入死亡状态；`0x268` 躯干 sequence 为 move 6，`0x26C` 腿部 move 7；`0x2A5` 调用 LEVEL native 5，参数 76；`0x2AB` 隐藏输入板；`0x2BA` 监听 sequence 完成再进入后续状态；`0x2F3` 调用 BROTHER native 1。帧范围、速度和模型来自同一 BIG MoveSet 及引用 Mesh。
- iOS `CBrother::HandleDamage` :136779 在血量归零时调用 export 2；`CBrother::FunctionResolver` :138856 的 native 1 调用 `CLevel::OnPlayerKilled` :118371，随后才进入失败流程。
- `CLevel::FunctionResolver` :117510 的 native 5 将参数除 256，保存全局对象时间倍率。`CLevel::UpdateNormal` :121317 将墙钟步长乘该倍率、截断并至少保留 1ms。它与 native 58 的选择性对象缩放是两个字段，不能混用。
- `CBrother::FunctionResolver` :138980 的 native 12 隐藏 HUD；native 16 查询死亡后道具选择器。当前重建尚未完整实现死亡后道具选择流程，本次不新增复活商品或虚构库存。

最初命令误选 `pack7 0`，该地图没有正式生存 LEVEL，未作为死亡复现依据；改用已有正式 `pack9 0` 后重现。

## 实现与结果

`CBrother::StartDeath` 共用原 export 2；普通伤害仍遵循无敌、护盾与防御，Windows 作弊入口直接调用死亡出口，故不受这些保护影响，也不重复计数。native 1 更新角色完成状态，`SurvivalSession::IsDeathComplete` 同时供正式退出条件和回归检查使用。装备后立即受伤也会先重新连接 LEVEL 上下文，避免死亡脚本丢失时间倍率调用。

`CLevel::TransformWorldElapseMS` 读取 native 5 写入的倍率，`SurvivalSession` 在死亡动画期间继续更新原关卡、实体、模型、相机、机关与掉落。原有 native 58 单独保留。正式游戏在完成通知后才记本次结算并退出场景；不允许死亡期间打开不可见暂停页或启动待处理换枪。重开清除死亡完成与 HUD 隐藏标志，并恢复关卡原初始化倍率；兄弟自己的完成通知不结束玩家游戏。

永久研究菜单 **80** / `--player-death-check --mute`，同时加入 `test-muted.ps1` Core。覆盖四张正式地图，每张分别验证真实伤害和 SDL 作弊码，且逐步检查模型时间、玩家不移动、不发射新弹、死亡只计一次、重开恢复、兄弟死亡与复活。测试中改变 native 5 参数，确认结束时机跟随动画时钟，没有另设固定等待秒数。

| 路径 | 原动画长度 | 每 16ms 推进 | 完成墙钟时间 | 结果 |
|---|---:|---:|---:|---|
| 原 BIG 致死演出 | 800ms | 4ms | 3216ms | 四图通过 |
| 作弊致死后测试倍率改为 128/256 | 800ms | 8ms | 1616ms | 四图通过 |

完成事件在帧终点后的解释器刷新发出，因此包含最后一次更新。3216ms 是原倍率、整数步长及动画完成事件共同计算的结果，不是新写入的时长。截图保留 `out/player-death-{pack2,pack7,pack9,pack12}-{start,middle,complete}.png`，已目视核对 pack9 倒地中途及结束姿态。

| 命令（研究 EXE，均带 `--mute`） | 退出码 | 日志 |
|---|---:|---|
| `--player-death-check` | 0 | `out/player-death-check.log` |
| `--survival-check --map pack9 0 --check-waves 2` | 0 | `out/death-survival-2waves.log` |
| `--profile-play-check` | 0 | `out/death-profile-play.log` |
| `--combat-feedback-check` | 0 | `out/death-combat-feedback.log` |
| `--postgame-presentation-check` | 0 | `out/death-postgame.log` |
| `--research` 输入 `80`（最终版本，含防串键） | 0 | `out/death-menu80-final.log` |
| `--boss-check`（含旧 stboss 输入） | 0 | `out/death-boss-regression.log` |

正式与研究 Release 构建：`MSBuild.exe gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /m /v:minimal`，退出 0，`out/death-final-build.log`。沙箱内 FileTracker 无权限，正常构建经工具审批通过，未修改工程跟踪选项。

`git diff --check` 退出 0；本轮回归前后核对 `saves/`、`userdata/` 共 110 个文件，SHA-256 全部保持不变，见 `out/death-protected-result.json`。用户原有 `.gitignore`、`CONTEXT.md`、`_IDA_OUT/refactor.md` 修改保留。

首次专项中 pack2 的总弹体计数断言误包含敌人继续开火，改为逐帧检查弹体 owner 后通过。最小复现的一波生存命令在修复后已通过死亡/重开检查，但整体仍因旧检查要求升到 2 级而退出 1（首波只有 10XP，升级需 15XP）；保持该旧断言，使用常规两波回归验证整体流程。结果见 `out/death-survival-after.log` 和两波日志，未把一波退出码写成通过。

## 使用与边界

在战斗中切英文输入，连续键入 **stsuicide**，无需回车；沿用相邻字母 2.5 秒超时规则。支持暂停时触发并恢复演出；空袭 Movie 正在接管场景时不触发。长按后缀不会提前完成作弊码或串到 E/C 等游戏按键。

验证范围是本次死亡流程与相关回归，不代表所有原版 UI 完整复刻。HUD 当前响应原隐藏通知，`CInputPad::Hide` 的完整分部退场动画和死亡后道具选择器仍未完整重建。正式结算由同一退出条件连接既有前端，本轮没有另做从原生存档开始、鼠标操作到结算的整段录像。

