# UI 行为与原版源码核对

> 研究快照：本文中的“当前实现”和旧路径对应调查时点。相关修复已实施，最终归属、仍未恢复的部分及验收以 [UI 重组结果](ui-optimization-result.md) 为准。

日期：2026-09-19。本次使用 research 流程做本地一手资料核对；未修改生产/测试代码，未构建。源码行号指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`。当前代码在研究期间正在迁移目录，下列旧行号指迁移前 `ui/ZMenuSurface.cpp`、`ui/ZStoreMenu.cpp`；新位置为 `ui/host/ZMenuSurface.cpp`、`ui/menus/ZStoreMenu.cpp`。

## 已确定且可以独立修复

### 1. 商店漏画原始 STORE_MENU，使用固定黑底代替

- 当前 `DrawStore` 只获取 `storeMenu` 的四个区域，没有实际调用该 Movie 的 Draw；`Begin:95` 针对 page 2/17/18 绘制固定 `(0,132,1024,627)` 黑矩形。
- 原 `CMenuStore::Draw:179478` 中 179487 先 `CMovie::Draw(this+12)`，随后才绘制聚焦卡片。`Init:180199` 初始化该 Movie，并将区域 0/1/2/3 绑定内容、分类、角色、换枪回调；`DrawOverlay:179501` 单独绘制筛选栏和附加窗口。
- `ui_movie.bt` 明确 STORE_MENU 包含四个用户区域、章节和 CMovieFill。研究索引 `ui-movie-catalog.json` 的 ordinal 37 / handle `0x030004CC` 记录 object 5 在单资源偏移 177 为 type 7、一个 42 字节关键帧；归档定位 pack0_core_xga、physical 120、BIG 偏移 `0x2a4edd`。
- 修复：恢复原 Movie 渲染及区域回调顺序，移除手写黑矩形。不能仅删除矩形而仍不绘制 STORE_MENU，也不能把资源填充颜色/尺寸再复制到代码。
- 建议断言：商店每帧实际绘制 STORE_MENU；绘制轨迹包含原 type 7；页面没有额外手写黑矩形；类别、滚动、玩家和换枪区域仍能响应。用现有相关截图检查层级与遮罩即可，不需全量基线。

### 2. 炼油背景重复绘制且通用宿主层钉死时间

- `Begin:94` 先画 Movie 36@1600；`ZRefineryMenu:422` 又画 `GLU_MOVIE_EXPLODIUM_BG`，时间为 refineryElapsed。资源名称表和 `ui-binary-templates.md:147` 确认是同一 Movie。
- 原 `CMenuGameResources::Draw:172363` 只在非空时画一次 this+16 的背景，再按状态画主 Movie；`CMenuSystem::Init:97435` 初始化共享 EXPLODIUM_BG，不等于宿主 Begin 每帧再画一次。
- 修复：删除通用 Begin 的炼油背景绘制，由炼油页面持有正确播放时钟并绘制。自动截图仍显式 `--mute`。
- 建议断言：炼油单帧背景绘制恰好一次；推进时间会改变使用的背景时刻，不固定 1600。

### 3. 商店购买和预览按钮的动作触发过早

- 当前 `StoreItemButton:81` 使用显示 label region 1 作为点击框，命中后仅 `NotePress` 并立即返回 true。`DrawStore:1310` 同帧执行 AcquireItem、装备、保存；展开页 PREVIEW 也是点击后直接切换 bool。
- 原 `CMenuMovieButton::Init:144910` 从 Movie region 0 读取命中区，在 region 1 绑定按钮显示 callback（约 144980–144993），并将 toggleMode 设为 3（144994）；不能把视觉 region 1 当作 region 0 的替代。
- `CMenuStoreOption::Bind:182080–182088` 为购买/预览/第三按钮逐个 Init，未覆盖该模式。`Select:144661` 选择 chapter 1 并进入 selected 状态；`Update:144715` 的 case 4/5、mode 3 只在 Movie 完成后 DoAction。`CMenuStoreOption::Update:181460` 每帧更新这些按钮。
- 原 `HandleTouchInput:144483` 在按下命中后进入 focus；拖出可取消，释放要求仍处于 focus 且命中。当前 Begin 仅检测“本次释放且总拖动距离小于9”，不能完整替代按钮自己的捕获和取消状态。
- 修复：真正恢复每个按钮的 Movie 状态、region 0 命中、按下/移出/释放及动作完成事件；交易由事件消费者执行。不能给全部按钮硬加固定 300ms 等待：应从 BIG 的 chapter 长度读取，并遵循各按钮的 toggleMode。
- 建议断言：按下和有效释放时库存/货币未变；chapter 1 完成后恰好交易一次；移出取消不交易；连续输入 busy 期间不重复交易；region 0 与 region 1 不同时按 region 0 判定。

### 4. PREVIEW 是应用预览配置，不能第二次点击撤销

- 当前展开卡片用 `shopPreview = !shopPreview`。
- 原 action 0x3D（94680）调用 `CStoreAggregator::PreviewItem` 写入独立预览 configuration，再通知菜单 refresh 61；`PreviewItem:156151` 始终调用 EquipItem，没有 toggle。
- `CMenuStore::Refresh:179674` 的 `case '='`（约179834）通知玩家模型 refresh 61，并记录 preview 事件，没有将配置恢复为正式装备。
- 修复：完成 PREVIEW 按钮动作时应用该项预览；重复预览同一项保持结果。退出聚焦/页面时按现有原版 reset 路径复位，另核对具体退出生命周期。
- 建议断言：同一未拥有商品连续预览两次，第二次仍显示该商品；正式 configuration、库存和磁盘存档均不变。

## 不应误报成缺陷的双槽行为

- 原 `GetItemStatus:155261` 在 155316 用 IsGunEquipped(...,-1)，查询两个枪槽。当前 `IsStoreObjectEquipped` 已如此处理。
- 原 `CPlayerConfiguration::SetGun:171692` 会查两个枪槽；若商品已经装备在任一槽且未指定强制参数，直接不覆盖。当前 `data/CPlayerConfiguration.h::SetGun` 已保留这一规则。不要为“切换选中槽装备已有枪”强行复制同一把枪到两个槽。
- 原 `CStoreAggregator::EquipItem:156082` 遍历商品引用，最多装两把枪，从活动槽起按 `(activeSlot + gunCount) & 1` 选择槽；当前 EquipStoreItem 相应处理已存在。完整礼包购买状态由 CPackageOfferMgr 独立记录，不能仅凭各物品已拥有推导；并行实现已在 OwnsBundle 中改用 IsPackagePurchased，不重复列为待修。
- 原 action 0x38 成功取得状态3后会转 action0x39（约94577），后者执行装备并同步预览配置（94617附近）。因此“买枪后自动装备”本身有原版依据，不应当作 UI 多做了业务而删掉；应迁移其执行位置并保留语义。

## 筛选的明确差异，但需原样本验证后修复

- 当前 `MatchesEquipmentSlot:8` 将所有零金币且零Warbucks道具标为奖励并排除。原 `InitFilteredList:158999` 主要依据 root category mask、ItemStatus、筛选位、excludedGameModes、OverrideItem 后的 displayOrder、隐藏字段及 IAP 产品后缀进行选择；已读区间中不存在“两个价格均零即隐藏”的规则。BT `entries/store_entry.bt` 的价格字段也不承担“奖励物品”标志。
- 这证明当前规则缺乏所查源码依据，但尚未给出一件被误隐藏、原版实际可见的 BIG 条目；不应直接称已重現商店缺物品。实施时先对原 BIG 枚举候选，记录资源定位和完整原过滤判定，再决定是否删除规则。
- 当前过滤把持有筛选实现为简单 `profile.Owns`；原 `InitFilteredList:159135–159253` 区分状态3/4、0x40000、0x80000及类别位组合，并对已装备项有专门分支。不能只按当前截图重写；建议先以原算法生成给定 profile 的过滤预期列表，再替换。
- 断言应比较真实商品引用及顺序，覆盖：无筛选、类别单选、已拥有、已装备但类别不匹配、模式排除、负 displayOrder 和 override、礼包。不要构造一套脱离 BIG 的替代商品数据源。

## Movie47 的结论边界

- Movie47 是 `GLU_MOVIE_MAP_PARALAX_COPY`（`ui-binary-templates.md:158`），不是证实的所有页面通用背景。`CMenuSystem::Init:97423` 只说明共享加载；`GetMovie:96280` 通过索引返回共享 Movie。
- 原 `CMenuSystem::Draw:96761` 绘制当前 stack/page、转场、header 与 overlay；已核对 `CMenuStack::Draw:147959` 只转调活动页面，不存在统一先画星图 Movie 的调用。原商店 Draw 明确只画 STORE_MENU。因此至少商店不能继续依赖宿主 Movie47@1600 充当背景。
- 建议将公共 Begin 限于宿主清屏与输入采样，各页面声明自己实际使用的背景。删除全局47前必须核对每个现有页面背景提供者；当前研究没有核完所有页面，不承诺删除后所有页面视觉正确。

## 证据和验证边界

本次复用已有 `docs/ui-store-source-research.md`、`ui_movie.bt`、`entries/store_entry.bt` 和 UI Movie 索引，只增量读取对应函数。未运行 010 Editor，未更改 BIG、原始存档或研究样本；未执行现有测试。上述“建议断言”属于待实现验证，不代表已通过。

