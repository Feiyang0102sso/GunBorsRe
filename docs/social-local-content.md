# BROS / BRO-OPS 本地内容恢复

用户指出：`_prep/UI_sample/ui.md` 和官方 `10-17-2011` 测试版可显示兄弟与任务，因此不能把整个页面空白归因于缺少服务器。

## 调研结论与方案

此前仅开放联网菜单框架，未实现本地内容绑定；“挑战实例全部来自服务端”的判断错误。此次按原资源加载和菜单消费流程恢复内容，保持进程内模拟连接的范围。

| 内容 | 原版来源与消费函数 | 实现 |
| --- | --- | --- |
| 默认兄弟 | `CFriendDataManager::InitDefaultBrother` 200797；`IDS_FRIEND_DEFAULT_BRO1/2`、`IDB_AVATAR_DEFAULT1/2`；`CGameFlow::Reset` 77330 选择另一位兄弟 | BIG 名称、头像、原默认装备和模型，不加入远端好友数量 |
| 默认等级 | `CFriendData` 内含 `CPlayerProgress`，构造函数 194437 初始化等级 1 | 与玩家等级分开显示 |
| Bro Buffs | `CGunBros::Init` 80477–80488 的 10 次 `AddPercentModifier`；`CFriendPowerManager::Init` 234195 绑定 BIG 字符串 | 原生规则、条件列表、零好友汇总、原图标与滚动 |
| 挑战定义 | `entries/challenge_template.bt`；`CChallengeManager::Template::Init` 239005 | 逐包读取 Section27，共 238 条；完整读取目标条件、引用并检查文件边界 |
| 轮换列表 | `GenerateChallengeList` 239247；`CRandGen::Seed/Generate/GetRandRange` 370208/370252/370346 | MT19937，以总模板数为种子，原包/条目交换顺序，按类别和周期选取 |
| 奖励 | `entries/prize_entry.bt`；`CPrize::Init` 204736；`CreateRewardQuantityString` 240630、`CreateRewardTierStatusString` 240515 | BIG 奖品图片及数量；使用挑战状态文本，避免误用好友通知格式字符串 |
| 周期与进度 | DataStore1017 v5；`InitProgressData` 242162、`GetProgressString` 240190 | 保留原存档周期、进度和计数；新周期才使用宿主 UTC 加 10 小时除以 86400 |
| UI | `ui_movie.bt`；`CMenuFriends::Init/Bind/Draw`、`CMenuChallenges::Init` 237003 | 原主 Movie、卡片、列表、侧栏、进度条和文字区域回调 |

源码行号均指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`。菜单引用另核对了 ARMv7 `MENU_FRIENDS` 0x402d70、`MENU_CHALLENGES` 0x402eb0。运行只依赖 `big/`；PC 版截图与 `challenges.txt` 是研究参照，没有成为运行时数据源。

## 任务及验收

- [x] 以真实绘制条目数建立红灯：原框架 page4 内容为 0，`offline-social` 退出 1。
- [x] 实现挑战模板解析和本地轮换、读取存档周期及计数。
- [x] 恢复默认兄弟卡片、模型、Bro Buffs 全部 10 条，以及挑战列表、目标、奖励和详情选择。
- [x] 使用原 Movie 的触摸区域、滚动章节与侧栏章节；修正将列表区域 0 当作条目、将连续进度条当作有章节 Movie 的错误。
- [x] 专项测试覆盖模板截断拒绝、不同周期选择、已有周期在其他宿主日期下仍匹配、任务点击、10 项 Buff 滚动与查看不写存档。
- [x] 最终受影响回归与 Debug / Release 构建。

## 本阶段边界

后续战斗阶段已接通计数、跨日更新、HUD 与本地领奖，见 [BRO-OPS 战斗闭环](bro-ops-combat.md)。下段保留当时仅恢复菜单内容的阶段范围。

默认兄弟和任务定义可在本地完整读取。远端好友列表、Recruit/Requests、好友使用奖励记录仍没有模拟数据。此阶段恢复显示与浏览，不实现新的挑战战斗计数提交、跨天存档更新、挑战领奖或真人同步；已有周期按存档展示，新账户按宿主时钟选择本地任务。内购仍沿用第一阶段的本地验证回调。

初始专项记录：`tests/social-content-red.log` 退出 1；`tests/social-content-test.log` 已通过，退出 0。实际截图位于 `tests/out/OriginalUI/offline-social/local-online/`。

最终验证：

- `pwsh -File tests/run.ps1 -Case offline-social,bank,planet-menu,progress`：4/4 通过，退出 0，`protected-changes=0`；记录 `tests/social-content-final-tests.log`。
- Debug 与 Release 的 `GunBrosRe.vcxproj` 构建均退出 0，输出到 `bin/<Configuration>/GunBrosRe.exe`；记录 `tests/social-content-debug-build.log`、`tests/social-content-release-build.log`。
- 最后按 `ChallengeTitleCallback` 修正标题为 font0、箭头按原 Sprite 尺寸居中，`pwsh -File tests/run.ps1 -Case offline-social` 再次通过，退出 0，`protected-changes=0`；记录 `tests/social-content-final-ui.log`。GUI EXE 不提供 `--test-output`，不能代替独立 `GunBrosTests` 入口。
