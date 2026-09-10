# 目录与共享引擎重构方案

状态：按三个包、tests、配置头文件和发行目录要求修订方案；本轮仅更新文档，尚未迁移源码、改名 EXE 或落实 Release 编译隔离。

本文件描述 Windows 重建工程的目标目录。[source_tree.md](source_tree.md) 记录原主程序符号确认的路径；下文的包结构是重构后的安排，不冒充原工程结构。

## 1. 确定的组织方式

源码主要分成三个包：

| 包 | 职责 |
|---|---|
| `engine` | 两个程序共用的引擎实现，承担资源读取、动画、脚本执行、绘制和基础运算。 |
| `gun_bros_re` | Gun Bros 游戏内容及其启动器，包括玩法、菜单、关卡、进度和存档。 |
| `gun_bros_viewer` | 独立资源查看器及其启动器，收纳 milestone 和资源预览；开发构建可调用 tests，测试实现不放在包内。 |

**GluScript、GluMovie、SpriteGlu 一起组成 `engine/glu` 包。** 三者是相关的脚本／动画模块，不要求理解成严格的上下三层依赖。用户所说的 `gluspirit` 在原工程中对应 `src/spriteGlu3/`，类型名是 `CSpriteGlu`；目录统一用 `sprite`，不引入 `Spirit` 的新拼写。

目录只做必要分组：取消上一版独立的 `apps`、顶层 `game` 和大量细分目录。启动器直接放在所属产品包中；不为每个类、每个菜单建立文件夹，也不把几千行文件原封不动换个目录。

## 2. 目标目录

```text
src/
├── engine/                         共享引擎
│   ├── core/                       数学、几何、字符串及基础结构
│   │   └── Paths.h                 共用路径常量和目录约定
│   ├── resources/                  BIG、TOC、流、资源定位与管理
│   ├── graphics/                   绘图、字体、模型、纹理和着色器
│   ├── platform/                   窗口、输入、音视频及平台适配
│   └── glu/                        Glu 脚本与动画包，整体归引擎
│       ├── script/                 原 gluScript 的解释执行
│       ├── movie/                  原 gluMovie 的时间轴和对象播放
│       └── sprite/                 原 spriteGlu3 的精灵与图集动画
├── gun_bros_re/                     游戏内容及启动器
│   ├── main.cpp                    正式游戏入口、启动配置与装配
│   ├── DebugConfig.h               游戏调试热键、作弊口令与开发常量
│   ├── gameplay/                   实体、战斗、地图、AI、关卡流程
│   ├── ui/                         菜单、HUD、动态绑定及转场
│   └── data/                       游戏条目、目录查询、进度及存档
├── gun_bros_viewer/                 查看器内容及启动器
│   ├── main.cpp                    查看器入口与命令路由
│   ├── DebugConfig.h               查看器实验开关、截图及测试默认值
│   ├── viewers/                    各类资源查看页
│   └── milestones/                 原里程碑编号／命令的兼容入口
├── tools/                          现有 Python 提取和研究脚本
└── third_party/                    第三方依赖

# 以下在仓库根，与 src 并列
tests/                              自动检查、实验驾驶、截图辅助
```

`tools` 与 `third_party` 是现有辅助目录，不是再增加两个产品包。`.h`、`.cpp` 就近放在一起，已有类名尽量沿用原符号。小模块先用文件区分；确实拥挤之后再增加一级目录，不预建空目录。

`gun_bros_re` 不再作为整个引擎和查看器的外层容器。当前位于其中的 `engine`、`research`、`milestones`、`glu_script`、`sprite_glu` 将按上图迁出。`runtime` 中的文件按实际职责分配，最终取消该混合目录。

## 3. 为什么需要拆

本轮调研快照中，`runtime` 有 67 个文件；行数仅作定位参考，不作为强制拆分标准。

| 现有位置 | 现状 | 重构方向 |
|---|---|---|
| [GameFrontEnd.cpp](../src/gun_bros_re/runtime/GameFrontEnd.cpp) | 约 7,656 行，菜单、状态、模型预览和大量检查混在一起。 | 菜单按功能拆文件，检查进入 tests。 |
| [MapScene.cpp](../src/gun_bros_re/runtime/MapScene.cpp) | 约 5,594 行，地图、绘制、机关、生存、查看及实验分支共存。 | 引擎运算、游戏世界与实验驾驶分开。 |
| [SurvivalHud.cpp](../src/gun_bros_re/runtime/SurvivalHud.cpp) | 约 1,248 行，HUD、通知、暂停和道具选择集中。 | 在同一个 ui 目录按职责拆文件。 |
| [MapScene.h](../src/gun_bros_re/runtime/MapScene.h) | 正式 `RunSurvival` 暴露多个研究／测试开关。 | 正式会话参数与工具实验参数分开。 |
| [GameFrontEnd.h](../src/gun_bros_re/runtime/GameFrontEnd.h) | 游戏接口暴露大量 `Run*Check`。 | 检查接口归 tests，不进入游戏公共头。 |
| [PackTables.h](../src/gun_bros_re/runtime/PackTables.h) | 实验脚手架逐渐成为正式资源依赖。 | 拆清包级读取与游戏条目查询的职责。 |
| `engine/CBitmapFont.h`、`CKeysetResource.h`、`CMeshBuffer.h` | 反向包含上层包中的资源／模型类型。 | 将公共资源与模型数据一起归入 engine，消除反向依赖。 |

当前已经有 [游戏工程](../gun_bro_re.vcxproj) 和 [研究工程](../gun_bro_research.vcxproj)，分别产出两个 EXE。研究工程设置 `ResearchTools=true` 后导入游戏工程，共享源码分别编译，尚未形成独立共享库。现有 milestone 源文件多数已限制为研究构建；问题还包括夹在正式文件内部的检查代码，不能只搬 milestone 目录。

## 4. engine/glu 的具体范围

| 当前来源 | 目标位置 | 保留的职责 |
|---|---|---|
| `glu_script/` | `engine/glu/script/` | Script、状态、变量、字节码和解释执行。 |
| `gun_bros/CMovie.*`、`runtime/MovieRenderer.*` | `engine/glu/movie/` | Movie 资源、对象、章节、播放状态、插值、区域变换和绘制。 |
| `sprite_glu/` | `engine/glu/sprite/` | SpriteGlu 数据、图集映射、动画步进与精灵展开。 |

这三个模块共用 engine 的资源、字体和绘图能力，作为同一个 Glu 包维护和编译，不额外建立三个独立库。

通用执行逻辑在这里；游戏原生函数的伤害、刷怪、装备和奖励语义仍由 `gun_bros_re` 提供。Movie 的区域回调由实际使用方绑定：游戏填入商品、角色和状态，查看器显示区域和资源信息。Glu 包不认识商店页面、当前账户或工具选中的条目。

`CMovieSprite`、`CMovieRegion`、`CEmbededMovie` 等已有原类职责，可按原函数逐步恢复到这个包内。保留源码中的数据与原生逻辑分工，不把 Flow 行为改成按物品 ID 分支的宿主效果。

当前文字轨、声音轨及嵌套 Movie 的行为缺口需要独立核对。归入一个包不代表已完成原版行为恢复，目录迁移和行为修复分别记录、分别验收。

## 5. 其余代码往哪里放

以下均以当前 `src/gun_bros_re/` 为迁移起点，同名前缀包含对应头文件和实现文件。

| 当前代码 | 目标位置／处理 |
|---|---|
| `engine/` 基础数学、字符串、几何 | `src/engine/core/`。 |
| `CBigFileReader`、流、`CResPackTOC`、`CResTOCManager` | `src/engine/resources/`；保留原格式、索引与引用语义。 |
| `CBitmapFont`、绘制批次、纹理、纯模型数据与骨骼运算、着色器 | `src/engine/graphics/`。字体无需再单独建包。 |
| 窗口、输入适配、音视频播放 | `src/engine/platform/`；游戏启动顺序留在游戏包。 |
| `PackTables` | 包级定位归 engine；`CGameObjectPack` 和游戏实体条目查询归 `gun_bros_re/data/`。 |
| `*Catalog`、`NativeProfile`、`OriginalProfile`、进度及装备数据 | `gun_bros_re/data/`；交互报告归 viewer，自动实验检查拆给 tests。 |
| `PlayerModel`、`EnemyModel`、`CombatScene`、`SurvivalSession`、拾取／机关／Powerup 行为 | `gun_bros_re/gameplay/`；通用绘制和计算调用 engine。 |
| `GameFrontEnd`、`SurvivalHud`、各类 Original 弹窗与选择器、加载和横扫 | `gun_bros_re/ui/`，按页面或独立职责拆文件。 |
| 菜单静态绑定 `.inc` 与对应查询代码 | `gun_bros_re/ui/`；保留生成来源和版本关系，不再建立多层 bindings 目录。 |
| `StartupSequence`、`HostSettings`、产品日志和工具启动入口 | 游戏启动放 `gun_bros_re/`；宿主路径与开发配置使用集中头文件，通用底层能力调用 engine。 |
| `MovieStudy`、资源目录输出、模型／地图预览页面 | `gun_bros_viewer/viewers/`。 |
| 各处 `Run*Check`、自动输入、固定实验种子、截图阶段 | 仓库根 `tests/`，仅开发构建编译／链接。 |
| 旧 `milestones/`、`research/main.cpp` | 分别归入 `gun_bros_viewer/milestones/`、`gun_bros_viewer/main.cpp`。 |

`SurvivalInputDriver`、`OriginalTextLayout` 等混合代码按函数职责拆：正式输入和商品控制码留在游戏，通用排版进入 engine，自动实验输入进入 tests。不是见到一个文件名就把整份文件全部上移。

## 6. 两个程序如何共享实现

正式游戏输出名确定为 **`GunBrosRe.exe`**；附属查看器暂定 **`GunBrosViewer.exe`**。源码包继续叫 `gun_bros_re`、`gun_bros_viewer`，不要求包名和 EXE 拼写相同。沿用现有 Visual Studio/MSBuild，用明确的项目引用构建共享代码，不同时迁移构建系统。

- engine 编译为共享静态库，两个 EXE 都链接它；相同配置只编译一份。
- `gun_bros_re/main.cpp` 单独编译为游戏入口。其余可复用游戏内容可构建为内部静态库，源码仍在 `gun_bros_re` 包内，不再增加一个顶层 game 包。
- viewer 查看纹理、字体、Movie 和普通模型时直接调用 engine；需要角色脚本、装备组装、机关或战斗实验时，链接上述游戏内容库，调用同一实现，不链接游戏的 main。
- engine 不依赖两个产品包；游戏内容不依赖 viewer；viewer 不复制游戏运算，也不通过包含生产 `.cpp` 访问内部代码。

两个进程各自持有窗口、设备、缓存和日志，共享的是实现和原始资源，不是进程内存。先用静态库，不引入 DLL、插件系统或万能全局上下文。

默认查看页只做资源选择、相机／时间／动作控制和结果展示。BIG 解析、依赖加载、动画插值、骨骼、几何及绘制由 engine 完成；伤害、刷怪和奖励等游戏规则由游戏内容提供。两个启动器只配置、装配和进入流程。

两个 EXE 共用原 `big/`。当前 `gun_bros_re.exe`、`gun_bros_research.exe` 的工程输出、脚本、游戏内工具启动入口和说明分别迁移到 `GunBrosRe.exe`、`GunBrosViewer.exe`；旧开发脚本可转发到新名称，发行包不再附带旧名 EXE。查看器暂定名称统一由 Paths.h 定义，后续改名不散改调用点。

### 发行目录

```text
GunBrosRe/                         发行根，也是 EXE 所在目录
├── GunBrosRe.exe                  正式游戏
├── GunBrosRe.cfg                  与游戏 EXE 同名的运行配置
├── GunBrosViewer.exe              附属查看器，名称暂定，可不安装
├── big/                          原 BIG 归档及包索引，只读
├── saves/                        玩家实际存档，可写
├── logs/                         游戏和查看器运行日志，可写
│   ├── GunBrosRe.log              与游戏 EXE 同名
│   └── GunBrosViewer.log          查看器日志，使用时产生
└── assets/                       BIG 外确实需要的运行资源，只读
    ├── shaders/                  当前运行时加载的宿主着色器
    ├── audio/                    BIG 外的背景音乐等音频
    └── startup/                  启动视频、配套音频及启动图片
```

发行顶层按用户指定的 EXE、同名 cfg、big、saves、logs、assets 收敛。游戏名称统一使用已确定的 `GunBrosRe`，对应 `GunBrosRe.exe`、`GunBrosRe.cfg`、`logs/GunBrosRe.log`。不带源码、Python、BT、反编译程序、解包目录、tests、out、userdata、旧配置文件或研究报告。assets 仅放实际运行依赖，不把解包文件全集复制进去，也不代替原 BIG 中的数据。

当前工程动态链接 `SDL3.dll` 并在构建后复制到 EXE 旁，尚不能直接删掉。保持现有链接方式时，它属于发行根必需的运行库文件；打包还需核对实际 CRT 等依赖。若最终要求根目录连运行库 DLL 都没有，须另行验证静态链接后再移除，不能靠挪进 assets 掩盖加载依赖。上述目录树描述产品目录，运行库文件按最终链接结果补齐。

### Paths.h 的唯一约定

- 共用头文件 `engine/core/Paths.h` 集中定义产品基名 `GunBrosRe`、暂定 `GunBrosViewer`，并据此确定 EXE、cfg 和 log 文件名，以及 `big`、`saves`、`logs`、`assets` 等相对路径常量，不包含开发机器绝对地址。
- 平台层从当前模块路径取得 EXE 所在目录，启动时一次性形成实际路径对象并传入使用方。双击、快捷方式、终端、查看器启动游戏都使用相同规则，不依赖当前工作目录，也不向父目录搜索仓库兜底。
- 默认只从发行根读取资源和写入账户／日志。已支持的 `--big`、`--profile` 显式覆盖各自目录，不连带改变其他路径；相对参数也统一相对 EXE 目录解析，开发脚本优先传绝对路径。
- 读取资源不自动创建空 big/assets；缺少资源时报告完整路径。首次运行仅按需要创建 saves/logs 和游戏同名 cfg，不出现 out、userdata 或源码目录。
- 正式程序不再依赖 `ASSET_ROOT` 指向仓库。开发调试使用打包暂存目录，或启动器明确传入开发资源位置；不能保留隐式“找不到就读源码树”的发行分支。
- 日志使用固定同名路径 `logs/GunBrosRe.log`、`logs/GunBrosViewer.log`，不再把 PID 拼进文件名；会话时间和 PID 记录在日志内容中。两个程序不互相覆盖，测试日志另走隔离输出。

### 当前路径的迁移表

| 当前使用位置 | 发行目标／处理 |
|---|---|
| `ASSET_ROOT/big/` | `<exe-dir>/big/`。 |
| `ASSET_ROOT/userdata/saves/` | `<exe-dir>/saves/`；已有账户显式导入到新位置，不能覆盖旧账户或只读样本。 |
| `ASSET_ROOT/userdata/logs/` | `<exe-dir>/logs/GunBrosRe.log`；查看器写自己的同名日志。 |
| 仓库 `gunbros.cfg` | 正式运行配置迁移到 `<exe-dir>/GunBrosRe.cfg`，开发开关转入 DebugConfig.h。 |
| `ASSET_ROOT/src/gun_bros_re/shaders/` | `<exe-dir>/assets/shaders/`；由构建／打包步骤复制必要文件。 |
| `ASSET_ROOT/mp3/` | `<exe-dir>/assets/audio/`；沿用经核对的音乐引用。 |
| `ASSET_ROOT/glu_logo/`、`png/Default-Landscape.png` | `<exe-dir>/assets/startup/` 中对应文件。 |
| `out/ui-original-*/`、`out/validation/`、`out/ui-movies/` 和其他固定测试输出 | 仅开发检查使用，逐步统一到明确指定的 TestRunRoot。历史产物原地保留。 |
| 仓库现有 `saves/` | **仍为只读原存档样本，不是发行玩家存档。** 不因同名而改变其权限或用途。 |

发行包应在独立暂存目录装配，例如 `dist/GunBrosRe/`；不能直接把仓库根作为发行根。新玩家目录不附带研究样本或测试账户，从原初始化逻辑和 BIG 默认数据创建存档。需要导入旧存档时使用明确的源／目标，移除现有启动时自动从仓库 saves 导入的隐式依赖。

### 测试路径单独传入

tests 驾驶器从测试脚本取得明确的绝对输出根，再创建 `out/tests/<run-id>/<suite>/`，其中按需要放 saves、logs、images 和报告。脚本以自身位置定位仓库，不以调用时的工作目录猜根路径。每轮使用独立 run-id；测试函数接收该路径，不各自拼写带日期的 out 路径。

原 BIG／assets 可作为显式只读测试输入共用；需要变异的样本先复制到本轮实验目录。测试存档只能在本轮输出下，不能回退到 `<exe-dir>/saves/`、仓库 saves 或旧 userdata。截图与报告保留，默认不自动删除已有产物。

Release 不含测试输出路径的执行入口，不运行 tests，不生成 images／画廊／实验目录。发行验收在干净暂存目录进行：改变工作目录、把目录移动到含空格／中文的新位置，两个 EXE 仍能启动；正常写入限于游戏同名 cfg、saves/logs，撤去查看器不影响正式游戏。

## 7. 长文件怎么拆，拆到什么程度

**GameFrontEnd**：先把检查函数提取到 tests，再按商店、星图／选关、结算／精炼、设置等职责拆成同目录中的文件。每个菜单自己的状态、更新、绘制和输入放在一起，流程控制只切页面和接收完成事件。沿用有证据的原类名，不保留一个全页面共享的巨型状态结构。

**MapScene**：通用资源／绘制计算进入 engine；地图实例、机关和关卡状态进入 gameplay；交互地图查看进入 viewer，自动实验驾驶进入 tests。正式生存入口不携带 `feedbackStudy`、`performanceStudy` 等工具开关，也不把这些开关原封不动塞进一个参数结构掩盖问题。

**MovieRenderer**：在 `engine/glu/movie/` 内区分资源、播放实例与绘制职责。游戏和查看器使用同一套章节、时间和区域结果，调用者无需自己重算。按实际复杂度拆文件，不要求每个小结构都单独建类或文件夹。

数百行但职责单一的解析器可以保留；同时包含解析、页面、仿真和检查的文件应拆开。目标是看名字能知道去哪里找，而不是追求最短文件或最多目录。

## 8. milestone 和检查永久保留

资源查看器按归档、纹理、字体、精灵、Movie、模型、角色、地图、脚本组织入口。旧 milestone 编号、名称和命令保留为兼容路由；实施前登记全部现有入口，逐项验证，不靠历史数量猜覆盖情况。

Arena、SurvivalPilot 和旧战役研究继续保留为资源／行为实验，使用同一个游戏内容实现。普通资源选择和预览留在 viewer；跳波、固定种子、无敌和自动输入等实验控制仅供开发构建。自动检查统一放在根目录 tests，由开发版 viewer 的命令模式调用，不交付第三个测试产品。Release 查看器保留正常资源浏览，但不注册测试别名；旧实验能力在源码和开发构建中永久保留。

查看器默认只浏览资源，不写玩家账户。开发版预览奖励或购买使用显式实验账户，不能修改发行根的 `saves/` 或旧 `userdata/saves/`；仓库原 `saves/`、BIG、解包样本及 iOS 主程序继续只读。游戏不安装 viewer 也必须正常运行。

## 9. tests 的位置、数量与 Release 隔离

测试放在**仓库根 `tests/`，与 src 并列**。先按 `MenuChecks.cpp`、`SceneChecks.cpp`、`ResourceChecks.cpp`、`ProfileChecks.cpp` 等职责组织，截图辅助集中到 `Capture`，不引入复杂测试框架或多层目录。

### 当前清点

统计口径：当前 `.cpp` 中顶层 `int Run…Check(...) {` 的函数定义，排除头文件声明和调用，不等于断言数或独立测试用例数。

| runtime 文件 | 检查入口数 |
|---|---:|
| GameFrontEnd.cpp | 20 |
| SurvivalHud.cpp | 4 |
| ArmorCatalog.cpp | 2 |
| NativeProfile.cpp | 2 |
| OriginalProfile.cpp | 2 |
| PickupCatalog.cpp | 2 |
| MissionCatalog.cpp、MovieStudy.cpp、OriginalPowerupSelector.cpp、PowerupCatalog.cpp、PropCatalog.cpp、StartupSequence.cpp、StoreCatalog.cpp | 各 1，共 7 |
| **runtime 合计** | **39** |

整个 `src/gun_bros_re` 按同一口径共 **42 个入口**，另外 3 个为 `RunLevelFlowCheck`、`RunWeaponCheck`、`RunDailyBonusCheck`。这不是全部测试场景：`RunSurvival` 等函数的 check／performanceStudy／feedbackStudy 分支、多个模式共用的检查入口，以及 Survey 和画廊扫描未计入。实施时补登记这些分支，不能只搬 42 个函数就称全部移出。

runtime 中有 **75 处 `SaveFrame()` 调用，分布于 8 个文件**：GameFrontEnd 43、MovieStudy 13、SurvivalHud 8、MapScene 5、OriginalPowerupSelector 2、StartupSequence 2、LoadingScreen 1、PickupCatalog 1。它们包含检查和预览截图，不是 75 张图片；循环和画廊一次可生成多张。

本轮只清点，**现有检查、截图能力及已有图片都保留，不删除**。迁移后测试图片统一写到 `out/tests/<run-id>/<suite>/`，原截图和报告继续可查，不批量迁移或清理历史文件。普通启动不自动生成图片；截图只由开发构建中明确运行的测试／截图命令触发。

### Release 是编译隔离，不是配置关闭

当前尚未满足要求：正式 main 仍接受 `--screenshot`，`CWindow::SaveFrame` 是普通成员实现；GameFrontEnd 调用 `EnableCheats(true)`，并处理 `chm/cht/chd/chc`。`gunbros.cfg` 的 `DebugMode=0` 只是运行时状态，不能证明发行程序没有这些能力。

| 构建 | 自动测试／截图 | 调试热键／作弊 | 正常游戏／资源浏览 |
|---|---|---|---|
| Debug | 显式启用，检查由开发版 viewer 调用 | 采用对应 DebugConfig.h 的配置 | 保留 |
| Release（两个 EXE） | **不编译、不链接、不注册命令** | **排除开发绑定表与处理器** | 保留 |
| Validation（需要验证优化构建时） | 显式启用，独立输出目录，不作为发行版 | 仅验证所需能力 | 使用与 Release 一致的优化设置验证共享实现 |

Validation 是构建配置，不新增产品包，需要时再加入。不能为了在优化构建跑测试而把功能放回 Release。

- tests 源码／库只加入开发测试目标。移走生产文件里的检查函数及分支，不能仅依赖链接器碰巧裁掉未使用代码。
- 测试截图的帧读取、翻转和 PNG 落盘归测试辅助或开发专用目标；正常绘制及资源解码继续由 engine 提供。产品类不常驻测试用的 SaveFrame 接口。
- 构建配置集中定义能力宏，如 `GB_ENABLE_TESTS`、`GB_ENABLE_CAPTURE`、`GB_ENABLE_CHEATS`，Release 固定为 0；开发绑定表、少量开发接口的声明和实现一起受控。DebugConfig.h 不得覆盖 Release 的禁用约束，也不靠 constexpr false 代替编译排除。
- Release 的 main、帮助、热键、菜单、旧命令转发均不提供测试／截图／作弊入口。未知参数给出说明，不再无条件转发到研究程序。
- `test-muted.ps1` 等脚本跟随新配置和 EXE 路径调整，不能继续默认 Release 带检查功能。
- 验收同时检查编译项／链接依赖和运行行为：Release 接收 `--screenshot`、`--movie-gallery`、检查参数或作弊口令时不得执行这些功能，不生成测试图片；正常游戏及资源查看仍可用。验证处理器与命令注册确实被排除，而非仅默认关闭。

## 10. 配置头文件与游戏同名 cfg

路径、调试热键、作弊口令和实验默认值继续集中到独立 .h。按用户补充，发行根保留 `GunBrosRe.cfg`，用于需要运行时读取／保存的正式宿主设置；它与编译期配置头分工，不把路径常量和调试绑定搬回配置文件。不再新增 config 目录或独立 paths.ini/debug.ini。

| 头文件 | 内容 | 使用范围 |
|---|---|---|
| `engine/core/Paths.h` | 产品基名、同名 EXE／cfg／log 及 big／saves／logs／assets 的相对路径常量 | 两个产品共用；测试输出由驾驶器显式传入，测试专用常量按开发能力宏隔离。 |
| `gun_bros_re/DebugConfig.h` | 游戏调试开关、热键、作弊口令到命令名的绑定及必要开发常数 | 仅 Debug／Validation 使用，Release 排除开发表及处理器。 |
| `gun_bros_viewer/DebugConfig.h` | 查看器实验热键、截图开关、固定实验种子和测试默认值 | 仅 Debug／Validation 使用，Release 保留正常资源查看。 |

两个 DebugConfig.h 分别管理各自产品的开发配置，不重复维护同一组常量；共用路径统一引用 Paths.h。引擎不因共用路径而依赖游戏作弊命令。

使用 C++17 的 `inline constexpr`、有意义的命名空间及简单常量表，避免头文件重复定义。头文件只定义配置，不在全局初始化时读取文件、创建目录或初始化窗口；修改常量后重新编译。

Paths.h 保存相对目录约定，不写死开发机器的绝对路径。实际路径统一遵守第 6 节的发行根规则；`--big`、`--profile` 仅覆盖各自目录，路径保留空格和 Unicode，不随当前工作目录漂移。头文件定义约定，解析后的路径由运行实例持有，不用宏到处拼接字符串。

调试绑定按“键／口令 → 命令名称”集中登记，操作实现仍在所属产品的开发源文件中；更换别名无需改输入循环。正常移动、射击等正式输入与 debug hotkey 分开，Release 去掉调试键不能破坏正常操作。

现有 HostSettings 的读取在迁移时调整为游戏同名 cfg，仅保留经确认需要持久化的正式宿主设置；开发默认值转入对应头文件，可切换状态由运行实例持有。cfg 沿用简单键值形式，避免新增通用配置框架；缺失键采用集中默认值，Release 不接受调试／作弊项，也不能通过编辑 cfg 启用未编译的功能。`IsConnected` 属于宿主模拟，不是真实连接事实，不在 Release 中提供模拟开关。旧 `gunbros.cfg` 不随发行包保留。本轮未修改实际读取逻辑；迁移时更新调用和说明。原版游戏选项及存档仍按原客户端规则持久化，不在 cfg 重复维护，也不改成编译期常量。

配置头允许放宿主路径、日志和开发控制，**禁止放从 BIG 抄出的 UI 坐标、价格、武器属性、奖励和关卡表**。原版资源事实仍从 BIG 读取，原算法常数留在有来源注释的对应实现。不把所有数字都塞进万能 Constants.h；格式枚举和单位换算等明确的模块常数仍就地维护。

## 11. 执行顺序与验收

这次修订确定包名和目录方向，下面的代码迁移任务尚未执行。

| 阶段 | 工作 | 验收 |
|---|---|---|
| 1 基线与工具分离 | 登记现有入口和可玩场景，先把长文件中的检查驾驶抽到根目录 tests。 | 游戏公共头不再暴露检查函数，旧检查在开发构建仍可执行。 |
| 2 三包与共享构建 | 建立 engine、gun_bros_re、gun_bros_viewer 的工程归属和共享库引用。 | Debug／Release 两个 EXE 构建通过；engine 无反向依赖；Release 不编译／链接测试、截图和作弊入口。 |
| 3 Glu 包与公共能力 | 合并 script/movie/sprite 至 engine/glu；迁移资源、字体、模型等共享实现。 | 游戏和查看器对相同资源的解析、播放和依赖结果一致；不复制第二份算法。 |
| 4 拆长文件 | 逐项拆菜单、HUD、地图和战斗，把检查驾驶留在 tests，路径及开发配置集中到独立 .h。 | 对应页面、动画、输入和战斗回归通过后再拆下一项。 |
| 5 打包与收尾 | 输出 GunBrosRe.exe 与暂定 GunBrosViewer.exe；统一同名 cfg/log、发行四目录与 assets 依赖，更新脚本、旧 milestone 路由、工程过滤器和文档。 | 干净发行目录可移动、可从任意工作目录启动；正常写入限于同名 cfg 和 saves/logs，Release 无测试／截图／作弊功能，工具不写正式账户。 |

每次只迁移一个可验证模块，保留同一输入和隔离账户的前后对照。常规运行、截图和自动检查统一 `--mute`；使用已有 Core、OriginalUI 等检查时核实实际覆盖范围，不把列出命令当成已经运行。

资源解析核对字段顺序、宽度、引用和文件边界；播放器核对章节、反向、显隐和完成事件；最终可玩回归覆盖进入地图、移动射击、刷怪、受伤死亡、重开、结算及进度重载。结构迁移和玩法纠错分别记录，未解决的行为缺口不能被重构完成掩盖。

## 12. 保留约束

- 保留用户全部注释和原函数依据；过时说明追加纠正，移动时更新引用和原名映射。
- 原资源数据继续从 BIG 获取；不把硬编码搬进新包或配置文件后称为数据驱动。原程序静态表保留可再生成关系。
- 保留已有模块中正确的原版行为。Windows 平台适配与原类职责区分，不凭新文件名补造原类。
- 不新增模糊的 runtime、misc、万能 common 目录，不按行数机械切文件。
- 本轮仅修订方案，未执行源码重排、重命名或构建改造。
