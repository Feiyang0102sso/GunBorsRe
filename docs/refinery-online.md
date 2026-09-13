# 精炼厂联网选项

## 方案与依据

用户反馈 fake connection 开启后高级精炼仍显示 OFFLINE。原因是 `RefineryMenu.cpp` 的启用条件仅允许零分钟档位，未读取宿主连接状态；锁定槽也缺少原购买按钮。按原版接通联网条件、解锁、剩余时间和领取，保留原 BIG 配置及存档格式。

- `entries/refinement_entry.bt` / `CRefinementManager::Template::Init` 177773：12 槽的分钟、效率、金币/Warbucks 价格和门控引用，数据从 BIG 加载。
- `CResourceMeter::Enabled` 173669：连接有效或持续时间为零即可启用。原计时来自 NGS 网络时间；本地服务使用现有宿主 UTC 时钟。
- `CreateContentMovie` 149176、`CreateContentSprite` 的 provider158 分支、`CreateContentString` 的 provider158 分支：购买按钮为 `GLU_MOVIE_BUTTON_SMALL`、core Sprite0 动画64、`IDS_SHOP_BUY`。
- `MeterCallback` 173873：价格单独使用 font0，放在购买按钮下方；按钮使用原 font5。按钮坐标与尺寸读取 Movie 区域。
- `GetElementAction` 153522 → action74 → `UnlockSlot` 178294：扣原币种价格并将锁定状态改为待用；持久保存。`CollectResources` 178237 将已领取槽恢复待用，故无需每次付解锁费。
- `BeginRefinement` 178519：投入 Xplodium，按分钟和效率启动；门控关联槽在前一槽开始使用时开放。原侧栏文案写作领取后开放，实际消费者在开始精炼时处理，不改写原文案。
- `GetRemainingTimeString` 177971 / `CResourceMeter::Update` 173928：计时与进度、完成后的领取状态。

## 原配置

| PREMIUM 时长 | 产出比例 | 一次性 Warbucks 解锁费 |
| --- | --- | --- |
| 5 分钟 | 120% | 5 |
| 15 分钟 | 150% | 10 |
| 2 小时 | 200% | 25 |
| 8 小时 | 300% | 60 |
| 24 小时 | 500% | 130 |

两类都有即时 100% 档。STANDARD 的其余档通过门控逐级开放：10 分钟 115%、30 分钟 130%、24 小时 150%、48 小时 200%、72 小时 400%。以上只记录解析结果，运行时仍读取 BIG，不使用文档作为资源表。

## 实现与验收

- `RefineryMenu.cpp` 读取 fake connection 状态，实时更新槽状态；恢复 BUY、币种价格、余额不足提示和现有补充货币流程。显示倒计时与比例进度，沿用 375ms 资源转移及存档消费者。
- `RefineryOnlineChecks.cpp` 使用真实 Movie 触摸区域和独立原格式存档，验证不足额不扣款、购买只扣一次、解锁重载、投入、半程存档、断开/重连、结束秒边界、金币领取一次，以及 STANDARD 完整门控链。
- `pwsh -File tests/run.ps1 -Case refinery-menu,progress`：最终 2/2 通过，退出0，`protected-changes=0`。其中精炼厂检查也保留离线即时槽、粒子、原触摸区域和返回商店回归。日志 `tests/refinery-online-final-tests.log`。
- 初轮检查失败来自新增截图目录未创建，补齐测试目录后通过；后续目视核对修正了价格与 BUY 文案的原版分层，再次通过。
- 实际案例：5 Warbucks 解锁，1000 Xplodium 投入，5 分钟后领取1200金币；存档重载保持解锁和截止时间，无法再次领取。
- 截图：`tests/out/OriginalUI/refinery-menu/refinery-online/refining.png`、`insufficient.png`，已目视核对。
- Debug / Release 的 `GunBrosRe.vcxproj` 构建均退出0，输出到 `bin/<Configuration>/GunBrosRe.exe`；日志 `tests/refinery-online-debug-build.log`、`tests/refinery-online-release-build.log`。

真实 NGS 同步及好友精炼收益加成未模拟；本地使用原基础效率，不添加虚构好友加成。

## 精炼作弊码与解锁动画（2026-09-13）

本阶段按用户授权补三个指令，并修复解锁时直接撤去眼睛图层的问题。任务分为共用指令处理、原动画状态恢复、菜单/战斗与原格式存档回归。

- `chplo`：增加 500 Xplodium。
- `stref`：所有正在精炼的槽减少 24 小时剩余时间；到期进入可领取状态，48/72 小时的任务仍保留剩余时间。保持原开始时间、总时长和产出倍率，不改变系统时间、签到或挑战周期。
- `stlockref`：现按用户追加要求涵盖 PREMIUM 和 STANDARD 的十个非即时槽，读取 BIG 的时长区分，保留两类各自的首个即时槽。有未解锁槽时全部解锁，全部已解锁时全部锁定。锁定取消槽内任务、退回原投入矿石，不发放产出、不退解锁费。对应槽位未提交的转移动画同时取消。普通槽解锁后仍沿用原门控逻辑；原先仅限五个付费槽的范围已被本轮要求取代。
- 三个指令在菜单和战斗均生效，沿用原存档记录保存。属于宿主调试行为，不声称为原版作弊码。

动画依据 `sprite_archetype.bt`、`ui_movie.bt` 及 `CResourceMeter::Update` 173928–174024、`Draw` 173788：锁定时 core archetype4 动画24+槽号%6停在首帧；解锁后使用 `CSpritePlayer` 推进，舱门 Movie 切换章节1并单次播放。眼睛播放完成才显示主按钮。购买、作弊解锁与 STANDARD 门控开放共用状态转换；重新锁定回到闭眼首帧，读入已解锁存档不重播。所有帧和时长来自 BIG。

验收覆盖：键盘识别、两入口加矿及持久化、短/长精炼推进24小时、锁定退矿不发金币、购买后动画期间不能投入、播放结束后能投入、作弊批量睁眼与闭眼复位。

最终结果：`pwsh -File tests/run.ps1 -Case debug-input,progress,refinery-menu` 3/3 通过，退出0，`protected-changes=0`；日志 `tests/refinery-cheats-final-tests.log`。补充验证菜单锁定会取消付费槽的未提交转移，而解锁不会触发取消。测试按16ms推进时，购买睁眼播放44帧后恢复点击；已目视核对 `refinery-online/opening-start.png`、`opening-middle.png`、`opening-finished.png`。Debug/Release 游戏构建退出0，日志分别为 `tests/refinery-cheats-debug-build.log` 和 `tests/refinery-cheats-release-build.log`。

## 精炼中点击反馈与普通槽作弊范围

用户补充：未完成精炼时点击外圈应有向外膨胀动画，`stlockref` 扩展到普通槽（首槽除外）。方案为恢复精炼中按钮的原点击章节，继续让精炼状态决定能否领取；锁定范围读取原 BIG 时长，不另建槽位表。任务分为实际点击复现、原资源/消费代码核对、动画及范围修改、视觉/存档回归。

- 复现命令 `pwsh -File tests/run.ps1 -Case refinery-menu`：退出1，`refining click pulse missing`。固定精炼秒数和界面毫秒，分别绘制点击和未点击的菜单，像素完全相同；日志 `tests/refinery-pulse-repro.log`。
- 原按钮为 core Movie32 `GLU_MOVIE_BUCKET_BUTTON`，BIG偏移 `0x2a4866`，物理文件115，957字节。按 `ui_movie.bt` 与 `CMovie::InitResource` 读取，章节边界为0/400/1200/1700/2300，总长3000ms。点击章节1包含外圈由1倍到约1.1倍的缩放及另一圈的扩张淡出，数据来自原字节，不写入运行时代码。
- `CMenuMovieButton::Select` 144661、构造函数145004：默认模式3，点击播放章节1一次；`Update` 144715结束后恢复待机。`CResourceMeter::Update` 173928以2倍速度推进主按钮。当前问题是重建始终固定待机章节2，没有记录点击播放位置，原资源绑定正确。
- 初次修复仅为精炼中点击保留独立播放位置，这个范围不完整，现已由下述“所有激活槽位”修正取代。
- 验收要求：同一时刻点击与未点击画面有差异；播放位置与原章节及2倍速度一致；动画完成可再次触发，离线取消；两类非即时槽均能锁定/解锁并退矿，两个即时首槽保留可用，存档重载保持结果。
- 验收结果：`pwsh -File tests/run.ps1 -Case progress,refinery-menu` 2/2通过，退出0，`protected-changes=0`，日志 `tests/refinery-pulse-tests.log`。原失败用例现通过；已目视对照 `refinery-online/refining-click.png` 和 `refining-untouched.png`，点击后蓝色外圈向外扩张，倒计时和产出相同。
- Debug/Release 游戏构建均退出0，输出至 `bin/<Configuration>/GunBrosRe.exe`；日志 `tests/refinery-pulse-debug-build.log`、`tests/refinery-pulse-release-build.log`。

## 所有激活槽位的点击流程纠正

用户指出空槽也应播放点击动画。原因是上一轮人为增加 `record.state == 2` 条件，只覆盖了截图中的精炼状态；不是 BIG 缺少定义。阶段方案：动画触发只判断按钮是否可交互，动画播放完成再分派槽位操作；分任务覆盖空槽复现、通用触发和操作时序、全部槽位及商店返回验收。

- BIG 的 `GLU_MOVIE_BUCKET_BUTTON` 定义图层、缩放关键帧、章节和时长；原程序 `CMenuMovieButton::Select` 144661决定点击播放章节1，`Update` 144715在播放结束后执行action73。这是资源与代码的职责分工，不是要求重写资源动画。
- `CMenuGameResources::SetupTransfer` 174423、`CResourceMeter::Selected` 173743再检查状态和矿石：有矿待投入或可领取时执行转移，空槽无矿或精炼中不改变资源。故任何已启用按钮均先播放动画，不应按矿石或精炼状态筛掉动画。
- `RefineryMenu.cpp` 移除状态2限制，`StartRefineryTransfer` 在点击章节完成后执行原有375ms转移准备；锁定/离线取消点击，重复点击不重启动画。动画数据继续读取BIG，不添加手写缩放或时长。
- 空槽复现日志 `tests/refinery-all-clicks-repro.log`：`refinery-menu`退出1，`empty active chamber did not animate`。新增回归验证所有12个已解锁槽位在无矿时动画可播放，有矿待用、精炼中和可领取状态均可播放；投入和领取必须等待动画完成，再经过原转移时长，矿石/金币不能提前改变。
- 最终 `pwsh -File tests/run.ps1 -Case refinery-menu` 通过、退出0、`protected-changes=0`，日志 `tests/refinery-all-clicks-tests.log`；包含全部12槽空载点击、四种资源状态、原触摸区域迁移、转移中存档、单次领奖和四类商店返回检查。截图 `refinery-online/empty-click.png` 验证零矿石时仍有外圈膨胀。Debug/Release 构建均退出0，日志 `tests/refinery-all-clicks-debug-build.log`、`tests/refinery-all-clicks-release-build.log`。
