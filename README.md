# Gun Bros Windows 重建

打开 `gun_bro_re.slnx`，使用 Visual Studio 的 x64 配置构建。整个仓库只有 `GunBrosRe.vcxproj` 一个工程，它用 `GbProduct` 属性（`Game`／`Viewer`／`Tests`，默认 `Game`）产出 Re、Viewer、Tests 三个 EXE：构建主程序时会自动递归构建 Viewer，Debug 再额外构建 Tests。编译选项、源码清单和自动测试规则都写在这一个 `.vcxproj` 中。

| 目录 | 用途 |
| --- | --- |
| `src/engine` | 通用资源、渲染、平台与 Glu Script/Movie/Sprite |
| `src/gun_bros_re` | 游戏启动、玩法、菜单、条目与存档 |
| `src/gun_bros_viewer` | 六类查看器、启动菜单和展示控制，复用主包游戏逻辑 |
| `big`、`assets` | 运行所需的原归档、媒体和宿主着色器 |
| `tests` | 检查实现、运行脚本与 `fixtures/saves` 原存档样本副本 |
| `bin/Debug`、`bin/Release` | 全部 EXE 及运行依赖，按配置统一输出 |
| `obj` | 中间文件、调试符号及构建日志 |
| `_prep` | 已整体忽略的历史参考资料，仅在必须核对原版证据时查阅 |

直接运行 `bin/Release/GunBrosRe.exe`；查看器是同目录的 `GunBrosViewer.exe`。Debug 的三个程序全部位于 `bin/Debug`，Tests 不参与 Release 构建。不生成内部静态库，不使用其他 EXE 输出目录。

VS 的 F5 默认启动 `GunBrosRe.exe`。要调试查看器或测试程序，在工程属性里把 `GbDebugTarget` 改为 `GunBrosViewer` 或 `GunBrosTests`；该值只写进被忽略的 `.vcxproj.user`，不影响构建产物。

Debug 和 Release 均启用已有作弊码。VS 的 F5／Ctrl+F5 默认有声音；自动测试通过 `--mute` 显式静音。

**本地 Deathmatch**：`IsConnected=1` 时，在星图选择 **DEATH MATCH → 已解锁星球 → 进入对局**；断网时与 Live 一样显示原不可用弹窗，匹配中断网取消匹配，结算再战也需连接。五张地图、五套比赛的生命、武器池、三杀胜利、复活时间和随机补给均读取原 BIG，比赛枪组随机选择。进入后在原关内商店的 **GUNS** 页选择双枪，点击下方槽位指定装备位置，点击枪卡装备并自动切换到另一槽；右侧仅显示 POWER、数值及类别。战斗中可再次打开商店换枪，道具页使用 **POWER UPS**。死亡后爆裂隐藏身体并打开倒计时商店，RESUME 或倒计时结束后复活。战斗沿用移动、鼠标射击、2/N/M 换枪、Space 暂停、R 重开及原道具快捷键；结算可 REMATCH。

`DeathmatchBot` 会选择互补双枪、发现和追踪对手、侧移射击、寻找掩体、绕障碍和争夺临时武器。EXE 旁的 `GunBrosRe.cfg` 使用 **`DMBotLevel=1`（默认 Easy）、`2`（Normal）、`3`（Hard）**，修改后重启生效。此设置仅影响 DM，Live 不变。新增 Normal 后，旧配置值 `2` 对应 Normal；继续玩 Hard 请改成 `3`。

Easy 保留每命两次商店、两次标准手雷、大小血包合计两次，消耗真实库存，实际复活才重置次数；购物按原价格与余额。Normal 仅可使用标准手雷与血包，无限库存、不限次数、不打开商店，禁止其他主动道具。Hard 可使用全部 PvP 合法道具，无限库存、不购买、不打开商店。Normal 与 Hard 均保留原冷却与使用条件，无限供应不写入账户。双方沿用原盔甲和枪械熟练度，PvP 不应用 BRO BUFF。击杀、经验、矿石及实际消费写入各自账户，不推进生存波次、不覆盖永久双枪。最后击杀后完整播放死亡与爆裂，停留0.8秒，再播放原淡出。详见 [结算与难度](docs/deathmatch-ending-and-difficulty.md) 和 [Deathmatch 实现与验收](tests/deathmatch-plan.md)，专项为 `pwsh -File tests/run.ps1 -Case deathmatch-data,deathmatch,deathmatch-feedback`。当前是本地玩家对 Bot，真实联网仍未实现。

`IsConnected=1`（或菜单输入 `chc` 切换）时，选择 **LIVE → 普通生存星球 → 任务 PLAY**，原匹配弹窗就绪后约 1.5 秒加入当前激活的本地 bot。进关使用原 KEYSET 的合作壁纸与 GLU_MOVIE_SPLASH 区域、每波双列统计、15 秒波间等待和多人最终结算。波间双方都可移动，最后阶段显示 5→1 倒计时；任何一方购物时双方与整个战场暂停。屏外队友有头像定位，倒地后显示原等待救援／正在救援粒子。玩家与 bot 分别保存经验、装备和道具；双方可救援，倒地时先处理原复活道具选择。机器人策略独立在 `BroAIDeathmatch.h/.cpp`，不跟随活着的玩家，会避敌、救援、每 3–6 秒请求换枪并使用库存道具。

联网 BROS 列表循环读取账户目录的 local-bots.cfg，可配置多个 bot 的名字、等级、装备和道具；Live 匹配当前激活的 bot，选择跨启动保存；未激活 bot 时使用配置中的第一位。Solo 选中好友后读取独立配置，始终使用原 CBrotherAI；合作策略 `BroAIDeathmatch` 仅在 Live 创建和执行，与新的 PvP 策略分开。BROS 使用原三项滚动布局及选中高光，关闭 fake connection 后恢复默认兄弟。bot 账户位于玩家存档目录下的 `local-friends/windows-test-bot-1/`，原 `1006` 只记录好友 XP 礼物，`A` 记录选中好友凭据，均不是完整好友装备档案。`brow/brok/bror` 控制测试 bot 换枪、死亡、复活；`bros/brop` 在 Live 中打开 bot 的 10 秒商店或随机用道具，所有这些命令均不作用于真人队友。Deathmatch 不允许 `bror` 绕过比赛复活。本地名单启用原 BRO BOOST 档位和实际加成；好友 XP 礼物服务尚未模拟。配置说明见 [本地机器人配置](tests/local-bots-config.md)。专项命令：`pwsh -File tests/run.ps1 -Case local-live,offline-social`，验收见 [Live 阶段记录](tests/live-mode-plan.md)。

`GunBrosRe.cfg` 的 `DrawFPS=1` 默认开启，缺少该字段也默认显示；设为 `0` 可隐藏。FPS 使用原 BIG 游戏字体，位于顶部等级栏左侧，从启动视频、登录菜单到战斗持续显示，不受 `DebugMode` 影响。FPS 和战斗侧栏均无黑底，菜单左侧不再重复显示 FPS，侧栏不显示快捷键说明。

`DebugMode=1` 时，战斗左侧显示诊断侧栏；`Shift+I` 切换，启动时默认开启，不出现在普通菜单。内容包括地图、波次/轮次、LEVEL 脚本状态、双方血量、坐标、敌人数/击杀、伤害/受伤次数、武器、弹体/粒子数、经验、矿石和增益。`LAST WAVE` 显示最近一次结算是否 Perfect，`BONUS XPLODIUM +N` 使用实际入账增量，不重新估算百分比。

战斗中 `Shift+C` 一次切换全部碰撞类别，与信息栏独立：青色为身体阻挡，黄色为子弹阻挡，橙色为弹体地形检测，绿色为兄弟，红色为敌人，紫色为弹体，灰色为禁用边/暂不参与碰撞的弹体。重合的地图边用不同线宽叠画。地图 Viewer 和 Arena 共用同一绘制实现；Viewer 保留自己的 C 键/操作栏入口。圆按运行时部件/弹体半径绘制，复杂敌人使用实际边集合，激光使用零半径射线，不使用模型外框代替碰撞。

Debug 和 Release 均保留调试快捷键；主程序 EXE 同目录的 `GunBrosRe.cfg` 中 `DebugMode=1` 启用，`DebugMode=0` 禁用，修改配置后重启生效。菜单或战斗中按 **Shift+M** 打开 BIG 地图浏览器；上下选择、左右翻页，Enter 或 LOAD MAP 载入，Esc 取消。试玩中 Shift+M 可换图，Esc 返回菜单，R 重开，Space 暂停。Shift+C、Shift+I、Shift+T 也使用同一个 DebugMode 开关，分别控制碰撞显示、信息显示和菜单教程重放。文字作弊码保持原样；`chd` 仍可切换运行时的同一 DebugMode 状态。

作弊码集中在 `src/gun_bros_re/cheats`：`CheatConfig.h` 配置指令、奖励数值、输入超时和 Boss 跳转参数，`CheatKeys.h` 配置调试快捷键；修改后重新构建。完整指令见 [作弊码说明](src/gun_bros_re/cheats/README.md)。调试显示与地图浏览仍在 `src/gun_bros_re/debug`，`DebugConfig.h` 配置文字、字体、透明度、布局和碰撞线样式；坐标使用 1024×768 逻辑画布，字体颜色来自原 BIG 图集。Viewer 复用碰撞显示；引擎仅提供通用窗口叠加及输入接口。

Shift+C 原先被 `ch...` 作弊码前缀识别吞掉；现在带 Shift/Ctrl/Alt/GUI 的按键不进入文字作弊识别。`debug-input` 覆盖启用作弊码时的真实 SDL Shift+C/I、重复按键、原 `chm` 命令、FPS/DebugMode 四种组合、透明背景及 GL 状态恢复。

Debug 试玩收到原关卡的完成事件后会返回地图列表，显示 `Mission complete`。之前主循环只处理死亡退出，导致 LV0/LV3 完成后停留在禁止移动的场景；`campaign-progression` 现覆盖两关最后敌人的死亡与会话退出条件。这里是研究入口的完成反馈，不冒充原版完整战役结算界面。

地图列表按原 BIG 的全部 pack 枚举，显示 MAP、关联 LEVEL 和 Mission；同一地图的不同任务分行。无关联 LEVEL、无原版玩家出生点、解析失败或多人任务会标明不可调用原因。试玩使用菜单当前存档的副本，带入等级、两把武器、当前武器槽、盔甲、武器熟练度和道具；试玩期间的消费和进度不写回正式账户。从正式战斗确认切图时，会先保存已获得的正常进度。

战役入口用于研究现存内容：六条战役已有战斗和局部触发流程，尚未验证全部通关。现有跨 LEVEL native 仅记录目标，未执行地图转场；关卡脚本检查仍能触发未实现的 native 37（原 `CLevel::AddTag`）。脚本存在不等于运行行为完整。浏览器不补造缺失脚本或出生点。

已验证的开门示例：选择 `pack2 / MAP 3 / Mission 14`（Lava 3），先穿过出生点下方的门，继续向下偏左拾取蓝色 `KEY A` 门卡，再回到出生点上方门前触发开门。单纯在出生点清怪不会满足这个开门条件。专项检查 `campaign-doors` 覆盖入口通行、未拾取时锁门、拾取后触发与门碰撞解除；检查会设置角色位置以隔离各段条件，不代表已自动走完全图或验证通关。

`pack2 LEVEL 0` 清完房间小兵却不生成最终敌人的问题已复现并修复：原脚本重置刷怪器后执行单次生成，未指定路径时应使用地图当前导航层；重建此前把它当作无效的链接路径，丢失生成请求。现按原 `CEnemySpawner::GetSpawnPointOffScreen` 回退到当前路径，并按 `CLayerPathMesh::GetSpawnLocation` 选择屏幕外网格中心。`campaign-progression` 从破坏开关、穿门开始，通过真实受击／死亡回调推进小兵阶段，验证最终敌人 `pack1 ENEMY 17` 出现、死亡及 LEVEL 完成标记；测试使用无敌与高伤害，不代表手动难度或结算界面验收。地图左下放置的 `pack1 ENEMY 9` 是另一对象，左侧拾取物也不是此流程的通关条件。

战役内容核对：`pack2 LEVEL 0` 出生点上方第一扇门由右侧控制开关（对象 75）被毁后开启，门本体不受伤；`campaign-targets` 已验证开关命中、开门和通行，以及截图对应炮塔 `pack1 ENEMY 16` 的命中与死亡（初始 70 点血）。角色移动目前仅解析地图／PROP 阻挡，没有处理该炮塔的实体阻挡。`pack2 MAP 6` 及 `pack7 MAP 5` 没有关联 LEVEL；`pack7 MAP 1/2` 还缺少玩家出生点，均保留不可调用标记。MAP、LEVEL 和 Mission 的编号互不等同。

`pack2 LEVEL 4` 救援顺序为左侧平台 84（1 人）、中间平台 86（2 人）、右侧平台 85（3 人）。踩台释放角色，保持站台并保护她走到终点完成传送；离开会中断。同区有下一人时，走下平台再踩入。已补齐 `CEnemy::SetPath` 的 export 2、`CLinkPathFinder` 路线模式、`CLevel::CheckForCameraChange` 的区域 export 7、native 48 平台绑定、平台激活边沿及 `OnEnemyTeleport` 的 export 9。`campaign-rescue` 覆盖中断／恢复、1+2+3 人及最终完成。待救角色仍使用原 1 点生命、阵营 2，会被玩家子弹打死；检查用无敌和高伤害处理敌人，并设置阶段起点，不代表手动难度验收。

`pack2 LEVEL 5` 是可持续刷波、主动撤离的关卡：第 5 波后打开门 14，第 10 波后启用左侧蓝色平台 0（266,994）；站上平台约 2 秒撤离，离开会取消。`campaign-portal` 从初始波次清敌推进，验证提前不可用、10 波后启用、取消和返回撤离，未直接修改波数或激活平台。

LV0 左侧墙要求弹体的 `DestroyWall` 属性（bit 0），不要求先清怪；例如原 BIG 中 The Wombat 引用的弹体具有该属性。已修复普通子弹命中路径错误套用血量过滤的问题：原 `CLayerCollision::TestCollisionSegment` 会检查零血量机关的有效碰撞边。`campaign-cache` 验证无属性时不破坏、有属性时破坏、穿过斜向门洞并拾取对象 84–86。范围伤害仍保留原 `CProp::CanCollide` 条件。

LV2 暂不能完整通关：原 LEVEL 等待敌人入场完成事件，但 MAP 引用的 ENEMY 18 没有对应路径出口行为及通知；等待 30 秒再击杀后仍停在 state 1。保留独立复现命令 `bin/Debug/GunBrosTests.exe --campaign-lava2-check --mute`（当前预期退出 1，不加入自动通过的回归集合），不替换敌人或伪造完成事件。全包 LEVEL、Mission、MAP6 依赖及破墙武器引用可用 `--campaign-content-check --mute` 重新核查；详见 [战役归档核查](tests/campaign-content-audit.md)。

在 Developer PowerShell 中：

```powershell
msbuild gun_bro_re.slnx /p:Configuration=Debug /p:Platform=x64 /m
msbuild gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /m
```

Debug 构建自动生成 `GunBrosViewer.exe` 和 `GunBrosTests.exe`，并运行 `progress`、`big-version`、`viewer-controls` 三项检查及六入口冒烟检查。其他检查按需运行，脚本自动增量构建测试程序：

```powershell
pwsh -File tests/run.ps1 -Case resources,progress,movies
pwsh -File tests/run.ps1 -List
pwsh -File tests/verify-runtime.ps1
```

集中修改时可传 `/p:SkipAutoTests=true`，完成后再选择相关检查。测试日志、截图和临时账户统一在 `tests/out`；测试不复制 EXE。代码、脚本和工程配置的注释统一使用英文。

构建和测试只依赖当前工程内的 `GunBrosRe.vcxproj`、`src`、`big`、`assets` 与 `tests`，不读取 `_prep`。只建单个产物时用 `/p:GbProduct=Viewer` 或 `/p:GbProduct=Tests`。原始 BIG、运行媒体和存档样本不提交版本库；新环境需要自行提供这些输入。

程序以 EXE 所在目录解析资源和相对路径，默认账户在该目录的 `saves`。已有 `userdata` 账户可通过绝对 `--profile` 路径继续使用；测试样本不会自动导入正式账户。

Viewer 根据 BIG 内容自动选择 `BigVersion`，只记录三档格式，最新一档为 `1`（含 3.6.0），旧格式依次为 `2`、`3`。集中配置在 `src/gun_bros_re/data/BigVersions.h`，不维护发行版本号列表，也不需要手填版本参数：

| BigVersion | 对象类型数 | 类型分段总数 |
| --- | ---: | ---: |
| 1 | 28 | 33 |
| 2 | 27 | 32 |
| 3 | 26 | 31 |

自动识别同时核对原 `___GAME_TOC_KEYSET` 和 `OBJECT_SCRIPT__COUNTS_`，并按实际对象类型数定位图片、声音、模型和字符串。Viewer 优先读取 `packTOC_xga.dat`，缺少该文件时读取普通 `packTOC.dat`；已有但为空或损坏的 XGA TOC 会报错，不自动换一套资源。未知或混合格式拒绝作为一个完整资源集打开。

使用 `GunBrosViewer.exe --big <资源目录>` 打开六项菜单：地图、原始模型、敌人、玩家武器、玩家装甲、竞技场。退出展示窗口后返回菜单；输入 0 退出程序。也可用 `--map [pack index]`、`--mesh [index]`、`--enemy [index]`、`--weapon [index]`、`--armor [index]`、`--arena [index]` 直接启动。按键沿用现有操作，由右侧英文操作栏按功能完整列出；点击 Hide 收起，点击右侧 < 展开，鼠标在栏内滚动可查看长清单。资源格式在启动时自动识别，不再占菜单项。

六类键位集中在 `src/gun_bros_viewer/ViewerBindings.h`，分别使用 `mapview`、`meshview`、`enemyview`、`weaponview`、`armorview`、`arena` 命名空间；重复键位各自保留。每个定义包含动作、键位、触发方式、分组和英文说明，输入分派与操作栏共用定义。面板与鼠标输入隔离由 `ViewerControls.h/.cpp` 负责，场景按剩余视口渲染。操作栏为 Windows 宿主界面，不读取或改写主程序配置。`pwsh -File tests/run.ps1 -Case viewer-controls` 验证改键、鼠标隔离、收起展开、缩放窗口及绘图状态恢复。

Arena 使用纯黑背景和跟随玩家的等比例镜头，默认放大至 150%；滚轮在场地内缩放（50%–400%），Home 恢复默认倍率。顶部信息栏固定显示 BIG 中的敌人名称、资源位置、武器、收发伤害与命中统计。G 投默认手雷，Q 投冰雷，E 投电雷；道具由原 POWERUP 脚本驱动，实验供应不消耗存档库存。玩家生命无限，但保留敌人攻击、伤害统计、受伤动作和反馈。R 重置战斗，左右方向键切换敌人；无限生命仅用于 Arena。

Arena 信息栏和血量数字使用与操作栏一致的 Windows 系统字体；顶部不重复显示手雷快捷键。蓝框按玩家移动限制及碰撞半径绘制，表示实际场地边界，随镜头缩放、移动，线宽保持 2 像素。

展示场景全部位于 `src/gun_bros_viewer/scenes/`。主包没有新增 viewer 专用模块；地图调用现有地图模块，敌人调用 `EnemyModel`，装备调用 `PlayerModel`、`WeaponEffects`，竞技场调用 `CombatScene`。Mesh Viewer 使用完整原始帧库与原贴图引用，不运行实体拼接；没有原贴图关联的模型明确标记为未贴图。

Viewer 使用 EXE 同目录的 `GunBrosViewer.cfg`，也可通过 `--config <文件>` 指定独立配置。字段、默认值、读取器和窗口标题统一在 `src/gun_bros_viewer/ViewerSettings.h/.cpp`，不再调用 `GameHostSettings`。当前支持 `WindowWidth=1600`、`WindowHeight=1200`、`EffectsVolume=3`（0–10）；窗口尺寸仍受可用桌面范围限制。已有文件保留注释和音量，缺失字段使用 viewer 默认值，旧 `DebugMode`／`IsConnected` 字段提示忽略。Tests 的研究配置另用 `GunBrosTests.cfg`，主程序仍使用 `GunBrosRe.cfg`。

旧 milestone 菜单及 viewer 中的游戏、战役、Movie、自动检查入口已移除；现有检查和研究命令归 `GunBrosTests.exe`，原命令可继续用于回归。资源版本详情使用 Tests 的 `--big-version`，原 3.6.0 固定引用检查使用 `--asset-sample-check`。UI viewer 暂不开发。

运行 `pwsh -File tests/viewer-smoke.ps1` 检查六项展示、敌人拼接、武器开火、原始帧变化和返回菜单。显式传 `--mute`，截图和日志位于 `tests/out/viewer-smoke/`；不运行全量截图基线。相对目录仍按 EXE 所在目录解析，外部资源目录建议传绝对路径。

这三档仅覆盖资源格式。小版本中的实体字段、Movie 或脚本差异仍以具体解析结果为准，不表示旧版玩法和存档兼容；正式菜单与生存流程仍要求 BigVersion 1。`big-version` 专项检查用独立小型 BIG 验证三档、普通/XGA、媒体索引、字符串、错配和截断，不依赖 `_prep` 原版资料。

需要保留 EXE 目录中手工更换的 BIG 时，构建传 `/p:SkipRuntimeStaging=true` 跳过运行资源复制。正常构建仍复制项目原有资源。
