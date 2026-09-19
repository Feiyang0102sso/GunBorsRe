# 商店 UI 源码职责核对与拆分建议

> 研究快照：本文中的“当前实现”和旧路径对应调查时点。相关修复已实施，最终归属、仍未恢复的部分及验收以 [UI 重组结果](ui-optimization-result.md) 为准。

日期：2026-09-19。范围：静态研究；未修改生产代码、未运行构建。反编译行号均指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`，不是原 `.cpp` 行号。

证据入口：[iOS 反编译实现](../_prep/_IDA_OUT/gunbros_3.6.0_IOS.c)、[原文件符号索引](../_prep/_IDA_OUT/source_tree.md)、[Movie 模板](<../_prep/_Big_tool/binary template/big_assets/ui_movie.bt>)、[商品模板](<../_prep/_Big_tool/binary template/big_assets/entries/store_entry.bt>)。总体迁移与验收见 [UI 职责拆分方案](ui-responsibility-plan.md)。

## 结论

商店需要按“页面编排 → 卡片集合 → 单卡片 → 数据绑定”和“商店业务 → 玩家预览 → 宿主支付适配”拆分。不能只把 `DrawStore` 分成几个继续接收整个 `ZMenuState` 的自由函数。原版已有明确的类边界，恢复其职责比另建泛化 UI 框架更直接。

准确原名是 **CMenuStoreOptionGroup**，没有检索到 **CMenuStoreGroup**。原类可以保留 C 前缀；新的宿主适配类型使用 Z 前缀。

## 已确认的原版边界

| 原类 | 原文件及源码证据 | 职责与所有权 |
|---|---|---|
| CMenuStore | `menuStore.cpp`；构造 178697、Init 180199、Bind 180011、RefreshCategoryContent 179124、Update 179525、Draw 179478、DrawOverlay 179501 | 拥有商店整体 Movie、滚动控制、商品集合、分类按钮、筛选按钮、换枪按钮及当前聚焦项。Init 将 STORE_MENU 的区域 0/1/2/3 分别绑定内容、分类、模型、换枪回调；SORT_BAR 的区域 1/2 绑定筛选按钮和标签。 |
| CMenuStoreOptionGroup | `menuStoreOptionGroup.cpp`；InitOption 233898、Init 233952、构造 234012 | 管理卡片对象和创建/销毁；Init 比实际商品多分配两个选项，前两项分别创建 option type 8、9，其他按配置创建；InitOption 统一注入字体。CreateMenuOption 175443 明确 type 8、9 均创建 CMenuTapjoyOption，分别传入 0、1；不能根据当前本地文案把它们命名为普通商品。 |
| CMenuStoreOption | `menuStoreOption.cpp`；Bind 181803、Init 182188、HandleTouchInput 181215、Focus 181402、UnFocus 181356、Update 181460 | 单张卡片拥有 SHOP_BOX 播放和聚焦状态、标题/价格/说明/属性/缩略图/熟练度内容及购买、预览按钮。Bind 向 provider 请求字符串、Sprite、Movie 和 action；各区域 callback 布置该卡片的内容。 |
| CStoreAggregator | `storeAggregator.cpp`；InitFilteredList 158999、SortFilteredList 155433、GetItemStatus 155261/156347、EquipItem 156082、PreviewItem 156151、AcquireItem 158044/158253 | 拥有商品缓存、筛选列表、类别及筛选条件，负责库存状态、交易资格、价格/属性文本、装备和预览配置、购买结果。由 CGunBros 创建（80235），不由单张卡片拥有。PreviewItem 复用 EquipItem，但写入传入的预览 configuration；实际装备重载 156158 使用正式配置。 |
| CMenuDataProvider | `menuDataProvider.cpp`；CreateContentUIMesh 148922、CreateContentMovie 149040、CreateContentString 151149、GetElementAction 153470、LoadData 154133 | 连接 MDS 数据表与具体内容/动作的绑定入口；并非 BIG 二进制读取器，也并非仅商店使用的查询器。CreateContentUIMesh 在 149028 实际构造 CMenuMeshPlayer；反编译返回类型 CMenuMeshEnemy* 不能直接当作真实返回类型。 |
| CMenuMeshPlayer | `menuMeshPlayer.cpp`；Bind 169660、BindPlayer 169300、Refresh 169399、HandleTouchInput 169460、Draw 169571、Update 169608 | 拥有预览 CBrother，执行角色更新、旋转、显示/聚焦、换枪完成反馈；Bind 直接创建 CBrother。Update 观察实际配置的枪槽变化，再通过 action 0x5D 通知菜单，不能点击时即视为换枪完成。 |
| CMenuMovieControl / CMenuMovieButton | `menuMovieControl.cpp` 140724 / `menuMovieButton.cpp` 144360 | 复用的滚动 Movie 控制与按钮状态；CMenuStore::InitSortButtons 178912 按 provider 行数创建按钮，不属于硬编码页面表。 |

以上文件映射同时见 `_prep/_IDA_OUT/source_tree.md` 第 153、169、174、175、181、188–190、244 行。

## 当前实现如何归位

| 当前代码入口 | 建议所有者 | 迁移内容 |
|---|---|---|
| `ZStoreMenu.cpp:835 DrawStore`：整体区域、分类、滚动、筛选、换枪与焦点编排 | CMenuStore | 拆成 Bind / Refresh / Update / HandleTouchInput / Draw / DrawOverlay；页面只协调对象和发出动作。 |
| `DrawStore:855` 起的 items/itemSlots/ordered 构造、类别/持有/模式过滤 | CStoreAggregator | 返回稳定的已过滤商品列表；页面的可见卡片槽位列表归 OptionGroup。业务商品索引与前置特殊卡片索引保持明确映射。 |
| `MatchesEquipmentSlot:8`、`IsStoreObjectEquipped:46`、`OwnsBundle:131`、`StoreItemKind:150`、`SubstituteStoreStats:170`、`StoreStatValues:296`、`EquipStoreItem:806` | CStoreAggregator，或复用已有 profile/configuration 接口 | 将状态查询、文本派生和配置操作移出绘制文件；不要复制 CProfileManager 已有交易实现。 |
| `CardRegion:60`、`DrawCardPrice:124`、`DrawStoreTemplate:192`、`DrawMasteryMeter:231`、`DrawPowerupCompatibility:268`、`AdvanceStoreCard:420`，DrawStore 中折叠/展开卡片和购买预览按钮 | CMenuStoreOption | 单卡片拥有时间游标、聚焦阶段、内容和按钮；文本排版继续复用现有 ZStoreText。 |
| `DrawStoreCategories:316`、`StoreFilterRows:359`、`AdvanceStoreFilter:374`、`DrawStoreGunSwap:490` | CMenuStore，按钮/滚动状态可下沉至已恢复控件 | 分类和筛选变化调用 aggregator，页面负责刷新卡片集合和 Movie。 |
| `ZMenuPreview.cpp` 的 `ZGameMenu::DrawEquippedPlayer`、`AdvancePlayerPreview:125`、预览配置/CBrother | CMenuMeshPlayer | GL、窗口、音频设备依赖通过宿主接口传入；生命周期和玩法预览不再寄居于通用 ZGameMenu。 |
| `DrawStorePrompt:605`、`ShowStoreFundsPrompt:462`、`ZMenuState::ShowStorePrompt` | 共享 popup/action 系统 + aggregator 失败信息 | 提示框其他页面也会使用；不能仅按 store 命名将全部提示状态塞进 CMenuStore。原 ShowPopup 96455 与现有 CMenuPopupPrompt 继续复用。 |
| `CompleteOfflineIAP:554`、`ZMenuState::BeginOfflineIAP`、currencyPending/ReadyAt/Simulated | Z 前缀的宿主支付流程适配；原交易语义归 aggregator/profile | 当前 4000ms 等待是已有用户授权的离线策略，不是原版 CStoreAggregator 代码常量；保持授权行为并明确适配边界。 |
| `DrawCurrencyCard:760` | 先核对原 option type 再归位 | 银行卡片仍需逐项核对 provider 分支；前两张特殊卡片已确认是 CMenuTapjoyOption（CreateMenuOption 175443），不能强塞进 CMenuStoreOption。 |
| `DrawModeOverlay:1334` | 对应 CMenuMovieMultiplayerOverlay | 与商店无所有权关系，应从该文件独立迁走。 |

## 状态迁移

`ZStoreMenuState`（ZMenuInternal.h:148）已经分组，但不是最终所有权：

- `shopCategory`、当前聚焦项索引、页面 focus 插值和分类/筛选控件归 CMenuStore。
- `shopFilter`、`shopExclusionFilter` 是商店查询条件，归 CStoreAggregator；筛选栏开合时间仍归页面/控件。
- `shopScroll`、`shopMotion` 归页面拥有的滚动控件；卡片集合归 CMenuStoreOptionGroup。
- `shopDetailTime/LastTick/Closing/Open` 等卡片状态归对应 CMenuStoreOption；选中卡片索引归页面，不能再复用跨页 selectedItem。
- `shopPreview` 表示页面对预览的请求；预览配置和 CBrother 生命周期归 CMenuMeshPlayer。正式 configuration 仍由 profile 管理，不能因预览修改存档。
- `shopGunSlot` 与角色实际换枪存在时序关系；明确“请求槽位”和“已完成槽位”，保留原 Update→action 通知链。
- 全局 `ZMenuState` 的 currency/IAP 和 popup 数据不全部移入 store；这些跨页流程由菜单系统/宿主服务持有。

## 原资源约束

已阅读 `ui_movie.bt` 与 `entries/store_entry.bt`。STORE_MENU 的 type 6 只是动态用户区域，文件并不包含商品集合、玩家对象或 callback；SHOP_BOX 的章节和各区域关键帧由 BIG 读取。商品 category、实体类型、itemClass、excludedGameModes、displayOrder 各自独立；不能把拆分后的接口统一成一个含糊的 category 参数。

目录建议：CStoreAggregator 可暂归 `ui/content/`，但它是 CGunBros 所有的商店业务服务，不是页面私有模型，也不仅提供显示内容。若项目已有正式业务服务目录，优先按职责归入那里，不为这次迁移单独新建多层框架。

拆分不应移动或重写磁盘解析器：`CStoreItem`/`ZStoreCatalog` 保留资源职责，CStoreAggregator 使用它们及 profile。动态绑定放在 CMenuDataProvider，不能把原资源坐标复制成 C++/JSON 表。

## 最小迁移顺序及验收

1. 先迁走 `DrawModeOverlay`；抽离页面头文件和商店状态声明，暂保留调用入口，避免同时改页面行为。
2. 恢复 CStoreAggregator 的列表/状态/文本查询职责，复用现有交易接口；核对类别、持有筛选、游戏模式排除、排序、礼包及特殊卡片索引。
3. 恢复 CMenuStoreOption 与 CMenuStoreOptionGroup，移动卡片状态和绘制；核对展开/收回、滑动遮罩、熟练度、购买按钮和前两项特殊选项。
4. 恢复 CMenuStore 的生命周期和控件编排，ZMenuState 只保留菜单系统导航/共享服务；核对切页返回、筛选开合、卡片焦点及输入互斥。
5. 将玩家预览迁入 CMenuMeshPlayer，并将宿主支付流程与共享提示归位；核对预览不修改正式配置、换枪动画结束后槽位同步、购买结果与保存失败路径。
6. CMenuDataProvider 涉及全 UI：先覆盖当前商店实际使用的调用，其他页面逐步迁入；不为本次拆分先搭一套全量抽象框架。

每批实现完成后执行对应现有开发测试，自动验证显式 `--mute`；本研究未执行测试。未核对每个 action、所有 card 子类型、原在线促销生命周期和正式购买失败保存语义，实施前应补齐对应证据，不把这些未知项描述成已经复刻。

