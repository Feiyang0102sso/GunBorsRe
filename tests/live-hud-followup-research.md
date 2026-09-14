# Live HUD 补充证据（2026-09-14）

加载部分更正：本记录把 `MENU_BOOT_LOAD` 描述符误用于进关，原因是遗漏菜单指针表的 `+4`。进关应使用 `MENU_GAME_LOAD`、Movie24 和 KEYSET 壁纸，详见 `live-loading-correction-research.md`。下文加载主图结论仅保留为错误调查记录，不作为当前实现依据；其他 HUD 证据不受影响。

范围：屏外队友头像、救援粒子、波间输入和倒计时、加载图。用户截图仅作为查找线索；以下事实来自 iOS 3.6.0 程序及对应 BIG 样本。本记录不改实现。

## 屏外队友头像

- `CRemotePlayer::Update`，反编译 `:229709–229801`，VA `0x166570`：每次更新先取角色 `GetBounds` 和 `CCamera::GetBounds`，两个矩形完全不相交才创建追踪标。回到相交范围时移除。不是按离玩家的固定半径判定。
- 角色活着时，`broIndex==0` 使用 indicator 类型 4，另一个使用 5；Live 倒地使用类型 6。GameType 3（Death Match）即使倒地仍走 4/5。
- `CLevelIndicator::Init :191638` 用 core SpriteGlu **archetype 1**。原静态表 `INDICATOR_ANIMS` 位于 VA `0x3C4AB0`，`:18762`：类型 4 是 `{64,64,65}`，类型 5 是 `{64,64,66}`，类型 6 是 `{69,69,70}`。前两项为进入/循环箭头，第三项是头像或倒地图标。
- `GetOrientation :191315` 将对象原包围盒中心投影到屏幕，再分别钳制 X/Y；左右及上边距为 `trunc(25*cameraScale)`，下边距为 `trunc(100*cameraScale)`。箭头朝投影点与钳制点的差向量，向上是 0 度。
- `Draw :191450` 先旋转绘箭头，再反向旋转绘头像，头像保持正立。`FadeOut/Update :191302/:191533` 的计时值 1000 每毫秒减少 5，实际淡出 **200ms**，alpha 为 `(1-cos(pi*remaining/1000))/2`。
- 对应原型样本：`_prep/big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0406_0x1b2bebc.bin`，5271 字节。格式查 `sprite_archetype.bt` 和 `sprite_global.bt`，不得直接把动画索引当 PNG 索引。

现有集成位置：`SurvivalHud.cpp:213` 已有上述 7 类通用绘制，`CLevelIndicator.h` 已有淡出；当前主要缺 `CRemotePlayer` 对等的角色屏外状态和绑定。可以由多人场景维护，再交给已有 HUD，不放进 bot AI。

## 救援中的两类效果与起身效果

`CPlayer::Update :100377–100479`，VA `0x772C0`，维护与救援距离有关的持续粒子，这和 `OnRevive(reason)` 是两件事。

1. 本地角色倒地、队友满足可救援条件（`mem+2124`）、非 Death Match：在本地尸体位置选择效果。若是本地存活、远端倒地且救援进度小于 1，则在远端尸体位置选择效果。
2. 两人距离 **严格小于 125** 选择 PLAYER 脚本资源槽 3，否则槽 2。
3. 槽变化时先 `CParticleEffectPlayer::Stop` 旧效果，再 `CParticleSystem::AddEffect` 创建新效果，并把 player 对象第一个 flag 设为 1。没有倒地者/无需救援时停止当前效果。救援进度另外由 `SetRevivePercent` 推进。

PLAYER 原样本 `16_PLAYER/pack0_core_xga_0037_0x6726.bin` 的资源表已再次核对：

| 用途 | PLAYER 资源槽与文件偏移 | core PARTICLEEFFECT ordinal | 物理文件 | 字节 |
|---|---|---|---|---|
| 起身瞬间效果 | 1，表项 `0x1D`，ordinal 在 `0x22` | 8 | `0022_0x5c82.bin` | 960 |
| 等待救援，距离至少 125 | 2，表项 `0x26`，ordinal 在 `0x2B` | 11 | `0025_0x5ef2.bin` | 395 |
| 正在救援，距离小于 125 | 3，表项 `0x2F`，ordinal 在 `0x34` | 12 | `0026_0x5f5f.bin` | 730 |

粒子文件目录为 `_prep/big_360_out/pack0_core_xga/0xf4e02223/12_PARTICLEEFFECT/`，表中物理文件均带 `pack0_core_xga_` 前缀。三个文件开头 spritePackHash 均为 `1482249506`（core）；直接按 `particle_effect.bt` 读取首字段：ordinal 8 有 6 emitters，首 emitter archetype 21 / animationMask 1；ordinal 11 有 3 emitters，首 emitter archetype 28 / mask 1；ordinal 12 有 5 emitters，首 emitter archetype 28 / mask 4。archetype 28 样本 `0433_0x1b2dadd.bin`，243 字节。

**不在代码中指定“绿色/红色”。** 两效果用不同原动画并叠加不同 emitters，直接播放 BIG 才能忠实显示；本次未独立渲染确认每个贴图颜色。用户猜测的红色复活动画很可能对应等待救援效果，仍不能将这个颜色关联写成已目视核验事实。

`CBrother::OnRevive :135970` 调 PLAYER export 7：reason 非零进 state 15，零进 state 16；两者都 `native_0x060B(1)`，即上表起身效果。`CBrother::FunctionResolver case11 :138963` 通过脚本资源表查 PARTICLEEFFECT，添加后 `SetAnchor` 到实际角色。state 15 另有冲击伤害及 LEVEL 时间缩放，不能以红/绿区分两个 reason。Flow 原始偏移、完整状态见 `flow-disassembly/pack0_core_xga_0037_0x6726.flow.txt`。

## 每波结算：双方仍能移动，暂停下一波脚本

- `CLevel::OnWaveCleared :116994` 在 Live、MissionType 1、非最后一波时设共享剩余时间 15000ms。
- `CLevel::Update :121325–121348` 在每一帧继续更新全部 `ILevelObject`；`:121363–121394` 只在计时大于零时跳过 LEVEL 脚本并扣减计时，没有停止角色。
- `CPlayer::Update :100377` 继续 `UpdateMovement/UpdateShooting`；`UpdateMovement :101384` 只看摇杆活动和角色可移动变量，没有波间计时条件；`CInputPad::GetStick :86330` 也只是取对应摇杆。
- `CInputPad::OnWaveClear :89764`、`SetUpOverlay :87598`、`SetUpCommonInterstitialOverlays :87675` 只是加入 Movie 队列，不切 Base 状态、不重置摇杆；`Base::Update :88368` 的 state 3 仍 `UpdateInput`。
- `CPlayer::OnWaveCleared :100886` 最后调用 `CBrother::OnWaveCleared`，PLAYER export 6（原样本 `0x5DF`）为空，不关闭移动变量。
- 商店/道具按钮对波间计时有单独限制（`:88602/:88679`），不能把它扩大成移动也被屏蔽。

因此应让双方都可移动；只把下一波 LEVEL 推进扣住。单方面清空本地输入而持续运行 bot 不对应原版。商店暂停则属于另外的多人状态，不应沿用这个规则。

## 缺失倒计时的直接原因

`CInputPad::OverlayWaveStart :86975–87080`，VA `0x5C66C`：

- 剩余时间 `<=1000` 时反向播放并收起结算 Movie。
- 仅在 `2000<=remaining<=6999` 时显示倒计时，数字 `trunc((remaining-1000)/1000)`，即 **5、4、3、2、1**。此公式已用原 ARM `0x5C6EC` 的减 1000 和 `0x5C7B4–0x5C7D0` 的除 1000 核对，反编译遗漏了格式化参数。
- font 是 core **1**。先取前缀文字宽度，在 region 4 水平居中绘前缀，再把数字独立绘在前缀末端；原版不是把整个“文字+数字”一起居中。
- 原 Strings CSV id 591，偏移 `0xB1CD`，长度 13，内容为 `Next wave in `，**没有 `%i`/`%d`**。当前 `OriginalNoticeNumber` 仅替换占位符，因此必然漏数字；应保留前缀并另绘数字。
- `CInputPad::Bind :90887–90898` 根据 mission type 2 使用 `IDS_MULTIPLAYER_WRAPUP_WAVE_START_SURVIVAL`，type 1 使用 `...ENDLESS`。

只读 ARM 输出保存在 `obj/live-overlay-wave-arm.txt`；研究命令：`D:/Python312/python.exe _prep/tools/disassemble_original.py OverlayWaveStart --max-bytes 1400`，反汇编本身成功。项目无 `.venv`，使用现有研究 Python 和既有 capstone。

## 加载图：原版明确等比 cover

`CMenuSplash::Load :160500–160562` 从 KEYSET 取图交给 AddImage。`BackgroundCallback :160872–160935` 同时读取图片宽高，计算 `max(region.width/image.width, region.height/image.height)`，X/Y 使用**同一个比例**；纵向贴 region 顶部，alignment 1 横向居中，否则 X=0。它是等比填满并裁切，不是拉伸，也不是完整 contain。

三张多人图均来自 BIG，原 PNG IHDR 为 **960×510**：

| KEYSET 序号 | Handle | 物理 PNG |
|---|---|---|
| 22 | `0x02000568` | `pack0_core_xga_0276_0xb522fa.png` |
| 25 | `0x0200056B` | `pack0_core_xga_0279_0xc6a679.png` |
| 26 | `0x0200056C` | `pack0_core_xga_0280_0xcdcda9.png` |

目录 `_prep/big_360_out/pack0_core_xga/0xb7178678/`。序号从实际 loading 日志与原资源 CSV/standard-catalog 交叉核对。Movie 为 `GLU_MOVIE_SPLASH_INTRO_MP`，ordinal 81。

### 后续查明：多人背景绑定错用了单人描述符规则

继续读取原 Mach-O，定位 `GLU_MOVIE_SPLASH_INTRO_MP` 字符串 VA `0x391583` 的数据引用，得到多人菜单描述符 **VA `0x4031B0`**，13 个 u32 原值：

`(1, 3741059, 3741085, 0, 3738366, 174, 1, 7, 0, 8, 0, 1, 2)`。

字符串字段解析为：`+4=GLU_MOVIE_SPLASH_INTRO_MP`，`+8=IDS_LOADING`，**`+16=IDB_SPLASH_MAIN_MP`**；`+20=provider174`，**`+24=alignment1`（居中）**。相比之下，已生成的单人 `OriginalSplashData.inc` 来自 VA `0x403230`，`+16=0`、`+24=0`。现有实现给多人换 Movie，却继续套用单人取图规则，正是错误所在。

`CMenuSplash::Load :160522` 优先取 descriptor `+16` 的图名；只有该字段为零才读取 KEYSET。故上述三张 960×510 图不能用作这个 Multiplayer Intro 的背景。

真正主图：`IDB_SPLASH_MAIN_MP`，logical ID **1463 / 0x5B7**，路径 `_prep/big_360_out/pack0_core_xga/0xb7178678/pack0_core_xga_0334_0xe1e117.png`，854018 字节，IHDR **960×640**。已目视：两兄弟开火，中央金色 `MULTIPLAYER / GUN BROS / DEATHMATCH` Logo。主图中的 DEATHMATCH 字样是该原画的一部分，不应人工擦除。

Movie81 region0 原始字段在 offset10对象中，idle1499ms时沿用1389关键帧：`x=0,y=0,width=960,height=640,parent=254,parentAnchor=0,selfAnchor=0,farAnchor=8,farX=254,farY=254,alpha=1`。它通过两边锚定覆盖整个屏幕，包含 header 后方。原 Movie 在 `CMenuSplash::Init :160418–160429` 定位于屏幕中心，背景是单独等比 cover 并横向居中；在 1024×768 可得到 1152×768、横向裁 64 像素，与原规则一致。不能以 header 130 像素为由手工减去背景高度。

`Bind :160600` 确实还调用 `CreateContentSprite(provider174,0,0)`，但本 Movie 只有3个user regions，第四 region overlay 并不存在，不能新增一个凭空坐标区。原表 provider174名为 `MDS_SPLASH_OVERLAY`（VA `0x423C10`），这不改变主图优先取显式 alias 的事实。

此补证修正了此前“多人只需切 Movie、背景继续读 KEYSET”的旧假设。应在加载适配中保留独立多人描述符字段，不能改成 contain 来掩盖资源选错。

已查格式模板：`ui_movie.bt`、`sprite_archetype.bt`、`entries/common.bt`、`entries/particle_effect.bt`、`flow_bytecode.bt`。所有 `:行号` 均指 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`；文件内偏移和运行时成员偏移明确分开。
