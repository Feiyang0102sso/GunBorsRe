# 商店模板试点：FILTER 筛选栏

后续进展：商品卡共用绑定已另立 UI4.2，见 [GUNS、ARMOR、POWER UPS 商品卡](store-cards-template.md)。以下保留 FILTER 试点当时的范围与验证记录。

## 范围与结果

用户要求先选简单部分，验证“从 BIG 模板与原源码复刻 UI”的路线。本次只处理商店 FILTER 的时间轴、文字区域、选项位置和命中区域；商品卡、滚动、模型、完整筛选业务仍按 PLAN 的 UI4 后续推进，不能把本次试点当成整个商店完成。

原实现用 `kSortClosedTime=101`、`kSortOpenTime=277` 在两个静态姿态间切换；文字以按钮触摸矩形居中，选项总在完全展开的位置绘制。本次从 BIG 读取章节边界，按当前播放时间同时计算底板、文字、选项和点击区域；关闭时倒放当前动画，中途反向保留当前帧。没有新增手工布局表或外部替代配置。

## 资源、模板与原函数

| 层次 | 依据及用途 |
|---|---|
| 原 BIG 资源 | core 的 `GLU_MOVIE_SORT_BAR`，Movie 41，handle `0x030004D0`。通过 `CResPackTOC::GetResource` 在正式运行时读取。 |
| 只读样本 | `big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0124_0x2a5476.bin`，753字节，960×640，400毫秒。 |
| Binary Template | `_Big_tool/binary template/big_assets/ui_movie.bt` 的 MovieChapter、MovieTransform、EmptyRegionGeometry。 |
| 章节 | 原资源章节 `[0,101,277]`；`CMovieChapter::GetChapterLengthMS`（反编译 `:109576`）规定中间章节终点为下章起点减1，因此章节0为0–100，章节1为101–276。上述数值是核对结果，运行时不复制这些值。 |
| 区域绑定 | `CMenuStore::Init :180331`：region 1绑定 `SortButtonCallback`，region 2绑定 `SortLabelCallback`；region 0由触摸处理读取。不能混用文字和命中区域。 |
| 初始与开关 | `CMenuStore::Bind :180092` 使用章节0；`HandleTouchInput :179259` 调用 SetReverse、ClearChapterPlayback、SetLoopChapter(1)；`CMovie::Update :109097` 按毫秒推进并限制章节边界。 |
| 文字 | `CMenuStore::SortLabelCallback :178766` 读取字体5和字符串宽度，以region 2的顶端及水平中心定位。字符串仍由 `IDS_SHOP_FILTER` 从资源解析。 |
| 选项 | `CMenuStore::SortButtonCallback :178813` 按区域水平居中排列，间距为按钮高度加半个高度；这是原代码规则。每个按钮的 Movie、Sprite、字符串来自原程序提取的 MDS 表，按钮几何继续读取 BIG。 |
| 输入状态 | `CMenuStore::Update :179558` 仅在展开状态更新选项按钮输入；倒放时仍按当前可见region绘制。 |

表中的行号均指 `_IDA_OUT/gunbros_3.6.0_IOS.c`，不是原 `.cpp` 行号。资料入口另见 [UI二进制模板](ui-binary-templates.md) 和 [原布局来源](ui-original-layout.md)。

## 实现位置

- `src/gun_bros_re/gun_bros/CMovie.{h,cpp}`：`GetChapterRange` 从解析后的章节和总时长求可播放区间，缺失章节明确失败。
- `src/gun_bros_re/runtime/GameFrontEnd.cpp`：`MenuState` 保存独立播放游标，`AdvanceStoreFilter` 推进非循环的商店章节，`DrawStore` 以同一时间解析region、绘制并命中。缓存中的CMovie资源不保存页面实例状态。
- 同文件 `RunStoreTemplateCheck`：隔离账户专项验证和截图；`src/gun_bros_re/main.cpp` 保留研究入口51及 `--store-template-check`。

## 验证与查看

运行 `bin/x64/Release/gun_bros_re.exe --store-template-check --mute`，或研究菜单51。测试账户仅在内存和 `out/` 范围使用；原 BIG、解包文件和原存档不修改。

- 专项检查：`out/store-template-check.log`，退出0。验证已核对的章节边界、移动中的区域、中途反向、开闭终点、多选、尚未到达的选项位置不能提前命中，以及货币/库存没有变化。
- 改变内存中的CMovie副本章节后，播放终点同步改变；原文件不修改。这项检查用于避免把写死时间伪装成模板驱动。
- 截图：`out/store-template-closed.png`、`out/store-template-opening.png`、`out/store-template-open.png`、`out/store-template-closing.png`、`out/store-template-closed-again.png`。展开和中间态已目视核对。
- Release构建：`out/store-template-build.log`，退出0。首次受限环境中MSBuild FileTracker访问失败；在获准的沙箱外构建后通过。
- 菜单集成：`--game-menu-check --mute`，`out/store-template-menu-regression.log`，退出0；包含购买/装备、展开商品卡、筛选、页面导航、游戏进度及保存重载的既有用例。
- Movie解析：`--movie-check --mute`，`out/store-template-movie-check.log`，175个Movie、0失败，退出0。
- 受保护文件：`out/store-template-protected-check.txt`，`saves/`及`userdata/`共23个文件，验证前后路径与SHA256无变化。`git diff --check`通过。

直接体验：运行根目录 `store-template-demo.cmd`，进入商店后点击右下角 FILTER，观察面板上移、选中 PISTOL/RIFLE，再点击移动到上方的 FILTER 收起。演示账户使用 `out/store-template-demo.dat`，静音仍执行资源解析与全部游戏逻辑。

## 仍待后续处理

现有筛选业务仍有按行号映射位掩码、Powerup仅展示已接入分类等历史简化；本次未宣称恢复完整 `CStoreAggregator`。商店卡片开合时间、横向滚动、按钮通用反馈、模型裁剪和其他手写布局仍留在 PLAN，不能借本次试点勾选 UI2/UI4 总项。
