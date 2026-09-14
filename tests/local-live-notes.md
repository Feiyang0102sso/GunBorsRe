# Live 本地合作试玩

> 以下为第一阶段历史记录。用户随后明确要求完整 Live 流程；当前实现、持久化与验证见 `live-mode-plan.md`。下文的“账户副本、不保存、会话内选择、LocalMatchBot”已被后续阶段取代，不是当前运行行为。

## 方案与验收（2026-09-13）

本轮按用户“先进入 Live 或 Death Match，加入独立机器人”的要求，先交付 Live。
fake connection 提供一个明确标为 LOCAL BOT 的本地队友，沿现有原版菜单进入选定星球。
机器人策略独立于原 CBrotherAI；共用角色、武器、碰撞、导航和 BIG 脚本。
试玩复制账户并禁止保存，不增加远端好友、好友加成或服务器奖励。

依据：CGameCenterManager::findMultiplayerMatch :260057 的两人请求；
CMenuFriends::BindFriendList :195969、CMenuFriendOption::Init :198675 的列表及区域；
ui_movie.bt、entries/player_template.bt、原 PLAYER bin 的 export 7 @0x5E1；
CPlayer::Update :100355..100436 的 125 距离救援、CLevel::SetRevivePercent :115322，
CBrother::OnRevive :135970。数值、动画、恢复血量及免疫时间仍由原 Flow 消费。
救援半径严格小于 125，累计速率 0.0001/ms（约 10 秒）；离开半径保留已累计进度。
原 PLAYER 初始化 @0x565 读取 CGame.variable[4]；因此必须在绑定角色前提供合作标记，
不能只在菜单上改模式名，否则死亡脚本仍走单人慢动作分支。
机器人移动／射击策略与匹配延时属于宿主模拟，不声称是原联网 AI。

任务顺序：本地匹配与好友列表 → 独立机器人与合作救援 → 接通菜单试玩 → 专项测试与构建。
验收覆盖：取消、断线、重复匹配；列表选择；实际 BIG 战斗、机器人移动射击；
玩家／机器人倒地互救、双倒地结束、重开与模式隔离；账户副本不写回。

Death Match 尚未接通：需另行复刻 InitDeathMatch :114880、RespawnPlayerForDeathMatch
:114940 和 UpdateMultiplayerStatistics :115164 及原对战 HUD、装备和任务选择。

## 已实现边界

- 原 BROS 页面保留默认兄弟，追加 LOCAL BOT 卡片，可点击查看；本地选择仅持续当前会话。
- Live 匹配成功后进入所选四个普通星球之一的生存地图，BOKOR 合作暂不接通。机器人用原默认装备，等级／生命按当前玩家等级计算。
- 模拟第二人采用独立 LocalMatchBot 策略，敌人在宿主选择最近存活兄弟；不模拟网络延迟、同步包或远端账号。
- 救援通过原 export 7 完成；双方倒地后回到选模式页，重新选择任务可再开一局。
- 本地试玩保持当前生存调度和 HUD，增加明确的试玩与救援提示；未声称原联网 HUD、匹配大厅及结算已完整复刻。
- BROBUFFS 和挑战好友数仍按真实本地记录，不因模拟队友增加。列表选择不改写原好友记录；选中 LOCAL BOT 后 Solo 使用此机器人策略，选回默认兄弟恢复原 CBrotherAI。Solo 仍按单人死亡规则结算。

## 验证结果

- Debug / Release 主程序构建均退出 0；Debug 自动 `progress` 退出 0。
- 第一轮 `local-live,offline-social,brother,player-death`：4/4 通过，受保护文件变化 0。
- 最终 `local-live,offline-social`：2/2 通过，受保护文件变化 0；包含列表真实点击、匹配取消／成功／断线、BOKOR 拒绝、合作两波、双方救援、双倒地、重开、选中机器人后的 Solo 死亡规则。
- BIG 实战截图与 BROS 卡片已目视检查。命令／日志在 `obj/local-live-*-build.log`、`obj/local-live-final-validation.log`，结果及截图保留在 `obj/local-live-evidence/`，最终专项原始输出在 `tests/out/`。
- 战斗验证使用固定步进及无敌，换波阶段用高伤害交付真实死亡回调；不是手动难度验收。曾发现测试直接推进场景漏交付死亡事件，改为通过 SurvivalSession 后两波通过，未修改关卡数据。
- 边界：普通星球合作已可试玩，正式网络服务、Death Match、BOKOR 合作、原完整多人 HUD／结算仍未恢复。Solo 死亡沿用既有 native 16 未实现日志，本轮未扩大到该原生函数的完整移植。
