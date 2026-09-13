# BRO-OPS 战斗闭环

## 方案与验收

本阶段按用户授权接通本地连接下的战斗挑战。先核对原模板与消费者，再实现计数、存档与 HUD；不改变挑战模板和奖品数据。

- 每日周期：`InitProgressData` 242162，网络时间 `(seconds + 36000) / 86400`，仅向前刷新，UTC 14:00 / 北京时间 22:00；本地连接由宿主时钟提供时间。
- 战斗计数：`OnWaveCleared` 76192、`UpdateFromLevelSession` 241437、`UpdateChallengeStatusData` 241118。逐波累计，单局条件在结束时清除未完成部分，完成状态不回退。
- 奖励：`IsRewardTierAvailable` 240047、`HandleChallengeCompletion` 240911、`AwardAvailableRewards` 241062；本人完成且好友人数满足才发对应 PRIZE，1017 奖励档位防重复。
- 界面：`PeripheralHUD::Bind/Draw` 88785/89341、`ChallengeInfoOverlay` 297137、`SetUpCommonInterstitialOverlays` 87677。使用 BIG 的 HUD_PAUSE、BRO_OPS_OVERLAY_SCROLL、BRO_OPS_OVERLAY_BOX、BRO_OPS_OVERLAY_INTERSITIAL，保持原区域回调与章节时长。
- 存档：1017 v5 的本地计数、周期和奖励状态；保持其他记录和未变更好友数据。

任务：实现上述链路；验证刷新边界、重复提交、目标筛选、结束重置、奖励重载，以及按钮按住/松开、逐波覆盖层和相关战斗回归。截图仅作结果验证，不作为坐标或文本的数据源。

## 实现结果

- `CChallengeProgress.cpp` 消费真实 CHALLENGE / PRIZE，更新并保存 1017 v5；不改写模板、任务文本或奖品。
- `CombatScene` 与 `WeaponEffects` 传递敌人、弹体、统计组及原枪械熟练度暴击标志；`PowerupScene` 记录实际消耗。`SurvivalSession` 在逐波结算和局结束时提交，波段条件使用全局波次。
- `SurvivalChallenges.cpp` 用原 Movie 的区域回调绘制列表。HUD_PAUSE 的章节 5 结束位置给出右侧触摸区域；core Sprite 0 的动画 170 绘制旗帜。按住显示、松开收起，逐波 UPDATE 使用原资源的 4500 毫秒时长。
- `MenuFlow.cpp` 在返回菜单后处理可领取奖励，一次处理一个挑战，展示原完成提示。依据为 `CMenuAction` 110 → `AwardAvailableRewards`；没有新增战斗中的定时发奖规则。道具库存已满或原商店等级门槛仍按已有原版发奖分支处理。
- `chc` / `IsConnected=1` 可在菜单或战斗中开启此链路；初始化之后的单局统计持续保留。宿主时钟代替网络时钟，跨日于菜单或下一次进入战斗时应用，不在一局中替换任务。

## 验证

- `pwsh -File tests/run.ps1 -Case offline-social,original-hud,native-profile-play,powerup-play,progress` 中 `progress`、`powerup-play`、`native-profile-play` 均通过；最后一项覆盖四个生存星球及 BOKOR，实际杀敌、过波、保存和重载。记录：`tests/bro-ops-regression.log`。
- 上述运行中的领奖断言最初错误地要求货币增长，实际选中的原奖品是道具。改为验证原奖品入库及奖励档位后，`pwsh -File tests/run.ps1 -Case offline-social,original-hud` 2/2 通过，退出 0；记录：`tests/bro-ops-ui-regression.log`。
- 专项覆盖刷新秒边界、时钟回拨、AI 击杀排除、关卡筛选、未完成单局目标归零、已完成不回退、完美波位图不重复累计、跨轮波段筛选、原道具奖励、保存重载防重复、长按与逐波列表、断开后隐藏旗帜。
- 两次验证的 `protected-changes=0`。最终截图：`tests/out/OriginalUI/offline-social/bro-ops/held.png`、`wave-update.png`、`reward-prompt.png`。
- `GunBrosRe.vcxproj` 的 Debug / Release 构建均退出 0；日志：`tests/bro-ops-debug-build.log`、`tests/bro-ops-release-build.log`。游戏位于 `bin/<Configuration>/GunBrosRe.exe`。

## 保留边界

真实好友进度、邀请响应、联机 GameType=2 对局没有模拟；额外好友奖励仅依据已有真实存档参与记录判断，不捏造参与人数。当前战斗统计覆盖玩家和 AI 兄弟来源；原 `CStatisticEnemy` 的第三类特殊环境来源及重复筛选引用导致的重复计数尚未逐项复现。原 `GetCircumstanceKills` 的低位组合存在不对称分支，已通过 ARM 0x178D8C / 0x178FEC 核对并保留，不能简化为统一的 AND 条件。

## 手动更新：chupdate

按用户要求新增 `chupdate`，菜单和战斗共用 `GameCheats::AdvanceChallenges`：先按当前时间初始化，再将存档挑战日前进一天，仍调用原周期重置和 BIG 模板选择算法。战斗中重新绑定 HUD 与计数管理器，丢弃旧组尚未提交的击杀和强化使用记录。每次指令保存，不改签到、系统时钟或连接开关；重启不回退。原版只允许周期向前，因此自然刷新需等真实挑战日超过手动推进后的存档日。

时间来源核对：`CNGSSession::handleResponseNetworkTime` 282821 从服务器响应读取毫秒时间，`tick` 282773 随客户端帧推进，`getNetworkCurrentTimeSeconds` 282796 转为秒；`CChallengeManager::InitProgressData` 242162 要求网络时间有效，再计算周期并本地生成列表。当前 fake connection 以本机 UTC 时间戳替代，不依赖操作系统选择的时区，也没有连接真实时间服务器。

验证：`pwsh -File tests/run.ps1 -Case debug-input,progress,offline-social` 3/3 通过，退出 0，`protected-changes=0`；记录 `tests/chupdate-tests.log`。覆盖完整键盘输入、连续两次菜单刷新、战斗指令、原列表生成结果、旧计数及领奖状态清空、保存重载、签到字段保持不变。
