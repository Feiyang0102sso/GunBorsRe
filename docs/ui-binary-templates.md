# Gun Bros UI 二进制模板与文件索引

2026-09-10 消费链纠错：SpriteGlu 的旋转位最终转置纹理坐标，不能额外反转U；否则 Movie144 的模式外框会叠成横线。Airstrike 的 pack5 Movie2 只含前景，外围来自持续播放的 core Movie131。详见 [原函数与像素验证](xp-ui-decoration-fix-2026-09-10.md)。

本次任务以 `big_360_out` 为输入，定位原 UI 时间轴并制作辅助理解的 010 Editor `.bt`。不修改 UI 实现，不修改解包文件；按用户要求未执行 010 Editor 编译或运行测试。

## 阅读入口

- [ui_movie.bt](<../_Big_tool/binary template/big_assets/ui_movie.bt>)：逐字段中文注释，覆盖原 CMovie 的八种对象类型。
- [完整结构索引](../out/ui-movie-catalog.json)：每个对象的偏移、类型、关键帧时间，以及用户区域的原始坐标、锚点、尺寸与显示状态。
- [只读索引工具](../src/tools/catalog_ui_movies.py)：可重复生成本页及 JSON，使用 Python 标准库。

## 来源与识别方法

原名来自 `gunbros` 内的 GLU_MOVIE_* 字符串；用原 CStringToKey 哈希查询各包解出的 TOC，得到资源 handle。外层 `big/packTOC_xga.dat` 指明每个包的 TOC 逻辑 ID；每包 resources.csv 将逻辑 ID 对应到 physical_id 和归档 Offset，再找到解包文件。最后按原 CMovie 各 Init 的读取顺序检查全部字节。未使用重建页面的手写坐标或手填 Movie 数量作为识别依据。

资源 handle、逻辑 ID、物理序号、原 BIG 偏移和 Movie 序号是不同的编号。Movie 序号 = handle - GLU_MOVIE_MOVIE 基址；普通 handle 的低 15 位是逻辑 ID。文件名中的末尾 Offset 是原 BIG 的位置，不是解压文件内的偏移。

**不要对整个 BIG 应用 ui_movie.bt，也不要把 0xf4e02223 目录中所有 bin 都当作 Movie。** 该目录还含地图、脚本、模型等其他二进制。部分 CSV 标成 bin 的文件现已改成 png 扩展名，索引工具按文件主名匹配。

## 主要界面模板

| 原资源名 | Movie | 解压字节 | 具体文件 |
|---|---:|---:|---|
| GLU_MOVIE_HEADER | 10 | 5940 | [pack0_core_xga_0093_0x2a2dac.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0093_0x2a2dac.bin>) |
| GLU_MOVIE_INFO_CLUSTER | 11 | 374 | [pack0_core_xga_0094_0x2a3188.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0094_0x2a3188.bin>) |
| GLU_MOVIE_WIPE | 12 | 300 | [pack0_core_xga_0095_0x2a321d.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0095_0x2a321d.bin>) |
| GLU_MOVIE_TRUNK_BUTTONS | 14 | 610 | [pack0_core_xga_0097_0x2a3350.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0097_0x2a3350.bin>) |
| GLU_MOVIE_MAP_PARALAX_COPY | 47 | 1934 | [pack0_core_xga_0130_0x2a5950.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0130_0x2a5950.bin>) |
| GLU_MOVIE_STORE_MENU | 37 | 222 | [pack0_core_xga_0120_0x2a4edd.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0120_0x2a4edd.bin>) |
| GLU_MOVIE_STORE_SCROLL | 38 | 859 | [pack0_core_xga_0121_0x2a4f62.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0121_0x2a4f62.bin>) |
| GLU_MOVIE_SHOP_BOX | 39 | 2518 | [pack0_core_xga_0122_0x2a505e.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0122_0x2a505e.bin>) |
| GLU_MOVIE_SORT_BAR | 41 | 753 | [pack0_core_xga_0124_0x2a5476.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0124_0x2a5476.bin>) |
| GLU_MOVIE_MISSION_MENU | 48 | 758 | [pack0_core_xga_0131_0x2a5b66.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0131_0x2a5b66.bin>) |
| GLU_MOVIE_MISSION_LIST | 49 | 996 | [pack0_core_xga_0132_0x2a5c62.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0132_0x2a5c62.bin>) |
| GLU_MOVIE_MISSION_BOX | 50 | 2371 | [pack0_core_xga_0133_0x2a5d77.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0133_0x2a5d77.bin>) |
| GLU_MOVIE_WAVE_SELECT | 65 | 326 | [pack0_core_xga_0148_0x2a6aaa.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0148_0x2a6aaa.bin>) |
| GLU_MOVIE_PLAYER_SELECT | 70 | 1181 | [pack0_core_xga_0153_0x2a6f3a.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0153_0x2a6f3a.bin>) |
| GLU_MOVIE_LIST_MENU | 71 | 1581 | [pack0_core_xga_0154_0x2a706b.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0154_0x2a706b.bin>) |
| GLU_MOVIE_OFFLINE_BROHOOD | 75 | 974 | [pack0_core_xga_0158_0x2a7483.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0158_0x2a7483.bin>) |
| GLU_MOVIE_WELCOME_NEW | 106 | 2666 | [pack0_core_xga_0189_0x2a8dc6.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0189_0x2a8dc6.bin>) |

## 从商店骨架开始读

STORE_MENU 的头部为 `C0 03 80 02 DC 05 00 00 06 00`：宽 960、高 640、时长 1500ms、6 个对象。四个 type=6 区域只是动态内容的布局槽位，不内嵌商品列表、玩家模型或回调函数。

| 对象序号 | 文件内偏移 | 类型 | 用户区域序号 |
|---:|---:|---|---:|
| 0 | 0xA | CMovieRegion | 0 |
| 1 | 0x32 | CMovieRegion | 1 |
| 2 | 0x5A | CMovieRegion | 2 |
| 3 | 0x82 | CMovieRegion | 3 |
| 4 | 0xAA | CMovieChapter | — |
| 5 | 0xB1 | CMovieFill | — |

四个区域按原 CMenuStore::Init (:180199) 分别绑定商品内容、分类、玩家模型、换枪按钮。对象序号与区域序号在本例恰好相同，其他 Movie 不保证相同。

文件大小核对：`10 + 4 × (3 + 37) + (3 + 4) + (3 + 42) = 222` 字节。
SHOP_BOX 则通过同一组区域的关键帧从折叠变为展开；章节文件数据为 800、1300，原引擎在内存中另补起始章节 0。

## 格式要点

| 类型 | 原类 | 文件单帧字节 | 本次对象数 |
|---:|---|---:|---:|
| 0 | CMovieSprite | 33 | 539 |
| 1 | CMovieTiledSprite | 48 | 152 |
| 2 | CEmbededMovie | 6 | 2 |
| 3 | CMovieText | 12 | 0 |
| 5 | CMovieChapter | 4 | 114 |
| 6 | CMovieRegion | 37 | 664 |
| 7 | CMovieFill | 42 | 51 |
| 8 | CMovieSoundSet | 6 | 8 |

- 单帧长度包括 4 字节时间，不包括对象的 3 字节头；所有字段小端、紧密排列。
- x/y 为 int16，alpha/scale/rotation 为有符号 16.16 定点数。宽高原读取为 UInt16，布局计算按 int16 使用。
- type=6 的末字节是可选回调标签，不是用户区域顺序编号；可见性在此前的 EmptyRegionGeometry 中。
- type=2 的末字节是 visible；原 CEmbededMovie::Refresh 另行使用取模推进子电影，不应把这个字节命名为 loop。
- type=7 最后 6 字节是两个 RGB 渐变颜色，不是两个 RGBA；type=8 最后两字节是声音索引与播放模式。
- 原读取器支持 type=3，但当前 175 个文件没有这种对象；三个 UInt16 的语义保留为待解引用，模板仅保证依据原读取顺序描述其宽度。
- 不能把 np_malloc 的内存步长直接当文件记录长度，尤其是 Sprite 33/36、Region 37/40、Text 12/40。
- 当前批次有 9 个只有 10 字节头部的合法空 Movie，不应因对象数为 0 丢弃，也不据此判断其历史用途。

## 如何关联其他资源

Movie 保存精灵与动画选择，不保存 PNG 像素。图片继续沿 `SPRITEGLU__BINARY_GLOBAL`、`SPRITEGLU__BINARY_ARCHETYPE_000` 和纹理页映射查找；字体入口为 `FONT_KEYSET`。菜单的 MDS 组合表存于原程序，不能在这些 Movie 中凭空增加按钮动作、商品价格或函数地址字段。

## 各包与完整文件索引

这批文件中共识别 **175 个 Movie**。名称栏为空表示未找到对应 GLU_MOVIE_* 别名，不表示文件无效。结构索引只描述资源布局，不是渲染结果或完整 UI 行为验证。

| 资源包 | Movie 基址 | 数量 | 解包 TOC |
|---|---|---:|---|
| pack0_core_xga | 0x030004A7 | 148 | [pack0_core_xga_0330_0xe17fa1.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0330_0xe17fa1.bin>) |
| pack1_xga | 0x03000407 | 3 | [pack1_xga_0318_0xe04095.bin](<../big_360_out/pack1_xga/0xf4e02223/pack1_xga_0318_0xe04095.bin>) |
| pack10_xga | 0x0300017C | 0 | [pack10_xga_0114_0x6bc063.bin](<../big_360_out/pack10_xga/0xf4e02223/pack10_xga_0114_0x6bc063.bin>) |
| pack11_xga | 0x0300025F | 0 | [pack11_xga_0220_0x7521ea.bin](<../big_360_out/pack11_xga/0xf4e02223/pack11_xga_0220_0x7521ea.bin>) |
| pack12_xga | 0x030001A6 | 0 | [pack12_xga_0111_0xb59af8.bin](<../big_360_out/pack12_xga/0xf4e02223/pack12_xga_0111_0xb59af8.bin>) |
| pack2_xga | 0x0300025B | 5 | [pack2_xga_0187_0x225553.bin](<../big_360_out/pack2_xga/0xf4e02223/pack2_xga_0187_0x225553.bin>) |
| pack3_xga | 0x0300056B | 3 | [pack3_xga_0344_0x211669.bin](<../big_360_out/pack3_xga/0xf4e02223/pack3_xga_0344_0x211669.bin>) |
| pack4_xga | 0x0300091F | 3 | [pack4_xga_1152_0x2e41b74.bin](<../big_360_out/pack4_xga/0xf4e02223/pack4_xga_1152_0x2e41b74.bin>) |
| pack5_xga | 0x030005D8 | 6 | [pack5_xga_0748_0x14dbea9.bin](<../big_360_out/pack5_xga/0xf4e02223/pack5_xga_0748_0x14dbea9.bin>) |
| pack6_xga | 0x03000165 | 1 | [pack6_xga_0097_0x62d3e2.bin](<../big_360_out/pack6_xga/0xf4e02223/pack6_xga_0097_0x62d3e2.bin>) |
| pack7_xga | 0x0300031E | 4 | [pack7_xga_0184_0x4cdd32.bin](<../big_360_out/pack7_xga/0xf4e02223/pack7_xga_0184_0x4cdd32.bin>) |
| pack8_xga | 0x03000179 | 1 | [pack8_xga_0114_0xab8c15.bin](<../big_360_out/pack8_xga/0xf4e02223/pack8_xga_0114_0xab8c15.bin>) |
| pack9_xga | 0x030002F0 | 1 | [pack9_xga_0152_0x39f73d.bin](<../big_360_out/pack9_xga/0xf4e02223/pack9_xga_0152_0x39f73d.bin>) |

### pack0_core_xga

| Movie | 原名 | handle | 字节 | 对象 | 文件 |
|---:|---|---|---:|---:|---|
| 0 |  | 0x030004A7 | 329 | 3 | [pack0_core_xga_0083_0x2a24e0.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0083_0x2a24e0.bin>) |
| 1 | GLU_MOVIE_HUD_PAD | 0x030004A8 | 532 | 13 | [pack0_core_xga_0084_0x2a2553.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0084_0x2a2553.bin>) |
| 2 |  | 0x030004A9 | 126 | 3 | [pack0_core_xga_0085_0x2a262c.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0085_0x2a262c.bin>) |
| 3 | GLU_MOVIE_HUD_PAUSE | 0x030004AA | 1111 | 8 | [pack0_core_xga_0086_0x2a267d.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0086_0x2a267d.bin>) |
| 4 |  | 0x030004AB | 669 | 6 | [pack0_core_xga_0087_0x2a277a.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0087_0x2a277a.bin>) |
| 5 | GLU_MOVIE_MOVE_STICK_INTRO | 0x030004AC | 1111 | 4 | [pack0_core_xga_0088_0x2a282d.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0088_0x2a282d.bin>) |
| 6 | GLU_MOVIE_SHOOT_STICK_INTRO | 0x030004AD | 946 | 4 | [pack0_core_xga_0089_0x2a2936.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0089_0x2a2936.bin>) |
| 7 | GLU_MOVIE_LEVEL_UP | 0x030004AE | 1175 | 9 | [pack0_core_xga_0090_0x2a2a0d.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0090_0x2a2a0d.bin>) |
| 8 |  | 0x030004AF | 1195 | 10 | [pack0_core_xga_0091_0x2a2b6f.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0091_0x2a2b6f.bin>) |
| 9 | GLU_MOVIE_RADIAL_WIDGET | 0x030004B0 | 697 | 5 | [pack0_core_xga_0092_0x2a2cbd.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0092_0x2a2cbd.bin>) |
| 10 | GLU_MOVIE_HEADER | 0x030004B1 | 5940 | 28 | [pack0_core_xga_0093_0x2a2dac.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0093_0x2a2dac.bin>) |
| 11 | GLU_MOVIE_INFO_CLUSTER | 0x030004B2 | 374 | 6 | [pack0_core_xga_0094_0x2a3188.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0094_0x2a3188.bin>) |
| 12 | GLU_MOVIE_WIPE | 0x030004B3 | 300 | 3 | [pack0_core_xga_0095_0x2a321d.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0095_0x2a321d.bin>) |
| 13 | GLU_MOVIE_BACK_BUTTON | 0x030004B4 | 610 | 5 | [pack0_core_xga_0096_0x2a3287.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0096_0x2a3287.bin>) |
| 14 | GLU_MOVIE_TRUNK_BUTTONS | 0x030004B5 | 610 | 5 | [pack0_core_xga_0097_0x2a3350.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0097_0x2a3350.bin>) |
| 15 |  | 0x030004B6 | 1142 | 10 | [pack0_core_xga_0098_0x2a341b.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0098_0x2a341b.bin>) |
| 16 |  | 0x030004B7 | 9593 | 25 | [pack0_core_xga_0099_0x2a3558.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0099_0x2a3558.bin>) |
| 17 | GLU_MOVIE_WRAPUP_SCREEN | 0x030004B8 | 1529 | 13 | [pack0_core_xga_0100_0x2a3a3f.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0100_0x2a3a3f.bin>) |
| 18 | GLU_MOVIE_WRAPUP_GALLERY | 0x030004B9 | 517 | 6 | [pack0_core_xga_0101_0x2a3c17.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0101_0x2a3c17.bin>) |
| 19 | GLU_MOVIE_MODEL_GALLERY_ITEM | 0x030004BA | 523 | 12 | [pack0_core_xga_0102_0x2a3caf.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0102_0x2a3caf.bin>) |
| 20 | GLU_MOVIE_WRAPUP_BOX | 0x030004BB | 614 | 14 | [pack0_core_xga_0103_0x2a3d62.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0103_0x2a3d62.bin>) |
| 21 | GLU_MOVIE_MISSION_END | 0x030004BC | 97 | 1 | [pack0_core_xga_0104_0x2a3e39.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0104_0x2a3e39.bin>) |
| 22 | GLU_MOVIE_MAP_RETICLE | 0x030004BD | 1586 | 10 | [pack0_core_xga_0105_0x2a3e70.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0105_0x2a3e70.bin>) |
| 23 |  | 0x030004BE | 1510 | 12 | [pack0_core_xga_0106_0x2a4006.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0106_0x2a4006.bin>) |
| 24 | GLU_MOVIE_SPLASH | 0x030004BF | 437 | 4 | [pack0_core_xga_0107_0x2a41a4.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0107_0x2a41a4.bin>) |
| 25 | GLU_MOVIE_BUTTON_SMALL | 0x030004C0 | 494 | 5 | [pack0_core_xga_0108_0x2a4233.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0108_0x2a4233.bin>) |
| 26 | GLU_MOVIE_BUTTON_MED | 0x030004C1 | 490 | 5 | [pack0_core_xga_0109_0x2a42cb.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0109_0x2a42cb.bin>) |
| 27 | GLU_MOVIE_BUTTON_LG | 0x030004C2 | 453 | 5 | [pack0_core_xga_0110_0x2a4367.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0110_0x2a4367.bin>) |
| 28 | GLU_MOVIE_MISSION_OBJECTIVES | 0x030004C3 | 943 | 10 | [pack0_core_xga_0111_0x2a43ff.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0111_0x2a43ff.bin>) |
| 29 | GLU_MOVIE_BUTTON_XL | 0x030004C4 | 420 | 5 | [pack0_core_xga_0112_0x2a44d5.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0112_0x2a44d5.bin>) |
| 30 | GLU_MOVIE_MISSION_OBJECTIVES_BOX | 0x030004C5 | 249 | 6 | [pack0_core_xga_0113_0x2a4563.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0113_0x2a4563.bin>) |
| 31 | GLU_MOVIE_EXPLODIUM | 0x030004C6 | 3568 | 25 | [pack0_core_xga_0114_0x2a45d5.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0114_0x2a45d5.bin>) |
| 32 | GLU_MOVIE_BUCKET_BUTTON | 0x030004C7 | 957 | 8 | [pack0_core_xga_0115_0x2a4866.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0115_0x2a4866.bin>) |
| 33 | GLU_MOVIE_BUCKET_FILL | 0x030004C8 | 523 | 6 | [pack0_core_xga_0116_0x2a4981.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0116_0x2a4981.bin>) |
| 34 | GLU_MOVIE_WAVE_CLEARED | 0x030004C9 | 1174 | 10 | [pack0_core_xga_0117_0x2a4a0c.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0117_0x2a4a0c.bin>) |
| 35 | GLU_MOVIE_EXPLODIUM_ICON | 0x030004CA | 949 | 12 | [pack0_core_xga_0118_0x2a4b69.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0118_0x2a4b69.bin>) |
| 36 | GLU_MOVIE_EXPLODIUM_BG | 0x030004CB | 2494 | 16 | [pack0_core_xga_0119_0x2a4c67.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0119_0x2a4c67.bin>) |
| 37 | GLU_MOVIE_STORE_MENU | 0x030004CC | 222 | 6 | [pack0_core_xga_0120_0x2a4edd.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0120_0x2a4edd.bin>) |
| 38 | GLU_MOVIE_STORE_SCROLL | 0x030004CD | 859 | 10 | [pack0_core_xga_0121_0x2a4f62.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0121_0x2a4f62.bin>) |
| 39 | GLU_MOVIE_SHOP_BOX | 0x030004CE | 2518 | 22 | [pack0_core_xga_0122_0x2a505e.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0122_0x2a505e.bin>) |
| 40 |  | 0x030004CF | 2383 | 16 | [pack0_core_xga_0123_0x2a528b.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0123_0x2a528b.bin>) |
| 41 | GLU_MOVIE_SORT_BAR | 0x030004D0 | 753 | 5 | [pack0_core_xga_0124_0x2a5476.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0124_0x2a5476.bin>) |
| 42 |  | 0x030004D1 | 796 | 12 | [pack0_core_xga_0125_0x2a5553.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0125_0x2a5553.bin>) |
| 43 |  | 0x030004D2 | 122 | 3 | [pack0_core_xga_0126_0x2a5614.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0126_0x2a5614.bin>) |
| 44 | GLU_MOVIE_AD_SPACE | 0x030004D3 | 431 | 6 | [pack0_core_xga_0127_0x2a5657.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0127_0x2a5657.bin>) |
| 45 | GLU_MOVIE_AD_SPACE_WIDE | 0x030004D4 | 431 | 6 | [pack0_core_xga_0128_0x2a56ff.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0128_0x2a56ff.bin>) |
| 46 | GLU_MOVIE_POP_UP | 0x030004D5 | 1680 | 10 | [pack0_core_xga_0129_0x2a57a5.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0129_0x2a57a5.bin>) |
| 47 | GLU_MOVIE_MAP_PARALAX_COPY | 0x030004D6 | 1934 | 25 | [pack0_core_xga_0130_0x2a5950.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0130_0x2a5950.bin>) |
| 48 | GLU_MOVIE_MISSION_MENU | 0x030004D7 | 758 | 9 | [pack0_core_xga_0131_0x2a5b66.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0131_0x2a5b66.bin>) |
| 49 | GLU_MOVIE_MISSION_LIST | 0x030004D8 | 996 | 8 | [pack0_core_xga_0132_0x2a5c62.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0132_0x2a5c62.bin>) |
| 50 | GLU_MOVIE_MISSION_BOX | 0x030004D9 | 2371 | 20 | [pack0_core_xga_0133_0x2a5d77.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0133_0x2a5d77.bin>) |
| 51 | GLU_MOVIE_PLANET_FLAG | 0x030004DA | 1013 | 9 | [pack0_core_xga_0134_0x2a5f80.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0134_0x2a5f80.bin>) |
| 52 | GLU_MOVIE_POWER_UP_LAYOUT | 0x030004DB | 1232 | 10 | [pack0_core_xga_0135_0x2a60a5.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0135_0x2a60a5.bin>) |
| 53 | GLU_MOVIE_PANIC_TOUCH_EFFECT | 0x030004DC | 112 | 1 | [pack0_core_xga_0136_0x2a61a7.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0136_0x2a61a7.bin>) |
| 54 | GLU_MOVIE_CHAMBER_OVERLAY | 0x030004DD | 732 | 7 | [pack0_core_xga_0137_0x2a61ea.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0137_0x2a61ea.bin>) |
| 55 |  | 0x030004DE | 858 | 11 | [pack0_core_xga_0138_0x2a6305.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0138_0x2a6305.bin>) |
| 56 | GLU_MOVIE_BROTHER_MENU_SCROLL | 0x030004DF | 853 | 7 | [pack0_core_xga_0139_0x2a641a.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0139_0x2a641a.bin>) |
| 57 | GLU_MOVIE_BROTHER_BOX | 0x030004E0 | 642 | 9 | [pack0_core_xga_0140_0x2a64fb.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0140_0x2a64fb.bin>) |
| 58 | GLU_MOVIE_BROTHER_REWARD | 0x030004E1 | 681 | 8 | [pack0_core_xga_0141_0x2a65bb.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0141_0x2a65bb.bin>) |
| 59 | GLU_MOVIE_BROTHER_BOX_END | 0x030004E2 | 280 | 5 | [pack0_core_xga_0142_0x2a6695.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0142_0x2a6695.bin>) |
| 60 |  | 0x030004E3 | 1894 | 22 | [pack0_core_xga_0143_0x2a66f9.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0143_0x2a66f9.bin>) |
| 61 |  | 0x030004E4 | 779 | 7 | [pack0_core_xga_0144_0x2a68df.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0144_0x2a68df.bin>) |
| 62 | GLU_MOVIE_SCROLLBAR_VERT | 0x030004E5 | 155 | 3 | [pack0_core_xga_0145_0x2a69ae.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0145_0x2a69ae.bin>) |
| 63 | GLU_MOVIE_SCROLLBAR_HORIZ | 0x030004E6 | 155 | 3 | [pack0_core_xga_0146_0x2a6a01.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0146_0x2a6a01.bin>) |
| 64 | GLU_MOVIE_SCROLLBAR_SMALL_VERT | 0x030004E7 | 155 | 3 | [pack0_core_xga_0147_0x2a6a57.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0147_0x2a6a57.bin>) |
| 65 | GLU_MOVIE_WAVE_SELECT | 0x030004E8 | 326 | 4 | [pack0_core_xga_0148_0x2a6aaa.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0148_0x2a6aaa.bin>) |
| 66 |  | 0x030004E9 | 787 | 11 | [pack0_core_xga_0149_0x2a6b1c.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0149_0x2a6b1c.bin>) |
| 67 | GLU_MOVIE_WAVE_SELECT_BUTTON | 0x030004EA | 343 | 4 | [pack0_core_xga_0150_0x2a6bdf.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0150_0x2a6bdf.bin>) |
| 68 |  | 0x030004EB | 1936 | 17 | [pack0_core_xga_0151_0x2a6c6d.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0151_0x2a6c6d.bin>) |
| 69 |  | 0x030004EC | 665 | 6 | [pack0_core_xga_0152_0x2a6e87.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0152_0x2a6e87.bin>) |
| 70 | GLU_MOVIE_PLAYER_SELECT | 0x030004ED | 1181 | 7 | [pack0_core_xga_0153_0x2a6f3a.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0153_0x2a6f3a.bin>) |
| 71 | GLU_MOVIE_LIST_MENU | 0x030004EE | 1581 | 12 | [pack0_core_xga_0154_0x2a706b.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0154_0x2a706b.bin>) |
| 72 | GLU_MOVIE_LIST_MENU_PAUSE | 0x030004EF | 1581 | 12 | [pack0_core_xga_0155_0x2a71ea.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0155_0x2a71ea.bin>) |
| 73 | GLU_MOVIE_LIST_MENU_BUTTON | 0x030004F0 | 470 | 5 | [pack0_core_xga_0156_0x2a7369.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0156_0x2a7369.bin>) |
| 74 | GLU_MOVIE_LIST_MENU_TEXT | 0x030004F1 | 289 | 4 | [pack0_core_xga_0157_0x2a740d.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0157_0x2a740d.bin>) |
| 75 | GLU_MOVIE_OFFLINE_BROHOOD | 0x030004F2 | 974 | 12 | [pack0_core_xga_0158_0x2a7483.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0158_0x2a7483.bin>) |
| 76 | GLU_MOVIE_ENLIST_OFFLINE_BUTTON | 0x030004F3 | 343 | 4 | [pack0_core_xga_0159_0x2a7585.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0159_0x2a7585.bin>) |
| 77 | GLU_MOVIE_POPUP | 0x030004F4 | 1619 | 19 | [pack0_core_xga_0160_0x2a7617.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0160_0x2a7617.bin>) |
| 78 | GLU_MOVIE_POPUP_BRANCH | 0x030004F5 | 1319 | 15 | [pack0_core_xga_0161_0x2a778b.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0161_0x2a778b.bin>) |
| 79 | GLU_MOVIE_PROMPT_BUTTONS | 0x030004F6 | 376 | 4 | [pack0_core_xga_0162_0x2a78d6.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0162_0x2a78d6.bin>) |
| 80 |  | 0x030004F7 | 1194 | 12 | [pack0_core_xga_0163_0x2a796a.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0163_0x2a796a.bin>) |
| 81 | GLU_MOVIE_SPLASH_INTRO_MP | 0x030004F8 | 363 | 4 | [pack0_core_xga_0164_0x2a7aec.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0164_0x2a7aec.bin>) |
| 82 | GLU_MOVIE_WEAPON_TOGGLE | 0x030004F9 | 376 | 4 | [pack0_core_xga_0165_0x2a7b6b.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0165_0x2a7b6b.bin>) |
| 83 |  | 0x030004FA | 1174 | 10 | [pack0_core_xga_0166_0x2a7c05.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0166_0x2a7c05.bin>) |
| 84 |  | 0x030004FB | 1207 | 10 | [pack0_core_xga_0167_0x2a7d5f.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0167_0x2a7d5f.bin>) |
| 85 |  | 0x030004FC | 46 | 1 | [pack0_core_xga_0168_0x2a7ebe.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0168_0x2a7ebe.bin>) |
| 86 | GLU_MOVIE_BG_OPTIONS | 0x030004FD | 91 | 2 | [pack0_core_xga_0169_0x2a7eea.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0169_0x2a7eea.bin>) |
| 87 | GLU_MOVIE_PERFECT_WAVE | 0x030004FE | 1386 | 8 | [pack0_core_xga_0170_0x2a7f2f.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0170_0x2a7f2f.bin>) |
| 88 |  | 0x030004FF | 394 | 10 | [pack0_core_xga_0171_0x2a8079.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0171_0x2a8079.bin>) |
| 89 | GLU_MOVIE_ACHIEVEMENT_OVERLAY | 0x03000500 | 472 | 3 | [pack0_core_xga_0172_0x2a8124.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0172_0x2a8124.bin>) |
| 90 | GLU_MOVIE_PLANET_MULT | 0x03000501 | 50 | 1 | [pack0_core_xga_0173_0x2a81c3.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0173_0x2a81c3.bin>) |
| 91 | GLU_MOVIE_MASTERY | 0x03000502 | 940 | 14 | [pack0_core_xga_0174_0x2a81fb.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0174_0x2a81fb.bin>) |
| 92 |  | 0x03000503 | 226 | 2 | [pack0_core_xga_0175_0x2a82fa.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0175_0x2a82fa.bin>) |
| 93 |  | 0x03000504 | 2128 | 20 | [pack0_core_xga_0176_0x2a8363.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0176_0x2a8363.bin>) |
| 94 |  | 0x03000505 | 899 | 7 | [pack0_core_xga_0177_0x2a8572.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0177_0x2a8572.bin>) |
| 95 |  | 0x03000506 | 1600 | 21 | [pack0_core_xga_0178_0x2a8660.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0178_0x2a8660.bin>) |
| 96 | GLU_MOVIE_BROTHER_TOGGLE | 0x03000507 | 481 | 6 | [pack0_core_xga_0179_0x2a87cc.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0179_0x2a87cc.bin>) |
| 97 | GLU_MOVIE_BROBUFF_MENU | 0x03000508 | 836 | 11 | [pack0_core_xga_0180_0x2a887c.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0180_0x2a887c.bin>) |
| 98 | GLU_MOVIE_BROBUFF_BOX | 0x03000509 | 385 | 7 | [pack0_core_xga_0181_0x2a898e.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0181_0x2a898e.bin>) |
| 99 |  | 0x0300050A | 206 | 5 | [pack0_core_xga_0182_0x2a8a27.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0182_0x2a8a27.bin>) |
| 100 |  | 0x0300050B | 246 | 6 | [pack0_core_xga_0183_0x2a8a8e.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0183_0x2a8a8e.bin>) |
| 101 |  | 0x0300050C | 246 | 6 | [pack0_core_xga_0184_0x2a8b14.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0184_0x2a8b14.bin>) |
| 102 | GLU_MOVIE_BRO_OPS_DETAILS | 0x0300050D | 478 | 12 | [pack0_core_xga_0185_0x2a8b9a.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0185_0x2a8b9a.bin>) |
| 103 | GLU_MOVIE_BRO_OP_BOX | 0x0300050E | 315 | 7 | [pack0_core_xga_0186_0x2a8c47.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0186_0x2a8c47.bin>) |
| 104 | GLU_MOVIE_BRO_OP_METER | 0x0300050F | 149 | 2 | [pack0_core_xga_0187_0x2a8cc7.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0187_0x2a8cc7.bin>) |
| 105 |  | 0x03000510 | 602 | 8 | [pack0_core_xga_0188_0x2a8d16.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0188_0x2a8d16.bin>) |
| 106 | GLU_MOVIE_WELCOME_NEW | 0x03000511 | 2666 | 28 | [pack0_core_xga_0189_0x2a8dc6.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0189_0x2a8dc6.bin>) |
| 107 | GLU_MOVIE_BRO_OPS_OVERLAY | 0x03000512 | 505 | 3 | [pack0_core_xga_0190_0x2a8fae.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0190_0x2a8fae.bin>) |
| 108 | GLU_MOVIE_BRO_GIFTS_OVERLAY | 0x03000513 | 505 | 3 | [pack0_core_xga_0191_0x2a9054.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0191_0x2a9054.bin>) |
| 109 | GLU_MOVIE_BROTHER_MENU_SCROLL_3_OPTION | 0x03000514 | 628 | 6 | [pack0_core_xga_0192_0x2a90fa.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0192_0x2a90fa.bin>) |
| 110 | GLU_MOVIE_BRO_BUFFS_DETAILS | 0x03000515 | 1659 | 23 | [pack0_core_xga_0193_0x2a91bf.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0193_0x2a91bf.bin>) |
| 111 | GLU_MOVIE_ADD_FRIENDS_POPUP | 0x03000516 | 2845 | 22 | [pack0_core_xga_0194_0x2a92bb.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0194_0x2a92bb.bin>) |
| 112 | GLU_MOVIE_ADD_FRIENDS_POPUP_SMALL | 0x03000517 | 2882 | 22 | [pack0_core_xga_0195_0x2a94de.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0195_0x2a94de.bin>) |
| 113 | GLU_MOVIE_ON_DUTY_STAMP | 0x03000518 | 255 | 2 | [pack0_core_xga_0196_0x2a970e.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0196_0x2a970e.bin>) |
| 114 |  | 0x03000519 | 2227 | 21 | [pack0_core_xga_0197_0x2a9771.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0197_0x2a9771.bin>) |
| 115 | GLU_MOVIE_WRAPUP_MENU_SCROLL | 0x0300051A | 403 | 5 | [pack0_core_xga_0198_0x2a995c.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0198_0x2a995c.bin>) |
| 116 | GLU_MOVIE_HUD_PAD_IPAD | 0x0300051B | 532 | 13 | [pack0_core_xga_0199_0x2a99fa.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0199_0x2a99fa.bin>) |
| 117 |  | 0x0300051C | 1537 | 10 | [pack0_core_xga_0200_0x2a9ad2.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0200_0x2a9ad2.bin>) |
| 118 | GLU_MOVIE_WAVE_WRAPUP | 0x0300051D | 5654 | 40 | [pack0_core_xga_0201_0x2a9c31.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0201_0x2a9c31.bin>) |
| 119 | GLU_MOVIE_WRAPUP_SCREEN_MP | 0x0300051E | 1618 | 14 | [pack0_core_xga_0202_0x2a9f8f.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0202_0x2a9f8f.bin>) |
| 120 | GLU_MOVIE_REMATCH_BUTTON | 0x0300051F | 888 | 7 | [pack0_core_xga_0203_0x2aa168.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0203_0x2aa168.bin>) |
| 121 | GLU_MOVIE_REMATCH_BUTTON_DISABLED | 0x03000520 | 957 | 8 | [pack0_core_xga_0204_0x2aa26d.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0204_0x2aa26d.bin>) |
| 122 | GLU_MOVIE_NETWORK_CONNECTION | 0x03000521 | 557 | 5 | [pack0_core_xga_0205_0x2aa389.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0205_0x2aa389.bin>) |
| 123 | GLU_MOVIE_INCENTIVES_POPUP | 0x03000522 | 2281 | 19 | [pack0_core_xga_0206_0x2aa42f.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0206_0x2aa42f.bin>) |
| 124 | GLU_MOVIE_BRO_OP_METER_BLUE | 0x03000523 | 218 | 3 | [pack0_core_xga_0207_0x2aa606.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0207_0x2aa606.bin>) |
| 125 | GLU_MOVIE_BRO_OPS_OVERLAY_BOX | 0x03000524 | 415 | 8 | [pack0_core_xga_0208_0x2aa662.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0208_0x2aa662.bin>) |
| 126 | GLU_MOVIE_BRO_OPS_OVERLAY_SCROLL | 0x03000525 | 631 | 7 | [pack0_core_xga_0209_0x2aa6f9.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0209_0x2aa6f9.bin>) |
| 127 | GLU_MOVIE_BRO_OPS_OVERLAY_INTERSITIAL | 0x03000526 | 276 | 5 | [pack0_core_xga_0210_0x2aa797.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0210_0x2aa797.bin>) |
| 128 |  | 0x03000527 | 11428 | 27 | [pack0_core_xga_0211_0x2aa814.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0211_0x2aa814.bin>) |
| 129 |  | 0x03000528 | 560 | 8 | [pack0_core_xga_0212_0x2aadbe.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0212_0x2aadbe.bin>) |
| 130 | GLU_MOVIE_GRENADE_BUTTON | 0x03000529 | 592 | 6 | [pack0_core_xga_0213_0x2aae63.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0213_0x2aae63.bin>) |
| 131 | GLU_MOVIE_POWERUP_MENU_NEW | 0x0300052A | 2115 | 16 | [pack0_core_xga_0214_0x2aaf2b.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0214_0x2aaf2b.bin>) |
| 132 |  | 0x0300052B | 747 | 18 | [pack0_core_xga_0215_0x2ab0d5.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0215_0x2ab0d5.bin>) |
| 133 |  | 0x0300052C | 490 | 5 | [pack0_core_xga_0216_0x2ab1e6.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0216_0x2ab1e6.bin>) |
| 134 | GLU_MOVIE_POWERUP_MENU_NEW_COPY | 0x0300052D | 286 | 7 | [pack0_core_xga_0217_0x2ab29a.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0217_0x2ab29a.bin>) |
| 135 | GLU_MOVIE_POWERUP_BUTTON | 0x0300052E | 592 | 6 | [pack0_core_xga_0218_0x2ab307.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0218_0x2ab307.bin>) |
| 136 |  | 0x0300052F | 472 | 6 | [pack0_core_xga_0219_0x2ab3cd.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0219_0x2ab3cd.bin>) |
| 137 | GLU_MOVIE_TUT_ARROWS | 0x03000530 | 1168 | 8 | [pack0_core_xga_0220_0x2ab497.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0220_0x2ab497.bin>) |
| 138 | GLU_MOVIE_UPGRADE_POPUP | 0x03000531 | 5094 | 32 | [pack0_core_xga_0221_0x2ab5c0.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0221_0x2ab5c0.bin>) |
| 139 | GLU_MOVIE_WEAPON_UPGRADE_MASTERY | 0x03000532 | 622 | 12 | [pack0_core_xga_0222_0x2ab8ee.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0222_0x2ab8ee.bin>) |
| 140 | GLU_MOVIE_MULTIPLAYER_AND_VERSUS_MAP | 0x03000533 | 1412 | 10 | [pack0_core_xga_0223_0x2ab9b0.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0223_0x2ab9b0.bin>) |
| 141 | GLU_MOVIE_DEATHMATCH_GUN_CARD | 0x03000534 | 206 | 5 | [pack0_core_xga_0224_0x2abaf4.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0224_0x2abaf4.bin>) |
| 142 | GLU_MOVIE_GUN_LAYOUT | 0x03000535 | 779 | 7 | [pack0_core_xga_0225_0x2abb5a.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0225_0x2abb5a.bin>) |
| 143 | GLU_MOVIE_DEATHMATCH_GUN_SLOTS | 0x03000536 | 1665 | 13 | [pack0_core_xga_0226_0x2abc29.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0226_0x2abc29.bin>) |
| 144 | GLU_MOVIE_DEATHMATCH_ONLY_POWERUPS | 0x03000537 | 398 | 10 | [pack0_core_xga_0227_0x2abd60.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0227_0x2abd60.bin>) |
| 145 |  | 0x03000538 | 168 | 2 | [pack0_core_xga_0228_0x2abdd0.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0228_0x2abdd0.bin>) |
| 146 | GLU_MOVIE_LOTTERY_POPUP | 0x03000539 | 1988 | 20 | [pack0_core_xga_0229_0x2abe22.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0229_0x2abe22.bin>) |
| 147 | GLU_MOVIE_LOTTERY_POPUP_2 | 0x0300053A | 1531 | 15 | [pack0_core_xga_0230_0x2abfd7.bin](<../big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0230_0x2abfd7.bin>) |

### pack1_xga

| Movie | 原名 | handle | 字节 | 对象 | 文件 |
|---:|---|---|---:|---:|---|
| 0 |  | 0x03000407 | 255 | 3 | [pack1_xga_0311_0xdff3bb.bin](<../big_360_out/pack1_xga/0xf4e02223/pack1_xga_0311_0xdff3bb.bin>) |
| 1 |  | 0x03000408 | 514 | 3 | [pack1_xga_0312_0xdff422.bin](<../big_360_out/pack1_xga/0xf4e02223/pack1_xga_0312_0xdff422.bin>) |
| 2 |  | 0x03000409 | 1675 | 5 | [pack1_xga_0313_0xdff4e8.bin](<../big_360_out/pack1_xga/0xf4e02223/pack1_xga_0313_0xdff4e8.bin>) |

### pack2_xga

| Movie | 原名 | handle | 字节 | 对象 | 文件 |
|---:|---|---|---:|---:|---|
| 0 |  | 0x0300025B | 55 | 2 | [pack2_xga_0178_0x224d8d.bin](<../big_360_out/pack2_xga/0xf4e02223/pack2_xga_0178_0x224d8d.bin>) |
| 1 |  | 0x0300025C | 10 | 0 | [pack2_xga_0179_0x224dc7.bin](<../big_360_out/pack2_xga/0xf4e02223/pack2_xga_0179_0x224dc7.bin>) |
| 2 |  | 0x0300025D | 112 | 2 | [pack2_xga_0180_0x224de3.bin](<../big_360_out/pack2_xga/0xf4e02223/pack2_xga_0180_0x224de3.bin>) |
| 3 |  | 0x0300025E | 988 | 2 | [pack2_xga_0181_0x224e24.bin](<../big_360_out/pack2_xga/0xf4e02223/pack2_xga_0181_0x224e24.bin>) |
| 4 |  | 0x0300025F | 178 | 1 | [pack2_xga_0182_0x224eea.bin](<../big_360_out/pack2_xga/0xf4e02223/pack2_xga_0182_0x224eea.bin>) |

### pack3_xga

| Movie | 原名 | handle | 字节 | 对象 | 文件 |
|---:|---|---|---:|---:|---|
| 0 |  | 0x0300056B | 255 | 3 | [pack3_xga_0337_0x20c198.bin](<../big_360_out/pack3_xga/0xf4e02223/pack3_xga_0337_0x20c198.bin>) |
| 1 |  | 0x0300056C | 10 | 0 | [pack3_xga_0338_0x20c1ff.bin](<../big_360_out/pack3_xga/0xf4e02223/pack3_xga_0338_0x20c1ff.bin>) |
| 2 |  | 0x0300056D | 10 | 0 | [pack3_xga_0339_0x20c21b.bin](<../big_360_out/pack3_xga/0xf4e02223/pack3_xga_0339_0x20c21b.bin>) |

### pack4_xga

| Movie | 原名 | handle | 字节 | 对象 | 文件 |
|---:|---|---|---:|---:|---|
| 0 |  | 0x0300091F | 255 | 3 | [pack4_xga_1145_0x2e3c243.bin](<../big_360_out/pack4_xga/0xf4e02223/pack4_xga_1145_0x2e3c243.bin>) |
| 1 |  | 0x03000920 | 10 | 0 | [pack4_xga_1146_0x2e3c2aa.bin](<../big_360_out/pack4_xga/0xf4e02223/pack4_xga_1146_0x2e3c2aa.bin>) |
| 2 |  | 0x03000921 | 10 | 0 | [pack4_xga_1147_0x2e3c2c6.bin](<../big_360_out/pack4_xga/0xf4e02223/pack4_xga_1147_0x2e3c2c6.bin>) |

### pack5_xga

| Movie | 原名 | handle | 字节 | 对象 | 文件 |
|---:|---|---|---:|---:|---|
| 0 |  | 0x030005D8 | 255 | 3 | [pack5_xga_0738_0x14d6990.bin](<../big_360_out/pack5_xga/0xf4e02223/pack5_xga_0738_0x14d6990.bin>) |
| 1 |  | 0x030005D9 | 514 | 3 | [pack5_xga_0739_0x14d69f7.bin](<../big_360_out/pack5_xga/0xf4e02223/pack5_xga_0739_0x14d69f7.bin>) |
| 2 |  | 0x030005DA | 1741 | 5 | [pack5_xga_0740_0x14d6ac3.bin](<../big_360_out/pack5_xga/0xf4e02223/pack5_xga_0740_0x14d6ac3.bin>) |
| 3 |  | 0x030005DB | 5470 | 43 | [pack5_xga_0741_0x14d6c9f.bin](<../big_360_out/pack5_xga/0xf4e02223/pack5_xga_0741_0x14d6c9f.bin>) |
| 4 |  | 0x030005DC | 79 | 1 | [pack5_xga_0742_0x14d7258.bin](<../big_360_out/pack5_xga/0xf4e02223/pack5_xga_0742_0x14d7258.bin>) |
| 5 |  | 0x030005DD | 622 | 5 | [pack5_xga_0743_0x14d7293.bin](<../big_360_out/pack5_xga/0xf4e02223/pack5_xga_0743_0x14d7293.bin>) |

### pack6_xga

| Movie | 原名 | handle | 字节 | 对象 | 文件 |
|---:|---|---|---:|---:|---|
| 0 |  | 0x03000165 | 255 | 3 | [pack6_xga_0092_0x6297d1.bin](<../big_360_out/pack6_xga/0xf4e02223/pack6_xga_0092_0x6297d1.bin>) |

### pack7_xga

| Movie | 原名 | handle | 字节 | 对象 | 文件 |
|---:|---|---|---:|---:|---|
| 0 |  | 0x0300031E | 10 | 0 | [pack7_xga_0176_0x4cd07b.bin](<../big_360_out/pack7_xga/0xf4e02223/pack7_xga_0176_0x4cd07b.bin>) |
| 1 |  | 0x0300031F | 10 | 0 | [pack7_xga_0177_0x4cd097.bin](<../big_360_out/pack7_xga/0xf4e02223/pack7_xga_0177_0x4cd097.bin>) |
| 2 |  | 0x03000320 | 10 | 0 | [pack7_xga_0178_0x4cd0b3.bin](<../big_360_out/pack7_xga/0xf4e02223/pack7_xga_0178_0x4cd0b3.bin>) |
| 3 |  | 0x03000321 | 10 | 0 | [pack7_xga_0179_0x4cd0cf.bin](<../big_360_out/pack7_xga/0xf4e02223/pack7_xga_0179_0x4cd0cf.bin>) |

### pack8_xga

| Movie | 原名 | handle | 字节 | 对象 | 文件 |
|---:|---|---|---:|---:|---|
| 0 |  | 0x03000179 | 255 | 3 | [pack8_xga_0109_0xab4fec.bin](<../big_360_out/pack8_xga/0xf4e02223/pack8_xga_0109_0xab4fec.bin>) |

### pack9_xga

| Movie | 原名 | handle | 字节 | 对象 | 文件 |
|---:|---|---|---:|---:|---|
| 0 |  | 0x030002F0 | 255 | 3 | [pack9_xga_0147_0x39b08c.bin](<../big_360_out/pack9_xga/0xf4e02223/pack9_xga_0147_0x39b08c.bin>) |

## 本次核对边界

执行 `D:/Python312/python.exe src/tools/catalog_ui_movies.py`，退出码 0。175 个资源均完整消费至文件末尾，原名映射均指向结构可读的 Movie。该检查核对文件定位与序列化长度，不代表 010 Editor 模板已实际执行。

`.bt` 的类型名称、字段排列依据原反编译，完整出处在模板头部。逐字段用途及消费路径已补入模板，研究边界见 [字段用途核对](entry-field-semantics.md)。未确认字段保留 Raw 后缀和说明。010 Editor 的格式属性参考 [官方 Template Variables 文档](https://www.sweetscape.com/010editor/manual/TemplateVariables.htm)。
