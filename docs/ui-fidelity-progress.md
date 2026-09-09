# 原版复刻四小时进度（2026-09-08）

## 授权与工作窗口

用户提供 `UI_sample/ui.md` 和原版截图，授权自主决定产品与技术细节，在 18:32–22:32 UTC（当地 13:32–17:32）持续推进。后台续跑 `gun-bros-2` 每 20 分钟承接本任务，到时完成验证并暂停。无需重复审批。

## 基线与依据

- 开始 git 已跟踪文件干净，`UI_sample/` 为用户新增未跟踪资料，保持原样。
- 前一轮主要使用原美术，但布局明显偏离：主导航顺序错误、标签放到图标下、商店九宫格和左侧小人物，截图为右侧大人物、两行横向卡片。
- 用户 iOS 截图优先；PC 截图补充细节，二进制及 BIG 核对具体数据，不照搬 PC 的已知错误。
- 正常账户及源存档不作自动测试，输出与测试账户使用 `out/`，全部常规运行 `--mute`。

## 阶段 A：主框架与商店

方案：核对 CMovie 原布局、字体与商店卡片，恢复截图中的主导航、三位等级、黑色商店背景、四分类、右侧大人物、两行商品和拥有/装备标记。保留研究入口和现有购买保存逻辑。

任务：
1. 读取原电影/菜单表，逐项对照用户截图。
2. 修正框架布局与字体；商店布局及切枪接真实装备状态。
3. 更新菜单命中检查，构建、截图目视及购买保存回归。

验收：新手/完美账户截图无重叠、货币和三位等级正确，四类可切换、商品可选可购买可装备、武器预览不污染存档，程序内导航与战斗回归通过。

## 后续阶段队列

- 星图拖动、模式选择、革命/波次与 BOKOR 独立选择。
- cfg 网络/调试开关与 ch 作弊输入，签到和银行等待到账。
- 兄弟模型和出生位置、首次教程原脚本。
- 加载小人、暂停/道具商店、熟练度→结算→炼化顺序。
- 性能定位及最终分阶段验收。按证据与实际进度调整顺序，未实现项明确保留。

## A1 已验证：导航和字体

- 主导航按 iOS 截图重排，原 MDS 表为数据目录而非屏幕顺序；标签置于图标上方原空条。
- 菜单蓝字使用字体 1、按钮字体 5，货币字体 0、机械等级字体 7，三位分格显示，200 级经验条保持空。
- Release 构建及 `--game-menu-check --mute` 退出 0；日志 `out/fidelity-a-header-build.log`、`out/fidelity-a-header-menu.log`，截图 `out/game-menu-preview-check.png` 已目视。
- 检查包含购买、炼化、双枪、两波战斗、道具连续购买、盔甲预览/装备、返回栈、波次锁定和货币保存。下一步单独重建商店，装备研究页继续保留。

## A2 商店结构与原资源

- 正式 STORE 使用黑底、四分类、右侧大人物和两行横向商品；旧装备研究页保留。Q 与蓝色按钮切枪，正式战斗也支持 Q。
- `CMenuStore::ItemCallback` :178878 明确每列两张；定位 core movie 39 原六边形卡及 800–1300 ms 展开，5:17/18 为 OWNED/EQUIPPED 斜标。0:63/66/74 实为灰色按钮，不能误当选中高亮。
- 原邀请卡 5:52、STARTER PACK 28 Warbucks/35 个对象直接读取；替代金币/绿币服务仍是本地流程。免费 Warbucks 卡的原美术尚未定位，当前为文字入口。
- 道具三模式可用性依据 `CStoreAggregator::IsItemExcludedFromGameType` :156283 的 `value8` 位；339 条商品/396 个引用检查通过，新增原数值数组输出 `out/store-check.txt`。
- `out/fidelity-a-store-menu.log`、`out/fidelity-a-original-cards-menu.log`、`out/fidelity-a-promotions-menu.log` 均退出 0。原电影/字体及新增商品精灵图 `out/fidelity-a-components.log` 退出 0；最新增加筛选/完美账户截图的回归运行中。
- `src/tools/ui_contact_sheet.py` 使用已提供的 Python/Pillow 运行时生成原电影联系图，输入与用户截图不修改。项目暂无 `.venv`；系统 Python 无 Pillow，使用 bundled runtime。

## 阶段 B 方案：星图与选择层次

依据截图与 `CMenuMission` 原触控状态，拆开模式→可拖动星图→Revolution/Horde→Wave→Play。星图保留所有五个实际星球及未知星球占位，BOKOR 不显示标准 Wave 网格。恢复原难度徽章、锁定/已完成提示，先核对资源再实现。保留波次锁定、回玩与四图 50×10 的实际调度。

验收：拖动不误触选球；从每个星球进入正确轮次/波次；锁定不可绕过；BOKOR 十档入口可启动；菜单回归与首尾波检查通过。

## B1 已验证与细修

- 原版模式徽章 8:25/26/29、革命编号 5:24–33、Horde 5:42–51 已用于模式→星图→革命→两行五列选波，以及 BOKOR→Horde→Play。星图支持带深度差的拖动，选择改为释放点击，拖动不触发选择。
- 原紧凑星球页保留为 `--menu-page 20`、EXE 研究入口 47。原装备和全部既有里程碑保留。
- `out/fidelity-b-menu.log` 与 `out/fidelity-b-perfect-menu.log` 退出 0，包括主流程、Horde 分支、回玩与锁定。目视后修正模式副标题、波次字体、卡片背景和滚动条溢出；后续截图等待页面切换后一帧稳定。
- `CMissionWaveStatus::WasWavePerfected` :192487 证实完美状态是逐波位数组，不能将所有通关都显示金色。已读取 iOS 存档位数组，本地格式升级 v7，旧 v1–v6 仍可读。战斗按实际无受伤结果存标记。
- 星图位置当前依照截图测量，原版拖动插值曲线尚未逐帧核对；LIVE/VS 仅本地预览，无远端匹配。

## 阶段 C 方案：配置、签到与本地银行

先核对原每日奖励/签到循环，再加入可编辑 cfg 的 IsConnected、DebugMode、ch 前缀作弊输入；chm 增加 5000 金币及 500 Warbucks，cht 推进下次签到。签到使用本机日历日，奖励和领取日随本地账户原子保存。银行购买先显示 Please Wait，3–5 秒后只结算一次。验收包含重复领取、重启、旧账户迁移和等待期间重复点击。

## C 已验证

- 原 `DAILYBONUS` 模板是全包全局索引，不是 core-local；奖励读到 125/500 金币、1/2/3 Warbucks，`CommitBonus` :209500 按五项取模。第六天回第一项，断签两天重置。
- `--daily-bonus-check --mute`、`out/fidelity-c-daily.log` 退出 0，涵盖七天循环、七次重复领取拒绝、断签、测试日偏移、保存重读和第 500 波完美位。
- 签到页使用原 movie 106，奖金随展示自动领取并保存；原五日面板、两块社交入口及真实奖励图片已接入。截图 `out/fidelity-c-greeting.png`。
- 本地银行等待四秒后到账，等待期间重复确认无效。`out/fidelity-c-menu.log` 退出 0，已有购买/换币流程通过；图形检查时钟在测试驱动注入，不缩短实际等待。
- 新 `gunbros.cfg`、`chm/cht/chd/chc/chh/chw/chi` 输入；DebugMode 提供 FPS/帧耗时与战斗坐标、伤害、完美奖金等。完美奖励横幅改回 `+10%`，精确值放 debug。
- `out/fidelity-b-original-profile.log` 原账户导入、战斗、重读退出 0，源存档不修改。本地格式 v8 向前兼容。

## 阶段 D：兄弟与战斗表现

先用真实初始化建立出生点断言，分别检查出生坐标、矩阵和 AI 重置；再对照截图修模型/默认装备与 HUD、暂停。初始复现：`--brother-check --mute` 退出 1，唯一新增失败为玩家/兄弟都在 `(1026,1932)`，距离 0；截图 `out/fidelity-d-brother-before.png`。

## D 已验证

- 出生点使用八方向可行走候选，优先相距四倍碰撞半径；`out/fidelity-d-spawn-fixed.log` 退出 0，距离 46，含两波、死亡与复活。
- 初帧上传 mesh frame 0 而未计算当前 idle 是姿态异常的独立原因；新增断言先失败（1191 个骨骼分量 vs 0），`CreatePlayerBuffers` 首次上传前 `PosePlayer` 后通过。`out/fidelity-d-pose-fixed.log`、全 73 把可发射武器 `out/fidelity-d-weapons.log` 均退出 0。
- 战斗恢复原双轮盘 1:6/7/8、红商店 1:27、蓝切枪 1:35、紫矿倍率 1:39、绿血条，常态调试文字移入 DebugMode。
- 原 movie 131/134 横向道具商店与装备条接入真实购买、左右槽和立即使用；滚轮/拖动浏览，菜单打开时冻结世界。原暂停菜单样式、滚动、音效/音乐设置和投降入口已接入。
- `out/fidelity-d-hud-final.log` 退出 0，含原资源绘制、购买 10 枚手雷、两边装备与滚动命中。`out/fidelity-d-controls.log` 退出 0，通过实际战斗操作路径注入购买、左右装备、切枪、暂停设置与恢复，继续存档到第 4 波。截图 `out/combat-controls-shop.png`、`out/combat-controls-pause.png`。
- 道具 Revive 的原脚本尚未接入；不会把无实现道具当作可购买商品。暂停描述与焦点动画还需细修。

## 阶段 E：首次教程及结算

先核对 CTutorialManager、原编译脚本和 CWeaponMastery。新账户进入选兄弟与原教程阶段，标记持久化；失败或关闭可重入，不提前标完成。结算改为熟练度、战果/击杀详情、炼化的顺序，并使用原电影。每个新增状态均需保存与回归检查。

## E1 原教程已贯通

- 实际使用 pack2 LEVEL 6 / MAP 7 的 game variable 6 分支；CLevel native 69 控制兄弟射击，71 推进、72 完成、73 查询道具。玩家脚本负责移动/射击双条件、换枪和投雷，关卡脚本负责敌人、拾取和阶段定时。
- `out/fidelity-e-tutorial-complete.log` 退出 0：0→1→2→3→4→5→6→完成，原脚本生成 id 501 手雷，真实拾取、投掷消耗及击杀大敌人。保存重读 `tutorialCompleted=1`、八阶段位 255、第二枪为原免费 ER97E Elite（pack5 GUN 4）。研究入口 49 / `--tutorial-check` 永久保留。
- 本地格式 v9 向前兼容；已有 v1–v8 及原存档导入按已完成处理，不强制老账户教学。新账户选人后进入教程；中断保留未完成，重新进入从原开头开始。
- 原 CPlayer::OnSwapGun 输入及 CBrother native 3 换枪释放回调接入。投射物也连接原关卡上下文，手雷震屏调用不再缺失。
- `out/fidelity-e-tutorial-hud.log` 退出 0，教学期间世界区域允许鼠标射击，切枪按钮仍正常命中。选人和对话框已恢复原头像/卡片，标题顺序和头像帧正进行目视细修。
- 当前八阶段位为宿主关卡检查点，明确区别于原 CTutorialManager 的 22 个已读菜单提示标记；后者尚未全量恢复。

## E2 结算、熟练度与炼化

- 正常死亡／投降后依次进入未满武器熟练度、Overview／Casualties、炼化。两把入场枪都已满时跳过熟练度；炼化前隐藏主导航，快捷键不能绕过。教程完成后的退出也接此流程。
- 原 movie 138/139、17/19/20、31/36/54 构成升级、星条、三项战果卡、敌人详情和六台炼化器；Casualties 使用实际击杀类型、原名称和 3D 模型。Standard/Premium 两页各六项时长、倍率、解锁条件读取原表。
- 每支枪记录本场 XP，检查点按增量落盘，投射物保留发射时的武器引用，切枪后仍记到原枪。v10 账户追加熟练度记录，保留 v1–v9 读取。升级购买推进至下一原阈值，扣款并保存。
- `out/fidelity-e-postgame-fixed.log`、`out/fidelity-g-social-check.log` 退出 0；实际两波：35 击杀、1 个敌人类型、枪 XP 34、炼化 36 矿、购买提升一级。包含双击炼化只到账一次、关闭升级页、切换击杀详情与重读。
- 当前枪 XP 只计本人使用该枪的最后击杀；原助攻归属及跨熟练度门槛奖励 XP 仍未完整接入。

## F 加载与性能

方案：先分段测量，再对重复模型读取、首次 HUD 展开分别试验；不凭主观流畅度宣称提升。

- `LoadingScreen` 使用原 core 0:124 跑动剪影（CMenuSystem::Init :97115），在资源加载边界泵事件并呈现；菜单目录、地图、角色与 HUD 加载接入。它不是异步解码器，单个长资源内部仍无中间帧。
- 导航恢复逐个弹出和轻微回弹；截图检查固定最终状态，避免把入场中间帧当布局错误。
- 新永久研究 50 / `--performance-check`：真实地图、双人、波次、投射物，共 1200 个渲染帧；关闭 VSync 仅用于测量。分段数据写 `out/performance-frames.csv`。
- VSync 基线约 6 ms 是显示同步等待，不能据此归罪几何构建。显式 buffer orphan 试验没有改善，已撤回。
- 同一场景的敌人模型／贴图／缓冲按模板复用，控制器、脚本和每个实例姿态保持独立；绘制每个模型前立即上传对应姿态。实测 10 次出生：1 次资源加载、9 次命中。
- 原 HUD 动画、字体、轮盘、数量徽章在加载期间预热。无 VSync 基线 `performance-unlimited.csv`：p50 0.368 / p95 0.785 / p99 4.652 / max 68.318 ms；`performance-prewarmed.csv`：0.361 / 0.781 / 3.753 / 9.341 ms。主要减少加载尖峰，不宣称所有战斗都获得同样帧率。

## G 社交与设置

- 离线 BROS／BRO-OPS 使用原 movie 75 的抱导弹人物、边框、原提示文案与红 RETRY。IsConnected=1 或 LOCAL PREVIEW 开启本地预览，不能连接已停运远端服务。
- BROS 恢复 On Duty 栏、头像、右侧大人物、原红印章 6:17、Brothers／Bro-Buffs／Rewards 页签和原邀请横幅 6:0。邀请弹窗可关闭，阻止点击穿透。BRO-OPS 的 Recruit／Requests 空状态可进入。
- 原选兄弟页保留为 page 29，从 OPTIONS / SELECT BRO 进入；本地八项活动保留在 Achievements。截图发现名字与贴图映射反向，现统一贴图 0=Percy、1=Francis，头像、选人和战斗标签一致。
- OPTIONS 恢复原 movie 86、纵向滚动按钮和焦点说明，音效／音乐／自动兄弟真实保存；暂停说明跟随焦点。`out/fidelity-h-game-menu-check.log` 退出 0，覆盖滚动后进入下级并多层返回、社交页、弹窗、设置保存、购买和结算。
- 修复目视发现的两个电影使用错误：movie 55 是整页 overlay，不能按其小文字 region 缩放；movie 111 直接使用原画面坐标，避免弹窗被放大到屏外。增加原社交精灵联系图 `out/ui-social-sprites.png`，175 电影检查通过。

## H 熟练度数据与加强验证

- ARMv7 地址 0x3C4AD0 的 UTF-32 字符串是 `weapons3`；SaveRestore 注册 :80431、CWeaponMastery::SaveToServer :192239 确认当前记录 1013。每条磁盘记录 14 字节（原运行时 12 字节）；旧 1005 保留但不用于当前熟练度。原存档四条 XP 导入并重读通过，源文件只读。
- CGun::Template::Init :127798 / :127835 会将移速／伤害表小于 100 的值补为 100。加强测试首次在 ER97E 升一级后读出零移速／零伤害，已按原实现修正；部分脚本发射器的发射间隔字段合法为零，不添加伪默认值。
- 熟练度发射间隔、普通伤害、暴击及移动倍率接入。暴击概率依据原 CRandGen::GetRandRange :370346 的闭区间，倍率使用模板值或原默认 10。随机数生成器仍是宿主实现。
- `out/fidelity-h-verified-weapon-check.log` 退出 0：76 模板，3 个仅展示，实际 73 把武器的发射／换弹／松开输入检查；新增各真实熟练度阈值前后验证。
- `out/fidelity-h-verified-tutorial-check.log` 退出 0：大敌人承受 5008 ms 实际枪击仍存活，靠近后投雷，23 秒内完成 0→6→-1；手雷 1→0、最终击杀 2、存档步骤位 255。第一次加强测试失败是试验驾驶员超出手雷有效距离，保留失败日志并通过真实移动修正试验路径。
- `out/fidelity-h-verified-hud-check.log` 退出 0。一次批量命令误用了不存在的 `--survival-hud-check`，退出 1 是参数拒绝；改用实际 `--hud-check` 已通过。
- 玩家射击模式四星球各前 20 波和第 500 波全部退出 0，逐项结果见 `out/fidelity-final-player-map-checks.txt`；使用 `--weapon 65 --mute`。未重跑当前二进制四图从头到尾各 500 波，前轮完整长跑证据不替代本轮验证。
- 另一次加 `--brother` 的测试有 5 项未通过，原日志保留于 `out/fidelity-final-map-checks.txt`。该测试模式会禁止玩家射击，只让默认装备 AI 清场：Haven 前 20 波停在第 15 波，四图第 500 波均超出清场条件。未发现非法出生或未实现脚本调用，但不能据此承诺 AI 可独立完成全部后期波次。普通双人游玩另由 profile-play / brother / menu 的实际战斗覆盖。
- 导入 XP 高于当前模板上限时，新增 XP 不再向下截断已有值。原存档回归新增低上限模拟及实际战斗后逐枪 XP 不回退断言。

## I 最后一次原图对照

- `CMenuUpgradePopup::UpdateInfoStat` :392886 与 `CStoreAggregator::GetStatValue` :156725 证实 NEXT 使用整数百分比，stat 3 为移速，stat 8 为熟练度对应的 NONE / LOW / MED / HIGH 暴击档位。原版只显示变化的普通属性和暴击行；不再误标为 AMMO。
- 弹窗改为 CURRENT / NEXT、蓝绿列、原字符串与红色关闭按钮 0:99。武器名移到图标上方，精确 XP 放入 DebugMode。升级扣款与保存不变，`out/fidelity-i-menu.log` 退出 0，截图 `out/fidelity-results-mastery.png` 已目视。
- 暂停页恢复 MDS_PAUSE_ROOT 的原说明与悬停高亮；处理 `^f1` 标题和 `^f0` 正文字体控制码，保留原段落。先目视发现控制码泄漏，修正后重新生成截图验证。
- 默认人物嘴里的雪茄已由原模型显示，截图可见；没有额外绘制伪附件。首选人物页、BROS 的头像、贴图及姓名已统一。
- 原图／运行截图并排验收页：`out/fidelity-review.html`，图片未合成到产品中，均来自实际菜单和战斗检查。账户、武器与动画时刻不同，未伪造像素相似率。

## 本轮验收汇总

| 检查 | 命令／证据 | 结果 |
|---|---|---|
| Release 构建 | `out/fidelity-delivery-release-build.log` | 退出 0 |
| Debug 构建 | `out/fidelity-delivery-debug-build.log` | 退出 0；此前完整 Debug 构建保留原有 CBitmapFont 有符号比较警告 |
| Release 核心 | `test-muted.ps1 -Configuration Release -Phase Core`；`out/validation/20260908-170534-579-Release-Core/results.json` | 20/20 完成；19 Passed、1 已知 PROP 43；正式账户／原存档变化 0 |
| Release UI | `test-muted.ps1 -Configuration Release -Phase UI`；`out/validation/20260908-170825-674-Release-UI/results.json` | 8/8 Passed，含 175 电影、13 字体、HUD、菜单和 BOKOR 0/9；受保护文件变化 0 |
| 最后弹窗改动 | `--game-menu-check --mute`；`out/fidelity-i-menu.log` | 退出 0，购买、结算、炼化、原存档商店及多层返回通过 |
| 四图玩家射击 | `out/fidelity-final-player-map-checks.txt` | 前 20 波与第 500 波，8/8 退出 0 |
| Debug 核心 | `out/validation/20260908-171217-132-Debug-Core/results.json` | 20/20 完成；19 Passed、1 已知 PROP 43；受保护文件变化 0 |
| 最后暂停改动 | `out/fidelity-delivery-Release-hud.log`、`out/fidelity-delivery-Debug-hud.log` | 两种配置 `--hud-check --mute` 退出 0 |

最终构建与完整回归之间只有弹窗／暂停显示及注释改动，各自追加菜单／HUD 检查；没有把不同二进制的完整回归说成最终文件全部重新跑过。最终文件 SHA256 与对应日志登记在 `out/delivery-state.json`。

交付前再次核对：23 个 `saves/` 与 `userdata/` 文件无新增、删除或哈希变化；13 组原图／运行截图全部存在。`git diff --check` 退出 0，原代码注释保留，资源和用户截图未加入提交。启动入口、研究 47–50、配置、全部日志及验收文档均留在当前工作区。

22:27 UTC 完成本轮交接，后台续跑 `gun-bros-2` 已暂停并核对状态；任务未归档，等待人工验收。授权窗口内约 3 小时 55 分钟完成上述阶段，不声称剩余未实现项已经完成。

本轮已改进主要界面与可玩流程，但**尚未达到逐像素／逐帧 1:1**。仍有菜单插值、星图拖动曲线、部分卡片排布与字体尺寸、免费 Warbucks 美术、教程 22 项提示标记、枪 XP 助攻和熟练度奖励 XP、复活道具与真实多人服务等缺口。自动战斗使用测试无敌及指定武器，不代表原版普通难度的平衡验收；纯 AI 后期清场失败单独保留。原资源、用户截图和 saves 不提交版本库。

## J 商店按原版 region 重建（2026-09-08 晚）

用户反馈：商店和原版"明显不是一个东西"，简介和熟练度全没了，选中商店时导航栏没有高亮，
一堆 UI 是歪的，另外每次切场景都有明显的换窗口动作。

- 根因不是 BIG 没解析出来。原版每个菜单屏幕的布局都由具名 CMovie 的 user region 定义，
  `GameFrontEnd.cpp` 一直在用手写坐标。完整依据与 region 数值见
  [原版界面布局的真正来源](ui-original-layout.md)。
- `MovieNames.inc` 从 60 个名字补全为二进制里全部 115 个 `GLU_MOVIE_*`
  （`GLU_MOVIE__SOUNDS_` 是声音段键，不是电影，已排除）。重跑 `--movie-check`
  得到完整的别名→序号映射，175 电影 0 失败。
- 商店改由 `GLU_MOVIE_STORE_MENU`(37) / `STORE_SCROLL`(38) / `SHOP_BOX`(39) /
  `SORT_BAR`(41) 的 region 驱动：分类条、横向传送带、折叠卡、展开卡、FILTER 下拉、
  右侧人物、换枪钮全部按原版尺寸摆放，`< 1/37 >` 分页和 BACK 按钮按原版去掉。
- 简介来自 `assets[3]`（此前错读 `assets[5]`）。卡面的 POWER 与 DMG/RPM/SPD 改为
  渲染原版模板串 `assets[5]` / `assets[4]`，`#KEY` 用 `statGroups` 按当前熟练度填值。
  展开卡接上 `GLU_MOVIE_WEAPON_UPGRADE_MASTERY` 的三格星条作为 UPGRADE LEVEL。
- 主导航选中项改用 TRUNK_BUTTONS 的第 1 章（点亮底板）；原来用的第 3 章是普通态。
- 免费 Warbucks 卡接上原美术精灵 5:36，此前只有文字。
- 新增 `--movie-regions`：截图时把 user region 画出来，后续每个屏幕都靠它对位。
- 集成检查新增展开卡用例（`out/game-menu-detail.png`），断言简介与属性列真的读到数据。
  脚本驱动下屏蔽真实鼠标拖动与滚轮，修掉了一次因物理鼠标划过窗口导致的偶发失败。

验收：

| 检查 | 命令／证据 | 结果 |
|---|---|---|
| Release 构建 | MSBuild `/p:Configuration=Release` | 成功 |
| 菜单集成 | `--game-menu-check --mute` 连续 3 次 | 均退出 0 |
| Release UI | `test-muted.ps1 -Phase UI` | 8/8 Passed，受保护文件变化 0 |
| Release 核心 | `test-muted.ps1 -Phase Core` | 20/20 完成，19 Passed，1 已知 PROP 43 |
| 原图对照 | `out/shot-store10.png`、`out/game-menu-detail.png` 与 `out/reference/` | 已目视 |

未做完的部分列在 [ui-original-layout.md](ui-original-layout.md) 的"仍未解决"一节：
`^fN` 字体表映射、价格货币图标、商品排序、STORE 的红色 `!` 角标、盔甲卡图标与
正负着色、道具的细分筛选。**单窗口宿主（消除切场景时的换窗口动作）按约定放在商店之后单独做，本轮未动。**

## K 商店与升级页第二轮（用户逐条反馈）

用户给了一张 STORE_ITEM 记录的十六进制标注图并追问"这些数据到底在哪儿"。
逐条对下来，之前列为"未解决"的项目全部有据可查，没有一处是编的或写死的，
对照表见 [原版界面布局的真正来源](ui-original-layout.md) 的第二轮小节。

- 商店顺序在 `CStoreItem` 尾部 int16，负数不进列表；礼包的一次性购买是尾部另一个字节，
  全表只有 STARTER PACK 为 1。两者都改成了有名字的字段并导出到 `out/store-check.txt`。
- 货币图标是精灵 23:1 / 23:7，熟练度星章是 5:39/40/41，兄弟头像是 0:161+序号，
  红色 `!` 是 0:140。新增角色 0 / 19 / 23 / 26 的联系图，避免以后再靠猜。
- 升级页整页改为 `GLU_MOVIE_UPGRADE_POPUP`(138) 的 region 驱动，CURRENT/NEXT 列出
  POWER/DMG/RPM/SPEED 的真实数值加暴击档位，星条开页时跑到实际进度。
- 点卡片不再直接预览，PREVIEW 按钮才预览；满级只剩 EQUIP；按钮字号统一、
  宽度取 MDS 指定的按钮电影；按钮按下播原版爆闪；换枪触发原版脚本事件；
  右侧人物可鼠标拖动旋转。

验收：

| 检查 | 命令／证据 | 结果 |
|---|---|---|
| Release 构建 | MSBuild `/p:Configuration=Release` | 成功 |
| 菜单集成 | `--game-menu-check --mute` | 退出 0，新增熟练度星章与展开卡用例 |
| Release UI | `test-muted.ps1 -Phase UI` | 8/8 Passed，受保护文件变化 0 |
| Release 核心 | `test-muted.ps1 -Phase Core` | 20/20 完成，19 Passed，1 已知 PROP 43 |
| 原图对照 | `out/store-v2.png`、`out/fidelity-results-mastery.png`、`out/game-menu-detail.png` | 已目视 |

集成检查的点击坐标随新顺序重算：Mad Dogs 落到第 2 列下卡、免费 ER97E 落到上卡；
道具用例改为买两次 Speed Boost（1 战争钞票换 5 个，共 10 个），因为原版把血包的
`displayOrder` 设成了负数，它本来就不该出现在商店里。
