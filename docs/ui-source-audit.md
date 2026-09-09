# 当前 UI 数据来源审计

## 目标与边界

用户要求恢复原版 UI，不能用猜测的布局、动画和文案冒充原版。原数据在 BIG 或原程序内的存放差异，不应转化为要求用户选择的技术负担。

本轮审计当前前端的来源及接入情况，不改游戏实现。检查范围为 `GameFrontEnd.cpp`、`MovieRenderer.cpp`、已提取 MDS 表、Movie 导出及原版反编译；不是全部 UI 的逐像素或逐帧验收，不能给出“错误数据占比”。

读取期间工作区已有其他未提交改动，部分前端内容也发生变化。本轮不覆盖这些改动；下表优先用函数名定位。审计时前端 SHA256 为 `8DFFB45D3C3AC8F9F4A1C0BC7F119649E949CE08D9600DEFEC2BFC65B0F55CEF`，后续修复前须复核当前实现。

## 已确认的问题

| 项目 | 当前实现及证据 | 原版依据或待定位内容 | 结论 |
|---|---|---|---|
| 主导航入场 | `GameMenu::Header` 固定调用 `Draw(10, 1600)`、`Regions(10, 1600)`，再以每项 65ms、总进度 220ms、`sin(entrance * pi) * 0.15` 等重做图标入场 | `out/movie-check.txt:419` 的 HEADER(10) 含真实时间轴及区域关键帧 | 已有时间轴未用于驱动该入场路径；静止布局读取成功不能证明动画已还原 |
| 商店字体控制 | `DrawStoreTemplate` 删除 `^fN`，按标签/数值固定使用字体 1/9，并自行分行、居中、转大写 | `CStoreItem::assets[4/5]` 的模板保留字体切换；当前函数注释明确承认映射未解 | 确认原格式指令被丢弃；须查原文本绘制器和调用处字体表，不能继续按截图指定颜色 |
| 星图位置与拖动 | `DrawStarMap` 的 `positions[6][3]`、`depth[]`、拖动边界与系数为手写值；注释说明参考用户截图 | 原 `CMenuMission` 的布局、投影和输入逻辑尚需追踪 | 确认该路径采用截图拟合；不能假定原版一定存在一张同结构坐标表 |
| 选关页整体缩放 | `DrawMissionBackdrop` 把 Movie 48 按 region 0 整体缩放到手写矩形，另行写文字、按钮和命中框 | MISSION_MENU(48) 的 region 0 原始数据为 `162,-180,636,126`，锚点指向屏幕中左；`out/movie-check.txt:2224` | 原区域数据已存在，但调用方仍覆盖目标坐标；各子区域和整部 Movie 的缩放需要按原回调核对 |
| 选轮与详情卡 | `DrawRevolutions`、`DrawMissionDetails` 使用 SHOP_BOX(39)，手写卡间距 260、详情矩形及波次格子 | 原程序 MENU_MISSION_DETAIL 配置指定 MISSION_MENU、MISSION_LIST、MISSION_BOX，见下方调用链 | 选关卡片绑定已确认偏离原版；恢复仍需配套区域和控制器，不能只换编号 |
| 设置页 | `DrawOptions` 手写 `labels/titles/bodies`、每项 85 的间距及不同位置的 x 偏移；该函数不调用 `OriginalMenuData` | `OriginalMenuData.inc:14` 已有 MDS_OPTIONS，包含原文案引用；原 `CMenuList::Init` 位于反编译 :140552 | 确认有可用原配置却未接入；Windows 专用退出/保存说明须与原菜单项区分 |
| 社交与本地活动 | `DrawSocial` 有手写人物矩形、页签和文字；`kActivityNames/Descriptions` 是八项本地活动 | 原离线页 Movie 75 及 MDS_OFFLINE_* 已接入一部分，在线实际数据与规则仍需单独核查 | 本地预览是此前授权的研究功能；其存在不证明原版页面已还原，也不能把本地活动认作原版活动 |

## 已有原版依据的部分

选关组件的只读二进制复核：ARMv7 符号 `__ZL19MENU_MISSION_DETAIL` 位于虚拟地址 `0x403130`、文件偏移 `0xB91130`。配置 word[1] 指向 `GLU_MOVIE_MISSION_MENU`，word[2] 指向 `GLU_MOVIE_MISSION_LIST`，word[9] 指向 `GLU_MOVIE_MISSION_BOX`。原 `CMenuMissionInfo::Init` (:189636、:189680) 用前两个字段创建背景和列表，:189701 将配置+9交给 `CMenuOptionGroup::Init`；`CMenuMissionOption::Init` (:191108) 用其首字段创建卡片 Movie。该链证明 MISSION_BOX 的用途，不只是依据英文名称推测。

- 商店主体使用 STORE_MENU(37)、STORE_SCROLL(38)、SHOP_BOX(39)、SORT_BAR(41) 的区域；不能因仍有局部缺口而全部推倒。
- 分类按钮通过 MDS_BUTTON_STORE_CATEGORIES 选择按钮资源；原 `CMenuStore::CategoryCallback` (:178790) 确实按前一按钮宽度加 4 排列。
- 原 `CMenuStore::ItemCallback` (:178878) 确实用区域半高加 5 安排第二张卡。这些有依据的代码常量可以保留。
- 商品名、简介、属性模板和数值已从各 BIG 的 StoreItem 及引用读取。问题还包括调用方如何解释和显示这些数据，不能只查解包层。
- MDS 商店分类表确实内嵌在原程序，二进制偏移等证据见 [布局来源](ui-original-layout.md)。禁止臆造不等于禁止保留原代码中的数据或计算。

## 修复顺序与验收方案

1. **主导航时间轴**：核对原 CMenuSystem 的章节和回调绑定，使用同一播放时刻驱动图形、区域及输入。验收原关键时刻的位置、缩放、透明度和显示状态；静态终帧截图不够。
2. **商店格式解析**：定位 `^fN` 的实际字体表及原换行规则。用包含多个字体切换、正负数与长描述的真实商品检查；不能用固定字体替代后宣布通过。
3. **星图、选轮、选波**：先完成原 CMenuMission 系列的资源与回调对应表，再替换当前截图拟合。覆盖拖动起止、收起/展开和点击区域；不能仅把 39 改成 50。
4. **设置与社交页**：逐项接入原表和 Movie 区域；保留已有独立研究入口，明确本地预览与原版恢复边界。

每个阶段只在原资源及原代码证据明确后修改。无法定位的字段记为未解，不补猜测值。修复验证继续使用 `--mute`；既有 Movie 解码或菜单点击检查通过，仅证明其各自覆盖的能力，不代表布局、字体或动画正确。

本轮完成的是来源审计与阶段方案，尚未执行上述 UI 修复或新的画面回归。
