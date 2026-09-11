# Gun Bros Windows 重建

打开 `gun_bro_re.slnx`，使用 Visual Studio 的 x64 配置构建。仅保留 Re、Viewer、Tests 三个 EXE 工程。编译选项、源码清单和自动测试规则直接写在各自的 `.vcxproj` 中。

| 目录 | 用途 |
| --- | --- |
| `src/engine` | 通用资源、渲染、平台与 Glu Script/Movie/Sprite |
| `src/gun_bros_re` | 游戏启动、玩法、菜单、条目与存档 |
| `src/gun_bros_viewer` | 查看器、里程碑和开发入口 |
| `big`、`assets` | 运行所需的原归档、媒体和宿主着色器 |
| `tests` | 检查实现、运行脚本与 `fixtures/saves` 原存档样本副本 |
| `bin/Debug`、`bin/Release` | 全部 EXE 及运行依赖，按配置统一输出 |
| `obj` | 中间文件、调试符号及构建日志 |
| `_prep` | 已整体忽略的历史参考资料，仅在必须核对原版证据时查阅 |

直接运行 `bin/Release/GunBrosRe.exe`；查看器是同目录的 `GunBrosViewer.exe`。Debug 的三个程序全部位于 `bin/Debug`，Tests 不参与 Release 构建。不生成内部静态库，不使用其他 EXE 输出目录。

Debug 和 Release 均启用已有作弊码。VS 的 F5／Ctrl+F5 默认有声音；自动测试通过 `--mute` 显式静音。

Debug 主程序在菜单或战斗中按 **Shift+F3** 打开 BIG 地图浏览器。上下选择、左右翻页、滚轮或鼠标选择条目，Enter 或 LOAD MAP 载入；Esc 取消。试玩中 Shift+F3 可换图，Esc 返回菜单，R 重开，Space 暂停。新增调试键位统一维护于 `src/gun_bros_re/DebugKeys.h`。

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

Debug 构建自动生成 `GunBrosTests.exe` 并运行对应检查：Re 为 `progress`，Viewer 为 `movies`。其他检查按需运行，脚本自动增量构建测试程序：

```powershell
pwsh -File tests/run.ps1 -Case resources,progress,movies
pwsh -File tests/run.ps1 -List
pwsh -File tests/verify-runtime.ps1
```

集中修改时可传 `/p:SkipAutoTests=true`，完成后再选择相关检查。测试日志、截图和临时账户统一在 `tests/out`；测试不复制 EXE。代码、脚本和工程配置的注释统一使用英文。

构建和测试只依赖当前工程内的 三个 `.vcxproj`、`src`、`big`、`assets` 与 `tests`，不读取 `_prep`。原始 BIG、运行媒体和存档样本不提交版本库；新环境需要自行提供这些输入。

程序以 EXE 所在目录解析资源和相对路径，默认账户在该目录的 `saves`。已有 `userdata` 账户可通过绝对 `--profile` 路径继续使用；测试样本不会自动导入正式账户。

Viewer 根据 BIG 内容自动选择 `BigVersion`，只记录三档格式，最新一档为 `1`（含 3.6.0），旧格式依次为 `2`、`3`。集中配置在 `src/gun_bros_re/data/BigVersions.h`，不维护发行版本号列表，也不需要手填版本参数：

| BigVersion | 对象类型数 | 类型分段总数 |
| --- | ---: | ---: |
| 1 | 28 | 33 |
| 2 | 27 | 32 |
| 3 | 26 | 31 |

自动识别同时核对原 `___GAME_TOC_KEYSET` 和 `OBJECT_SCRIPT__COUNTS_`，并按实际对象类型数定位图片、声音、模型和字符串。Viewer 优先读取 `packTOC_xga.dat`，缺少该文件时读取普通 `packTOC.dat`；已有但为空或损坏的 XGA TOC 会报错，不自动换一套资源。未知或混合格式拒绝作为一个完整资源集打开。

使用 `GunBrosViewer.exe --big <资源目录> --viewer` 打开查看器；`--big-version` 或菜单 **81** 显示识别结果和各包类型信息。相对目录按 EXE 目录解析，研究目录建议传绝对路径。`--maps`、`--meshes`、`--mesh 0`、`--map pack2 0` 等原入口沿用自动识别。M1 使用当前资源的字符串引用；原 3.6.0 固定引用检查保留为 Tests 的 `--asset-sample-check`。

这三档仅覆盖资源格式。小版本中的实体字段、Movie 或脚本差异仍以具体解析结果为准，不表示旧版玩法和存档兼容；正式菜单与生存流程仍要求 BigVersion 1。`big-version` 专项检查用独立小型 BIG 验证三档、普通/XGA、媒体索引、字符串、错配和截断，不依赖 `_prep` 原版资料。

需要保留 EXE 目录中手工更换的 BIG 时，构建传 `/p:SkipRuntimeStaging=true` 跳过运行资源复制。正常构建仍复制项目原有资源。
