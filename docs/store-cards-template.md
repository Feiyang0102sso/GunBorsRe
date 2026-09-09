# 商店商品卡：GUNS、ARMOR、POWER UPS

本轮范围为 PLAN 的 UI4.2：把商品卡的共用显示链接回 BIG 与原菜单代码。FILTER 试点记录见 [store-template-pilot.md](store-template-pilot.md)。BANK 点击后进入独立货币页面，本轮没有将它误作同类装备卡重写。

## 数据与布局来源

运行时仍从 `big/` 读取；`big_360_out/` 是只读研究样本。没有新增手工商品表、字体配色表或 JSON 布局替代资源。下列原函数行号均指 `_IDA_OUT/gunbros_3.6.0_IOS.c`。

| 资源 | 只读样本（均位于 `big_360_out/pack0_core_xga/0xf4e02223/`） | 用途 |
|---|---|---|
| `GLU_MOVIE_SHOP_BOX`，Movie 39，handle `0x030004CE` | `pack0_core_xga_0122_0x2a505e.bin` | 12 个商品区域，折叠、展开、显隐和透明度 |
| `GLU_MOVIE_MASTERY`，Movie 91，handle `0x03000502` | `pack0_core_xga_0174_0x2a81fb.bin` | 商店专用熟练度子 Movie，8 个回调区域 |
| `GLU_MOVIE_DEATHMATCH_ONLY_POWERUPS`，Movie 144，handle `0x03000537` | `pack0_core_xga_0227_0x2abd60.bin` | Powerup 模式兼容性子 Movie，7 个区域 |
| STORE 条目 | 各包 `CGameObjectPack` 的 STORE 分区 | 缩略图、名称、说明、折叠/展开属性、价格、类型、显示顺序 |
| 字体与字符串 | core `FONT_KEYSET` 及 STORE 字符串引用指向的包 | 字形、行高、字距、颜色、货币图标和文本内容 |

对应 BT：`_Big_tool/binary template/big_assets/ui_movie.bt`、`entries/store_entry.bt`、`entries/gun_template.bt`、`entries/armor_template.bt`、`entries/powerup_template.bt`（后三者同在 `big_assets/` 下）。字段总索引见 [entry-field-semantics.md](entry-field-semantics.md)。

## 原函数到重建实现

主要实现位于 `src/gun_bros_re/runtime/GameFrontEnd.cpp`，宿主字体/字符串接口位于 `runtime/MovieRenderer.h/.cpp`。

| 原消费函数 | 本轮实现与修正 |
|---|---|
| `CMenuStoreOptionGroup::InitOption :233898` → `CMenuStoreOption::SetFont :181499` → `SetupTextBox :181194` | `FormatStoreText` 的 `^f0..4` 对应菜单字体 **1、2、4、3、0**。读取真实字宽、行高；保留换行、空白和字体切换，不再把数值统一改成蓝色大数字。 |
| `CTextBox::paint :104153/:104193` | byte 0 表示居中，byte 1 才是右对齐。折叠模板居中，展开属性与说明左对齐；混排字体在行内垂直居中，按当前区域与父裁剪区的交集绘制。 |
| `CStoreAggregator::SubstituteStatsInString :156816`、`GetStatDataRow :156670` | `StoreStatValues`/`SubstituteStoreStats` 读取 STORE 八组属性。枪械按当前熟练度取行；SPD 保留正负号，其他字段按原逻辑显示绝对值。未知 token 保留可见，不静默删除。 |
| `CMenuDataProvider::CreateContentString :151994/:151999` | `assets[4]` 是展开属性，`assets[5]` 是折叠属性；ARMOR 与 POWER UPS 使用同一条链。删除旧 ARMOR 详情中另画的 DEFENSE/ATTACK/SPEED/XP/XPLODIUM 五行替代内容。 |
| `CMenuStoreOption::Bind :181803` | 用完整展开姿态确定文本换行宽度，用当前帧区域确定位置、透明度、裁剪。属性与说明不再等动画结束才突然出现。 |
| `ThumbCallback :181036`、`TitleCallback :180599` | 缩略图按 PNG 原尺寸放入区域5；宽度超出时靠左，否则居中，垂直居中。名称绑定区域6、字体1，不再根据截图另算图框或缩放标题。 |
| `CreateItemCategoryString :157413` | 读取 `ResId("IDS_SHOP_SORT3") + STORE.type`；**资源 ID 连续不等于别名后缀连续**。修正头盔被标成 TORSO、Speed Boost 被标成 BUCKS 的错误。 |
| `CreateItemCostString :157573/:157583`、`LevelCallback :180799` | 从 BIG 的 `IDS_SHOP_COMMON/RARE` 格式串替换价格。当前资源为 `Ω%i` / `δ%i`，货币图标就是字体0中的字形，不再另画一个按行高放大的 Sprite。 |
| `PropertiesCallback :181022`、`PurchaseInfoCallback :180849` | 按 MDS 引用的按钮 Movie 读取宽高；折叠按钮较窄时居中、较宽时右对齐，已拥有装备的按钮移到区域7。展开区保留左右按钮的实际宽度后居中显示购买提示。 |
| `Focus :181402`、`UnFocus :181356`、`Update :181486` | `AdvanceStoreCard` 从 SHOP_BOX 章节1取边界，按原代码的 **4× 毫秒增量**正放/倒放。收起完成前保持模态，避免穿透购买；中途反向保留当前位置。 |
| `CMenuStore::Init :180297`、`SetupFocusInterp :179101` | 聚焦中心来自 STORE_MENU 区域0：`x+w/2-w/16, y+h/2`。125毫秒中心移动是原程序常量，仍留在代码中；不是另造资源参数。 |
| `CMenuDataProvider::CreateContentMovie :149246`、`CMenuStoreOption::Bind :181883` | `DrawMasteryMeter` 使用商店的 MASTERY，而非升级弹窗的 WEAPON_UPGRADE_MASTERY。枪械 XP 决定章节内目标时间，原 Update 用2×速度推进子 Movie，标题/星标/暴击说明按其区域绑定。 |
| `CreateContentSprite :150114/:150159` | 熟练度星标使用 archetype 26 的动画5–7；折叠角标使用24–26。原编号来自程序绑定，几何与图片继续取资源。 |
| `CreateContentMovie :149265`、`Bind :182003`、`GameTypeCallback :180652` | `DrawPowerupCompatibility` 使用原子 Movie 的行位置、背景和显隐；标签来自原三个多人模式字符串，STORE.value8 决定173/174状态图标。 |
| `CreateQuantityOwnedString :157324`、`CreateContentSprite :149994`、`CornerCallback :180757` | `DrawStoreQuantity` 在区域3显示库存数字和0:87/88角标，替换遮挡属性的手写 `OWN` 标签。 |
| `CStoreAggregator::GetItemStatus :155316` | 消耗品不因已有库存就成为 OWNED 装备；保留库存角标、价格和再次购买。 |
| `CMenuMovieButton::Focus :144634` | 分类选中效果播放每个按钮自己的章节3。旧代码用 Movie ordinal 差值猜 Sprite 编号，导致 POWER UPS 高亮出现无关图标。 |

注意：`HandleTouchInput :181298` 本身也允许点击卡片主体收起，并非只允许点卡外。本轮保留此行为，并改成当前章节倒放。商品文字中 `REQUIRES LEVEL: 0` 是 `CreateItemLevelString :157426` 拼接真实 requiredLevel 的结果，没有擅自改成新的提示规则。

## 验证入口

- 研究菜单 **52**，或 `--store-card-check --mute`：单独验证商品卡；**51** / `--store-template-check --mute` 同时验证 FILTER 和商品卡。
- 字体映射、换行、772条非空真实文本无未解析属性 token；内存中修改 STORE.DMG 后文字跟随变化；内存中修改章节与总时长后开合边界跟随变化；4×播放及中途倒放。
- GUNS、ARMOR、POWER UPS 各保存折叠、展开中、展开、收起中、收起五张截图：`out/store-card-<类别>-<阶段>.png`。
- ARMOR 展开预览不购买、购买后装备；POWER UPS 展开连续购买两次；检查独立存档重载和原测试账户未变化。
- 最终构建：`out/store-card-build.log`，退出0。最终商品卡专项：`--store-card-check --mute` → `out/store-card-check.log`，退出0，18张截图。FILTER 与商品卡联合检查：`out/store-card-final-template-check.log`，退出0（最后一次消耗品 OWNED 状态修正另由商品卡专项覆盖）。

- 最终菜单回归：`--game-menu-check --mute` → `out/store-card-menu-regression.log`，退出0，包含购买/装备、消耗品重复购买、展开卡片、筛选、导航和保存重载。
- Movie 回归：`--movie-check --mute` → `out/store-card-movie-check.log`，175个 Movie、0失败、退出0。
- `git diff --check` 退出0；`out/store-card-protected-check.txt` 确认 `saves/` 与 `userdata/` 的23个文件集合和 SHA256 均未变化。
- 目视检查了 GUNS 展开与中间态、ARMOR 展开、POWER UPS 展开与购买后数量10的画面。卡片区域的透明度遵循原关键帧；并未宣称与原设备截图逐像素相同。

直接体验：根目录 `store-template-demo.cmd` 使用独立账户进入商店。点商品主体展开，点 PREVIEW 预览，点主体或卡外收起；滚轮/拖动浏览其余商品。

## 仍未完成

本轮不是 UI4 整体完成。横向滚动与回弹、完整筛选/优惠/礼包/锁定状态机、通用按钮按压过程、人物预览相机与模型裁剪、升级弹窗其他绑定、BANK 独立结算页面仍列为后续工作。公开服务器缺失的业务也没有伪造为资源事实。

`FormatStoreText` 是当前商店实际字符串所需的 CTextBox 子集；扫描的772条文本没有 `^i` 内联图片。本轮不宣称实现其他页面可能使用的全部 CTextBox 指令。
