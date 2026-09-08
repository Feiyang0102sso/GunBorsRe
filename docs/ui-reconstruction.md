# 原版 UI 重建：2026-09-08 上课期间

> 本轮已交付：默认游戏入口、原启动媒体、菜单／HUD、默认兄弟装备、本地下级流程和补充脚本行为。最终构建的 Core／UI／Boundary 回归完成；四个星球均有完整 500 波通过记录。不同阶段的构建及未完成边界见文末汇总，不能据此称为完全等价原版。

## 授权与验收目标

用户授权约四小时自主推进，产品和技术细节由代理决定，无需重复等待批准。主要恢复原版 UI、Glu 启动视频、原音乐、默认兄弟装备及联网按钮的本地下一级页面。EXE 默认进入游戏，全部研究入口永久保留。

## 调研事实

- 开始时工作区干净；已有 38 个研究入口及四星球生存闭环，当前 `GameFrontEnd` 为临时宿主菜单。
- `glu_logo/glu_logo_landscape.m4v` 与独立 WAV 均存在；MP3 为 `game_0` 和 `1`–`6`。
- iOS `CBGM::Play`（:60102）及 `NextTrack`（:60204）表明菜单曲为 0，战斗曲依序 1–6 循环，不能无依据改成每个星球固定一首。
- 兄弟初始化直接读取玩家当前枪及盔甲，导致装备串用；初始盔甲原依据为 `SetDefaultArmor`（:171753）：腿 2、胸 1、头 0。
- 原菜单基于 `CMovie` 自有时间轴（:109263），包含精灵、文字、嵌套电影、交互区域等；不是直接嵌入网页。优先解析原布局和图集，避免另画替代美术。
- 旧 PC 版复制到 `out/original-pc-study/` 观察，原目录和原存档保持只读。用户本轮补充：2.3–2.4 之间的泄露开发版，最后的 Jungle 未完成，启动和按钮报错属于已知参考限制。

## 阶段方案与任务清单

1. 启动及兄弟：默认游戏、显式 `--research`；独立初始兄弟装备，验证与玩家高等级装备隔离。
2. 原媒体：Windows 宿主直接解码 M4V/MP3；启动播放、跳过、菜单及战斗曲切换、静音验证，保留媒体研究入口。
3. UI 基础：解析原 CMovie、原精灵及字体，恢复标题、导航、星球选择、商店、装备、炼化与弹窗；新增布局研究入口及自动检查。
4. 下级流程：原联网按钮均可进入对应本地页面，逐项补齐实际页面与返回路径；依据剩余时间继续补特殊模式和未实现游戏行为。
5. 分阶段构建及回归，截图目视核对、真实鼠标导航、进入战斗／死亡／重开／保存检查；更新验收和未解项。

## 验证约定

常规运行与测试使用 `--mute`，仍完整解码资源。实验账户与截图写 `out/`，不覆盖正常账户。每阶段写明命令、退出码、覆盖范围及失败原因，不能把资源解析或构建通过等同于完整复刻。

## 已完成阶段（UTC 12:54–14:24）

- 默认游戏、显式 `--research`；M4V 实际 125 帧及独立 WAV 同步播放，可跳过；标题页读取原 PNG。Windows Media Foundation 解码，无 ffmpeg 运行依赖。七首 MP3 全量解码验证，战斗依原 CBGM 顺序循环。
- 兄弟独立默认头／胸／腿和手枪、ER97E Elite 步枪；不复制玩家高级装备。原 AI 每帧随机换枪请求经场景安全执行，装备重建、死亡复活回归通过。
- 175 个原 CMovie、13 组字体资源记录、148 个 core 时间轴画廊；采用原图集、锚点、层序、渐变填充。新增 Mach-O ARMv7 静态菜单表提取器，113 个 MDS 表及七项导航标签来自 iOS 数据。
- 正式菜单接入原美术与字体，包含星球／波次、装备／商店、12 槽精炼、兄弟选择、设置、原 14 页帮助、本地账户／排行榜／活动、关于、退出弹窗。联网入口允许进入相应本地页；真实在线服务没有伪装成已连接。
- Profile v3 保留旧版本读取，保存音效／音乐／兄弟选项、玩家兄弟外观、各星球累计击杀及一次性本地活动领奖位。八项离线活动是宿主适配规则，不宣称等于原 BRO-OPS 服务数据。
- 原战斗下沿、血量／经验区域、暂停图标、金属弹窗已绑定真实状态；鼠标操作共用键盘路径，界面点击不向世界开枪。重开后击杀结算基准显式归零，避免累计进度少记。

阶段验证：`--media-check`、`--movie-check`、`--movie-gallery`、`--game-menu-check`、`--brother-check`、`--original-profile-check`、`--hud-check` 均已有退出码 0 日志。主要日志为 `out/ui-stage2-media.log`、`out/ui-stage3-gallery.log`、`out/ui-stage4-menu-final.log`、`out/ui-stage5-brother-check.log`、`out/ui-stage5-original-profile-check.log`、`out/ui-stage5-hud-check.log`。HUD 的五种截图已目视；尚需实际鼠标穿过完整启动与战斗菜单验证。

## 下一阶段：空袭与剩余原脚本行为

原 `CPowerup::Update` (:188652) 在电影完成时触发事件 14:0；输入面板完成为 14:4，随后执行原范围伤害。空袭 0／10／11 分别使用 pack5 电影 1／3／5，不能在点击时立即全屏清怪。任务：保留使用期间脚本实例、按真实电影／选择器结束派发事件、屏幕粒子和声音、真实范围伤害与库存结算；验证重复点击、死亡／重开取消、暂停计时与三档空袭。Tantrum 旧字段的倍率消费路径仍需查证，不直接套用新三种 Frenzy 的效果。

三档空袭已按原脚本时间轴实现，首次伤害分别在 3312／8560／5312 ms，伤害 240／500／1600；范围外目标不受伤，重复点击不扣库存，重开取消余下效果。`out/ui-stage6-airstrike-check.log` 退出码 0。十三套字体均已实际解码，严格验证 175 个电影：`out/ui-stage7-movie-check.log` 退出码 0。

## 下一阶段：剩余 UI 流程与 BOKOR

- 修复嵌套页面返回栈和同一点击穿透到下一页的问题，使用真实按钮命中测试覆盖往返。
- pack11 的十个 type 2 Mission 共用 LEVEL 0，原名 Horde 1–10。先补 CLevel 秒表、对象时间倍率和兄弟标签调用，再验证原脚本实际出怪和跨 Horde 推进；通过后提供独立选择入口，不混入四星球的 50×10 进度。
- 本地账户单独保存 BOKOR 成绩，界面明确其模式；死亡／重开沿用已验证的战斗路径。
- 桌面自动操作因物理 Esc 已停止。本阶段仅使用独立 QA EXE 的程序内检查，不再发送鼠标键盘输入；实际人工游玩未验证部分仍在验收记录中保留。

## 货币入口补全方案

`CStoreAggregator::CurrencyPurchase` (:155009) 明确 type 14／15 是金币／War Bucks 包，type 16 使用 value32 区分兑换方向。补齐顶部余额入口、原目录商品、确认页和双向兑换。在线支付阶段采用用户授权的本地成功路径，确认页明确标注 LOCAL MODE／NO REAL PAYMENT；货币数量仍读取原包，不触发任何外部支付。验收覆盖不足余额、双向兑换、一次点击只确认一次和保存读取。

## 已完成补充阶段（UTC 14:24–16:02）

- 三档空袭按 `CLevel::FireSplashDamageForceAttribute`／`CPowerup` 的原规则修正：相机视野中心、半径 3000、flags 2、固定伤害不再乘盔甲攻击。旧 Tantrum 的 21 秒计时、电影与库存已接，遗留倍率仅按当前 iOS 读写证据保留。
- BOKOR 加入独立十档选择页与研究 45／46：原慢速、敌人上限、秒表、兄弟名字渐显、Horde 连杀计分、最好成绩。Profile v5 兼容读取 v1–v5；Horde 进度不污染四星球。
- 原蓝／绿按钮及 glow、原锁定／可玩／已完成波次精灵已接；新增大波次页 19，支持回玩旧轮次。商店可预览未买装备，实际账户与装备不变。按钮底板填充实际点击范围，修正长文字越出底板。
- HUD 使用原升级电影 7、普通过波 34、完美过波 87；队列按原时长播放，暂停冻结，重开清空。Boss 提示会清除待播通知，使用原词条与电影 34，完成回调解除脚本等待。
- 相机原 native 39、74–76 与玩家输入开关 77／78 已接；跟随／固定目标、余弦插值、边界、震动和死亡后持续更新有对应宿主。多人复活节点 80 保存配置；单人不伪造多人复活。
- 长跑暴露遗漏的 native 56／57：原 `SetXplodiumMultiplier` 使用整数百分比，`AddXplodium` 保留百分之一的余量。补全击杀和完美波奖励消费路径，避免后期奖励偏低。20 次 105% 的 1 点奖励得到 21 点，加到 200% 后再发 1 点得到总增量 23；精确断言通过。
- BOKOR 独立计分断言经三次真实敌人死亡通过：敌人经验 2，两次玩家击杀及一次兄弟击杀合计 18 points，玩家得到 4 XP，连杀为 2。
- 常规生存检查现在累加 CLevel／CEnemySpawner 的未实现调用，防止“过波成功”掩盖主线 native 空缺。19 关卡的纯流检查仍覆盖研究资源，战役未接接口继续在 CSV 与日志明确列出。
- 修复 v1 存档兼容测试夹具的错误截取位置；修复研究菜单新增 45／46 项被旧上限 44 拦截的问题。工程及 filters 已同步新源文件与研究工具。

阶段证据：`ui-stage15-powerup.log`、`ui-stage18-hud.log`、`ui-stage18-menu.log`、`ui-stage19-flow.log`、`ui-stage20-flow.log`、`ui-stage23-menu.log`、`ui-stage23-hud.log`，均位于 `out/`。最终完整回归记录后附。

## 交付边界

旧 PC 副本启动 assert 572，未成功完成原版游玩；桌面输入在物理 Esc 后停止，后续使用程序内测试与截图，未继续操作用户窗口。当前主要界面采用原美术／字体／时间轴，但若干页面仍为宿主布局，真实多人、在线好友数据、战役全通关与部分特效细节尚未完成。详见 [未解行为](unresolved-behavior.md)，不能把本轮结果称为完全等价原版。

默认 Release EXE 已重新构建。为保留当时运行中的旧游戏窗口，旧二进制安全改名为 `bin/x64/Release/gun_bros_re.previous.exe`，没有结束该窗口；启动器仍使用更新后的 `gun_bros_re.exe`。

## 后续核对（UTC 16:02 起）

- 原 `GLU_MOVIE_TRUNK_BUTTONS` 定位为 core movie 14；`CMenuMovieButton::Focus` 使用章节 3。主导航选中／悬停状态现在使用该原青色底板。
- `CMovieEmptyRegion::GetMetricsAtTime` (:182420) 将宽高当有符号 short 读取。core movie 15 的三个 `-3` 宽度关键帧此前误作 65533，已修正解析与插值，并加入真实记录断言；重新检查 175 个电影和全部 core 画廊。
- 修正正式入口对菜单渲染失败的返回值传播，避免错误路径保存账户后误报退出成功。
- 研究 45／46 已用显式 stdin 选择实际运行：分别生成 BOKOR 菜单截图及完成 Horde 实战，退出码均为 0。只向新建研究进程输入，不操作原用户窗口。

## 长跑暴露的问题与修复（UTC 16:15 起）

- Haven 第一次完整打完 500 波，但检查仍失败：此前遗漏了 64 次 native 50 指示器删除调用。保留失败日志 `out/validation/20260908-105248-132-Release-LongRun/survival-pack7.log`，没有将“敌人清完”算作整体通过。
- 补 `CLevelIndicator` 与 native 49／50，使用 Mach-O ARMv7 地址 `0x3C4AB0` 的原七类动画表，core archetype 1。淡出按原 1000 单位每毫秒减 5，实际 200 ms 余弦衰减；目标进入内部视野或消失时退休。BIG 脚本的删除调用实际仅一个参数，采用该参数，未照抄反编译的可疑 `a3[1]`。
- 自动敌人／拾取物箭头跟随原 Spawn 路径，并使用独立实例句柄，防止同名拾取物之间跳转。HUD 七类箭头／图标、原动画实际时长、目标销毁及重开清理均有程序内检查；截图 `out/hud-check-indicators.png` 已目视。
- Stage 27 重跑结果：Haven 500 波退出 0；Ceres 2 第 234 波无伤害进展超时退出 1；Yeroc 全部 500 波完成，但 native 82 未实现导致退出 1。原失败报告保留在 `out/validation/20260908-112216-820-Release-RemainingPlanets/`，前后原存档／正式账户变化为 0。
- Ceres 自动试跑器原来只检查碰撞接受条件，遗漏 `enabled`／`targetable`；现与兄弟 AI 的目标过滤一致，避免把地图中仍可响应碰撞的非攻击目标作为稳定交战对象。此修改只影响测试输入，正式玩家控制和敌人 AI 未改。单独从 233／234 波开始都能继续，因此需要从第一波重跑确认累计路线问题已经消除。
- Yeroc native 82 依据 `CPlayerStatistics::SetStatBit` (:220857) 恢复记录 42 的 0–31 位，重复设置幂等；无效位不修改。正式进度保存时与账户按位合并，Profile v6 兼容 v1–v5。远端成就上报仍是独立未解项。位 1／4／31、重复调用、越界、重开和存档回读有精确断言。

## 最终验证记录

所有运行均传 `--mute`。长跑采用真实移动／开火输入和原敌人／子弹脚本，玩家设为无敌；结果不能替代普通难度手感验收。

| 构建／检查 | 结果与证据 |
|---|---|
| Stage 28 Release Core | 18 项完成：17 Passed，1 个精确 PROP 43 KnownDataIssue；`out/validation/20260908-112652-543-Release-Core/` |
| Stage 28 Release UI | 8／8 Passed；`out/validation/20260908-112824-414-Release-UI/` |
| Stage 28 四图最终两波 | 4／4 Passed，未实现调用均为 0；`out/validation/20260908-113239-886-Release-Boundary/` |
| Stage 27 六条战役 | 6／6 移动与战斗烟测通过，未验证完整通关；`out/validation/20260908-112227-628-Release-Campaign/` |
| Debug 核心回归 | 18 项完成，同一个已知 PROP 43；`out/validation/20260908-110525-881-Debug-Core/` |
| Stage 28 Debug 补测 | `out/ui-debug-final-flow.log`、`out/ui-debug-final-hud.log`，均退出 0；HUD 9 态／7 类指示器 |

以上组合回归的原存档／正式账户 SHA256 比较均为 0 变化。Stage 29 是交付候选：补统计位与自动试跑目标过滤，最新结果另附；不将此前构建的检查冒充最终构建全部重跑。

### Stage 29 统计位修复构建

- Release 构建退出 0：`out/ui-stage29-build.log`。正式启动器 EXE 与 QA EXE SHA256 相同：`34A708EE4FEA22ECCBCA6D1FFE723EDC7841E83BDB18A005DBD9F0D3CEC3FEA4`。
- Release Core：`out/validation/20260908-113738-998-Release-Core/`，18 项完成，17 Passed／1 KnownDataIssue；退出 0，保护文件变化 0。
- Release UI：`out/validation/20260908-113902-386-Release-UI/`，8／8 Passed；退出 0，保护文件变化 0。
- Release Boundary：`out/validation/20260908-114021-161-Release-Boundary/`，四图第 499–500 波 4／4 Passed；退出 0，保护文件变化 0。
- Debug 构建、原关卡流／指示器／统计位和完整菜单检查退出 0：`out/ui-stage29-debug-build.log`、`out/ui-stage29-debug-flow.log`、`out/ui-stage29-debug-menu.log`。
- 交付画面 `out/delivery/planets.png`、`out/delivery/bokor.png` 由正式 Release EXE 直接生成并目视检查，均退出 0。BOKOR 的正式入口为主导航 **GAMES → BOKOR / HORDE**。
- `git diff --check` 退出 0；工程 244 个 C++ 源／头项目条目均存在且无重复，变更没有删除原注释。`out/delivery/manifest.json` 记录正式／Debug／QA EXE、源文件 SHA256 与组合回归原始结果。

### 最后一个长跑缺口：弹窗关闭

Stage 29 的 Ceres 2 完整 500 波退出 0，未实现调用为 0，确认了试跑目标过滤修复。Yeroc 同样完成全部战斗，但最终计数仍有五次 native 70；失败报告保留在 `out/validation/20260908-113728-097-Release-FinalPlanets/`。

原 native 70 调用 `CGame::ClearDialogPopup(false)`，进而清除当前电影章节；`CDialogPopup::Update` 标记结束后，`CGame::Update` (:76636) 还会派发关卡事件 4。最终实现保留这个完成事件，在下一次宿主更新中关闭弹窗并通知关卡，避免在脚本 native 内同步重入解释器。原弹窗完整章节动画仍属 UI 时间轴细节；当前文字弹窗的阅读时长继续明确采用宿主规则。

中间 Stage 30 的 QA 长跑为纠正完成事件而主动停止，不能计为通过。Stage 31 的 Release／Debug 专项检查均退出 0，覆盖脚本打开、延迟关闭、关闭已空弹窗；`out/ui-stage31-flow.log`、`out/ui-stage31-debug-flow.log` 保存结果。Stage 31 UI 回归 8／8 Passed，保护文件变化为 0：`out/validation/20260908-115454-889-Release-UI/`。

最终正式 Release EXE 与 `out/delivery-bin/gun_bros_re.exe` 相同，SHA256 为 `0995763464D2EB5513D5AB41E592A6DBAAB667536AAE672247BE800977A1EF58`。`out/final-bin/` 是前一候选，验收请使用正常启动器或 `out/delivery-bin/`，不要继续启动前一候选。

Stage 31 核心回归完成 18 项（17 Passed／1 个原 PROP 43 KnownDataIssue），退出 0，报告 `out/validation/20260908-115604-306-Release-Core/`。四图最终两波 4／4 Passed，退出 0，报告 `out/validation/20260908-115712-418-Release-Boundary/`。两组保护文件比较均为 0 变化；此前 UI 的 8 项也已全部通过。

## 最终交付汇总

本轮从 UTC 12:54 推进至约 17:04；略超约定四小时，用于完成最后一个长跑暴露问题的回调修正和重新验证。默认启动器、Release／Debug 构建均已更新，用户原来运行的旧窗口没有关闭。请重新运行根目录 `play.cmd`（声音）或 `play-muted.cmd`（静音）验收新版；研究菜单由 `research.cmd` 或 `--research` 进入，保留全部 46 项。

| 星球 | 完整 500 波通过的构建与记录 | 结果 |
|---|---|---|
| Cerberus Prime | Stage 21，`out/validation/20260908-105248-132-Release-LongRun/survival-pack2.log` | 退出 0，28777 次击杀，未实现调用 0 |
| Haven | Stage 27，`out/validation/20260908-112216-820-Release-RemainingPlanets/pack7/run.log` | 退出 0，29412 次击杀，未实现调用 0 |
| Ceres 2 | Stage 29，`out/validation/20260908-113728-097-Release-FinalPlanets/pack9/run.log` | 退出 0，28682 次击杀，未实现调用 0；两只地图固定对象仍存活，不影响原脚本完成条件 |
| Yeroc Sina | **最终 Stage 31**，`out/validation/20260908-115443-651-Release-YerocDelivery/pack12/run.log` | 退出 0，28600 次击杀，未实现调用 0，失败数 0 |

以上是不同阶段的完整长跑，**并非宣称四图都在 Stage 31 从第一波重新跑过**。最终 Stage 31 已另外完成 Core 18 项、UI 8 项、四图第 499–500 波 4 项，以及 Debug 关卡／弹窗专项检查。最后 Yeroc 报告的保护文件 SHA256 比较仍为 0 变化；早先失败和主动停止的记录保留，不计入通过次数。

最终版本可验收启动、选关、装备／购买、炼化、战斗、死亡／重开、保存、BOKOR 和本地在线入口；仍未完全恢复的原版布局、弹窗章节动画、战役全流程、远端服务／多人同步及具体资源缺口见 [未解行为](unresolved-behavior.md)。`out/delivery/manifest.json` 是最终二进制与源文件校验记录；生成画面示例在 `out/delivery/`。
