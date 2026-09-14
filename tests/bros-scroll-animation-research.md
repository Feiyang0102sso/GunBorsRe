# BROS 列表滚动、选中动画及默认兄弟核对（2026-09-14）

本次仅研究，不改游戏实现。证据为 iOS 3.6.0 ARMv7 原程序、BIG 解包样本与对应 BT；当前重建代码仅作故障定位线索。

## 原 BROS 使用三项滚动 Movie

直接只读解析 `_prep/gunbros` 的 ARMv7 slice（文件基址 `0x78F000`）Mach-O 符号表，`__ZL12MENU_FRIENDS` 位于 VA **`0x402D70`**。按原菜单消费者读出：

| DWORD 下标 | 值 | 用途 |
|---|---|---|
| 1 | `GLU_MOVIE_BROBUFF_MENU` | 菜单主 Movie |
| 10 | `GLU_MOVIE_BROTHER_MENU_SCROLL_3_OPTION` | 好友名单滚动 Movie |
| 11 | 33 | 好友数据 provider |
| 12 / 13 / 14 | 0 / 0 / 2 | 控件模式、首尾边界参数 |
| 17 | `GLU_MOVIE_BROTHER_BOX` | 好友卡 |
| 27 | `GLU_MOVIE_BROTHER_MENU_SCROLL_3_OPTION` | BRO BOOST 滚动 Movie |
| 30 / 31 | 0 / 2 | BRO BOOST 首尾边界参数 |
| 39 | `GLU_MOVIE_BROTHER_MENU_SCROLL_3_OPTION` | 另一好友页滚动 Movie |
| 42 / 43 | 0 / 2 | 同样的首尾边界参数 |

消费链：`CMenuFriends::Init :197410`、`BindFriendList :195969`；后者将 descriptor DWORD 12、16 传给 `CMenuMovieControl::Init`，DWORD 13、14 传给 `SetBoundsOptions`。`CMenuMovieControl::SetBoundsOptions :140797` 的上界是 `max(0, count - tailOptions - 1)`，因此这里是 **`max(0, count - 3)`**。下界是上界与首边界参数 0 的较小者，即 0。

当前 `OriginalSocialContent.cpp` 在 BROS 分支把名单切成四项版、`visible=4`。当默认兄弟加三个 local bro 合计四条时，上界被算成零，最后一条落到页面下面却无法滚动。修复应恢复原 descriptor 选定的三项版；不能按 Movie 总 callback 数等同可见项数。

## Movie 布局、过渡槽与进场动画

BT：[`ui_movie.bt`](../_prep/_Big_tool/binary%20template/big_assets/ui_movie.bt)，原读取器 `CMovie::InitResource :109263`、`CMovieChapter::Init :109532`、`CMovieRegion::Init :109751`。章节轨文件只存后续章节起点，引擎补 chapter 0 起点 0。

资源样本及解析记录见 `_prep/out/ui-movie-catalog.json`：

| 原资源 | Movie ordinal / handle | 原样本 |
|---|---|---|
| `GLU_MOVIE_BROTHER_MENU_SCROLL_3_OPTION` | 109 / `0x03000514` | `pack0_core_xga/0xf4e02223/pack0_core_xga_0192_0x2a90fa.bin` |
| 四项版 `GLU_MOVIE_BROTHER_MENU_SCROLL` | 56 / `0x030004DF` | `pack0_core_xga/0xf4e02223/pack0_core_xga_0139_0x2a641a.bin` |
| `GLU_MOVIE_BROTHER_BOX` | 57 / `0x030004E0` | `pack0_core_xga/0xf4e02223/pack0_core_xga_0140_0x2a64fb.bin` |

上表样本均位于 `_prep/big_360_out/`，只读。Movie 109 为 960×640、1500 ms、628 字节；章节起点 `[0,400,700]`。chapter 0 是从屏幕下沿逐张进入的进场段；三个卡片分别于 252、289、325 ms 到达静止位置。chapter 1→2 是滚动一条的 authored 过渡，300 ms；不是任意匀速平移模板。

- region 0 是交互范围：`(-8,-22,600,242)`，callback tag 1。
- region 1、2、3 在 400 ms 的 y 为 4、78、152，卡片均 572×66，条目间距 74。
- region 4 是下方补位槽，400 ms 仍锚定屏幕下沿，700 ms 到达 y=152。它不是第四个静止可见条目。
- Movie 56 同样有额外补位槽，不能因为有五个 tag≥2 的 callback 就算五个可见条目。

`CMenuMovieControl::Init :142239` 先用 chapter 1 获取交互区域，再用 chapter 2 绑定所有 tag≥2 的 option callback。`CMenuFriends::FriendListCallback :195774` 绘制前按列表顶部和屏幕剩余高度裁切，结束后还原 clip，滚动条在列表右侧。恢复布局时应复用 Movie 和原 viewport，不手写新卡坐标。

## 选中高光不是鼠标悬停

原调用链：`CMenuFriends::Refresh` 的 action 100（`:196868`）先 `CFriendDataManager::SetActiveFriend`，随后对旧的选中项 `UnFocus`、新选中项 `Focus`（`:196880–196897`）。因此这里的 Focus 表示当前选中的兄弟，不是 Windows 鼠标 hover。

| 原方法 | 章节与方向 |
|---|---|
| `CMenuFriendOption::Show :198376` | chapter 0，loop chapter 0，关闭循环播放标志 |
| `Focus :198360` | 清 reverse；跳 chapter 1；loop chapter 1；开启循环播放标志 |
| `UnFocus :198345` | loop chapter 1；reverse=1；关闭循环播放标志，反放退出高光 |
| `Update :198390` | 每帧推进卡片 Movie；另推进好友使用奖励子 Movie |
| `Select :197811` | 返回 0，没有独立点击动画 |

`CMenuFriends::OnShow :196212`、`OnFocus :196272` 会重新 Focus 当前活动好友；`OnUnFocus :196135`、`OnExit :196171` 对其 UnFocus。恢复进场和选中动画需要每条卡保留播放时间/方向，不能让所有卡永远显示 chapter 0 末帧。

Movie 57 为 2000 ms；文件 offset `0x1AB` 的章节对象给出 1000 ms，即运行时 `[0,1000]`。offset `0x1B2` 的 sprite 轨是高光，原 archetype 6、mapping 0、animation 5；直接按 BT 读取五帧：

| 帧偏移 | 时间 ms | alpha |
|---|---:|---:|
| `0x1B5` | 1002 | 0 |
| `0x1D6` | 1203 | 1 |
| `0x1F7` | 1503 | 0 |
| `0x218` | 1800 | 1 |
| `0x239` | 2000 | 0 |

这是原资源已有的循环明暗高光；直接播放该轨，不额外画假边框。其他好友使用奖励动画由 `CMenuFriendOption::Bind :198572` 从 provider 加载，取决于原好友奖励状态；当前 fake service 没有真实 XP gift 时不能伪造可领奖状态。

## 默认兄弟名字与好友失效回退

`CFriendDataManager::InitDefaultBrother :200797` 按传入兄弟配置 `brotherIndex` 选择 `IDS_FRIEND_DEFAULT_BRO1`（0）或 `IDS_FRIEND_DEFAULT_BRO2`（非 0），从 BIG 读字符串再写 nickname，同时复制默认兄弟配置。字符串容器解析 `_prep/out/binary-research/container-catalog.json :52515–52527` 给出本地 string ID 511 为 `Percy Gun`、512 为 `Francis Gun`。名字应从这两个原 alias 获取，不能使用通用 `LOCAL BOT` 代替默认人物。

`CFriendDataManager::ValidateActiveFriend :199934` 在当前好友凭据无法在好友列表中找到时，把 active friend 指向默认兄弟并重置凭据。`CProfileManager::HandleFriendListUpdate :202778` 无论更新成功还是失败，都先执行该验证。`FriendsManagerInfoLoad :200740` 也会在现有好友对象失效时回退默认兄弟并清凭据。

**证据边界：** 上述原逻辑证明的是“好友不可用时回退默认”，本次没有证实 iOS 对每一次瞬时网络断开都会删除仍缓存有效的好友。用户已明确要求 fake connection 关闭时切回默认；宿主应在连接开关生效处将运行时好友选择、启动参数和显示对象统一回退，并避免随后从本地配置又恢复不可用好友。这是该宿主适配的明确需求，不应声称已发现原版无条件断网回退函数。
