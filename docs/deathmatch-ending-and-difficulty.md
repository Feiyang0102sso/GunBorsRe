# DM 最后击杀动画与 Bot 难度

2026-09-14。按用户反馈修复结算抢跑，并增加仅对 Deathmatch 生效的 Easy / Normal / Hard 三档 bot。用户确认 Hard 道具无限库存、无需购买；随后要求增加仅标准手雷与血包无限供应的 Normal。原版 PvP 禁用规则、冷却和使用条件仍保留。

## 恢复断网入口限制

用户最新要求覆盖此前本地 DM 离线可玩的规则。与 Live 共用现有 `IsConnected` 状态：断网点击 DM 显示原 `MDS_PROMPT_MP_UNAVAILABLE` 第2项，保持当前模式；匹配过程中断网取消匹配、返回 Solo 并显示第0项；断网不能请求或完成再战。恢复连接后仍匹配本地 bot。

依据：`CMenuMovieMultiplayerOverlay::SetSelection`（反编译250261行）对 Live 与 DM 均查询 provider 82，不可用时弹出表189第2项并提前返回。弹窗继续由原 BIG `GLU_MOVIE_POPUP`、菜单条目和字符串绑定，已核对 `ui_movie.bt`；没有新增提示文案或布局数据。此前 Windows 适配在模式选择、匹配服务、连接刷新及再战处分别放行了 DM，本次统一移除这些豁免。

任务与验收：复现断网点击 → 恢复共用检查 → 验证两种模式警告、DM 断线取消、在线匹配及再战 → 构建 Debug / Release。

- 修复前 `pwsh -File tests/run.ps1 -Case offline-social` 退出1：Live 原弹窗正常，DM 断网直接改变模式；日志 `obj/dm-offline-red.log`、`obj/dm-offline-red-stdout.log`。
- 修复后 `pwsh -File tests/run.ps1 -NoBuild -Case offline-social -TimeoutSeconds 180`：1/1通过、退出0；两种入口原弹窗、直接匹配拒绝、匹配断线取消均通过，`protected-changes=0`，日志 `obj/dm-offline-prompts-validation.log`。DM 弹窗截图在 `tests/out/OriginalUI/offline-social/local-online/315228156/offline-mode-2.png`。
- 组合回归中 `local-live` 通过，DM 菜单匹配、HUD、结果、在线再战与断网禁止再战检查通过。`deathmatch-feedback` 随后在旧800ms时长断言失败：工作区现有 `MatchEndingHoldMs=100`，实测动画完成后96ms淡出。本轮未改动该时长或放宽断言；日志 `obj/dm-offline-validation.log`、`obj/dm-offline-feedback-stdout.log`，不能将此组合标为全通过。
- Debug / Release 游戏构建均退出0，日志 `obj/dm-offline-Debug-build.log`、`obj/dm-offline-Release-build.log`，两个 EXE 已更新。`git -c core.safecrlf=false diff --check` 通过。

## 方案与验收

任务顺序：真实最后击杀回归 → 核对 PLAYER / CBrother / CLevel / Movie → 修复死亡展示与结算出口 → 接入 cfg 和难度策略 → DM、Live、道具回归 → Debug / Release 构建。

- 最后一次击杀锁定比分，停止新输入、攻击和复活；继续 PLAYER 死亡脚本及已产生的粒子。两边同时死亡也分别等待。
- 死亡完成且爆裂粒子耗尽后停留 800ms，再播放原 `GLU_MOVIE_MISSION_END`。此前人为加入的3000ms停留按用户最新反馈缩短到约0.8秒；该停留属于 Windows 展示策略，并非已核实的 iOS 原版常量。
- EXE 旁 `GunBrosRe.cfg`：`DMBotLevel=1` 为 Easy（默认），`2` 为 Normal，`3` 为 Hard。旧值 `2` 现对应 Normal，继续使用 Hard 须改成 `3`。旧文件缺字段保持 Easy；非法值报错。重开保留本局难度，重启程序读取修改后的 cfg。
- Easy 保留原来每命两次商店、两次标准手雷、合计两次血包及实际库存。Hard 可使用所有已实现且 STORE 允许 PvP 的道具，不限库存和每命次数，不请求或进入商店。500ms 是 bot 选择道具的输入频率，具体效果与冷却仍由 BIG / Flow 决定。
- Normal 沿用 Easy 的标准手雷与三种血包白名单，但不限制每命次数、不消耗库存、不进商店；不使用其他手雷、护盾、炮塔等主动道具。仅移除宿主预算，效果、冷却与使用条件仍执行原 BIG / Flow。道具决策沿用500ms频率，手雷保留原 Easy 的交战状态与距离条件。
- Normal / Hard 使用临时虚拟可用数量，不向 bot 存档赠送道具，也不消耗现有库存。玩家、Solo 和 Live 不获得这项供应规则。Live bot 策略不读取 `DMBotLevel`。

## 证据与根因

原程序：`_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`。已核对 `entries/player_template.bt`、`entries/powerup_template.bt`、`entries/store_entry.bt` 和 `ui_movie.bt`。

- PLAYER `pack0_core_xga_0037_0x6726.bin` 的 export 2：`0x592` 检查 DM；`0x59A` 发出爆裂粒子；`0x5A6` 隐藏身体；状态7等待动作完成，状态8在 `0x2F3` 调 native1。不能把隐藏身体当作动画完成。
- `CBrother::HandleDamage` 约136779行启动死亡出口；原 `CLevel::OnPlayerKilled` 约118637–118690行处理击杀及结果；`CGame::SetMissionWrapUp` 75619行装载原结束 Movie。本地实现按同帧比分锁定后另行等待展示结束。
- 修复前 `SurvivalSession::Update` 结果分支立即返回，仅更新淡出；宿主 `effects.SetPaused` 也按比赛结束暂停，因此死亡状态和粒子无法继续推进。旧测试仅投降，遗漏致命伤害路径。
- `WeaponEffects` 将爆裂来源与位置锚点分开记录；发射器结束后仍等待其已经发出的粒子，不用截图估算粒子寿命。新结束分支不推进伤害判定或新波次。
- STORE 的模式位由 `CStoreItem::IsItemExcludedFromGameType` 156283行消费；道具使用仍经过 `CPowerup` 查询出口及 `CBrother::UsePowerup`。Hard 的无限供应是明确的宿主难度规则，不复制资源道具表。

## 验证记录

- `pwsh -File tests/run.ps1 -Case deathmatch-feedback -TimeoutSeconds 90`：修复前退出1，`results cut off death animation complete=0 visible=0`，总淡出1520ms。日志 `obj/dm-final-kill-red.log`、`obj/dm-final-kill-red-stdout.log`。
- 首次修复后同专项退出0，最后击杀至结果6688ms，角色动画完成且粒子结束；日志 `obj/dm-final-kill-green.log`。
- `pwsh -File tests/run.ps1 -Case deathmatch-data,deathmatch-feedback,debug-input -TimeoutSeconds 120`：3/3通过、退出0，见 `obj/dm-difficulty-check.log`。
- 最终 `pwsh -File tests/run.ps1 -Case deathmatch-feedback,deathmatch,local-live,powerup-play,powerup-selector,player-death -TimeoutSeconds 600`：6/6通过、退出0，见 `obj/dm-final-regression.log` 与 `tests/out/results.json`。累计8个不同专项通过，均显式静音，`known-data-issues=0`、`protected-changes=0`；未重复全量截图基线。
- 五图反馈专项均通过：9种可用 PvP 道具、8种被模式规则排除的已实现道具；Hard 通过真实 bot 决策连续回血5次，原动画真实投掷3次，并使用 Easy 范围外道具。原冷却、失败请求、零账户库存、禁止商店、玩家死亡、双方同时死亡、投降及重开均通过。该检查验证行为边界，不宣称测量了真人对局胜率。
- 此前3秒停留方案的最后击杀样本中，死亡和粒子约2192–2224ms结束，约5184–5216ms开始淡出，6688–6720ms进入结果页；16ms步进存在一帧内的边界误差。`tests/out/Core/deathmatch-feedback/deathmatch-final-burst.png` 已人工查看，原爆裂碎片和粒子正常显示，未被淡出遮住。
- Debug / Release 游戏均构建成功、退出0：`GunBrosRe.vcxproj /p:Configuration=<Debug或Release> /p:Platform=x64 /p:SkipAutoTests=true /m /v:minimal /nologo`。日志为 `obj/dm-final-Debug-build.log`、`obj/dm-final-Release-build.log`；输出在 `bin/Debug/GunBrosRe.exe`、`bin/Release/GunBrosRe.exe`。
- 两个现有 `GunBrosRe.cfg` 均补入 `DMBotLevel=1`，保留其他设置。`git -c core.safecrlf=false diff --check` 通过。

## Normal 追加验收

用户反馈 Hard 过强后新增 Normal，配置映射调整为1/2/3。保留当前 cfg 的数值和其他设置：Debug 当前为2，更新后对应 Normal；Release 当前为1，继续使用 Easy。

- `CMPMatch` 分开控制无限次数/库存和道具种类：Normal、Hard 均禁止 bot 购物并解除每命预算，只有 Hard 绕过 Easy 的标准手雷/血包白名单。`DeathmatchBot` 的 Normal 分支只调用标准手雷与血包入口。
- `pwsh -File tests/run.ps1 -Case deathmatch-data,deathmatch-feedback,debug-input,local-live -TimeoutSeconds 180`：4/4通过、退出0，见 `obj/dm-normal-validation.log`。五图均验证 Normal 开放4项（标准手雷及三规格血包）、拒绝其余13项已实现道具；实际回血5次、投掷3次、原冷却、不扣账户库存、不进商店、Hard范围与Live隔离均通过。`known-data-issues=0`、`protected-changes=0`。
- Debug / Release 游戏构建均退出0，命令选项同上，日志为 `obj/dm-normal-Debug-build.log` 和 `obj/dm-normal-Release-build.log`。`git -c core.safecrlf=false diff --check` 通过。

## 停留缩短至0.8秒

用户反馈3秒人为停留过长，改为死亡动画及爆裂粒子完成后等待800ms，再开始原淡出。只调整 `SurvivalSession::MatchEndingHoldMs`；回归同时检查等待时长上下界784–816ms，防止再次拖长或提前截断动画。

- `pwsh -File tests/run.ps1 -Case deathmatch-feedback -TimeoutSeconds 180`：五图专项通过、退出0，日志 `obj/dm-hold-800-validation.log`。各图完成动画后784ms开始淡出（16ms步进误差），最后击杀至结果4384–4512ms，比原3秒方案缩短约2.2秒。动画完整性、双方死亡、重开及既有难度检查均通过，`protected-changes=0`。
- Debug / Release 构建均退出0，日志 `obj/dm-hold-800-Debug-build.log`、`obj/dm-hold-800-Release-build.log`，两个游戏EXE均已更新。`git -c core.safecrlf=false diff --check` 通过。
