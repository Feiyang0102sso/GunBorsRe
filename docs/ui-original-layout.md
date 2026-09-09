# 原版界面布局的真正来源（2026-09-08 调研）

## 结论先说

已核对的商店等菜单，其主要布局骨架来自**具名 CMovie 里的 user region**（`MovieObject::type == 6`）。不能据此断言所有页面的全部坐标都在 BIG 内：原菜单代码还计算子控件的位置和间距。
`CMenu*::Init` 把资源名交给 `Engine::ResId` 取到电影，再用 `CMovie::SetUserRegionCallback(movie, N, ...)`
把第 N 个 region 绑到一个绘制回调上。region 自带 3×3 锚点和"远边绑定"，所以同一份 960×640 的
作者数据能在 1024×768 的 iPad 上摆正——这正是本项目 `MovieRenderer::GetMetrics` 已经实现的机制。

原版数据来源需要区分三部分：BIG 中的布局、动画及关联资源；`gunbros` 程序内的 MDS 菜单静态表；原菜单函数中的布局计算。例如 `_IDA_OUT/gunbros_3.6.0_IOS.c:148468` 注册 `MDS_BUTTON_STORE_CATEGORIES`，`:178790` 的 `CMenuStore::CategoryCallback` 按按钮宽度加 4 排列分类，`:178878` 的 `ItemCallback` 按区域半高加 5 排列第二张商品卡。原版代码中的这些常量与重建时凭截图填写的坐标不是同一种来源。禁止臆造并不意味着原版全部数据必须来自 BIG；原程序静态表在重建版中采用编译内嵌还是运行时读取，属于另一个尚待讨论的实现选择。

只读二进制复核：当前 `gunbros` 的 ARMv7 切片起点为 `0x78F000`，`MDS_BUTTON_STORE_CATEGORIES` 的虚拟地址 `0x41A940` 对应文件偏移 `0xBA8940`。表头指定 `pack0_core`、4 条记录、每条 64 字节；首条包含 `IDS_SHOP_CATEGORY1`、图标编号 62/63、`GLU_MOVIE_BUTTON_SMALL` 和动作字段 64。这证明原程序内保存了实际菜单组合配置，不仅是 BIG 文件名。

`GameFrontEnd.cpp` 绝大多数屏幕用的是手写常量（`categoryX[] = {8, 112, 246, 480}` 之类），
所以"一堆 UI 都是歪的"。数据没丢，是渲染层没读。商店已按本文改完，其余屏幕待办。

## 电影别名 → 序号

`src/gun_bros_re/runtime/MovieNames.inc` 原先只有 60 个名字（从反编译正文里 grep 出来的），
漏掉了只作为数据表字符串存在的一半。现已改为直接从 `gunbros` 二进制里抽取全部 116 个
`GLU_MOVIE_*` 字符串。重跑 `--movie-check --mute` 后 `out/movie-check.txt` 里的
`alias pack0_core <名字> movie=<序号>` 就是完整映射，175 个电影 0 失败。

主流程相关的关键项：

| 名字 | 序号 | 用途 |
|---|---|---|
| GLU_MOVIE_HEADER | 10 | 顶部货币条 + 7 个主导航按钮（region 0..6 是图标，7.. 是文字条） |
| GLU_MOVIE_INFO_CLUSTER | 11 | 右上等级/经验簇 |
| GLU_MOVIE_WIPE | 12 | **场景切换过场**（CMenuSystem::Transition1/2Callback） |
| GLU_MOVIE_TRUNK_BUTTONS | 14 | 主导航按钮本体，chapter 0/100/200/300/401 = 各按钮状态 |
| GLU_MOVIE_STORE_MENU | 37 | 商店屏幕骨架 |
| GLU_MOVIE_STORE_SCROLL | 38 | 商店横向传送带 |
| GLU_MOVIE_SHOP_BOX | 39 | 商品卡（折叠 + 展开两态） |
| GLU_MOVIE_SORT_BAR | 41 | FILTER 按钮与下拉 |
| GLU_MOVIE_MISSION_MENU / _LIST / _BOX | 48 / 49 / 50 | 星图与选关 |
| GLU_MOVIE_WAVE_SELECT | 65 | 选波 |
| GLU_MOVIE_PLAYER_SELECT | 70 | 选兄弟 |
| GLU_MOVIE_LIST_MENU | 71 | 设置类滚动菜单 |
| GLU_MOVIE_WRAPUP_SCREEN / _BOX / _GALLERY | 17 / 20 / 18 | 结算 |
| GLU_MOVIE_WELCOME_NEW | 106 | 签到 |
| GLU_MOVIE_OFFLINE_BROHOOD | 75 | 离线社交页 |
| GLU_MOVIE_BUTTON_SMALL / MED / LG / XL | 25 / 26 / 27 / 29 | 按钮底板，尺寸由 MDS 表指定 |

## 商店屏幕（CMenuStore）

`CMenuStore::Init` (:180199) 建三个电影，`CMenuStore::Bind` (:180011) 绑数据：

- `a1+12` = STORE_MENU(37)，注册 4 个 region 回调：
  0 → `ContentCallback`（商品列表）、1 → `CategoryCallback`（四个分类页签）、
  2 → `PlayerMeshCallback`（右侧 3D 人物）、3 → `GunSwapCallback`（换枪圆钮）。
- `a1+16` = STORE_SCROLL(38)，交给 `CMenuMovieControl` 做横向翻列。
- `a1+332` = SORT_BAR(41)，region 1 → `SortButtonCallback`、region 2 → `SortLabelCallback`。

### STORE_MENU(37) 的 region（换算到 1024×768 后）

| region | 原始 xy/wh (960×640) | 父锚点 | 屏幕矩形 | 内容 |
|---|---|---|---|---|
| 0 | 2,-131 760×348 | 3 中左 | (2, 253, 760, 348) | 商品列表 |
| 1 | 12,142 940×34 | 0 左上 | (12, 142, 940, 34) | GUNS/ARMOR/POWER UPS/BANK |
| 2 | 81,185 400×440 | 1 上中 | (593, 185, 400, 440) | 右侧人物 |
| 3 | -90,146 90×90 | 2 右上 | (934, 146, 90, 90) | 换枪钮 |

调研时代码里对应的手写值分别是分类条 y=142 但宽度写死 {100,130,230,130}、
人物面板 (548,717,476,565)、换枪钮 (907,153,80,80)——都对不上。

### STORE_SCROLL(38)

5 个卡槽（region 1..5），每槽 244×328，横向间距 258，屏幕外的两槽 alpha 0.5；
object 9 是右侧 337×640 的渐隐遮罩（原图里第三列卡片被人物压住并淡出就是它）。
chapter 1400 的关键帧整体左移 258，就是"翻一列"的原生动画。原版没有 `< 1/37 >` 分页，
也没有 BACK 按钮。

### SHOP_BOX(39)：折叠卡与展开卡是同一张

chapter 0（t=0..800）折叠态 254×164；chapter 2（t=800..1300）展开成 500×328。
用户说的"简介、熟练度全没了"就是展开态的那些 region：

| region | 折叠 | 展开 | 内容 |
|---|---|---|---|
| 0 | 0,0 254×164 | 0,0 500×328 | 整张卡 |
| 1 | 5,3 162×127 | 5,3 162×293 | 图标 |
| 2 | 5,130 162×29 | 5,296 162×29 | 分类（PISTOL/SHOTGUN…） |
| 3 | 214,-6 42×42 | 462,-6 42×42 | 右上角标 |
| 4 | 146,35 96×98 | — | 折叠态右列（POWER 数值） |
| 5 | 6,4 146×156 | — | 折叠态图标区（OWNED/EQUIPPED 斜标铺这里） |
| 6 | 12,4 150×22 | — | 折叠态名称 |
| 7 | 10,136 232×22 | 10,136 480×22 | 价格行 |
| 8 | — | 244,4 247×128 | DMG/RPM/SPD + UPGRADE LEVEL 三格星条 |
| 9 | — | 10,164 480×116 | 描述段落 |
| 10 | — | 10,284 480×34 | 底部按钮行（PREVIEW / REQUIRES LEVEL / BUY） |
| 11 | — | 146,35 96×98 | 展开态右列 |

### 数据确实已经解析出来了

- 简介在 `CStoreItem::assets[3]`（当时误记为 5，见下面的模板串一节），`ReadGameString` 能直接读。
- `CStoreItem::statGroups[0..7]` 每组四个值 = 四个升级等级。以 Whippersnappers 为例
  （`out/store-check.txt`）：`stat0=2,4,6,8`（卡面 POWER）、`stat1=10,12,14,16`（DMG）、
  `stat2=169,203,236,287`（RPM）、`stat3=12,12,12,12`（SPD）。PC 版详情卡在 LVL1 显示
  "DMG 12 / RPM 203 / SPD +12"，与下标 1 完全吻合。

所以不是 BIG 没解析出来，是这些字段从来没有画到屏幕上。

### FILTER

SORT_BAR(41)：region 0 是 222×56 的 FILTER 按钮（锚点 8＝右下），region 1 是 184×496 的下拉面板，
chapter 0/101/277 = 收起/展开中/展开。选项来自 `MDS_BUTTON_STORE_SORT_GUNS`（ALL / OWNED /
PISTOL / RIFLE / SHOTGUN / SPREAD / HEAVY / SPECIAL / LASER）、`..._ARMOR`、`..._POWERUP`。
调研时的实现漏了 OWNED 这一项。

## 主导航高亮

`out/` 的对照裁图确认：原版选中的图标有一块点亮的青白底板，右上还有红色 `!` 角标；
调研时 RE 两者都没有。原版按钮是 `CMenuMovieButton`，状态就是 TRUNK_BUTTONS(14) 的 chapter；
入场逐个弹出的动画也是 HEADER(10) 自带的时间轴（t=702…1400 落下，7043…8000 收起），
不需要自己写 sin 缓动。

## 窗口切换

每个阶段各自 `CWindow::Open`：logo（StartupSequence.cpp:53）→ 前端加载窗（GameFrontEnd.cpp:221）
→ 前端菜单窗（GameFrontEnd.cpp:118）→ 战斗窗（M3Map.cpp:3004）→ 回前端又是新窗。
一次正常游玩要建 5 个 OS 窗口，这就是"每次切场景都有明显的换窗口动作"。
原版是单窗口，切换用 GLU_MOVIE_WIPE(12) 过场。

## 第一轮改造结果（商店）

`GameFrontEnd.cpp` 的 `DrawStore` 已重写为由上面这些 region 驱动，手写坐标全部去掉：

- 分类页签的位置与宽度来自 `MDS_BUTTON_STORE_CATEGORIES` 指定的按钮电影
  （SMALL/MED/XL/MED），间距 4 像素来自 `CMenuStore::CategoryCallback`。
  文案取 `IDS_SHOP_CATEGORY1..4`。
- 卡片改成 STORE_SCROLL 的横向传送带：三列可见、列距 258、右侧列半透明并被
  电影自带的渐隐遮罩压住。拖动与滚轮走同一个像素位移，松手吸附到整列。
  原版没有的 `< 1/37 >` 分页和 BACK 按钮已移除。
- 折叠卡与展开卡都用 SHOP_BOX 的 region 摆放。点卡片播 800→1300 的展开动画，
  展开后显示 DMG/RPM/SPD、UPGRADE LEVEL 三格星条、分类、价格、简介、
  REQUIRES LEVEL 和 PREVIEW / BUY-EQUIP-UPGRADE。
- FILTER 与下拉改用 SORT_BAR。收起的静止态是第 101 毫秒那一章，0 毫秒时整块
  还在屏幕外。选项来自 `MDS_BUTTON_STORE_SORT_*`，补回了原来漏掉的 OWNED。
- 右侧人物与换枪钮按 region 2 / region 3 定位。
- 主导航选中项改用 `GLU_MOVIE_TRUNK_BUTTONS` 的第 1 章（点亮底板），
  原来用的第 3 章是普通态，所以看不出高亮。

### 卡面文字是原版模板串

`CStoreItem::assets` 的实际内容（`out/store-check.txt` 现在会全部导出）：

| 槽位 | 内容 |
|---|---|
| 1 | 图标 PNG |
| 2 | 名称 |
| 3 | **简介**（原来错当成 5，所以简介一直没出来） |
| 4 | 展开卡的属性模板，如 `^f0DMG ^f2 #DMG ^f0RPM ^f2 #RPM ^f0SPD ^f2 #SPD`；礼包在这里放 `^f0  Best  Value!` |
| 5 | 折叠卡的右列模板，如 `^f0Power ^f4 #POWER` |

`#KEY` 由 `statGroups` 按当前熟练度等级填值：stat0=POWER、stat1=DMG、
stat2=RPM、stat3=SPD、stat7=升级价格。模板在自己的卡片 region 内换行，
这正是原版把 POWER 和数字上下叠起来的原因。

### 仍未解决

- `^fN` 到字体表的映射没有定论。原版盔甲卡的 DEF/ATK 是绿色、SPD 是红色，
  与字体 3/2 吻合；但折叠卡 `^f4 #POWER` 在 iOS 上是蓝色数字，不是字体 4 的黄色。
  当前按 iOS 截图取用：标签字体 1、数值字体 9，控制码只用于分词。
- 价格旁的货币图标还没定位到精灵，暂时用 `28 WARBUCKS` 这样的文字。
- 商品排序与原版不同：本项目按 pack/index 顺序，iOS 上 ER97E 排在 Mad Dogs 之前。
  `CStoreAggregator::SetRootCategory` 的排序规则未还原。
- STORE 图标右上的红色 `!` 角标：美术是精灵 5:38，但触发条件未还原，没有实现。
- 盔甲卡目前不显示图标，展开卡的 DEF/ATK/SPD 也没有按正负取绿/红。
- 道具的 SORT12/13/14/21 分类无法映射到已解析的记录，只保留 ALL 与 OWNED。
- 盔甲分类名用的是本项目的 HELMET/ARMOR/PANTS，原版是 HEAD 等。

## 第二轮：其余数据也都在包里

用户问"现在的 UI 是从哪儿获得的数据"。逐条对上之后，之前"造不出来"的几项全部有据可查：

| 需要的东西 | 来源 |
|---|---|
| 商店行顺序 | `CStoreItem` 尾部 int16（本项目改名 `displayOrder`）。Whippersnappers=1、ER97E=10、Mad Dogs=13、ER97S=16…… 与 iOS 截图逐格吻合；**负数＝不进列表**，全表 142 条是隐藏的 |
| 礼包只能买一次 | `CStoreItem` 尾部另一个字节（改名 `singlePurchase`）。全表 339 条里只有 STARTER PACK 是 1 |
| 金币 / 战争钞票图标 | 精灵角色 23 的 1 号与 7 号。`CMenuSystem::Load` 就是把 character 23 和菜单一起加载的 |
| 卡片上的铜/银/金星标 | 精灵 5:39 / 5:40 / 5:41（圆底星章），等级取 `CGun::Template::GetMasteryLevel` |
| 升级页的兄弟头像 | 精灵 0:161 + 兄弟序号（0:161 Percy、0:162 Francis；19:0/19:1 是带名牌的大卡，不是这里用的） |
| STORE 图标的红色 `!` | 精灵 0:140（触发条件仍未还原，故未接） |
| 升级页整页布局 | `GLU_MOVIE_UPGRADE_POPUP`(138) 的 12 个 user region：头像、关闭、星条、CURRENT/NEXT 表头与两列、武器名、图标、标题条、底部购买页签 |
| 升级条填充动画 | `GLU_MOVIE_WEAPON_UPGRADE_MASTERY`(139) 自带时间轴，章节即三格；开页时从 0 跑到实际位置 |
| 按钮按下动画 | 按钮电影（BUTTON_SMALL/MED/LG/XL）第 1 章就是松手爆闪，按下后在原位播 300 ms |
| 按钮宽度 | `MDS_BUTTON_STORE_ITEMS` 每行指定的按钮电影：BUY/EQUIP 用 SMALL，UPGRADE 用 LG，所以 UPGRADE 天然更宽，不需要缩字 |
| 换枪动画 | `CPlayer::OnSwapGun` :101048 把输入事件 5 交给玩家脚本，动画在脚本里；换枪槽后直接触发该事件 |
| CURRENT / NEXT 的数值 | `CStoreItem::statGroups[0..3]` 取当前与下一等级；表头字符串 `IDS_UPGRADE_CURRENT_LEVEL_TITLE` 与 `IDS_UPGRADE_NEXT_LEVEL_TITLE_*` |

新增的研究产物（都进了 `--movie-check`）：`out/ui-menu-sprites-*.png`（角色 0 全部 193 个动画）、
`out/ui-currency-sprites-23.png`、`out/ui-currency-sprites-26.png`、`out/ui-portrait-sprites.png`。
`out/store-check.txt` 现在导出每条记录的 6 个字符串资源和全部尾部字段。

### 这一轮改掉的

- 商店按 `displayOrder` 排序、负数隐藏；促销卡与礼包在三个分类页都出现。
- 价格改成货币精灵 + 数字，不再写 "28 WARBUCKS"。
- 礼包买过之后显示 OWNED，不再重复购买。
- 点卡片只展开，不再直接换模型预览；预览由 PREVIEW 按钮切换，已拥有的条目不显示该按钮。
- 卡片右上角按熟练度显示铜/银/金星章；满级后右侧只剩 EQUIP。
- 按钮字号统一（字体 5 原生），宽度由 MDS 指定的按钮电影决定。
- 按钮按下播原版爆闪；换枪触发原版换枪脚本事件；右侧人物可以鼠标拖动旋转。
- 升级页整页改为 movie 138 的 region 驱动：兄弟头像正确、星条开页填充、CURRENT/NEXT
  显示 POWER/DMG/RPM/SPEED 的真实数值加暴击档位。

### 仍未解决

- `^fN` 到字体表的映射仍无定论（盔甲卡的绿/红与字体 3/2 吻合，但折叠卡 `^f4` 在 iOS 上是蓝色数字）。
- STORE 图标红色 `!` 的触发条件。
- 盔甲卡不显示图标；盔甲展开卡的 DEF/ATK/SPD 未按正负取绿/红。
- 道具的 SORT12/13/14/21 分类无法映射到已解析记录，只保留 ALL 与 OWNED。
- `value242`（两条手雷记录为 1）与 `value8`、`value32` 含义未知。
