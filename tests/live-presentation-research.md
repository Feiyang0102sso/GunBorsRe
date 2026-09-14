# Live 呈现与商店原版证据

最终加载更正：`IDB_SPLASH_MAIN_MP` 属于启动菜单，不能用于关卡加载。进关使用 `MENU_GAME_LOAD`、Movie24 的 header 下方区域及 KEYSET 壁纸；按用户要求 Live／Deathmatch 分池。完整菜单指针表与机器码补证见 `live-loading-correction-research.md`，替代前次主图结论。倒计时等 HUD 补证仍见 `live-hud-followup-research.md`。

研究日期：2026-09-14。范围仅包含 GameType 2 的每波结算、最终结算、加载画面、双方战斗商店。未修改实现代码。本文行号均指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`；`mem+N` 是运行时成员偏移，不是磁盘偏移。

## 证据与边界

- 一手消费者：iOS 3.6.0 反编译代码。Movie 文件通过 `_prep/out/ui-movie-catalog.json`、`_prep/out/binary-research/resource-inventory.json` 定位，并读取对应原始解包字节；PNG 已目视核对。
- 结构模板：`_prep/_Big_tool/binary template/big_assets/ui_movie.bt`、`big_keyset.bt`。Movie 为 10 字节头、逐对象变长记录；type 5 章节每帧 4 字节，type 6 region 每帧 37 字节。不能将对象序号当 region 序号。
- **章节索引特别注意**：`CMovieChapter::Init` 109553–109566 先插入隐式 chapter 0=0，再把文件的 time 依次放入 chapter 1..N。WAVE_WRAPUP 的文件 `[0,516]` 对应运行时 `[0,0,516]`；SPLASH_INTRO_MP 的 `[1499,1793]` 对应 `[0,1499,1793]`；WRAPUP_SCREEN_MP 的 `[1000]` 对应 `[0,1000]`。下文表中的“章节轨时间”是磁盘原值。
- Menu 静态表：`_prep/out/menu-statics.json` 来自 `_prep/tools/extract_menu_statics.py` 对 `_prep/gunbros` ARMv7 Mach-O 的只读提取，已核对该提取器的 VA 映射与记录结构。静态菜单表不是重新手写的资源数据。
- 本文不把现有重建实现、旧截图、回归日志当作原版证据。原版资源仍应在运行时从 BIG 读取。
- `GameType` 与 `Mission::type` 是不同枚举；下面明确区分。不能因为二者都出现 1、2、3 就合并。

## 每波结算：不是单人 WAVE CLEARED 加一行文字

### 状态与时序

1. `CLevel::OnWaveCleared`（116897）先结算并调用 `UpdateMultiplayerStatistics(this, 1, perfect)`（116991）。
2. 116994–117001：仅当 `GameType == 2`、`Mission::type == 1` 且还有下一波时，将 `level+316436` 设为 **15000 ms**，传入两份统计：`level+316436`、`level+0x4D448`，调用 `CGame::OnWaveCleared`。
3. `CGame::OnWaveCleared`（76192；76246–76255）进入 `CInputPad::OnWaveClear`（89760）。顺序为波次清理提示、可选 Perfect 提示、挑战更新、双人统计。`OnRevolutionClear`（89600 附近）也通过同一个 common overlay helper 插入双人统计。
4. `CInputPad::SetUpCommonInterstitialOverlays`（87675）保存两份统计指针至 `InputPad+9232/+9236`，若有统计则加入 `InputPad+8388` Movie，并将 playback target 设为 `duration-1`（87704–87708）。
5. `CLevel::Update`（121363–121390）在该倒计时非零时，减去更新步长，不推进对应波状态；归零时清理本波统计及 ready 标记，保留累计统计。
6. `OverlayWaveStart`（86979；87010–87016）在剩余时间 <=1000 ms 时反向播放统计 Movie，并将 playback target 改为 -1。剩余时间 2000–6999 ms 时显示下一波倒数。反编译器丢失了部分 variadic 格式参数，不能单凭缺失的 `SWPrintF` 参数抄出倒数取整公式。
7. `InterstitialSequenceCallback`（86336）清理完成状态或收起 HUD，再给 `CLevel::HandleEvent(..., 2)`。15 秒不是 Movie 动画长度，也不是单独追加到所有提示之后的额外等待。

### 原 Movie

`CInputPad::Init` 中 88235–88236 明确把 **GLU_MOVIE_WAVE_WRAPUP** 绑定到 `InputPad+8388`。

| 字段 | 原值 |
|---|---|
| 包 | pack0_core_xga |
| 类型内 ordinal / handle | 118 / 0x0300051D |
| 文件 | `_prep/big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0201_0x2a9c31.bin` |
| 长度 / 画布 | 5654 字节 / 960×640 |
| 动画长度 | 1000 ms |
| 对象 / region 数量 | 40 / 23 |
| 章节轨 | object 39，文件偏移 5643；时间 0、516 ms |

所有布局、锚点和逐帧变化读取该 Movie。region 绑定的完整入口是 `CInputPad::Bind`（90769；90969–91107）。

| region | 回调后缀 | 内容 / 数据 |
|---:|---|---|
| 0 | OverlayNamePlayer | 本地玩家姓名 |
| 1 | OverlayNameBrother | 远端玩家姓名 |
| 2 | OverlayIconPlayer | 本地头像 |
| 3 | OverlayIconBrother | 远端头像 |
| 4 | OverlayWaveStart | 下一波提示与倒数 |
| 5 | OverlayKillsTitle | IDS_MULTIPLAYER_WRAPUP_KILLS |
| 6 | OverlayAssistsTitle | IDS_MULTIPLAYER_WRAPUP_ASSISTS |
| 7 | OverlayXplodiumTitle | IDS_MULTIPLAYER_WRAPUP_XPLODIUM |
| 8 | OverlayXPTitle | IDS_MULTIPLAYER_WRAPUP_XP |
| 9 | OverlayDeathsTitle | IDS_MULTIPLAYER_WRAPUP_DEATHS |
| 10 | OverlayTotalXPTitle | IDS_MULTIPLAYER_WRAPUP_TOTALXP |
| 11 | OverlayKillsPlayer | 本地 u16，统计 mem+4 |
| 12 | OverlayAssistsPlayer | 本地 u16，mem+6 |
| 13 | OverlayXplodiumPlayer | 本地 u32，mem+12 |
| 14 | OverlayXPPlayer | 本地 u32，mem+20 |
| 15 | OverlayDeathsPlayer | 本地 u16，mem+16 |
| 16 | OverlayTotalXPPlayer | 本地 u32，mem+40 |
| 17 | OverlayKillsBrother | 远端 u16，mem+4 |
| 18 | OverlayAssistsBrother | 远端 u16，mem+6 |
| 19 | OverlayXplodiumBrother | 远端 u32，mem+12 |
| 20 | OverlayXPBrother | 远端 u32，mem+20 |
| 21 | OverlayDeathsBrother | 远端 u16，mem+16 |
| 22 | OverlayTotalXPBrother | 远端 u32，mem+40 |

数值消费者见 87110–87202。远端数值全部先检查统计 `mem+48` 有效标志，不能尚未拿到统计就伪造零。统计 `mem+0` 是本地波间剩余时间。

`CInputPad::Bind`（90883–90900）根据 **Mission type** 选下一波文案：type 2 → `IDS_MULTIPLAYER_WRAPUP_WAVE_START_SURVIVAL`；type 1 → `IDS_MULTIPLAYER_WRAPUP_WAVE_START_ENDLESS`。其余六类标题在 90904–90936 读取原 StringPack。

`CLevel::UpdateMultiplayerStatistics`（115164）区分本波增量、会话累计，GameType2 通过 packet 0xD 发送 `StatisticPacket`（115252–115277）。`SyncWaveStatisticsWithHost`（114435）是统计校正入口。不得将每波面板的单波击杀直接替换成整局击杀。

## 最终结算：单独多人菜单与两列统计

`CGunBros::ShowWrapUpMenu`（78109）分派：GameType2 → menu 24（78132–78140）；GameType3 → menu 25（78151）；其他 → menu 23。`CMenuSystem::Init`（97645–97647）对应 `MENU_POST_GAME_WRAPUP`、`MENU_POST_GAME_WRAPUP_MP`、`MENU_POST_GAME_WRAPUP_MP_DM`。

**GLU_MOVIE_WRAPUP_SCREEN_MP**：pack0_core_xga，ordinal 119，handle 0x0300051E；`.../0xf4e02223/pack0_core_xga_0202_0x2a9f8f.bin`。1618 字节，960×640，3025 ms，14 对象、8 regions。章节轨 object 1，文件偏移 55，边界 1000 ms。

`CMenuPostGame::Init`（166022；166089–166136）绑定：

| region | 内容 |
|---:|---|
| 0 | 返回按钮定位，由 Bind 的 GetUserRegion 消费 |
| 1 | CategoryCallback：概览/伤亡/目标分类按钮，以及多人附加按钮 |
| 2 | HeaderCallback：主标题，来自 provider 92 的会话结果 |
| 3 | HeaderCallback：进度副标题，来自 provider 92 的会话进度 |
| 4 | ContentCallback：当前分类的完整滚动内容，不能直接当单一数值槽 |
| 5 | PlayerNameCallback：本地玩家名 |
| 6 | PlayerNameCallback：远端玩家名 |
| 7 | 多人继续/重开状态按钮定位；Bind 使用最后一个 region |

数值在 region 4 内由 `CMenuMovieControl` → `CMenuOptionGroup` → `CMenuPostGameOption` 组装。`Bind` 165740–165790 使用数据提供器决定组数、行数和选项；`OverviewCallback` 164593 负责绘制组。因此应复用原卡片/滚动资源和双列数据提供器，不能将两列值硬塞到主 Movie 并照截图排坐标。

静态 **MDS_ICON_POSTGAME_MP**（ARMv7 VA 0x4092D0，9 条含空末项）的原标题顺序：

| 原表项 | 标题 alias |
|---:|---|
| 0 | IDS_WRAPUP_MP_KILLS |
| 1 | IDS_WRAPUP_MP_ASSISTS |
| 2 | IDS_WRAPUP_MP_DEATHS |
| 3 | IDS_WRAPUP_XPLODIUM |
| 4 | IDS_WRAPUP_EXPERIENCE |
| 5 | IDS_WRAPUP_OBJECTIVES |
| 6 | IDS_WRAPUP_PERFECT_WAVES |
| 7 | IDS_WRAPUP_BEST_STREAK |
| 8 | 空项 |

这不是所有条目同时显示的承诺。实际过滤/数量还由原动态 provider 决定。`CMenuDataProvider::CreateContentString` 的 152755–152814 明确映射：provider 92 的 item 0=Outcome、1=Progress、2=KillsForSession、3/4=双方姓名；provider 94（及 93、95 的转换）映射 0=Kills、1=Assists、2=Deaths、3=Xplodium、4=Experience、5=ExperienceGift、>=6=Bonus。第二维只接受玩家 0、1。

原字符串构造函数入口：`CreateAssistsString` 74685、`CreateDeathsString` 74880、`CreateKillsString` 74953、`CreatePlayerNameString` 75155、`CreateXplodiumStringForSession` 75224、`CreateOutcomeStringForSession` 75445。单波统计与会话统计是不同字段；本研究不替代战斗归属算法核对。

### 章节、停留、重开

`CMenuPostGame::SetState`（164739）状态 0 从 chapter 0 播开场；状态 1/2/3 保持 chapter 1，分别处理升级/相关提示；状态 4 开启分类、返回和多人按钮，保持可交互。`Update`（165459）待 Movie/提示完成后推进。没有“显示 3 秒后自动退出”的原逻辑；3025 ms 是资源动画时长，不是结算页停留期限。

`UpdateMultiplayer`（165096）查询 provider 100 item 0 column 1 的非负状态，决定显示哪组 replay 控件。`MDS_POSTGAME_REPLAY_MP` 原标签包括 `IDS_WRAPUP_MP_REPLAY_EXIT`、`IDS_WRAPUP_MP_REPLAY_REQUESTED`。应保留重开等待/对方离开这一层，而不是直接重置关卡。

Deathmatch 使用独立 **MDS_ICON_POSTGAME_MP_DM**（VA 0x4097E0）：Kills、Deaths、Xplodium、Experience、Best streak 五项，没有助攻项；Outcome 使用 `IDS_WRAPUP_DEATH_MATCH_DRAW/LOSS/QUIT/WIN`（75513–75528）。其多人按钮也额外增加一组（165211–165225），不能将合作结算无条件用于 DM。

## 加载画面：完整背景加 Movie 时间轴

`CMenuSplash::Init`（160386）创建菜单配置指定的主 Movie；regions 0/1/2/3 分别为 Background/Text/BodyText/Overlay（160429–160450）。`Load`（160500）从明确 image alias 或 `KEYSET_SPLASH_IMAGES[index]` 加载背景图，`BackgroundCallback`（160872）按照原 region 做等比覆盖绘制。`Bind`（160568）另用 `KEYSET_SPLASH_TEXT[index]` 绑定正文。

因此 **GLU_MOVIE_SPLASH_INTRO_MP 是整个加载菜单的 Movie 容器，背景图片通过回调绘制；不是一枚可叠在任意单人 splash 上的 logo**。该资源自身只有 region/章节，无内嵌 logo sprite。

资源：pack0_core_xga，ordinal 81，handle 0x030004F8；`.../0xf4e02223/pack0_core_xga_0164_0x2a7aec.bin`；363 字节、960×640、2600 ms、4 objects / 3 regions。章节轨在 offset 161，时间 1499、1793 ms。

`CMenuSplash::OnShow`（160788）chapter 0；`Update`（160678）chapter 0 完成后设 chapter 1 并执行进入动作（160715–160722）；`OnExit`（160805）按菜单配置切 chapter 2，退出后执行配置动作。实际等待取决于菜单配置和资源加载，不应把 2600 ms 当固定加载完成期限。

**KEYSET_SPLASH_IMAGES**：原包 `.../0x69e5d35c/pack0_core_xga_0284_0xd1541b.bin`，110 字节，u16 count=27，后跟 27 个 u32 handle。`CMenuSystem::Init`（97467–97481）设置多人起点=count−5=22。

| index | handle | 文件名（均在 pack0_core_xga/0xb7178678） | 目视内容 |
|---:|---|---|---|
| 22 | 0x02000568 | pack0_core_xga_0276_0xb522fa.png | 美女手持 LIVE MULTIPLAYER logo |
| 23 | 0x02000569 | pack0_core_xga_0277_0xbbe0f3.png | 双男枪手背景 |
| 24 | 0x0200056A | pack0_core_xga_0278_0xc1b5ef.png | 双兄弟与社交图标 |
| 25 | 0x0200056B | pack0_core_xga_0279_0xc6a679.png | LIVE logo、倒地和救援说明 |
| 26 | 0x0200056C | pack0_core_xga_0280_0xcdcda9.png | 红色 VS DEATHMATCH logo |

选择函数：`GetMultiplayerSplashScreenStartIndex` 96142；`GetMultiplayerSplashScreenIndex` 96148 返回旧值并 mod 3 自增。menu action 0x18/0x19/0x1A（94112–94130）：GameType3 取 start+4；其他多人，轮转值非零取 start+index+2，否则取 action−24+start。这意味着该份 iOS 反编译分支在 index=2 时也会取到最后一张 DM 图；目前按字节/源码如实记录，不自行“修正”成三个想象中的合作图片。

另有 **IDB_SPLASH_MAIN_MP**，logical id 1463，`.../0xb7178678/pack0_core_xga_0334_0xe1e117.png`，960×640。`CGunBros` 初始化中 80474 全局 `SetStaticImage` 使用它，不能据名字认定它是仅进入 Live 的加载画面。

## 双方独立战斗商店与 10 秒同步等待

这里原类是 **CPowerUpSelector**，同时有 Powerup 和 Weapon 模式，并非主菜单 CMenuStore。原行为是双方轮流发起浏览，另一端进入同一浏览界面的只读镜像；不是两人同时在各自商店无限停留。

### 打开与仲裁

- 本地 `CInputPad::ShowPowerUpSelector(this, afterDeath, cancellable, mode)`（90310）在 GameType2 设 **10000 ms**（90345–90359）。输入 HUD 状态须为 8/9，且没有待打开请求（90374–90377）。
- 90379–90393 发送 packet **5**：首字节 afterDeath，随后逐个 powerup 的库存字节；并设请求等待 **750 ms** 和请求时间戳（90395–90396）。`CInputPad::Update` 91321–91341 在等待归零后正式 Show。
- 收到对方 packet 5：`CRemotePlayer::ProcessPacket`（229807；230035–230047）用另一重载 `ShowPowerUpSelector`（90210 附近）传入远端库存与校正后的时间戳。90235–90239 检查本地 pending 时间与 HUD 状态，不能无条件用后到的商店覆盖先到者。
- 接收重载同样设 10000 ms（90247–90263），Show 后状态 7，摇杆停靠并清零输入（90290–90301）。

### 浏览、显示与暂停

`CPowerUpSelector::Show`（186421）：

- `a6 != nullptr` → `mem+5148=true`，明确表示正在看对方的库存/浏览。选择对方姓名（186469–186479）。
- GameType2 Show 后令 `level+275620=1`（186490–186491）；这是暂停战斗的原逻辑。GameType3 刻意跳过这个写入。
- remote browsing 的 PowerUpSelect 回调设 null（186495–186503），本地不能替对方选购；取消按钮也只给主动浏览方（186481–186488）。
- `SetupPowerUps` 用远端库存建立列表（186492），不是本地库存复制一份。默认选项由原 afterDeath 筛选与资源属性决定。
- 主 Movie `mem+4` 的 region 1=内容、2=玩家名及剩余时间、3=货币、4=模式切换（186983–187004），region 0用于取消按钮定位（186560）。`DrawPlayerNameAndTimer`（185362）只在限时且状态 1..3 时显示；旁观方显示对方姓名。

资源别名 **GLU_MOVIE_POWERUP_MENU_NEW**：core ordinal 131 / handle 0x0300052A，`.../0xf4e02223/pack0_core_xga_0214_0x2aaf2b.bin`，960×640、1000 ms、5 regions；章节轨 offset1285，时间0/100/900。原加载入口 187652–187665 确认它绑定到主 selector `mem+4`，`GLU_MOVIE_POWERUP_MENU_NEW_COPY`（physical217，6 regions）绑定到选中物品详情 `mem+412`；另加载 `GLU_MOVIE_POWER_UP_LAYOUT` 到 `mem+3136`、`GLU_MOVIE_GUN_LAYOUT` 到 `mem+3468`，以及 Deathmatch 专用 GUN_SLOTS 到 `mem+616`、GUN_CARD 到 `mem+820`。

### 同步命令与退出

`CRemotePlayer::ProcessPacket`：

| packet | 行号 | 实际行为 |
|---:|---:|---|
| 5 | 230035 | 打开商店，含模式标记/远端库存/时间 |
| 6 | 230323 | OptionUse |
| 7 | 230319–230331 | OptionEquip，slot 0，remote=true |
| 8 | 230313–230331 | OptionEquip，slot 1，remote=true |
| 9 | 230062–230082 | OptionPurchase，物品逻辑标识+数量 |
| 10 / 0xA | 230087 | OnResume，结束浏览 |

这里没有发现“滚动每一像素”的对应 generic packet，不能把 6/7/8 称为滚动同步包。要复刻对方滑动的视觉，应进一步查看 CMenuMovieControl 或控制同步代码；本研究只确认已消费的选择、购买、装备、退出命令。

`UpdateCancelButton`（185467）使用 `CMultiplayerMgr::AdjustTimeStep` 扣减倒数，弹窗打开仍会调用它（186598–186601）。主动方到零时隐藏 popup 并 `OnResume`；旁观方不自行到零结束，等待主动方结束同步（185516–185524）。

`OnResume`（184975）：仅主动方发送 packet 0xA（184992–184996），双方中任一仍活着就返回战斗状态 8，否则进入死亡状态 4（184998–185007）。它不是固定回到生存状态。通过 `SetAnimation` 和原完成回调恢复，不应直接跳帧。

### Deathmatch 差异

- GameType3 普通浏览无 10 秒硬限制；死后浏览限制来自 `level+316556−1000`（90352–90356、90255–90261），不是 GameType2 的 10000。
- GameType3 不设置合作暂停标志（186490），并有专用武器/冷却逻辑；`UpdateMPMatchCoolDownTimers` 仅在 GameType3 每帧调用（91344–91346）。
- 因此“所有多人统一10秒暂停”不是此份 iOS 3.6.0 的已确认规则。若用户要求未来 DM 也统一10秒，这是显式玩法变更，应与原版分开记录。

## 实现验收建议

1. 进入 Live 时真实读取多人背景 keyset 与原 Movie，文字来自对应 StringPack；检查章节和资源加载动作。
2. 一波内双方各击杀并共同攻击敌人；统计面板显示两人的本波数值、死亡和累计 XP，15 秒统一倒数结束后才推进下一波。
3. 主动方打开商店，另一端进入只读镜像；显示主动方名字、库存、货币、倒数，战斗暂停；购买/使用/装备仅作用于主动方账户。
4. 提前关闭及超时均同步恢复；双方已死时走死亡后续，不能被关闭商店复活。
5. 最终结算采用 menu24 的双列项、分类页与 replay 状态；将 3025 ms 只视作 Movie 动画，页面等用户操作。
6. 用 GameType3 验证没有意外继承合作暂停、助攻列和固定10秒。本文仍未覆盖 DM 战斗规则，也未声称已复刻以上流程。

## 补证：最终 overview 双列的实际组装

原 ARMv7 静态结构 `MENU_POST_GAME_WRAPUP_MP`（VA `0x4033F0`）直接读取 `_prep/gunbros`：主 Movie=`GLU_MOVIE_WRAPUP_SCREEN_MP`，scroll Movie=`GLU_MOVIE_WRAPUP_MENU_SCROLL`，value provider=94；option 配置从结构的第9个 DWORD 开始：Movie=`GLU_MOVIE_WRAPUP_BOX`、factory=11（`CMenuPostGameOption`）、标题/图标 provider=97。单人结构 VA `0x403350` 使用93/96，DM VA `0x403490` 使用95/98。这是原可执行文件静态结构，不是从重建实现反推。

`GetElementValueInt32`（150821）对94/95返回2组，对93返回1组。`GetElementCount`（153806）对94的元素0返回6行，元素1返回9。`CMenuPostGame::Bind`（165755–165790）把每组所有选项的 contentId 改为组索引0/1，把末行 elementId 改为 `min(otherCount, MissionType + otherCount - 4)`。因此 MissionType1 的 Live overview 两列分别取玩家0/1，行 elementId 为 `[0,1,2,3,4,6]`：击杀、助攻、死亡、Xplodium、经验、Perfect Waves。MissionType2末项改为7（Best Streak）；MissionType0末项为5，需保留其与 provider94 的 experienceGift 分支关系，不能将该非正式组合任意改成 objectives 数值。

`CMenuPostGame::OverviewCallback`（164593）：双组分支164637–164646对左右两组使用**同一个 row index**。左卡位置=(row.x,row.y)，右卡位置=(row.x+row.width−card.width,row.y)。仅单组分支把一行拆成同组的 `2*row` 和 `2*row+1`；不要把单人索引公式套入 Live。

### 卡片 region 和字体（已复核易错处）

`GLU_MOVIE_WRAPUP_BOX`：core ordinal20、handle `0x030004BB`，物理文件 `0xf4e02223/pack0_core_xga_0103_0x2a3d62.bin`，1500ms，无文件章节。

| Region | 原矩形 x/y/w/h | 内容 | 字体 |
|---|---|---|---|
| 0 | 0/0/370/128 | 卡片边界 | 无 |
| 1 | 10/6/80/80 | 图标及粒子，取region中心 | 无 |
| 2 | 10/88/350/34 | **标题** | **font0** |
| 3 | 108/10/240/50 | **数值** | **font6** |

证据闭环：`CMenuPostGameOption::Bind`（249937）将 value provider94字符串存 mem+68，将标题provider97字符串存 mem+64；`Draw`（249820）region2读 mem+64、字体mem+72；249848 region3读mem+68、字体mem+12；`SetFont`（249715）slot1写mem+72、slot0写mem+12；`CMenuPostGame::Bind`（165782–165787）font6绑定slot0、font0绑定slot1。将 region2当数值、region3当标题会反转原布局。

### 滚动 Movie 的章节、行回调和移动量

`GLU_MOVIE_WRAPUP_MENU_SCROLL`：core ordinal115、handle `0x0300051A`，物理文件 `0xf4e02223/pack0_core_xga_0198_0x2a995c.bin`，1500ms；文件章节[500,800]，运行时章节[0,500,800]。

| Region index / type | 矩形大小 | time0位置 | time500位置 | 后续关键帧 |
|---|---|---|---|---|
| 0 / 1 | 800×360 | −10,−22 | 同前 | 裁切/触控范围 |
| 1 / 2 | 780×130 | 0,12 | 0,4 | time700:0,−155 |
| 2 / 3 | 780×130 | 0,86 | 0,146 | time800:0,4 |
| 3 / 4 | 780×130 | 0,160 | 0,334 | time800:0,146 |

行region的 time0 使用 parent=254、parent_anchor=7；后续关键帧使用 parent=255、parent_anchor=0。应由原 Movie 引擎解算父锚，不能把 time0局部坐标直接当屏幕位置。

`CMenuMovieControl::Init`（142239）对所有 region.type>=2 绑定 `OptionCallback`，回调最后参数=1（142267），令 `CMovieRegion::Draw`（110062）传 **type/tag**，而非 region ordinal。非过渡状态的行索引为 `control.mem28 + type - 2`（140902）；mem32非零时另按mem92方向增减1（140903–140908）。当前这份 Movie 恰好 type=index+1，因此可写成 `scrollOffset+region.index-1`，但资源引擎本身必须使用type。

`CalculateBaseVelocity`（141749）在chapter1/2取所有行region的中心Y差，将非零差求平均、除以300ms换算速度；它不是从卡片高度硬填步长。上述三行原Y差为159、142、188，整型平均163；具体帧间移动仍由Movie关键帧插值负责。父锚、章节与动态回调索引都应保留，不能只画静态两行后按手写142px滚动。

## 补证：每波姓名和头像

`OverlayNamePlayer`（87326）与 `OverlayNameBrother`（87293）调用字符串 `OverlayDraw` 的第二参数=1；实际 `CFontMgr::GetFont`（86566）使用该参数。因此姓名为 **font1**。反编译首参数显示 `(CInputPad*)3` 是该静态辅助函数被误识别的伪this，不能解释为font3。同理数字辅助函数（87087）的伪this=2不能解释为font2；数值回调传入字体参数0。

头像不依赖 GameCenter 图片。`CInputPad::Bind`（91125–91137）直接用 core `SpriteGlu` archetype0，玩家选择字段 `CGunBros+409` 为0取animation161、非0取162；兄弟字段 `+541` 为0取163、非0取164。`OverlayIconPlayer`（86506）/`OverlayIconBrother`（86496）直接绘制 inputPad+9240/+9292 的sprite，于region左上角绘制，sprite自带原点，不额外居中。

按 `sprite_archetype.bt` 核对原物理文件 `0xf4e02223/pack0_core_xga_0405_0x1b2a966.bin`（logical1534）：292个sprite、292个frame、193个animation。animation161/162/163/164记录偏移12472/12478/12484/12490，分别唯一引用frame209/210/211/212、duration10Ms=10（100ms）、byte13Raw=1。无GameCenter头像的本地bot应走这些原角色图标，不能凭空加入替代头像资源。

## 补证：复活进度条没有 Movie region

`CLevel::DrawReviveBar`（120152）直接调用原生 `DrawRect`/`FillRect`，未读取HUD Movie或region。检查另一兄弟及死亡/可救援状态后，取救援目标的世界bounds：宽=`trunc(30*cameraScale)`，高=`trunc(4*cameraScale)`，x位于目标bounds中心、y取bounds.y，然后调用Camera屏幕坐标转换（120199–120235）。边框颜色`0xFF7F8C98`，内边距=`trunc(cameraScale)`，填充宽度=`(width−2*padding)*progress`，高度=`height−2*padding`，填充色经`Utility::Brighten(0xFF64B6FD,0)`。无有效目标时清progress（120237）。这些是原代码常量而非丢失的BIG布局；不应寻找或伪造一个“复活HUD region”。

## 补证：最终结算重开按钮

真正控件表是 `MDS_BUTTON_POSTGAME_REPLAY_MP`（原VA `0x41A430`），不是只有头像文字的 `MDS_POSTGAME_REPLAY_MP`。`CMenuPostGame::Bind`（165653–165680）以provider142元素0/1创建两个 `CMenuMovieButton`（mem+492/+572），都放在主Movie最后一个region（MP region7）的中心，字体1。

| 表元素 | 文本alias | Movie alias | action/parameter |
|---|---|---|---|
| 0 | IDS_WRAPUP_MP_REPLAY1 | GLU_MOVIE_REMATCH_BUTTON | 136/0 |
| 1 | IDS_WRAPUP_MP_REPLAY2 | GLU_MOVIE_REMATCH_BUTTON_DISABLED | 193/0 |

正常资源core ordinal120、handle `0x0300051F`、物理 `0xf4e02223/pack0_core_xga_0203_0x2aa168.bin`，888B；disabled为ordinal121、handle `0x03000520`、物理 `...0204_0x2aa26d.bin`，957B。两者2400ms，2个region：region0是100×100按钮范围（time0 x/y=−50/−50；time400 x/y=−48/−50），region1文字95×22（time0 x/y=3/52；time400=−68/52）。实际图形由各Movie的Sprite对象绘制。

正常Movie文件chapters=[400,900,1399,1899]；disabled=[400,900,1400,1900]，两者均另有隐式运行时chapter0。不能拿2400ms当点击延时，也不能把主region7直接当文字布局。

`UpdateMultiplayer`（165096–165162）每帧取provider100 item0/content1：返回>=0选择normal，负值选择disabled；连接可用性变化时先隐藏normal，等待其退出后显示disabled。`DrawOverlay`（165436）两者都调用Draw，可见性由按钮生命周期控制。此disabled皮肤用于对方连接状态，不等同于“点击重开后进入等待”的状态替代。重开握手的进一步入口是action136，本文仅确认控件资源和绑定。

## 补证：远端商店姓名与倒数的 ARM 绘制参数

`CPowerUpSelector::DrawPlayerNameAndTimer`（185362，ARM VA `0x10679C`）由主Movie `GLU_MOVIE_POWERUP_MENU_NEW` 的 **region2** 回调（186989–186994）调用。门槛为 remainingMs（selector+5152）>=0，selector状态（+4972）为1..3。以下将除2定义为原有符号整型除法，正常字体尺寸为非负数。

令原region为 `(x,y,w,h)`，`cx=x+w/2`、`cy=y+h/2`；`nameWidth/nameHeight` 来自font5的MeasureString/GetHeight，`timerWidth/timerHeight`来自font11，`zeroWidth`来自font11的GetCharWidth(`'0'`)：

| 模式/内容 | 字体 | 绘制左上角 |
|---|---|---|
| 远端店主姓名（selector+5168） | 5 | `(cx−nameWidth/2, cy−nameHeight/2)` |
| 远端店主倒数 | 11 | `(cx+nameWidth/2+zeroWidth−timerWidth/2, cy−timerHeight/2)` |
| 本地打开，无额外姓名 | 11 | `(cx−timerWidth/2, cy−timerHeight/2)` |

两者在同一行以各自字体高度居中。原版先让**姓名自身居中**，然后在其右侧画倒数，并未把姓名和倒数组成整体再居中，也未让倒数移到下一行。准确横向关系是倒数中心在 `cx+nameWidth/2+zeroWidth`；不是固定像素间距。字体布局仍由BIG字体资源提供。

原反编译漏掉了两个关键副作用：ARM `0x106980 add r4,r4,r5,asr #1` 把当前横向中心推进半个姓名宽；`0x106AC4 add r4,r0,r4` 再推进font11数字0宽；`0x106B14 sub r3,r4,r1,asr #1` 才求倒数左缘。`0x106B04`算倒数Y并写栈首参；末次DrawString完整参数为 `(font11,timerString,-1,timerX,timerY,-1,-1,currentColor)`。

文本格式原VA `0x3C4A5C`的12字节=`25 00 00 00 64 00 00 00 00 00 00 00`，即原32位宽字符 **`%d`**，没有括号、冒号、s或前导零。`0x106A30`先加500，随后有符号乘法除1000（0x106A40–0x106A48），所以显示值为 **`trunc((remainingMs+500)/1000)`**，并非向上取整：9500ms显示10、9499ms显示9、499ms显示0。字符串缓冲区为4个原wchar。

核对方法：仅从 `_prep/gunbros` ARMv7 Mach-O切片读取 `0x10679C..0x106BD0`，使用Capstone 5.0.9反汇编；工具依赖位于忽略的 `_prep/out/research-python`，未修改游戏代码与资源。

## 补证：Live 对 BOKOR（MissionType2）的菜单限制

已追通的原菜单链**没有GameType2专门禁用MissionType2的规则**。BOKOR仍须通过正常等级、脚本与存档前置条件；不能把这项结论当作默认解锁。

- `CMenuMissionOption::Bind`（190865–190876）通过配置的provider135创建Play按钮，并按按钮action是否193设置可操作标志。
- 原 `MDS_BUTTON_PLAY`（VA `0x4180C0`）只有1条：`IDS_MISSION_PLAY`、`GLU_MOVIE_ENLIST_OFFLINE_BUTTON`、action23/parameter0。Movie alias中的OFFLINE字样本身不构成模式限制，最终动作按GameType分派。
- `CMenuDataProvider::GetElementAction`（153611–153633）在provider135分支读取对应Mission，只有 `Mission::IsLocked()` 或 **missionType==1** 时将action改193，然后将实际Mission索引写回parameter。ARM `0xCED74`调用IsLocked，`0xCED80`读取Mission+0x3C、`0xCED84 cmp r0,#1`，确认比较值为1而非2。type1的卡片直Play禁用应与其波次选择按钮分开理解，不能反推type2不支持多人。
- `Mission::IsLocked`（164280–164364）执行Mission脚本export2，按玩家等级、前置波次、商品状态判断锁定；此函数未因GameType2拒绝type2。
- action23（93956起）在GameType>=2且未触发原单人覆盖条件时走多人匹配。94050–94069将所选Mission ID、参数与当前GameType直接写入匹配数据；该路径不拒绝type2，也不把它自动替换为type1。只有GameType3额外选择随机MP match ID（94064–94065）。

因此当前重建不应额外添加“Live禁用BOKOR”分支。此结论覆盖所检查的客户端菜单与发起匹配逻辑；已停服的真实匹配服务器是否有另外限制无法由本地二进制证明。

### BOKOR/Live 得分与波间结算差异

`CLevel::GetScore`（114268）返回level+303604。这里 **303604=`0x4A1F4`**，所以 `OnEnemyKilled`（119660）的十进制成员地址与119787/119793的`loc_4A1F4`是**同一字段**，不能拆成“本地分数”和“对方分数”。ARM GetScore `0x8C070..0x8C07C` 用 `movw #0xA1F4; movt #4; ldr`；OnEnemyKilled三条路径 `0x94DC0`、`0x94DDC`、`0x94E34`也都组成同一个偏移，已核对。

该分数字段累计双方符合原OnEnemyKilled条件的击杀贡献，但不是两份独立同权分数直接相加。以原局部变量描述，GameType2的 `!v149` 分支（119656–119665）加 `(streak+1)*XP`；`v149`分支（119786–119804）先给本地经验，再加 `2*XP*(streak+1)`，随后streak自增，触发HUD倍率变化。数值按原上限钳制（119667–119670）。这与本地击杀双倍并推动本地连杀、其他击杀单倍的分派对应；不能因共享字段而把两条加权分支合并。

`CInputPad::OnScoreChange`（119750）收到该共享累计值；MissionType2同时用它更新原统计28（119754–119755）。结束时 `CGame` 的MissionType2分支从同一个 `GetScore` 取值写高分（76080–76095），所以HUD与最终高分口径一致，包含另一位玩家通过上述路径带来的贡献。

**BOKOR不显示Live普通生存的15秒每波统计面板。** `OnWaveCleared`（116994–117001）严格要求GameType2、MissionType1、非最后波才置15000ms并传两份统计指针。MissionType2进入else（117003–117007），调用普通OnWaveCleared并传两个null统计指针。不要因通用 `WAVE_WRAPUP` 回调存在type2文字分支就倒推该调用路径会显示面板；最终多人overview的第六项仍按前述MissionType2规则选Best Streak。
