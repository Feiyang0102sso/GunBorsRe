# 2026-09-09 八小时原版 UI 与存档复刻

## 授权与边界

本轮由用户明确授权自主决定产品和技术细节，时间为 2026-09-09 05:29:20–13:29:20 UTC（用户本机 00:29–08:29）。不延长历史阶段授权，不把旧验收当作 1:1 完成。后台续跑已创建，自动化 ID 为 `ui`，截止后暂停。

原 `big/`、`big_360_out/`、`saves/`、`gunbros` 只读；资源和布局取 BIG，语义与原算法同时查 BT、iOS 读取器和消费者。无法证实的内容保留未知。所有运行验证 `--mute`，使用隔离账户。起始 Git 工作区干净。

## 本轮交付与后续入口

- 启动：`bin/x64/Release/gun_bros_re.exe`，Windows GUI；`gun_bros_research.exe`独立永久研究入口1–68。正式账户为`userdata/saves/`，源`saves/`只读导入；无源按原构造及BIG建立。
- 主要完成：商店人物投影／裁剪／原换枪Flow、熟练度和升级六态、Bank、原导航与逐页菜单、签到／选兄弟／教程、精炼、结算、暂停、HUD通知、战斗道具、原格式存档读写和游戏／研究程序分离。资源入口与原函数证据在下方各阶段。
- 可直接验收：[17组原图／重建图](../out/ui-original-2026-09-09/review.html)、[76款BIG武器研究图库](../out/ui-original-2026-09-09/weapon-reference.html)、[人工验收步骤](acceptance.md)。同枪首组为Ion Blaze，pack5:31；不得用其他枪的大小推定它的比例。
- 验证：Release OriginalUI 18/18、Core20/20符合预期（含PROP43已知原异常）；最新完整商店与76枪实拍`final-gallery-check.log`退出0。Release／Debug双EXE构建通过，实际GUI独立启动及系统菜单启动研究工具通过。原21份存档最终SHA无变化，无遗留测试进程。
- 边界：不是完整1:1。下一阶段优先用同Ion Blaze、同兄弟／盔甲和相同原动作帧量化差异；随后补原列表惯性／回弹、所有退场和粒子，接战斗道具“购买更多→Bank→返回”及复活效果。真实联网／多人和所有存档客户端业务尚未恢复。具体原资源未知保持原值，不靠截图补表。
- 当前为开发构建，仍从项目原资源根读BIG，未制作独立安装包。改动留工作区待用户验收；原资源与存档未提交版本库。最终二进制SHA见`out/ui-original-2026-09-09/delivery-binaries.json`，验证汇总见同目录`delivery-verification.log`。

## 阶段方案与验收

1. 商店人物与熟练度：先保存现状截图，查 `CMenuMeshPlayer`、`CMenuStore::PlayerMeshCallback`、`CMenuStoreOption` 及 Movie/Sprite BT。用实际绘制和资源变异检查复现比例、裁剪和时间轴问题，再逐项修复；不按截图补相机数字。
2. 存档：核对所有客户端的读写与默认构造，正式启动读取 `saves/` 原数据，写入独立可写存档；不存在源目录时按原构造语义创建对应记录，验证重载和原件哈希。
3. Bank 与逐页审计：建立实际调用路径和证据清单，按原资源及菜单绑定逐页恢复；去除假支付、假奖励、假联网状态进入正式经济的路径。
4. GUI 与工具：Windows GUI 主程序，里程碑保留独立工具入口；拆除主游戏对 milestone 实现的依赖。验证无控制台启动、工具菜单兼容、命令行测试日志与退出码。
5. 相关菜单、Movie、存档与实战回归，立即更新 PLAN 已验证的细分项，截止时交接完成与未完成内容。

阶段顺序可按依赖调整；不把成功构建当作行为验收。

## 当前进度

- 已阅读 PLAN、项目约定、商店商品卡历史证据、验收与夜间交接；现有 UI4.2 明确未完成相机、裁剪和完整熟练度/Bank。
- 基线命令：`bin/x64/Release/gun_bros_re.exe --store-card-check --mute`，日志 `out/ui-original-2026-09-09/baseline-store.log`，退出0；该旧检查未能发现人物被裁剪。
- 商店人物修复已通过：基线旧检查退出0但头顶被切平。新增实际 GL viewport/MVP 检查后退出1，原点 `(789.173,734.044)` 对原公式 `(806,713)`、比例 `5.998` 对 `5.573`，scissor=1。修复后同检查 match=1、scissor=0，完整三类商品卡检查退出0。命令均为 `--store-card-check --mute`；日志 `player-red-check.log`、`player-check.log`，构建 `player-build.log`，均位于本轮 out 目录。
- 原依据：`CMenuStore::Bind :180082` 从 STORE_MENU 区域2取 bounds；`CMenuMeshPlayer::Draw :169571` → `CBrother::DrawUI :136337` 用活动躯干 mesh `maxZ-minZ` 缩放；`CMeshCamera::OrientForUI :98863` 全页像素投影，90° X/180° Z、z=-500；`CGraphics2d_OGLES::SetWidthAndHeightMappedOrthoProjection :378817` 深度0..32767。重建 `BuildPlayerUIMatrix`，不以联合包围盒或手写 cameraWidth 定标。读取器未改，已复核 mesh.bt/ui_movie.bt；现有独立装备研究页的旧相机尚保留，不代表所有模型页完成。
- 基线图片 `out/ui-original-2026-09-09/baseline-guns-open.png`，修复后 `out/store-card-guns-open.png` 已目视检查，头顶完整、武器不再被侧栏切断。新增两位兄弟 × 七类真实商品枪（含 Dual Spread）与区域位置/宽高变异，14次实际GL投影均match=1；三类商品卡完整检查退出0，日志player-variants-check.log。

## 进行中：升级弹窗

- 已确认旧 `DrawMastery` 固定主 Movie 在750ms，自设900ms填充；原程序是0开场、1待机、2升级、3闪光、4关闭、5结束六态，主/子 Movie 1×更新；开场时星条不推进，升级从当前值继续，满级闪光后关闭。
- 原 `SetStarsPlaybackTime :393038` 用当前等级章节 `end-start-1`；`UpdateInfoStat :392886` 的 NEXT 列显示百分比变化，仅非零变化项目参与布局，暴击行独立。旧实现显示全部绝对值且字体/偏移自设，待替换。
- 已新增 `gun_bros/CMenuUpgradePopup.h/.cpp` 六态播放控制，已接入真实DrawMastery。按当前MovieRegion、原字体与MDS按钮绑定；NEXT显示百分比。新增入口53 / --upgrade-popup-check，13阶段截图、章节长度变异、开场/升级防重复扣款、银金两级、满级闪光后自动关闭与磁盘重载均通过。日志upgrade-full-check.log，构建player-variants-build.log均退出0。

- 时间边界补证：CMovie::Update :109055–109262仅在时间超过chapter.end时完成，待机循环保留超出量；不是到end立刻结束。已测试变异开场到end仍Opening，下一毫秒Ready。
- 层级补证：CMenuSystem::Draw :96761先底层菜单，再DrawBetweenMenuAndHud暗幕，再HUD，最后升级弹窗；Update :96904消耗底层输入。正常商店/实战/结算/精炼回归menu-check.log、最终通用背景menu-background-check.log均退出0。
- Movie回归：--movie-check --mute，175个Movie退出0，movie-check.log。未声称通用Movie/Sprite所有边界已1:1；商店卡内熟练度Movie91、swap和不足货币弹窗仍须分别核对。

## 存档：发现与已实施修复

- 旧ImportOriginalProfile只读5类记录并硬设tutorialCompleted=true，忽略1001角色/枪槽、1007教程、1008精炼、1009签到等。默认游戏还使用独立文本profile.dat；这不满足本轮要求。
- 原CPlayerConfiguration::Reset :171865第二枪是pack5 ordinal4，第二弹种45；旧SetDefaults把两枪都填core ordinal0。属于原生默认选择，必须从对应BIG引用加载并校验，不沿用错误宿主初始表。
- 原CProfileManager::GetDataStoreFileName :203008组成clientId_storeId；LoadFromDisk :203429不选.perfect扩展。旧导入器优先1000.perfect/1003.perfect无原依据，正式读取应按标准文件名。
- 已保存saves原件SHA256基线 out/ui-original-2026-09-09/source-saves-sha256.csv。正式方案：只读导入saves到独立userdata/saves原格式记录，后续读写该目录；没有源目录时按已核对原构造及BIG创建同种记录。保留未知原字段，不能生成伪值填掉。旧研究文本存档仅保留兼容研究入口，不覆盖用户旧文件。

- 已实现runtime/NativeProfile：18个已注册DataStore（1000..1018，排除1015）及`-1_PDST`；保留未解释字段，按原512字节封装、前后padding、owner与CRC32/BZIP2写出。未改payload的记录保留完整原文件字节。每文件临时写入后原子替换；原格式不支持跨全部客户端的事务。
- 默认正式启动使用`userdata/saves/`，首次只读导入`saves/`标准文件名；没有源目录时按原构造、BIG及AcquireDefaultGear创建。`--profile <目录>`可隔离账户，显式`.dat`仍作为旧研究兼容路径。未读取`.perfect`替代标准存档。原目录写保护；21个源文件SHA256再次核对，Changed=0。
- 已投影并写回：原货币/XP/等级、两枪和盔甲、活动枪槽和兄弟、库存及强化数量、50×10波进度/完美位图、22个教程提示、12槽精炼原float/毫秒/秒字段、签到3字段、47统计、武器熟练度；其余客户端保留原payload。进度映射按BIG Planet.mapSlot排序且Mission.type=1→LEVEL引用，不能拿存档LEVEL引用套旧地图硬编码。
- 首次启动标志不等于教程完成。CMenuAction::DoAction case0x4E :94882在选择兄弟时清除firstLaunch；教程native72只修改关卡教程步。原CGameFlow::OnMissionSuccess/Failure在Mach-O中是空函数（0x4a610/0x4a614，BX LR），不能从缺少反编译正文虚构结算逻辑。
- 签到按CDailyBonusTracking :209284/:209316的秒数累计：不足172800秒延续，达到该间隔重置；86400秒为一天。Windows UTC时钟是明确的离线适配，未冒充原网络授时。菜单进入时RefreshUsageData，即使不领取也记录访问；签到物品走原AcquireItem award分支，禁止改写资源商品价格。付费物品购买接原统计10/11/12。
- 初始CContentTracker根据BIG各包Armor/Gun/Planet/Powerup计数创建已阅位图，AcquireDefaultGear仅标原商品第一对象；已有原位图保留。
- 新入口54 / `--native-profile-check --mute`：18记录原字节往返、奇数payload、CRC损坏拒绝、语义修改/重载、未知字段保留、缺少源目录初始化/选兄弟持久化、签到时间边界及不领取访问连续性均通过。日志native-storage-check、native-semantic-check、native-launch-check、native-daily-check、native-content-check均退出0。
- 实际原账户商店截图native-store.png已生成并核对，读取原普通存档的余额、装备及第二枪槽。新UI模型回归覆盖这套实际装备；不能拿旧文本账户的实战回归代替原格式实战验收，该项待补。

## 菜单模型追加审计

- 导入真实装备后发现大枪遮挡头部，不能直接凭截图缩小它。已追查CMenuMeshPlayer::BindPlayer :169300→CBrother::SpawnForUI :135894，原路径调用Flow export9，旧实现误用战斗export1。新增SpawnForUI及UpdateUI，UI腿部按原前一时刻躯干相位同步；实际状态已从1切到17，交换经过18。
- 原CMesh::Init :97965按CMoveSetMesh::IsFrameUsedInMoves保留帧时间、跳过未使用几何/骨骼，ComputeBounds取第一个保留帧。读取器已接入原过滤条件，播放器及武器MoveSet传入；缓冲创建选第一个有效帧，防止空顶点缓存。mesh.bt已复核；不改原字节布局。
- native-ui-spawn-check完整商店卡回归退出0；过滤帧之后的native-mesh-store-check仍在运行。真实大枪遮挡是否还涉及其他原流程需继续查证，现不宣称全部装备视觉1:1。取样截图native-model-{slot}-{phase}.png、native-store-ranges.png。

## 07:25 UTC 阶段验证与下一步

- native-mesh-store-check完整商店卡回归已退出0，修正上文运行中状态。大枪遮头原因仍未完全证实，不能凭截图缩放武器。
- GUI完成主/研究EXE分离：gun_bros_re.exe为Windows子系统2、wWinMain；gun_bros_research.exe为Console子系统3，旧里程碑及新53–55入口永久保留。独立obj目录，共享源清单。
- 地图/实战主体完整移动runtime/MapScene，EnemyModel移动runtime，旧M3Map研究入口薄包装保留，原注释未删。研究SurvivalPilot通过可选ISurvivalInputDriver注入，正式不链接它，不再创建性能测试导航网格。runtime/gun_bros不再包含milestones头文件。
- 主窗口系统菜单（Alt+Space／右键标题栏）有Research tools；--research显式打开工具，研究参数转发保留退出码和日志。Explorer无控制台，日志userdata/logs/game-PID.log。修复最初__argv崩溃，Windows入口由__wargv转UTF-8。
- 仅主EXE和SDL3.dll、不带研究EXE的隔离运行成功：gui-standalone.png、gui-standalone.log，GameExit=0、ToolsPresent=False、console=0。GUI转发native-profile-check退出0；完整菜单/实战/结算/精炼gui-runtime-menu-check退出0。系统菜单尚未实际点击验收。
- SurvivalSession移除script.states>100猜测；正式按Planet→Mission.type1→LEVEL→mapRef加载，并显式区分archive研究。无指定LEVEL时也按Mission类型/地图匹配，歧义报错。正式四生存关卡不再使用kPlanetMaps，教程/BOKOR/星图仍待核对。
- 新入口55 / --native-profile-play-check --mute：真实普通存档装备及当前枪槽，每星两波后写回重载；native-four-planets-check.log退出0，四星均有击杀且进度2波。余额、双枪、盔甲、兄弟、枪槽、库存均保持。
- 商店旋转正在按CMenuMesh::HandleTouchInput :168943 / UpdateRotation :168846恢复：按拖动起点与区域宽度计算角度，松手deltaMs>>1度回零；替换旧dragX*.012累加。原Movie区域作为输入，不增设布局表。
- Bank下一步查原STORE货币分类及MDS。UI_sample/ui.md保留用户早前要求内购等待3–5秒后本地到账，因此保留明确的Windows离线交易适配；金额/商品必须来自BIG，不能编造实际支付状态或价格。上文去假支付指去除虚构数据及冒充真实付款，不取消已授权的本地演示。

## 07:58 UTC：Bank 与商店补验

- `extract_menu_statics.py` 更正原菜单表动作读取：GetElementAction :153502 使用 table + index×64 + 8/+12；原提取器错误读取了下一条记录的动作。已从只读 Mach-O 重新生成 113 张表，Bank 四筛选参数为17/14/15/16，不再猜按钮含义。
- Bank 接入原 STORE_MENU／STORE_SCROLL／SHOP_BOX／SORT_BAR，货币类14–16没有objects引用，也没有可展开的cost string。卡片原图、名称、数量与方向来自BIG。按LevelCallback :180839、Focus :181402放BUY/CONVERT，取消旧自制Bank列表和确认页。
- 新 `CMenuPopupPrompt` 使用GLU_MOVIE_POPUP章节；按BindContent :207403、ContentCallback :206498及原字体0/0/1/5计算内容高度、开合目标、视觉显示时机。已支持IAP等待和两种兑换不足提示、点击关闭；非真实支付，4秒为用户UI_sample明确授权的Windows离线适配。
- 新研究入口56／`--bank-check --mute`，25阶段覆盖等待防重复与导航门禁、两个兑换方向、余额不足、关闭动画、修改内存测试副本金额以及全部16商品实际点击与原格式存档重载。`bank-all-items-check.log`退出0。先前逐商品测试暴露第二行BUY不响应，原因是input仍限制在STORE_MENU内容框；现输入与绘制共同使用原STORE_SCROLL viewport，回归通过。
- 失败提示截图bank-phase-7.png与Bank全商品截图bank-phase-9..24.png已生成；7号失败提示已目视。大枪遮头仍保留待查，不凭截图缩小。
- `CMenuMesh`触摸旋转按原168846/168943实现，区域内开始捕获、离开区域继续、松手deltaMs>>1回正；model-rotation-check退出0。
- 卡内熟练度修正为GetElementValueInt32 :151132的99比例，避免未到阈值提前推进；三颗星按CreateContentSprite :150114固定初始帧，动画由Movie91区域负责。210个真实枪等级边界及章节变异检查通过。
- `--store-template-check --mute`整体回归（筛选动画与命中、210熟练度边界、原投影与两兄弟七类枪、原账户装备、三类商品展开/预览/购买/重载）退出0，日志bank-masterymeter-store-regression.log。
- 尚未完成升级不足币的动态文字与原两按钮弹窗、全部其他UI页面、原模型所有姿态及通用Movie/Sprite全部细节。下一阶段继续逐消费者复核，不能把本阶段通过当作全UI 1:1。

## 08:22 UTC：升级不足币与换枪

- 升级失败使用原 MDS_STORE_PROMPT_MOMONEY 和 MDS_BUTTON_STORE_INGAME_PROMPT 两按钮：显示原币符号、总价和差额，DISMISS不改余额，BUY选择原CacheLowestAppropriateIAPItem对应BIG商品。4秒离线到账后再次升级，扣款和熟练度保存均只执行一次。
- Capstone只读原Mach-O复核GetLastFailPurchaseInfo :156610的SWPrint参数及BindStandardPopupButtons :206290的mode1按钮顺序，补反编译遗漏。工具src/tools/disassemble_original.py，输出fail-purchase-arm.txt与prompt-buttons-arm.txt。
- 原ShowForGuns :394113及GunSwapButtonCallback :392767支持商店入口切换另一把未满级装备枪。切换重置星条，不重播弹窗开场，不改变实际装备及余额。测试先发现子Movie原点重复叠加，再发现原账户第二枪本来已满级；改为明确的隔离未满级测试数据后通过。
- `--upgrade-popup-check --mute`退出0，13主阶段、银金升级、15不足币阶段、换枪与原格式重载通过，日志upgrade-swap-fixed-check.log。卡内Movie91已在上一阶段通过；普通商店不足币的三按钮原提示、完整按钮状态动画与通用弹窗排队仍待恢复。
- 下一阶段方案：核对CMenuList/CMenuOptionGroup和原配置存储，按原Movie区域、MDS和字符串重建设置页；验收原选项顺序、滚动/选择、动态文字、开关与保存重载。同步逐项核对星图和活动页面，移除正式流程的虚构奖励。

- 08:24 UTC：完整 --game-menu-check --mute 退出0，Bank首次使用原Movie区域计算BUY测试坐标，修正旧测试380位置未命中现卡片的问题。主流程实战、结算、精炼、购买装备、余额保存通过；活动与设置的旧模拟断言只是回归基线，下一阶段仍须替换。
- 新COptionsMgr恢复独立p文件（CRC+32字节），原默认开关、AutoBro三态、未知字节保留与损坏拒绝。--native-profile-check退出0，日志native-options-check.log；无原p实物样本，按原读写和构造验证，不声称已与实物p比对。

## 08:36 UTC：设置页与独立 p 配置

- 原MENU_OPTIONS地址0x402e50：LIST_MENU/列表偏移2/首尾边界1、1/LIST_MENU_BUTTON/LIST_MENU_TEXT；MDS_OPTIONS十项。移除旧11项手写文字、假QUIT条目、手填逐行位置、标题和描述，按CMenuList :140175、CMenuListOption :144029及CreateContentString :151507绑定。根OPTIONS没有BACK（原menuId193）。
- 动态SFX/MUSIC、AutoBro三态、通知、挑战推送来自原ResolveActionString :95788。通知是COptionsMgr mem+37，挑战推送是CPlayerProgress mem+86；保存各归其位，未伪造网络已开启或账号已登录。
- 独立p格式恢复为CRC32/BZIP2+32字节配置。COptionsMgr::Reset/Read/Write :51336–51458与ctor :51462为依据，新增options.bt；源saves未提供p，默认按原构造，Windows无iOS设备能力值则保留构造零。未知32字节字段往返保留。CRC异常明确拒绝并保留文件，没有静默替换原件。
- 原body包含标题与^f字体控制，按原font0/6排版，LIST_MENU_TEXT分页与MDS_SCROLLBARS原垂直条；GetTimestampString :52021可核实原构建名和时间，已加入ABOUT。缺少原bundle版本元数据与远端标识，不填伪值。
- 新永久研究入口57 / --options-check --mute，开场门禁、十项真实点击、五项偏好、子页动作、native保存重载、正文分页与滚动边界退出0，日志options-pagination-check.log。截图options-opening、options-entry-0..9、options-about-scroll/last-page；首屏和ABOUT末页已目视。
- 原列表惯性、越界回弹、自动聚焦运动仍待完整移植；目前位置沿原Movie逐帧读取，Windows滚轮按选项数输入。此项不是整个UI6完成。完整菜单回归在运行，旧活动奖励仍待下一阶段移除。

## 09:02 UTC：原离线社交页与活动奖励边界

- 设置页后完整 --game-menu-check --mute 已退出0，日志options-menu-regression.log。
- CMenuFriends::Bind :197028、CMenuChallenges::Bind :236612在原ProfileValid=false时直接使用OFFLINE_BROHOOD第1章，区域0绑定MDS_BUTTON_CONNECTIVITY，区域1绑定原两种font0居中文本；原Credentials.dat存在性决定描述，宿主isConnected开关不能冒充NGS身份。
- 正式native页4/5/11/13现走该原离线路径，移除正式入口中的LOCAL PREVIEW、硬填按钮和自编八活动。ClaimActivity在native边界拒绝；旧.dat研究兼容路径保留原注释与研究能力，不再污染正式账户。
- 新永久研究入口58 / --social-check --mute，八种页面/凭据组合、Movie原区域真实RETRY点击、循环边界、拒绝全部旧奖励和余额重载退出0，social-check.log；social-original-5-0.png已目视。
- CChallengeManager::InitProgressData :242162明确要求isValidNetworkTime，以(networkSeconds+36000)/86400换周期并BuildCurrentChallengeList。当前缺NGS有效时间与身份，不用本机日期造周期；1017原2114字节记录继续完整保留。挑战三档发奖本身在客户端HandleChallengeCompletion :240918实现，后续可恢复，不能直接断言所有奖励均服务器实现。
- 下一阶段：按CMenuMission和原Planet/Mission/Level引用重建星图、模式与轮/波选择；先核对Movie及原绑定，再实现并验证四颗正式星球入口。

## 09:10 UTC：原星图与模式选择第一阶段

- 原PLANET逐包枚举，按mapSlot绑定缩略图；前四个宿主进度槽从Mission.type=1及槽位排序产生。移除正式星图对包名、行星坐标、直径和视差系数表的依赖。Planet::CreateLargeImage实际使用thumbnail的包，已按原函数纠正。旧显式.dat研究图保留。
- MAP_PARALAX_COPY作为有边界时间轴，原拖动速度上限2、600px/s除数、惯性衰减、章节停点、750/800速度选框与标牌、350ms入场alpha、两次选择及reticle退场章节已接。动态行星按Movie区域的原图层插入，不能统一盖在星云前景上。
- CMovieEmptyRegion::GetMetricsAtTime :182335 / CalculateLocation :182636表明区域逻辑宽高不乘绘制scale，改变锚点时先求两端布局再插值；CMovieRegion::Draw :109978围绕区域中心做旋转/缩放。现区域回调在原层次执行并携带该变换，模式缩到右下角不再布局跳变。
- CMenuMovieMultiplayerOverlay使用原Movie140、MDS_BUTTON_MP_TOGGLE、font0和原章节开合；离线点LIVE/VS保留原模式，显示MDS_PROMPT_MP_UNAVAILABLE第2项。通用原提示新增无插图布局与条目索引，未手填提示正文。
- 新永久研究入口59 / --planet-menu-check --mute退出0：模式开合、两种离线拒绝、锚点中点/原175像素未缩命中框、四颗正式星球双次选择与reticle结束后进入、Planet-Mission-Level对应原存档均通过。planet-selected-0及mode-original-folding已目视。
- 此阶段尚未完成原等级提示、标牌全部动画、促销/好友倍率和content-new标记；选轮/选波页仍是下一阶段。旧整体菜单回归不代表新native星图专项。
- 区域变更后的设置专项region-options-regression.log退出0；商店专项仍在运行，随后补升级与175 Movie回归。

### 原任务选择阶段方案（09:27 UTC）

- 依据：`MENU_MISSION_DETAIL` VA0x403130；CMenuMissionInfo 188886–189726；CMenuMissionOption 189831–191158；对应 Movie48/49/50/65 的 BT 和原字节。
- 顺序：恢复 Mission export2 条件收集与原 provider25/26/27/135/136/137；再接 Movie 任务列表、卡片焦点/展开、波次区域和原文字；最后四星球及 BOKOR 逐项点击/边界回归。
- 验收：卡片坐标、尺寸与章节来自 BIG；任务标题/说明/要求和关卡引用来自 Mission；波次数量来自 LEVEL；解锁使用脚本需求和原存档进度；正式入口不得调用旧手绘卡片。
- 本阶段之前 region-options/store/upgrade/movie 回归均已退出0；175个Movie解析无失败。

### 09:50 UTC：原任务页、选波与 BOKOR 存档

- `CMissionScriptContext` 执行实际 Mission export2；native0/1/2按原 FunctionResolver 收集返回值、等级和资源前置条件。50个生存/BOKOR任务无未知native。IsLocked严格区分无脚本、LEVEL(type7)及STORE(type22)条件，不把所有runtime64直接当等级。
- 正式任务页使用原Movie48/49/50/65/67，按原Layer回调绘制；原字体、正文、轮次图、锁图、三标签、PLAY、50波、完美标记均来自BIG和原存档。卡片焦点125ms/4倍展开、首尾章节和分页姿态已接；惯性、手势随动、弹性和滚动条拖拽尚未完整恢复。
- 新研究入口60 / `--mission-menu-check --mute`：50卡片展开、2000波次按钮真实命中与传参、10个BOKOR PLAY、全新账户脚本锁定均退出0。这是UI逐项验证，不是2000场实战。最新日志mission-style-check.log；任务图已目视。
- 补回星球的RADIAL_WIDGET原独立Movie及chapter1循环。标签字体5来自CMenuOptionGroup::Init，选中发光是按钮chapter3；按钮内容改为type6原层回调，保留后续发光覆盖。原^fN文字字体前缀也由资源解析，不按标签ID补丁。
- 星球等级要求使用原flag51区域2/3和IDS_PLANET_REQUIRED_LVL。Planet/Mission/LEVEL目录与引用已用于正式启动，BOKOR不再靠pack11序号推断，教程也读取原LEVEL.mapRef。
- BOKOR按原CMissionWaveStatus和CMissionHighScore写1003/1016，后者值在磁盘record+10；死亡时按原OnMissionFailure→OnMissionSuccess记录高分。10个真实Mission引用的新记录、较低分保留、完美位图、原格式重载通过（native-horde-profile-check.log）。原saves无非空1016，测试明确使用隔离新记录。
- 实战使用原账户装备，四颗生存星球各两波及BOKOR首两波、原Mission引用高分21035重载退出0（native-horde-play-check.log）。源BOKOR已有wave500，因此继续补清除隔离副本单条LEVEL记录的回归，验证从空记录递增；原件不动。
- 下一阶段方案：根据CMenuNavigationBar::Init/各区域回调，移除全局Header的手写货币/等级坐标、经验条颜色、按钮拟合与自编入场曲线。验收原导航顺序、原字号、原章节、命中和新旧菜单回归；未恢复的服务器徽标不造数据。

### 10:20 UTC：原顶部导航完成，精炼阶段调研

- HEADER/INFO_CLUSTER/按钮按 CMenuNavigationBar 原区域回调、字体0/1/7、原经验色及 NAVBAR_MAIN 七枝顺序组合；开场、隐藏、重入场和选中动画取BIG章节。金币文字按原32位消费者显示，不用缩写、缩放或手填位置。
- 入口61 / --header-check --mute 验证七按钮、开场禁点、隐藏/重入场及内存资源区域变异；header-position-check.log退出0。升级/商店回归退出0，header-native-store.png已目视。未造NGS徽标，原大枪遮脸问题仍留证追踪。
- BOKOR清除隔离副本对应LEVEL记录后，实际两波创建wave2、高分21035并重载；四颗生存星球各两波仍通过，native-horde-empty-level-check.log退出0。原件未改。
- 精炼方案：依据MENU_GAME_RESOURCES VA0x402d50、CMenuGameResources 171900–174611、CRefinementManager 177731–178550和refinement_entry/ui_movie/sprite_archetype.bt，按原六区域绑定炉子、标题底板、状态/填充/锁动画、分类及侧栏。离线按原Enabled保留零时长炉子，不能用宿主联网开关伪造身份。
- 精炼验收：两分类原位置/文字、原chapter入场门禁、离线拒绝、375ms转移后开始、填满后单独领取、余额/原1008保存重载及资源区域变异；未验证粒子或联网分支明确记录。

### 10:28 UTC：精炼验收与签到方案

- 入口62 / --refinery-menu-check --mute通过两分类、十个离线炉子禁点、原区域变异、375ms转移/填充/单独领取、原1008/1000余额保存重载。日志refinery-clamp-check.log退出0；175 Movie回归退出0。
- MDS_ICON_STANDARD原金币引用4:43，BIG原型4只有33动画。回查CSpritePlayer::SetAnimation :58861确认原生钳到末动画32；MovieRenderer按通用消费者实现，保留原引用并记录日志，未按ID换图。锁住的彩色炉盖按Update :173929固定初帧，解锁前不播放。分类动作按DoAction :95106传条目序号，非静态parameter。
- 精炼尚缺原转移粒子、联网解锁/倒计时完整分支与所有退场联动；当前零时长离线闭环有来源和实测。未用已连接开关制造有效NGS账户。
- 签到方案：CMenuGreeting 207809–208747、CDailyBonusTracking 209284–209595、WELCOME_NEW Movie106和DAILYBONUS/PRIZE BT；恢复原标题/两按钮/原图片高度缩放/奖励数量/标记/原离线社交说明，删除自编活动数量。UI_sample已授权本机时间签到，仅该奖励分支允许离线。
- 签到验收：开场禁点、离开页面才执行原action95发奖、退场章节、防重复/五日循环/错过一天、cht时钟适配和原1009重载；社交身份及奖励仍不伪造。

### 10:48 UTC：签到、选兄弟与原始启动链

- 签到恢复 WELCOME_NEW 原图层回调、font6标题、PNG按区域高度16.16缩放、原奖品数量优先级、原状态灯及两按钮；社交区使用原离线说明，不再显示自编活动数量。进入页不发奖，OnExit动作95仅执行一次，按SetChapter(1,true)后Reverse从800倒放到0。`cht`推进累计签到秒数，启动时间保留真实时钟，避免重启读未来时间。原1009和钱包保存重载通过。
- 入口63 / `--greeting-check --mute`：七次原按钮命中、入场零发奖、退场单次发奖、倒放、五日循环/七日样本、断签重置、cht、两次原存档重载均通过（greeting-input-check.log退出0）；共用PNG投影改动后商店卡回归退出0。greeting-native-full.png为正式GUI完整截图。联网数量、朋友加成和原签到教程17尚未恢复。
- CMenuPlayerSelect :194878–195343 + ui_movie.bt：Movie70的原区域、font6标题、入场停章和左右选择章节取代页25/29手写布局。选择动作0x4E更新两位兄弟关系/firstLaunch；章节结束才进入教程或返回设置。入口64 / `--player-select-check --mute`覆盖无saves源创建、两兄弟×首次/设置、开场禁点、动画结束门禁、四次原格式重载，player-select-ready-check.log退出0。
- CGunBros::EnterShell :79648确认原首次进入选兄弟，其他进入签到；正式启动去掉旧宿主TAP TO PLAY中间页。CMenuAction21 :93929确认GAMES仅请求PlayHaven more_games，缺发布服务时不改变当前页；正式导航不再进入自造游戏列表。E快捷键指向原商店，旧装备研究页不再作为正式快捷入口。
- 持枪待查：再次核对CMesh::Init的W取反、GetNodeAt插值、DrawHeirarchy矩阵列主序与原shader、DrawUI挂点4/2、SpawnForUI export9/状态17/动作覆盖槽9，未找到允许额外改角度或缩放的证据。当前原存档枪与UI_sample截图不是同一枪，不能凭不同武器截图填补。新增player-ui日志保留实际枪、状态、动作、时刻、原范围与覆盖索引。
- 下一阶段：按CMenuPostGame、PostGameInfo/Status/Casualty、原POSTGAME/RESULTS Movie和存档统计客户端恢复结算，先厘清布局/数值来源/退出联动，再实现与实际死亡返回联验。

### 11:20 UTC：原结算页及九项菜单回归

- `MENU_POST_GAME_WRAPUP` VA0x403350、CMenuPostGame 164559–166204、CMenuPostGameOption 249755–249964；Movie17/115/20/18/19、原MDS、字体0/5/6与区域回调取代正式页27/28手绘内容。单机默认兄弟按provider93显示三项；不虚构好友赠送经验。生存进度、Horde高分/最长连杀/HH:MM:SS取实际关卡结果及原字符串格式。
- 开场章节结束才弹熟练度；BACK先原按压章节再倒放退出，XPlo非零进入精炼、零返回星图。敌人图库按原资源平坦索引排序，单种居中，拖动驱动原章节；敌人按原模板UI缩放、整数XY包围盒和原DrawUI投影绘制，不用小视口裁切。
- `MovieRenderer::Region`补原隐藏逻辑区域查询。BACK_BUTTON仅有一个不可见type6区域0，原输入和绘制独立；不能以visible=false当作没有触框。原Sprite与模型绘制仍遵守可见标记。
- 永久入口65 / `--postgame-menu-check --mute`：实际原存档生存两波35击杀、BOKOR两波196击杀/9种敌人，结算两标签、全部敌人加载、开场/按压/倒放门禁、两条XPlo退出路径、存档重载退出0（postgame-gallery-check.log）。postgame-original-2-casualties.png已目视。
- 共用隐藏区域修复后的九项专项（商店、升级、设置、精炼、签到、任务、Header、选兄弟、175 Movie）均退出0：`postgame-regression-*.log`。
- 剩余：图库完整惯性/越界回弹/吸附、overview/卡片完整Hide退场、远程好友奖励和联网提示未完成。原大枪遮脸问题仍未完全解释，不能凭本轮图声称模型姿态1:1。

### 11:24 UTC：暂停与战斗HUD阶段方案

- 审计确认旧SurvivalHud暂停按钮、正文、部分操作栏仍手写，旧RESTART按钮不在原MDS_PAUSE_ROOT中。正式native路径改用MENU_PAUSE VA0x4032f0、MENU_HELP_PAUSE VA0x402fd0及GLU_MOVIE_LIST_MENU_PAUSE；旧研究HUD保留能力和注释。
- 原provider2在单人模式跳过多人AI切换；XGA/iPad保留摇杆停靠，所以根列表六项。动作/文字来自原MDS，帮助为MDS_HELP，字体/滚动/区域来自BIG。菜单动画使用独立渲染时钟，暂停不停止UI。
- 验收六个原区域真实点击、入场禁点、帮助14项与BACK、音效/音乐/停靠偏好原p保存重载；再核对HUD底栏区域、原仪表绘制与输入。资源编号补丁单独检查来源，未确认数据保持未知。

### 11:52 UTC：暂停、HUD与武器目录验收；战斗选择器方案

- 暂停入口66恢复原六项及14条帮助，真实BACK、音效/音乐/停靠偏好原p保存重载通过。HUD入口67恢复原区域、原尺寸摇杆与输入道具field29、生命/经验渐变及CInputPadMeter余弦过渡，5触框/20图标/区域变异/3插值用例通过。原暂停完整惯性和退场、HUD入场ReFill/连杀弹动尚未全接。
- WeaponCatalog去掉pack5 ordinal56分类及58/63“unused”补丁，改由单枪STORE引用取得商品分类；无STORE引用不等于废案。76武器、商店卡、暂停、HUD、四星球与BOKOR原存档实战回归全部退出0：final-*.log。
- 战斗商店方案：对照CPowerUpSelector::Init/Bind/SetupPowerUps/PowerUpControlCallback（184203–187670）、CMenuMovieControl与ui_movie/powerup_template/store_entry.bt，恢复Movie131/52/134及原MDS按钮，按STORE.displayOrder排序，不再维护道具序号表。价格/名称/图像和不可用状态来自原资源及库存/脚本。验收全部条目、原区域命中、装备/使用资格、入场门禁、原存档购买重载；完整触摸惯性、联网和武器模式单列边界。

### 12:17 UTC：战斗选择器完成；HUD通知方案

- 永久入口68 / `--powerup-selector-check --mute`：按原STORE排序15条，14个受支持购买、14库存图标、38装备/立即使用触框、3种原提示、区域变异及原存档重载通过，selector-prompt-check.log退出0。复活原条目保留，当前效果未支持时显示原不可用提示，不扣钱。缺少币时按原双按钮显示缺额，购买更多进入明确的离线提示；战斗内到Bank的完整返回链仍未接。
- Movie131章节为0/0/100/900，52为0/200/700，134无章节轨；均由BIG解析，未把重复章节或无章节当异常后补表。图标、数量、价格及按钮采用原消费者尺寸。完整触摸惯性/弹性、选择退场、使用粒子及多人武器模式尚未恢复。
- HUD通知方案：CInputPad::OnLevelUp/OnWaveClear/OnLevelStart/OverlayDraw及原ARM参数、ui_movie.bt；升级恢复原Movie7两条，清波Movie34后按真实完美判定排Movie87，字体11及文字来自BIG。正式游戏使用通知结束回调释放LEVEL event2，旧研究定时入口保留并标明边界。验收原文本、层顺序、资源时长、队列清理和正式四星/教程/BOKOR实际流程。

### 12:33 UTC：通知闭环通过；商店换枪顺序方案

- 原通知专项67覆盖两段升级、普通/Horde/Boss开场、清波和完美奖励共7段原时长；原字体11、%i格式及百分号均核对BIG实值。overlay-format-hud-check.log退出0；首版只检测%d漏了%i，已按日志发现并补足检查，未把首版通过当作文字正确。
- 正式SurvivalSession由最后一个Movie完成发送LEVEL event2；四生存星球各2波和BOKOR2轮保存重载通过，overlay-horde-native-play-check.log退出0。BOKOR新一轮开场也需要事件2恢复原慢动作，已修复首轮测试暴露的停滞。旧固定1200ms仅留在明确研究路径。
- 教程入口49增加缺源原格式账户实战，原移动/射击/换枪/手雷完成，获得枪械保存重载通过。tutorialSteps只为宿主执行轨迹，不写成伪原字段；原firstLaunch由选兄弟行为清除。
- 换枪方案：CMenuMeshPlayer::Refresh action92先调用OnSwapGun，原Flow状态17→18→19由native3改活动枪；同一兄弟解释器及正在播放的旧动作保持，新枪仅在原调用时切换，后续回到17使用新枪覆盖动作。预加载两枪分别拥有资源与解释器，避免重建兄弟提前替换画面。原角度和比例不另加补丁；验证旧枪收起、native3时刻、双向切换、原存档活动槽重载及资源生命期。

### 12:49 UTC：商店换枪和最终回归

- 双枪分别从BIG预加载，原兄弟解释器保持；旧躯干完成当前举枪动作后才在下一序列使用新覆盖。`player-swap-check.log`退出0，真实原存档双向三次交换每次528ms，actor、旧／新torso生命期和原1001活动槽重载均通过。UI计时更正为原UpdateUI直接截断毫秒，普通战斗控制器保持其原算法。
- 换枪按钮的手填Sprite缩放0.8和文本高度0.26已移除，采用MDS_BUTTON_STORE_GUN_SWAP→GLU_MOVIE_WEAPON_TOGGLE、font6和原子Movie触框；位置按GunSwapCallback右边界减原按钮宽度。原入场／按压结束action92／idle／分类隐藏章节已接，真实命中和边界专项加入商店研究入口52。
- 已更新当前验收文档，明确GUI／研究工具、原账户路径、原启动链、离线服务和未完成范围；旧GAMES→BOKOR、本地文本正式账户等说明已撤下。
- Release与Debug本轮双EXE均构建通过；按钮最新增量与18项OriginalUI整体回归正在验证，不能把前一版构建结果代替本版结果。

### 12:59 UTC：整套回归和GUI系统菜单

- Release OriginalUI 18/18通过：`out/validation/20260909-074838-234-Release-OriginalUI`，protected-changes=0。包含换枪原按钮入场、真实子触框、按压末毫秒、分类隐藏和三次原存档切槽重载。
- Release Core 20/20按预期完成：`out/validation/20260909-075515-247-Release-Core`，known-data-issues=1（原PROP43畸形）、protected-changes=0。保留完整菜单购买／装备／实战／结算／精炼回归，未删旧检查以制造通过。
- 仅GUI EXE+SDL3.dll的隔离目录`gui-standalone-final/`启动原商店退出0，ToolsPresent=False、console=0，store.png已目视。真实存档枪38的巨大模型遮脸仍明显，未把此图标成1:1。
- 实际Windows系统菜单Research tools触发独立伴随EXE，GUI正常退出0；`gui-system-menu-result.log`保留自身窗口句柄、子进程及console=0。初次WMI查询被系统拒绝，改为读取本次GUI启动日志PID；随后发现Process.MainWindowHandle指到辅助窗口，改为只枚举本次PID的原生窗口，最终真实菜单命中通过。没有修改其他进程。
- 商店人物改为使用本页统一时钟，便于原Movie和角色按同一delta驱动；盔甲预览保留原活动枪槽，避免默认切回枪0。追加完整原账户鼠标点击→按钮原按压→PLAYER native3→DrawStore保存重载，当前Debug专项验证中。


### 13:05 UTC：原图武器对照方案

- 最新Debug完整商店真实按钮点击→原按压动画→PLAYER Flow→活动枪槽→原1001重载通过，native-click-debug-check.log退出0；Release对应检查运行中。商店之外行为未再变更，20项Core结果仍适用。
- 原IMG_0800是银蓝枪，源标准存档槽0为Apathy Bear、槽1为Cataclysm X4，两者与原图不同。使用BIG目录中的Infinity Laser作为待确认候选，建立独立研究账户并保留同一原盔甲，仅生成对照截图。名字仅用于此明确研究fixture选择，不成为运行时按ID/名字分支，不修改正式账户；对照确认前不能据此判定遮脸是原版缺陷或镜头错误。

### 13:15 UTC：交付验证与装备对照扩展

- Release完整商店按钮联验通过，`delivery-store-check.log`和`reference-gun-check.log`退出0；原Movie按钮→PLAYER Flow→原1001活动槽保存重载均验证。Debug对应结果见`native-click-debug-check.log`。
- Infinity Laser截图已排除：模型的金色外壳／青色条带与IMG_0800银蓝枪不同。研究入口52改为遍历BIG原步枪／激光枪分类，输出包hash与ordinal命名的独立账户截图，继续寻找同装备对照；不改变正式账户和玩法，不把候选视为已匹配。
- `OriginalMenuData.inc`和`OriginalNavigationData.inc`补提取工具格式版本、重建命令、gunbros及反编译源SHA256；重新提取两文件数据正文逐行完全一致。表来自原程序静态常量，Movie／Sprite／字体／文本等仍从BIG解析，不另立手填资源源头。
- 本地验收索引`out/ui-original-2026-09-09/review.html`包含16组原图／重建图；原图、输出图不修改，链接均存在。账户、装备和动画时刻不同处明确标注，不将此索引当作逐像素一致证据。
- `delivery-verification.log`重新核对源21文件零变化、18项OriginalUI和20项Core退出结果、`git diff --check`退出0。Core中的PROP43按预期暴露原异常；输出路径为本轮目录。Windows系统菜单与独立无研究EXE启动的证据仍见12:59记录。

### 13:26 UTC：定位同枪原图

- 全目录对照中已目视识别IMG_0800的银蓝圆盘枪为 **Ion Blaze，pack5 GUN ordinal31，hash2520453，STORE特殊类5**；对应`reference-gun-2520453-31.png`。银蓝外壳、圆盘、枪托、弹匣与原图一致，解释了此前从步枪／激光类查找未命中。保留Infinity Laser错误候选记录，不把原截图装备写回正式账户。
- 验收页首组改为同枪同盔甲比较，现17组；余额、动画帧和明暗仍有差异，不能声称逐帧1:1。原Cataclysm遮脸仍须其自身原版画面验证，不能用Ion Blaze来判定该动作错误或正确。
- 全武器原UI图像按BIG目录逐项生成，研究入口52永久保留；`weapon-reference.html`记录原包hash／序号／BIG名称，不靠手填资源表选择正式武器。
- 最终Release／Debug构建均退出0（`final-gallery-release-build.log`、`final-gallery-debug-build.log`）。PE复核两配置游戏subsystem2、研究subsystem3。此前Debug完整编译仍报告HUD／道具数量布局整数像素转float的C4244，日志保留；不宣称全工程零警告。

### 13:29 UTC：本轮截止

八小时授权到期，停止新增实施，后台自动化 ui 已暂停。最后测试退出0，原件21文件零变化，无遗留测试进程。交付和未完成项见文首；不延长本轮授权。
