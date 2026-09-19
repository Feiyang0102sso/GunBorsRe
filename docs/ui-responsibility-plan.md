# UI 原版职责对照与拆分方案

日期：2026-09-19。状态：用户已授权并实施；本文保留初始调研快照，最终归属、行为修正与验证见 [UI 重组结果](ui-optimization-result.md)。

## 结论与依据范围

下一阶段以 `ZMenuInternal.h`、`ZMenuFlow.cpp`、`ZStoreMenu.cpp` 为入口，恢复菜单系统、页面、控件、内容提供者各自的状态所有权。已有 `CInputPad`、`CPowerUpSelector` 和 Movie 解析能力继续使用。迁移单位是一个可验证的职责及调用链。

本文核对当前生产代码、iOS 3.6.0 反编译实现、原文件符号索引和 `ui_movie.bt`。文中反编译行号均指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`，不代表恢复的原 C++ 文件行号。原类身份与下述 Windows 目录分组分别说明；目录分组是建议。

本轮没有重新解析 BIG 全量样本，没有完整还原虚表，没有确认所有菜单动作分支。方案不能作为全部 UI 行为已经对齐的结论。

## 一、当前问题所在

| 当前代码 | 已观察到的职责混合 | 拆分目标 |
|---|---|---|
| [ZMenuInternal.h](../src/gun_bros_re/ui/ZMenuInternal.h)，1084 行 | 页面状态、历史、购买等待、角色预览、鼠标、窗口、资源缓存、导航、页面粒子、测试记录集中 | 分别交给页面、菜单系统、内容提供者、宿主适配和测试 |
| 同文件 `ZMenuState::Navigate/Back` | 导航直接重置商店、星图、设置、炼油厂、结算等私有字段 | 栈处理切换；页面在自己的显示、退出、焦点流程中维护状态 |
| [ZMenuFlow.cpp](../src/gun_bros_re/ui/ZMenuFlow.cpp) | 主循环了解页面编号、页面关闭条件、奖励、输入屏蔽和渲染顺序；升级弹窗通过临时修改 `state.page` 绘制底页 | 系统调度当前菜单和弹窗；业务动作只调用所属模块 |
| [ZStoreMenu.cpp](../src/gun_bros_re/ui/ZStoreMenu.cpp)，1396 行 | 分类、过滤、商品卡、换枪、购买、装备、提示和模式覆盖层混合 | 商店、商品卡、商品聚合器、预览、弹窗和模式覆盖层分归原类 |
| [ZMenuPreview.cpp](../src/gun_bros_re/ui/ZMenuPreview.cpp) | `ZGameMenu` 持有角色预览的装备副本、换枪状态、声音和敌人预览 | 角色预览归 `CMenuMeshPlayer`；敌人继续使用已有 `CMenuMeshEnemy` |
| [tests/ui/MenuChecks.h](../tests/ui/MenuChecks.h) | 测试直接包含巨型内部头，多个测试操作 `ZMenuState/ZGameMenu` | 随每批迁移调用真实模块接口；测试输入脚本和轨迹收集归 tests |

行数只帮助定位；验收以状态所有权和行为一致性为准。

## 二、原版真实组织

### 2.1 系统与栈

- `CMenuSystem` 有 `SetBranch/PushMenu/SetMenu/PopMenu`、导航条、共享弹窗和转场。`Update` 处理弹窗输入屏蔽、导航繁忙状态、旧新分支与 Movie 完成；`Draw` 区分正文、导航、页面覆盖层和弹窗。依据：反编译 96214、96614、96666、96700、96761、96871；[原实现](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c)。
- `CMenuStack` 保存栈记录，以及当前菜单和待切换菜单。`PushMenu` 保存当前选择索引，`SetMenu` 创建待进入对象，`Update` 等待当前对象结束后交接；`LoadMenu/BindMenu` 处理加载与绑定。依据：147654、147695、147785、147889、148246。不能将它等价成一个 `vector<unsigned>` 的立即切页。
- `CMenuNavigationBar` 自己更新 Header Movie、等级和货币内容以及按钮。依据：143145、143166、143357、143538。当前 `ZGameMenu::Header` 应迁回这个职责。
- `CMenuStack::CreateMenuInstance` 明确创建 `CMenuList`、`CMenuSplash`、`CMenuPlayerSelect`、`CMenuMission`、`CMenuMissionInfo`、`CMenuPostGame`、`CMenuGameResources`、`CMenuStore`、`CMenuFriends`、`CMenuGreeting`、`CMenuChallenges`。依据：148162。页面类映射应从这里核实，不能按当前中文页面名推定原类。

### 2.2 Movie、控件与资源

- `CMovie` 的布局区域、章节和时间数据来自 BIG；动态内容由菜单回调绑定。`STORE_MENU` 四个用户区域分别绑定内容、分类、角色模型和换枪按钮。依据：[ui_movie.bt](<../_prep/_Big_tool/binary template/big_assets/ui_movie.bt>)、`CMenuStore::Init` 180199；[资源定位记录](../_prep/docs/ui-binary-templates.md)。
- `CMenuMovieControl` 负责选项列表、选择、滚动速度、越界回弹和触摸；`CMenuMovieButton` 负责显示、隐藏、选中及相应动作时序。依据：140797、140984、141086、141506、141622、141837、142239、144483、144715。当前 `ZMenuScrollMotion` 是带 Windows 输入的重建实现，不能仅改类名就视为完整原控件。
- 引擎已有 [CMovie::Playback](../src/engine/glu/movie/CMovie.h)。菜单/控件使用独立播放实例，共享只读资源；原生控件的状态转换仍由控件处理。迁移前逐项核对播放速度、反向、循环和完成事件，不能把所有手动时间字段直接批量替换。
- `CMenuDataProvider` 原实现有静态配置注册、内容创建及元素动作查询等职责，入口见 148357 之后。现有 `ZMenuData.inc` 是原程序静态表提取结果，属于消费逻辑证据；BIG 里的价格、文本和布局仍从 BIG 读取。[当前入口](../src/gun_bros_re/ui/ZMenuData.h)。

## 三、建议目录与文件归属

以下是目标分组，按阶段落地，不一次创建全部空类或接口。

```text
ui/
  system/       CMenu、CMenuSystem、CMenuStack、CMenuAction
  controls/     CMenuNavigationBar、CMenuMovieControl、CMenuMovieButton
                CMenuOption、CMenuOptionGroup、CMenuListOption
                CMenuMesh、CMenuMeshPlayer、CMenuPopupPrompt
  content/      CMenuDataProvider、CStoreAggregator，以及有来源的静态表
  menus/        CMenuStore、CMenuStoreOption、CMenuStoreOptionGroup
                CMenuMission、CMenuMissionInfo、CMenuMissionOption
                CMenuGameResources、CMenuPostGame、CMenuPostGameOption
                CMenuList、CMenuGreeting、CMenuPlayerSelect
                CMenuFriends、CMenuChallenges、已有推广/升级/加载菜单
                CMenuMovieMultiplayerOverlay
  hud/          CInputPad、CPowerUpSelector、ZHudResources、ZHudState
  host/         ZGameFrontEnd、ZMenuSurface、ZMenuWipe、ZLoadingScreen
```

- `system/controls/content/menus` 是当前宿主工程的组织建议；原文件名依据见 [source_tree.md](../_prep/_IDA_OUT/source_tree.md)。原版是多个 menu*.cpp，不能将这些新目录称为恢复出的原目录。
- `CStoreAggregator` 原由 `CGunBros` 创建（80235），负责共享商品业务，包括筛选缓存、状态、交易、装备及预览配置操作。放入 `ui/content/` 是当前分组建议，其生命周期不属于单张卡片或单个商店页面，也不只是展示内容查询。原 `CStoreItem`、`CProfileManager`、`CRefinementManager` 和 BIG 读取继续在 `data/`。商店详细核对见 [商店专项研究](ui-store-source-research.md)。
- 不另造第二套 Movie/Sprite 引擎。`src/engine/glu/movie` 的解析、播放和绘制适配保持原归属。
- 本地联机模拟和用户要求的模拟购买策略继续归现有 `host/`，只拆 UI 与模拟策略之间的调用。它们不能因菜单迁移而被包装成原 iOS 网络实现。
- 已有敌人 `gameplay/enemy/CMenuMeshEnemy` 可继续由敌人模块维护；本阶段不为目录整齐再次移动它。
- 商店准确原名为 `CMenuStoreOptionGroup`，不能使用未经证实的 `CMenuStoreGroup`。其两个前置选项在原工厂中为 `CMenuTapjoyOption`（233952、175443）；特殊卡与普通商品卡的映射须单独保持，宿主替代行为继续明确标记。
- `CMenuGameResources::CResourceMeter/CTransferEffect` 优先保留原嵌套类型归属，可使用同一类的多个 cpp 文件；不额外制造宿主顶层类。

### 3.1 当前文件到原职责的映射

| 当前文件/内容 | 目标职责 | 依据与边界 |
|---|---|---|
| `ZMenuFlow.cpp` 的分支、切页、弹窗、输入屏蔽 | `CMenuSystem`、`CMenuStack` | 96761、96871、147889；窗口泵、截图和键盘适配归宿主 |
| `ZMenuNavigation.cpp` 的 Header | `CMenuNavigationBar` | 143357、143538；主分支关系由系统持有 |
| `ZMenuInternal.h` 的滚动状态、按钮动画 | `CMenuMovieControl`、`CMenuMovieButton` | 141086、141837、144715；鼠标/滚轮转换留宿主 |
| `ZMenuSurface.cpp` | 拆为宿主绘制表面、系统共享资源、各页面资源/效果 | 当前 GL 方法可以留 Z；页面专属特效不继续存放在通用表面 |
| `ZMenuPreview.cpp` 的玩家预览 | `CMenuMeshPlayer` | 原文件符号入口 169198；换枪完成与装备副本归预览对象 |
| `ZStoreMenu.cpp` | `CMenuStore/Option/OptionGroup` 与 `CStoreAggregator` | 原类符号 178697、180478、233898；具体分工见商店专项 |
| `ZStoreMenu.cpp` 尾部 `DrawModeOverlay` | `CMenuMovieMultiplayerOverlay` | 250020、250772；从商店文件移出 |
| `ZPlanetMenu.cpp` 的星球选择 | `CMenuMission` | `PlanetCallback` 161457、`SetSelectedPlanet` 161868、`Bind` 162721 |
| `ZMissionMenu.cpp` 的任务信息与选项 | `CMenuMissionInfo`、`CMenuMissionOption` | 原文件符号 188880、189803；历史研究入口继续保留，不改变正式模式范围 |
| `ZRefineryMenu.cpp` 的炼油与转移特效 | `CMenuGameResources` 及嵌套类型 | 171917、172390、173264、173545；炼油数据与收益规则留 `CRefinementManager` |
| `ZPostGameMenu.cpp` 的结算与卡片 | `CMenuPostGame`、`CMenuPostGameOption` | 164541、165459、249708；结果从游戏流程传入，绘制不重新结算奖励 |
| `ZPostGameMenu.cpp` 的升级弹窗 | 扩充已有 `CMenuUpgradePopup` 的对应职责 | 392452 之后；不要再建第二套升级弹窗 |
| `ZOptionsMenu.cpp` | 设置/帮助归 `CMenuList/CMenuListOption`；角色选择归 `CMenuPlayerSelect` | 140481、140552、144029、194878；不存在按当前命名推定 `CMenuOptions` 的理由 |
| `ZGreetingMenu.cpp` | `CMenuGreeting` | 原文件符号入口 207809；每日奖励持久状态继续由 data 管理 |
| `ZSocialContent.cpp`、`ZLocalOnlineMenus.cpp` 和炼油文件尾部社交函数 | `CMenuFriends/CMenuChallenges` 及所需选项；本地策略归 host | 工厂 148162、原文件索引；完整社交状态映射留到该批实现前核对 |
| `CInputPad*`、`CPowerUpSelector*` | 继续由现有原类负责，按需移入 `hud/` | 第一批不重写战斗 HUD；清理其对菜单内部头的依赖时定向回归 |

## 四、状态所有权和接口

### 4.1 撤掉两份总状态

`ZMenuState` 的字段逐批移动至真正的所有者：

| 状态 | 所有者 |
|---|---|
| 当前/待切换分支、转场、全局弹窗与输入准入 | `CMenuSystem` |
| 某分支返回路径、菜单参数、保存的选择项、当前/待切换页面 | `CMenuStack` |
| Header Movie、导航按钮和指标的显示状态 | `CMenuNavigationBar` |
| `shop*`、星图、任务列表、结算、炼油等页面字段 | 对应菜单；其中列表运动、按钮和商品卡播放归各自控件 |
| 商品筛选、商品状态、待处理交易和失败原因 | 按原消费关系分入 `CStoreAggregator`；持久库存/货币继续归 profile |
| `equippedPreview`、预览配置、换枪过程和预览音频 | `CMenuMeshPlayer` 与已有音频适配 |
| 结算卡、炼油转移、模式选择的粒子实例 | 对应页面/选项；共享池与资源仍可由系统持有 |
| 鼠标、滚轮、窗口、GL 表面、物理时钟 | 宿主适配；测试可传同一种帧输入和时间 |
| 游戏启动参数与返回结果 | 前端/游戏流程交接；菜单传明确选择结果，不把整份页面状态交给游戏 |
| Bot、模拟连接、模拟购买计时策略 | 已有 host 模块；菜单只观察状态并调用明确操作 |

最终删除 `ZMenuState` 和 `ZGameMenu` 的聚合职责，不能改名为 `CMenuSystemContext` 再把全部字段原样搬入。`ZMenuInternal.h` 最终删除，调用方直接包含所用接口。

### 4.2 接口约束

1. 页面生命周期采用已有原方法含义，如 `Init/Load/Bind/Update/Draw/OnShow/OnExit/OnFocus/OnUnFocus/IsBusy`。恢复当前实际使用的接口即可；精确虚表槽位和参数须实施前继续核对。
2. 页面只接收它使用的资源、数据和菜单协作接口；保持显式依赖，不建立可任意查取整个游戏对象的通用容器。
3. 菜单系统不直接写 `store.shopFilterBound` 等页面字段；切换走页面生命周期。返回选择和必要的页面参数由栈记录承载，不能把所有退出都实现为清空所有状态。
4. 将当前 `Draw*` 中的购买、装备、保存逐项归回原动作/聚合器/数据管理者；绘制读取已经确定的状态。动作仍必须在原按钮/动画消费点执行，不能统一提前到鼠标按下，也不能机械延迟到整帧结束。
5. 复用 `CMovie::Playback` 与真实资源区域。绘制区域的宿主回调可以是私有嵌套实现，通常没有必要为每个 callback 新建公开头。
6. 保存窗口与 GL 资源生命周期约束：资源释放必须先于 GL 上下文销毁。页面销毁、异步结果、预览声音和粒子仍需明确收尾。

## 五、实施顺序与验收

本节是供审核的阶段范围，审核后再细化具体修改任务。

| 阶段 | 实施范围 | 可核验的完成条件 |
|---|---|---|
| A：公共控件和宿主接口 | 明确帧输入/时间，恢复正在使用的 Movie 按钮、列表、导航职责；缩窄 `ZMenuSurface` | Header、弹窗输入屏蔽、滚动停止及按钮完成动作保持一致；没有重复播放状态 |
| B：商店纵向迁移 | 商品聚合、页面、卡片、选项组、角色预览；移走模式覆盖层；接入最小系统/栈生命周期 | 商店页面无需整份 `ZMenuState`；购买/装备/换枪/返回/保存链通过；已迁移函数旧实现删除 |
| C：系统与其余页面逐页完成 | 系统/栈覆盖所有现用切换；星图与任务 → 炼油与结算 → 设置/问候/角色选择 → 社交 | 每页状态有唯一所有者；退出动画完成后切换；弹窗不会把输入传到底页 |
| D：移除兼容入口 | 删除 `ZMenuInternal`、旧总状态及旧门面；整理 HUD 路径和测试依赖 | 生产与 tests 都调用同一真实接口；工程仍一个 vcxproj、三产物；研究入口完整 |

迁移期间可保留短期调用桥，但必须逐页删掉旧状态和旧执行路径；桥不能同时维护新旧两份状态。A/B 的范围应足以让一个真实商店闭环使用新接口，避免先造出一套无人使用的通用框架。

验证选择已有用例，不重跑全量截图基线：

- 公共控件/系统：`header`、`options`、`loading-wipe`、`scene-transition`；按影响补已有 Movie 播放检查。
- 商店：`store-cards`、`store-template`、`dual-weapon`、`bank`、`package-purchase`、`mastery-upgrade`。
- 页面：`planet-menu`、`mission-menu`、`refinery-menu`、`postgame-menu`、`postgame-presentation`、`greeting`、`player-select`、`offline-social`，按批次选取。
- 正式交接：Debug/Release 三产物构建和菜单进入战斗再返回的定向回归；所有代理自动运行显式静音。
- 必要的新增断言覆盖风险：动作只执行一次、底页不接收模态输入、离页取消拖拽、返回恢复选中项、预览不写库存、奖励不随重绘重复、同资源多播放实例互不干扰。

用例名称核对自 [tests/run.ps1](../tests/run.ps1)。本轮未执行上述用例，不能引用历史通过记录作为新方案已验证。

## 六、保留未知与实施前补查

- `CMenuSystem/CMenuStack` 已确认对象和分工，尚需逐分支补全 action、焦点、加载、退出和转场的完整执行表。当前页面号与原菜单类型编号不是同一种编号。
- 公共控件的浮点宿主运动与原定点计算是否完全一致，本轮没有作数值差分。
- `CMenuDataProvider` 的动态条目众多，应按实际迁移页面恢复必要分支，不以“通用数据提供者”之名复制现有硬编码。
- 商店对单品、套装、IAP、筛选和特殊选项的分工，以专项证据为准；原静态表不能被转换成人工维护的新资源事实源。
- 已有 `ZMenuWipe` 的 GPU 捕获是 Windows 适配。系统可以调用它，但本轮不声称它与原双分支实时绘制的所有行为完全等价。
- 用户已授权的本地联机、加载图片池、桌面提示等差异需要保留；职责归位不能擅自撤销这些行为。
