# UI 包职责重组结果

日期：2026-09-19。范围：用户授权的 `src/gun_bros_re/ui` 全包整理、相关调用方和测试。本次实施与下述验收已完成；未完整恢复的原版能力在末节列明。

## 目录与所有权

原 UI 根目录有 52 个 `.h/.cpp`，其中 29 个 Z 文件；重组后为 102 个源码文件，其中 23 个 Z 文件。根目录只保留原来的 7 个 `.inc`。数量按物理文件统计，不把重命名当作行为恢复。

| 目录 | 职责与主要类型 |
|---|---|
| `system/` | `CMenuSystem` 的页面协调、`CMenuStack` 的活动页/待切换页/历史、`CMenuNavigationBar` 的导航条播放 |
| `menus/` | 商店、任务、星图、好友、挑战、炼油、结算、欢迎、设置、玩家选择等原菜单职责 |
| `controls/` | `CMenuMovieButton`、`CMenuMeshPlayer`、`CTextBox`、弹窗；保留明确标识的 Windows 滚动和推广入口适配 |
| `content/` | `CMenuDataProvider` 的原静态表、`CStoreAggregator` 的筛选与装备规则 |
| `hud/` | 原有 `CInputPad`、`CPowerUpSelector`，以及必要的宿主状态快照和资源适配 |
| `host/` | Windows 窗口、输入、会话循环、GL 资源生命周期、加载/转场、本地在线与购买模拟 |

原 `ZMenuInternal.h`（1084 行）已拆除。原 `ZStoreMenu.cpp`（1396 行）的筛选、卡片、分类/换枪、购买等待、模式覆盖层已分别归位。原 `ZPostGameMenu.cpp`（723 行）中的升级弹窗、在线结算和独立卡片不再混在一处。

商店分类、筛选、换枪声明归 `CMenuStore.h`；按钮绘制声明归 `CMenuMovieButton.h`；购买与模态声明归 `host/ZStorePurchase.h`。商品卡头不再承载这些跨模块入口。

页面状态由对应 `CMenu...` 类型持有。角色模型与换枪播放归 `CMenuMeshPlayer`；挑战选中、选项光效和侧栏归 `CMenuChallenges`。星球图片及标记绘制归 `CMenuMission::Presentation`；模式、结算和炼油粒子分别归原页面的嵌套 `Effects`。这些 GPU 实例由窗口聚合持有，确保在 GL 上下文销毁前释放；嵌套辅助类型不是声称发现了原版新类。

## 本次按原实现修正的行为

1. 商店实际绘制 BIG 的 `STORE_MENU` 底层，移除宿主固定黑矩形及商店误用的星图背景；炼油背景不再重复绘制。宿主不再统一画 Movie47；任务详情独立绘制该共享星图 Movie，保留星图游标且不重绑旧页的星球回调。其他页面按各自原绑定绘制。按 `CGameApp::HandleRender` 将宿主清屏改回黑色；设置/炼油共享背景以本页 delta 推进，普通 Bind 保留游标，重建菜单系统时归零。
2. 商品按钮按照原 `CMenuMovieButton` mode 3 播放：释放后进入选中章节，越过章节结束时派发一次动作。禁用、滚动、切页会取消旧动作；指针离开列表不取消已经选中的播放。
3. PREVIEW 为幂等选择，不因重复点击退出预览；预览不直接改正式装备。
4. 筛选恢复原类别、持有位18、装备位19的组合语义。ALL 与取消最后选项的空筛选分开。移除“零价道具就是奖励”的推断式排除，保留原 hidden 和 displayOrder。
5. 导航记录 pending，在宿主帧边界提交，避免页面绘制途中切换导致同帧叠画。欢迎页仍等待原倒放与一次奖励提交。升级弹窗通过正文页查询绘制底页，不再临时改活动页号。转场提交帧与后续 WIPE 帧均屏蔽输入，避免新页在遮罩创建前接收穿透点击。
6. 显式 `.dat` 研究入口也通过 BIG 的 `Planet → Mission.type1 → Level → mapRef` 取得地图，删除 `kPlanetPacks/kPlanetMaps`。查询与原存档初始化共享，不生成手工替代数据。

筛选一手证据、真实资源引用及边界见 [筛选核对](ui-store-filter-source-audit.md)；导航、退出及跨分支区别见 [导航核对](ui-navigation-source-audit.md)。背景绑定、共享回调清理与全局清屏依据见 [背景核对](ui-background-source-audit.md)。其他原实现证据见 [商店职责研究](ui-store-source-research.md) 和 [行为核对](ui-behavior-source-audit.md)。

## 保留的资源与自创功能

- 七个 `.inc` 全部保留原路径与原始字节，已与重构前 SHA-256 清单比较。它们仍被代码包含，保存原主程序静态绑定，不能视作未使用垃圾。
- BIG 继续提供 Movie 布局、章节、Sprite、字体、文本、商品和星球数据。主二进制提取表负责资源绑定、动作和枚举；C++ 实现组装、状态与绘制调用。
- `TAP TO CONTINUE`、Windows 输入、调试入口和本地在线模拟保留。作弊反馈的文案状态与绘制移到 `cheats/CheatFeedback.h`。启动引导属于正常宿主 UI，不因自创就归入 cheats。
- 角色投影验证断言与独立期望计算移入 `tests/ui/StoreChecks.cpp`。生产模型只公开实际渲染矩阵。帧注入改为通用 `ZMenuInputFrame/InjectTap`；断言和场景编排仍在 Tests 产物。

## 验证记录

后续用户发现已装备商品仍显示 EQUIP：本轮原有检查未覆盖按钮隐藏，且旧双枪检查包含错误预期。现已修正商品卡逻辑并增加实际渲染回归，详见 [已装备按钮修复](ui-equipped-button-fix.md)。下述历史通过记录不代表该问题当时已修复。

阶段构建、运行输出位于本地忽略目录 `obj/ui-refactor-20260919/`，最近一批运行的测试样本和截图位于 `tests/out/`；该目录会由脚本重建，选定截图与 Release 结果另存于上述日志目录的 `debug-regression-evidence/`、`release-regression-evidence/`。测试脚本自动传入 `--mute`，并核对 BIG、原存档样本和用户数据的受保护文件哈希。

- 阶段 8：商店卡片、银行和 UI 提示通过；礼包的重新装备检查发现仍按立即点击断言。随后根据真实按钮章节时长修正等待，保留装备和存档断言。
- 阶段 13：`offline-social, planet-menu, refinery-menu, postgame-menu, store-template, package-purchase, dialog`，7/7 通过，受保护文件变化为 0。
- 新增真实 BIG 的 4 条商品 × 6 种筛选组合验证，以及 pending/busy/只提交一次的导航检查；未改写原资源或构造替代商品表。
- 最终 Debug/Release 的 Game、Viewer、Tests 六个产物均构建成功，退出码 0。日志分别为 `final-debug-build4.log`、`final-release-build3.log`。
- Debug 定向回归 18/18 通过：`original-saves, mastery-upgrade, options, offline-social, planet-menu, mission-menu, header, refinery-menu, greeting, player-select, postgame-menu, store-template, ui-feedback, package-purchase, promotion, loading-wipe, scene-transition, dialog`。见 `final-debug-regression.log`。
- Release 最终回归 13/13 通过：`mastery-upgrade, options, offline-social, planet-menu, mission-menu, refinery-menu, greeting, player-select, postgame-menu, package-purchase, loading-wipe, scene-transition, game-menu`。含实际 Game 入口，Release Game 不截图，菜单图片由 Tests 产物生成。见 `final-release-regression.log`。
- 黑色清屏与背景游标最后合入后，Debug 补查 `options, refinery-menu, loading-wipe, game-menu`，4/4 通过，受保护文件变化为 0；见 `final-debug-background.log`。
- 另有重组后商店卡片、银行、原 HUD、暂停菜单、道具选择器的定向检查通过。上述批次 BIG、原存档样本和用户数据的受保护文件变化均为 0。
- 转场回归覆盖 1400ms 冷加载、提交帧点击和遮罩中途点击；两个跨分支目的页正确，遮罩中点为 350/700ms，各分类切换不触发跨分支转场。截图人工检查覆盖任务详情、结算及正式商店入口。
- 最终完整性检查：7 个 inc 的 SHA-256 无变化、失效 UI include 为 0、`git diff --check` 通过。原解释性注释随职责迁移保留，文件头的文件名对应新位置更新。

构建曾遇到 MSVC HostX86 的 C1001/D8040 内部错误，改用已安装的 x64 宿主编译器及四个编译进程后继续。未为绕过编译器错误修改玩法逻辑或永久改工程架构。

## 复刻边界

这次恢复的是有证据的职责、状态所有权和已核实行为，不能等同于恢复了完整原工程源文件、继承树和虚表。

- `CMenuStack` 已区分 pending 与 active，并接入欢迎页忙状态；原版各页面全部 OnExit/Hide 子控件编排、完整异步 Load/Bind 与双分支资源生命周期尚未完整恢复。现有 WIPE 保留为独立宿主转场，不能称为完整原 CMenuSystem 分支系统。
- 原在线商品 override 标签、IAP 产品尾缀和完整排序附加优先级仍有研究边界，未根据价格或截图补造。
- `CTextBox` 归拢现有已支持的字体切换与换行布局，不宣称覆盖原富文本解析器所有 token。
- `ZMenuScrollMotion`、`ZPromotionPopup`、`ZLoadingScreen`、`ZMenuWipe`、资源快照和窗口适配保留 Z。它们确实含 Windows/本地模拟逻辑，仅改成 C 名称会误导后续逆向。

初始方案保存在 [UI 职责方案](ui-responsibility-plan.md)，本文和实际源码覆盖其中已过时的“当前实现”描述。
