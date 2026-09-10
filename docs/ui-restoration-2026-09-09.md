# 正式 UI 去除替代实现

范围：用户指出的暂停菜单、Invite、Free Warbucks、菜单横扫和加载 CG，以及正式导航可达的其他手写 UI。截图仅用于结果核对，运行时读取 BIG。

## 阶段与验收

1. 暂停入口统一到 `MDS_PAUSE_ROOT` / `MDS_HELP` 与 LIST_MENU 系列；删除旧四项布局。验证无原存档的独立地图和正式存档均能滚动、进入帮助、返回并切换设置。
2. 商店两张推广卡按 `CMenuTapjoyOption`，弹窗按 `CMenuInviteFriends` 与原免费货币菜单绑定；核对 Movie 章节、字体、STR、图标和关闭命中。远端邀请/广告没有接入，不生成成功或奖励。
3. 横扫按 `CMenuSystem::Transition1Callback/Transition2Callback` 的两个裁剪区域，时长和位置读取 `GLU_MOVIE_WIPE`。检查旧、新内容、输入隔离、商店分类及导航路径。
4. 加载按 `CMenuSplash` 的图片/文字/覆盖层绑定，加入进入战斗及返回菜单资源加载阶段。检查图文来源、加载动画和保留同一个窗口。
5. 审计正式页面的可达分支，清除剩余替代布局与文案，保留独立研究入口。逐项记录未恢复的远端服务，不能据局部通过宣称所有 UI 完成。

## 已复现

- `--pause-check --mute` 原测试退出 0，只覆盖 `originalUi=true`。
- 将同一检查切换为独立地图调用方状态后，退出 1：`native-hits=0 back=0 failures=4`，日志 `out/ui-pause-red.log`。旧四项布局实际存在于 `SurvivalHud::Buttons/Draw`，由存档存在性决定是否启用。
- 商店 Invite 卡实际跳到手写账户页 9，Free Warbucks 跳到 Bank 页 17，没有进入原弹窗。
- `LoadingScreen.h` 仅黑底及固定缩放的小人，没有加载 CG 或 STR 提示。

## 一手依据

- `_Big_tool/binary template/big_assets/ui_movie.bt`：章节轨、用户区域、锚点；对应 `CMovie::InitResource :109263`，区域编号不是对象编号。
- `CMenuInviteFriends::Init :248641`、`Bind :248470`、`Draw :248330`：ADD_FRIENDS_POPUP_SMALL / ADD_FRIENDS_POPUP，标题五个 STR、正文 STR、区域 6 的平台图标和区域 7 的关闭图标。
- `CMenuTapjoyOption::Focus :221924`：动作 125 打开 Invite，130 打开免费货币入口。
- `CMenuSystem::Transition1Callback :96265 / Transition2Callback :96250`：分别按 WIPE 用户区域裁剪两个菜单。

## 实现与审计结果

| 已发现的替代实现 | 本次处理 | 原版依据及运行位置 |
|---|---|---|
| 非原存档/独立地图启用四项暂停菜单 | 删除旧 Buttons/Draw 分支，统一多级列表、帮助和设置 | `MDS_PAUSE_ROOT`、`MDS_HELP`、LIST_MENU；`SurvivalHud::DrawOriginalPause` |
| 旧六列战斗购买页、固定角标、合成结算覆盖 | 删除，统一原 PowerupSelector/Controls；结算交给原 postgame 菜单 | `CPowerUpSelector`、`CInputPad`；`SurvivalHud.cpp`、`MapScene.cpp` |
| 手写波次/升级全屏通知文字 | 删除旧 NOTICE，使用 LEVEL 事件、原 Movie 和 STR；调试统计仅在 debug 显示 | 原 `CLevel` 消费链；`QueueOriginalNotice` |
| Invite 跳账户页、Free Warbucks 跳 Bank | 卡片动作 125/130 打开各自原弹窗，留在商店；入场、关闭、图标及文本按原绑定 | `CMenuTapjoyOption::Focus :221924`、`CMenuInviteFriends`、`CMenuIncentives :292917..293260`；`OriginalPromotionPopup.h` |
| 推广卡固定字位/缩放 | 使用 Sprite bounds、font6；Invite 标题由 STR 取得 | `CMenuTapjoyOption::Bind :222059`；GET FREE/WARBUCKS 是原程序 UTF-32 字面量，生成器从 Mach-O 提取，不伪称 STR |
| 普通购买失败/成功自制 toast | 失败接原三按钮、总价/差额 STR；成功刷新商品卡和存档 | `CMenuAction::DoAction :94606`、`MDS_STORE_PROMPT_MOMONEY`、`MDS_BUTTON_STORE_PROMPT`；`ShowStoreFundsPrompt/DrawStorePrompt` |
| 商店统一缩小文字、固定按压/滚动时间、三列常量、局部筛选位 | 字体按原 font5；时间从 Movie 章节取，列从区域取；按 MDS 和 STORE.type/value8 筛选 | `CMenuStore::Bind :180050`、`CStoreAggregator::InitFilteredList :159135/159199`；`DrawStore/PlateLabel/DrawPress` |
| Options 帮助手写段落/返回按钮 | 共用原 LIST_MENU 列表及实际14条 MDS_HELP，原 BACK Movie | `DrawOptions`；进入帮助/返回恢复父列表位置 |
| 手写旧装备页、星图、选轮页、模式按钮、日奖励/活动、假账户/排行榜、旧社交弹框、旧炼化/结算、全局 BACK | 删除替代绘制函数及正式路径，统一已恢复的原版页面；缺原存档数据时明确失败 | `GameFrontEnd.cpp`；真实原存档为正式入口，旧 .dat 仍供独立领域检查 |
| 菜单瞬时切换 | 按原 WIPE 的区域0/1裁剪旧/新页面，使用原横扫图层与时长；转场中屏蔽输入 | `CMenuSystem::Transition1Callback :96265 / Transition2Callback :96250`；`MenuWipe.h` |
| 黑底小人加载页 | KEYSET_SPLASH_IMAGES/TEXT 同索引取 CG/STR，绑定 SPLASH、HEADER、INFO_CLUSTER、小人和 IDS_LOADING | `CMenuSplash :160325..161008`、`CMenuSystem::Bind :97132`；`OriginalLoadingSplash.h/LoadingScreen.h` |
| 默认队友写死姓名、文字0.7倍及固定偏移 | 默认本地队友姓名留空；有绑定姓名时按原 font0 字宽/字高居中 | `CLevel::Bind :121881..121939`、`DrawBrotherLabel :120351`；`MapScene/SurvivalHud` |
| 指示箭头固定屏幕边距 | 原25/25/100乘相机 viewport factor，再换算到宿主画布 | `CLevelIndicator::Init :191653`、`GetOrientation :191317`；`SurvivalHud::Draw` |

### 数据与执行逻辑的边界

- KEYSET 严格按 `big_keyset.bt` 的 u16 数量/u32 handle 解析，要求文件边界完整；原加载 keyset 有27项，普通流程按 `CMenuSystem::Init :97481` 排除末5项。22组图文全部从 BIG 验证；index14是原有空 STR，保留为空。
- 原 `CMenuSystem::GetSplashScreenIndex :96132` 循环取图。宿主目前在进程内维护轮转序号，没有写回原1006菜单状态中的跨启动序号。
- 原 SPLASH 章节为0/300/700、duration1000；进入游戏描述符的退出动画标记为1，回菜单为0。读取资源前先播放入场，资源读取期间泵事件并绘制，入游戏播放退出章节。参数来自 BIG/原描述符。
- `src/tools/extract_ui_transition_statics.py` 从只读 `gunbros` 提取描述符和原生字面量，生成文件保留输入 SHA256 和 VA；没有另建人工资源表。
- 横扫 GPU 纹理是 Windows 绘制适配，旧页面采用上一帧缓存，新页面继续绘制。区域、关键帧、图层、时长均来自原 Movie。它尚不等同于原完整 MenuStack 在转场中同时更新两个菜单。
- FB、Game Center、AdColony、Tapjoy 和 More Games 服务未接入。保留原入口与图像，点击记录对应原动作，不发送邀请、不发放假奖励。Bank 4秒离线到账属于 `UI_sample/ui.md` 中已有用户授权，继续明确作为宿主模拟，金额读 BIG。
- 保留用户此前明确要求的桌面标题提示、键鼠适配以及 debug/研究输出；这些没有冒充原资源。原程序的枚举、绑定编号、颜色、算法常量也不会因“有数字”而删除。
- 保留已有研究 EXE 入口，旧验证假页面的 `--game-menu-check` 改为原导航/设置/社交/推广/加载检查；`--hud-check` 聚合原控制/选择器/暂停。新增研究入口73推广弹窗、74横扫与加载。
- 本项清除的是已发现的正式可达替代布局，不将其等同于所有原版菜单、网络功能及全部动画都已复刻。暂停列表原惯性/完整退场、其他既有 UI1–UI7 未勾选项仍见 PLAN；没有用新假数据填补这些缺项。

## 验证记录

所有命令从仓库根目录执行，运行带 `--mute`；存档测试使用 out 中隔离副本。

| 命令（研究 EXE） | 结果 | 证据 |
|---|---|---|
| `--pause-check --mute`，独立地图调用方状态 | 修前1，修后0 | `out/ui-pause-red.log`；原列表6项、帮助14项及返回/设置 |
| `--options-check --mute` | 0 | `out/ui-options-restored.log`；14条帮助及原 BACK |
| `--promotion-check --mute` | 0 | `out/ui-promotion-green.log`；真实商店卡→原弹窗、入场门控、图标/关闭、不离开商店，普通不足币三按钮 |
| `--loading-wipe-check --mute` | 0 | `out/ui-loading-wipe-check.log`；22组CG/STR、裁剪及方向锚点、实际 Store→Options/Play/Armor、输入隔离 |
| `--bank-check --mute` | 0 | `out/ui-bank-restored.log`；25阶段、全部16商品、兑换/不足币/保存重载 |
| `--store-template-check --mute` | 0 | `out/ui-store-restored.log`；772条文本、三类卡片开合/预览/购买、真实不足币购买路径、76款武器渲染、原双枪按钮保存重载 |
| `--scene-transition-check --mute` | 0 | `out/ui-scene-transition-restored.log`；4阶段同一窗口/GL context，generation=1 |
| `--hud-check --mute` | 0 | `out/ui-hud-restored.log`；原控制、按钮反馈、20个替换图标、选择器、6项暂停/14项帮助/设置重载 |
| `--dual-weapon-check --mute` | 0 | `out/ui-dual-weapon-restored.log`；两个装备标记、槽位互异和保存重载 |
| `--native-profile-play-check --mute` | 0 | `out/ui-native-play-restored.log`；四星球各2波、Horde、原装备与原存档重载 |
| `--profile-play-check --mute` | 0 | `out/ui-profile-play-restored.log`；真实原HUD按钮与键盘路径、2波保存后续玩至4波 |
| `--game-menu-check --mute` | 0 | `out/ui-menu-restored.log`；原导航、帮助、社交、推广和转场的聚合入口 |
| `--postgame-menu-check --mute` | 0 | `out/ui-postgame-restored.log`；原实战结算开闭、分页、Casualties、无重复奖励 |

构建：`MSBuild.exe gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /m /v:minimal` 退出0，日志 `out/ui-restoration-build.log`。`git diff --check` 通过。生成器重跑与已生成内容逐字节一致，日志 `out/ui-transition-generator.log`。

交付：`bin/x64/Release/gun_bros_re.exe` 与 `gun_bros_research.exe` 已更新。正式游戏与研究入口分别保留。

旧 `.dat` 控制检查原先点击已删除 UI 的固定坐标，曾失败。现改为查询与生产 Pointer 共用的原 Movie 命中区域，验证打开/关闭选择器、切枪、暂停/继续及 Q/E/1/2/F/R 分派；其库存是明确的隔离测试样本，原购买和左右装备另由 `RunOriginalPowerupSelectorCheck` 覆盖。战斗控制器里遗留但已不显示的自造 shopMessage 字串及分支同时删除。

截图：`out/ui-promotion-125.png`、`out/ui-promotion-130.png`、`out/ui-store-funds-original.png`、`out/ui-loading-cg-0.png` 至 `-2.png`、`out/ui-wipe-menu-0.png` 至 `-2.png`。

验证中发现并修复：横扫帧缓冲的负高度导致X/Y同时翻转，改为只翻转纹理V，并用非对称颜色锚点和真实文字截图复验；Bank及Powerup旧测试沿用非原版筛选位，按实际 MDS/STORE 语义修正测试，而没有恢复错误运行逻辑。

## 追加修复：Spire、横扫与启动交接

范围与验收：纠正 Haven 开局问号目标；首次进入商店分类、星球进入 REV 均完整播放 BIG 横扫并阻止点穿；按用户补充，Logo 结束立即交接 Splash，资源加载期间右下角播放原小人，进出战斗继续使用 CG。任务按对象绑定、转场计时、启动交接、实战回归依次实施。

### 原因与原版依据

- **问号错绑同 ID 物件。** pack7 map6 的 layer2 先登记 Spire，随后 layer3 登记静态物件；两层各有 objectId=0。重建的 props 数组已按绘制位置排序，用它搜索 ID 错选了 `(522,160)` 的静态物件。Spire 的原放置点是 `(1039,1188)`。检查 `maps/map.bt`、`entries/common.bt` 与原 `CLayerObject::OnStart :126250`、`CLevel::AddObject :116854`、native49 `:117927` 后，改为维护对象登记顺序，一次解析并保存实例标识。边界中心按 `CProp::GetBounds :123561` 和 `CLevelIndicator::GetOrientation :191317`，由实际三层 Sprite 边界计算，本资源结果 `(1037,1149)`；这些坐标是诊断输出，没有写进运行配置。原 PROP33 脚本见 `out/binary-research/flow-disassembly/pack7_xga_0071_0x3dba.flow.txt`，state2 `0x163` 调用 native0x0531，目标 local0 为原值0。
- **横扫被冷加载耗时直接推进到结尾。** 旧检查未模拟一帧内的资源加载，前次通过不能覆盖用户现场。现在新页面加载完成、首帧开始横扫时重置计时起点；原 WIPE 的700ms、关键帧、裁剪区域均未修改。检查注入两倍原时长的帧内负载，然后验证350ms时横扫仍活动、只启动一次且点击不穿透。覆盖 Options、Play、Armor、Powerups、Bank 及实际星球点击→REV 路径。BT/原消费函数仍为 `ui_movie.bt`、`CMenuSystem::Transition1Callback :96265 / Transition2Callback :96250`。
- **启动与战斗 CG 分离。** `RunStartupSequence` 在视频开始前读取原 bundle `png/Default-Landscape.png`，结束或跳过时立即绘制它。`LoadingScreen` 的启动路径保持该图并播放 core Sprite0:124（原 `CMenuSystem` 加载指示器），没有 SPLASH CG 章节，也不消耗 CG 轮转索引。Splash 图片是原程序 bundle 资源，不是 BIG 中的提示 CG；此启动编排按本次用户明确要求。正常进入/离开战斗保留 BIG/STR 加载页。正式默认启动及显式 page14 的两轮预载都使用此路径。

实现位于 `MapScene.cpp` 的 `MapPropWorld`、`SurvivalSession/CLevel`、`GameFrontEnd.cpp`、`StartupSequence.cpp`、`LoadingScreen.h`。稳定实例标识属于宿主对象引用适配，未修改 BIG 数据。

### 追加验证

以下研究命令均带 `--mute`；隔离账户位于 `out/`。Release 正式与研究 EXE 均已更新，构建退出0，日志 `out/ui-followup-build.log`。

| 检查 | 结果与证据 |
|---|---|
| `--combat-feedback-check` | 退出0，`out/indicator-followup-green.log`；原登记顺序、真实边界、同 ID 敌人后生成不抢目标，`original-order=1 bound=1 duplicate-retained=1`。修前错误目标见 `out/indicator-followup-probe.log` |
| `--loading-wipe-check` | 冷加载复现修前退出1、修后0，`out/wipe-cold-load-red.log` / `out/wipe-cold-load-green.log`；全部转场活动且350ms、22组CG/STR仍通过；REV 截图 `out/ui-wipe-revolution.png` |
| `--intro --advance 5000 --screenshot out/startup-video-handoff.png` | 退出0，视频真实结束分支直接绘制 Splash；`out/startup-video-handoff.log` |
| 正式 EXE `--menu-page 14 --screenshot out/startup-title.png --profile out/startup-followup-profile --skip-intro --mute` | 使用 `Start-Process -Wait -PassThru` 收取退出0；`out/startup-title.log` 两次均为 `startup-loading`，没有 `loading-splash` |
| 启动生产绘制路径0/200ms截图 | `out/ui-startup-loading.png` / `out/ui-startup-loading-next.png`；像素变化仅右下角小人，其他像素与视频结束 Splash 一致，`out/startup-pixel-check.log`。截图在 Present 前捕获，修正最初误取上一帧的检查时机 |
| `--play --map pack7 6 --advance 2400 --screenshot out/spire-indicator-followup.png` | 退出0，截图问号朝向下方 Spire 所在方向，未再指向上方同 ID 静态物件 |
| `--native-profile-play-check` | 退出0，四星球各2波及 Horde、原装备/原存档保存重载；`out/ui-followup-native-play.log` |
| `--scene-transition-check` | 退出0，4阶段同一窗口/GL context，generation=1；`out/ui-followup-scene.log` |

`git diff --check` 通过。以上为定向回归及截图核对，不等同四星球全部500波人工游玩，也未逐帧证明所有原版菜单动画完全一致。

