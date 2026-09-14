# Live 进关壁纸纠正依据

日期：2026-09-14。只读核对 iOS 3.6.0 原程序、BIG 对应解包字节及 BT；未修改原件。这里纠正 `live-hud-followup-research.md` 中把启动菜单描述符用于关卡加载的结论。

## 结论与实现边界

Live 和 Deathmatch 的**进关**加载使用 `MENU_GAME_LOAD`、`GLU_MOVIE_SPLASH`（core Movie 24），背景来自 `KEYSET_SPLASH_IMAGES`。`GLU_MOVIE_SPLASH_INTRO_MP`（Movie 81）和 `IDB_SPLASH_MAIN_MP` 是 **MENU_BOOT_LOAD 启动菜单**。此前查询了正确的启动描述符，却用错了场景，同时把 Movie 81 的全屏背景区域套在本应位于 header 下方的 960×510 壁纸上。

用户此次要求 Live、Deathmatch 各用自己的壁纸，应据此明确分池：Live 的原资源候选为末尾五项中非 Deathmatch 的前四项（22–25），Deathmatch 是末项 26。**这项分池是当前用户要求；不能宣称完全等同于此份 iOS 程序的轮转算法。** 原版机器码确实有 Live 部分轮转落到 Deathmatch 图的分支，下文保存证据。所有图片仍从原 KEYSET 读出，不复制到新资源表。

## 菜单 ID 到真实描述符

原 Mach-O 的 ARMv7 符号表直接记录：

| 符号 | VA | Movie 字段 +4 | 图名字段 +16 | alignment +24 |
|---|---:|---|---|---:|
| `__ZL14MENU_BOOT_LOAD` | `0x4031B0` | `GLU_MOVIE_SPLASH_INTRO_MP` | `IDB_SPLASH_MAIN_MP` | 1 |
| `__ZL14MENU_GAME_LOAD` | `0x4031F0` | `GLU_MOVIE_SPLASH` | 0 | 0 |
| `__ZL15MENU_SHELL_LOAD` | `0x403230` | `GLU_MOVIE_SPLASH` | 0 | 0 |

`MENU_GAME_LOAD` 的 13 个原始 u32 是 `(1,3741097,3741085,0,0,0,0,29,0,31,65536,1,2)`。`+8=IDS_LOADING`；`+28=29` 为载入动作，`+36=31` 为后续动作，`+44` 的退出动画标记为 1。

原代码 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`：

- `CMenuAction::DoAction :94110–94130`：actions 24、25、26 最终 `SetMenu(...,18,index,8)`。
- `CMenuSystem::SetMenu :96700–96732` 取的是 `*(this + 4*menuId + 4)`，不是 `this[menuId]`。
- 构造函数 `:97639–97641` 把 `this[18]=MENU_BOOT_LOAD`、`this[19]=MENU_GAME_LOAD`、`this[20]=MENU_SHELL_LOAD`。因此菜单 ID 18 对应 `this[19]`，即进关描述符 `0x4031F0`。遗漏表起始的 `+4` 正是前次错误来源。
- `CMenuSplash::Init :160386–160450` 从描述符加载 Movie 并绑定区域；`Load :160522–160561` 仅在 `+16` 非零时覆盖 KEYSET 图片。进关描述符该字段为零。

## 原壁纸集合与真实轮转

BT：`_prep/_Big_tool/binary template/big_assets/big_keyset.bt`；原 `CKeysetResource::Load :359730` 读 u16 数量及 u32 句柄。

`KEYSET_SPLASH_IMAGES` 样本为 `_prep/big_360_out/pack0_core_xga/0x69e5d35c/pack0_core_xga_0284_0xd1541b.bin`，110 字节；u16 count=27，逐项位于 `2+index*4`。五张 PNG 的 IHDR 均为 **960×510**，已逐张目视，不根据文件名猜测内容。

| 索引 | handle | `0xb7178678/` 下物理文件（前缀均为 `pack0_core_xga_`） | 目视内容 |
|---:|---|---|---|
| 22 | `0x02000568` | `0276_0xb522fa.png` | 美女手持 LIVE MULTIPLAYER 标志，蓝色底 |
| 23 | `0x02000569` | `0277_0xbbe0f3.png` | 两个持枪角色，蓝色 GUN BROS 底 |
| 24 | `0x0200056A` | `0278_0xc1b5ef.png` | 两位默认兄弟及社交图标，蓝色底 |
| 25 | `0x0200056B` | `0279_0xc6a679.png` | LIVE 标志与红橙等待、绿色救援图示 |
| 26 | `0x0200056C` | `0280_0xcdcda9.png` | 红色 VS DEATHMATCH 标志 |

这些条目通过 KEYSET 按序消费；在主程序字符串及原名字表中没有查得各自的独立 `IDB_SPLASH_*` 名字。唯一命中的 `IDB_SPLASH_MAIN_MP=0x020005B7` 是另一个 960×640 启动宣传图。

`CMenuSystem::Init :97467–97481` 设置 `start=count−5=22`。`GetMultiplayerSplashScreenIndex :96148–96154` **返回旧索引**，随后把存储值更新为 `(old+1)%3`。正常初始化由 `COptionsMgr+42` 读入（`:97462`），并非保留 CMenuSystem 构造时的 −1。`COptionsMgr::COptionsMgr :51472` 清零 mem+12 至 mem+47，因此无既存配置时是 0；`:97296` 保存轮转值用于下次启动。

按原 action 分支（`:94119–94130`）：GameType 3 固定 `start+4=26`；其他 GameType 在 cycle=0 取 `start+action−24`，cycle 非零取 `start+cycle+2`。所以 action24 在正常周期 0/1/2 的序列确实为 **22、25、26**；action25 为23、25、26；action26 为24、25、26。

额外回查 ARMv7 原始指令，排除了反编译加一、返回值或起点误读：

| VA | 原指令字 | 含义 |
|---|---|---|
| `0x702A4` | `E19020F1` | LDRSH r2,[r0,r1]，读旧值，r1=0xDF4 |
| `0x702AC` | `E2823001` | ADD r3,r2,#1 |
| `0x702C0` | `E18030B1` | STRH r3,[r0,r1]，存模 3 后新值 |
| `0x702C4` | `E1A00002` | MOV r0,r2，返回旧值 |
| `0x69D14` | `E1140001` | TST r4,r1，r1=0xFFFF，检测 cycle 非零 |
| `0x69D18` | `1A000003` | BNE 到非零分支 `0x69D2C` |
| `0x69D2C` | `E0844000` | ADD r4,r4,r0，其中 r0=start |
| `0x69D30` | `E2844002` | ADD r4,r4,#2 |

当前用户的模式分池要求优先；记录以上为原 iOS 分支与目标行为的明确差异，不推断为别的版本，也不掩盖为“原版就是四张 Live 轮转”。

## 正确 Movie 区域及比例

BT：`_prep/_Big_tool/binary template/big_assets/ui_movie.bt`。读取器 `CMovie::InitResource :109263`、`CMovieRegion::Init :109751`，区域消费者 `CMovieEmptyRegion::GetMetricsAtTime :182335`。

`GLU_MOVIE_SPLASH` 为 core Movie 24，handle `0x030004BF`，样本 `_prep/big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0107_0x2a41a4.bin`。437 字节完整消费到 EOF，头部 `(width=960,height=640,duration=1000,objects=4)`。

- region0 的对象头在 offset10，第一帧 offset13：`time=0,x=0,y=132,width=960,height=640,alpha=1,scaleX/Y=1,parent=254,selfAnchor=0,parentAnchor=0,farAnchor=8,farX=254,farY=254`。时间700ms同几何，999ms alpha=0。远端锚定使背景从 **header 下方 y=132 到屏幕底**，不是字面高度640，也不是 Movie81 的整个屏幕区域。
- region1 是 loading 文字，对象offset124；region2 是说明文字，offset275。沿用资源和已有回调绑定，不手写新文字区域。
- chapter轨offset426，两个章节点300ms、700ms；引擎补隐式章节0，正常idle是chapter1。不能继续沿用 Movie81 的1499/1793ms。
- `CMenuSplash::BackgroundCallback :160872–160935` 计算 `max(region.width/image.width,region.height/image.height)`，X/Y共用比例，即等比 cover。alignment0 原生横坐标为0，Y为region.y；alignment1才水平居中。本资源 region.x 本来为0，因此现适配使用 region.x 与原值等效。

验收应查实际绑定 Movie24、KEYSET 图和 y=132 的资源区域，目视确认 Live 不再显示金色 GUN BROS 启动宣传图，图中圆 logo 保持圆形。不要用 contain、硬编码减 header 高度或强制改 PNG 尺寸代替上述修正。

## 核对方法

使用仓库既有 `extract_menu_statics.read_image` 只读解析 Mach-O 段与 ARMv7 符号表；用 Python `struct` 读取描述符、ARM 指令、KEYSET、Movie 完整帧与 PNG IHDR；用 `view_image` 目视五个原 PNG。项目无 `.venv`，本机 `D:/Python312/python.exe` 执行上述只读核对，成功退出码0。未运行010 Editor，BT仅作为已对照原读取器的字段说明。
