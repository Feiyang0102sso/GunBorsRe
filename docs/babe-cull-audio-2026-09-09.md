# 队友（babe）随机、弹体出屏剔除与死亡音效（2026-09-09）

用户反馈三点：死亡音效在 pack12 特别吵；救援队友的位置和种类固定（只在一处刷新、固定三叉枪）；该队友的子弹会飞过整张地图，一开枪满地图都是死亡音效。三点相互关联，本轮一并处理。行号均指 `_IDA_OUT/gunbros_3.6.0_IOS.c`。

## 一、babe 本来就是随机的：脚本掷骰，宿主没给种子

pack12 生存 LEVEL（`big_360_out/pack12_xga/0xf4e02223/08_LEVEL/pack12_xga_0011_0x125b.bin`，清单 `out/binary-research/flow-disassembly/pack12_xga_0011_0x125b.flow.txt`）导出 0 在开局就掷一次骰子：

```
@0x314F local[1] = CGame.native_0x0401(0, 99)          // Utility::Random(0, 99)
@0x315A if  (local[1] < 25) spawner.spawn(res 1, layer 10, node 0, object 601)
       elif (local[1] < 50) spawner.spawn(res 2, layer 10, node 2, object 601)
       elif (local[1] < 75) spawner.spawn(res 3, layer 10, node 4, object 601)
       else                 spawner.spawn(res 4, layer 10, node 6, object 601)
@0x31EB CLevel.native_0x0514(23)                        // 播放提示音
```

四个 resource 是脚本资源表里的 pack12 ENEMY 3/0/1/2，各自带自己的武器；四个 node 是路径层 10 的四个点。救援在函数 43：`sendEnemyMessage(601, 5)` → `SetStatBit` → 对话 string5「You Saved a Babe!」。所以**种类与位置本来就是四选一，且一一对应**。

宿主没随机的原因不在这段脚本，而在随机数源：原版 `Utility::Random` 走单例 `CRandGen`，其构造函数用 `CStdUtil_Cocoa::GetTimeSeconds` 播种（:370383），每次开机都不同；本移植给每个脚本宿主一条自己的 LCG，初值固定为常量 1，从不播种，于是每次运行掷出同一串数，永远是同一个 babe、同一个点。

修复：`CLevel::SetScriptRandomSeed`；`SurvivalSession::Restart` 在 `Bind` 前播种（导出 0 开局就掷骰，必须早于 Bind），并让种子随每次重开推进，对应原版那条一直在走的时钟流。正式游玩由 `MapScene::RunSurvival` 用 `steady_clock` 播种；`check` 与截图运行保留固定默认值，研究结果仍可比对。

实测（探针构建把播种也应用到截图运行，跑四次）：

```
[spawner] tracked spawn resource=2 object=01675822:0 layer=10 node=2 id=601
[spawner] tracked spawn resource=4 object=01675822:2 layer=10 node=6 id=601
[spawner] tracked spawn resource=1 object=01675822:3 layer=10 node=0 id=601
[spawner] tracked spawn resource=2 object=01675822:0 layer=10 node=2 id=601
```

四个模板、四个节点都出现了。修复前固定为 resource=2 / node=2。

## 二、子弹飞越全图：原版会在出屏时剔除

`CBullet::Update` :63537 在移动之后立刻判断：

```
if ( CBullet::CanBeCulled(this) && (flags & 0x100) == 0 )   // 非光束
    CBullet::Remove(this, 1);                                // :60862，仅置删除位，不触发命中
```

`CanBeCulled` :60583 取 `CCamera::GetBounds` 的镜头矩形，与弹体自身包围盒**逐轴、且只在其行进方向上**比较：向左飞且已在左边界之外、向右飞且已在右边界之外（Y 同理）才剔除；朝镜头飞或横穿镜头的都保留。模板 flag 0x10 免除剔除，光束（flag 0x100）不走这条路径。

本移植此前只有 3000ms 到期一条退路，弹体因此能穿过整张地图，在屏幕外继续命中并触发死亡音效——正是用户描述的「一开枪整个地图都是死亡音效」。现按上述规则实现于 `WeaponEffects`：`SetViewBounds` 接收镜头矩形，由 `SurvivalSession::UpdateCamera` 用与指示器同源的 `CCamera` 矩形提供；没有镜头矩形的场合（竞技场、各研究探针）不剔除，与之前一致。

## 三、死亡音效：原版没有去重，宿主窗口按用户反馈放宽

再查一次（与 `entries/sound_effect.bt` 中此前的记录一致）：

- `CMoveSetMeshController::Update` :134066 每次跨过声音帧就直接 `CSoundQueue::PlaySound` :102896，无任何判重；
- `CMediaPlayer::PlayInternal` :363259 每次调用都 `CSoundEvent::CreateInstance` 新建一个事件，Cocoa 后端有 32 个 OpenAL 源池。

也就是说原版是**同一 WAV 的多份叠加播放**：一炮打死一批时它们几乎同相叠在一起，听感就是「一声」。单声道播放不会变响，所以在本移植里若把跨帧的重复也放出来，就变成连续重触发的「吵」。

因此保留并放宽宿主适配：**移动帧声音（死亡/动作音）在同一份还在播放期间不再重触发**，窗口取该 WAV 自身解码时长；枪械 cue 仍是原来的同帧合并，连发不受影响。这一条明确标注为宿主音频适配，不是原版行为。

pack12 三波实测（同一份 pilot、同样 42 kills / 229 shots）：放宽前 `sounds=649`，放宽后 `sounds=440`。

## 验证

| 命令 | 结果 |
|---|---|
| `--combat-feedback-check --mute` | 新增 `[feedback-cull] leaving-culled inbound-alive=1 after-far-edge=0 failures=0`；`[audio-health-check] group-death kinds=1 actors=12 sounds=1`、`next-tick new-sounds=0`、`move-window first=1 covered=1 after-4s=2`，全部 failures=0 |
| `--survival-check --mute` | 退出 0 |
| `--survival-check --map pack12 0 --check-waves 3 --mute` | 退出 0，wave=3 kills=42 invalid=0 |
| `--brother-check` / `--level-flow-check` / `--horde-check 0` / `--powerup-play-check` / `--weapon-check` / `--arena-check` / `--prop-check` / `--tutorial-check` | 均退出 0 |

新增检查：`[feedback-cull]` 用真实 BULLET 模板、真实更新路径，只额外提供一个镜头矩形，验证「背向离开→立刻剔除」「朝向进入→保留并穿过→远端剔除」，且都发生在 3000ms 到期之前。

Release 构建：`MSBuild.exe gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /m /v:minimal`，退出 0。

## 验证边界

- 死亡音效窗口是宿主适配。原版没有去重窗口，本轮也没有找到；若日后定位到原版机制，应以其为准替换这段。
- 剔除只在提供了镜头矩形的战斗场景生效。原版多人分屏会取两名玩家视野的并集（:60623），本移植是单人，未实现该分支。
- babe 的救援、对话与统计位沿用既有实现，本轮只修了掷骰的随机源；babe 各模板自身的武器数值未改动。
- 脚本随机流现在因时间播种而每次不同，正式游玩不再可逐帧复现；研究检查仍走固定默认种子。

## 补充：一个 WAV 只占一个声部（2026-09-09 晚）

用户反馈 pack9 炮塔的开火与转动声比其他 SFX 大得多，并指出原版枪械音效不应该叠加、音量应与其他音效相当。

原版侧再确认：`CMediaPlayer::PlayInternal` :363259 每次调用都新建一个 sound event，确实会叠加；每个声部的增益是 `CSoundEvent_Cocoa::SetVolume` :303199 的 `播放器音量 × 事件音量 × 0.001`。本移植没有这两个音量的原始取值，按 1.0 直出，于是叠加几份就直接翻倍——重复越快的音效越响，炮塔那种连发就盖过全场。

因此把宿主混音规则收紧为**一个 WAV 同时只占一个声部**：

- 非移动帧的一次性音效（枪械 cue、prop 音效、敌人脚本音效）在重播前先 `Stop(key)` 再播——**触发频率不变，音量恒定**，不会因为快而变响；
- 循环音（native 27，例如炮塔转动）若同一 owner 已经在放同一个 key，就**不再重新起头**。原先每拍重新 `StopOwner`+`Play`，等于每 16ms 重放一次起音，听感就是嗡嗡的噪音；
- 移动帧音效（死亡/动作音）保持上一节的窗口规则，仍是"在播期间不重触发"。

`--combat-feedback-check` 新增 `[audio-health-check] one-voice`：用真实 SOUNDEFFECT 连续 5 拍触发同一音效，要求 `retriggers=5`（速率没被吞）且并发声部不超过 1。注意 `--mute` 下不开设备流，声部数恒为 0，这一半断言只有在非静音运行时才有实际约束。

## 再补：音效总线没有余量（2026-09-09 深夜）

上一节的"一个 WAV 一个声部"没解决问题——用户反馈武器开火和炮塔仍然吵得要死。原因不在叠加，在**增益本身**。

原始素材实测（`big_360_out/*/30_WAV/`，268 个 WAV）：

```
268 个文件中 118 个峰值 ≥ -0.5 dBFS，RMS 最高 0.368
```

也就是说近一半音效本身就压到了满刻度。本移植的音效播放器音量默认 1.0（`CAudioPlayer::Impl::volume`），从未被调过；音乐走 `CBGM` 明确设成 0.3。于是每一声枪响都是满幅输出，两声叠上就直接削顶——听感就是又响又糊。

原版那条链路：`CSoundEvent_Cocoa::SetVolume` :303199 的每声部增益 = `播放器音量 × 事件音量 × 0.001`；`CMediaPlayer::SetVolume` :361532 把播放器音量钳到 **0..10**，`CSoundEventPCM` 构造函数 :363616 把事件音量设为 **100**。所以刻度就是 `播放器档位 ÷ 10`。音乐总线是同一套刻度：`CBGM::SetVolume` :59971 乘 0.3。

原版把音效档位设成几，本轮没能在反编译里定位到（构造函数初值是 0，赋值点未找到），所以**不假装知道**：把它做成配置项，默认 3——与音乐总线同级，也就是每声部 0.3。

- `CAudioPlayer::SetEffectsGain/GetEffectsGain`：进程级音效增益，新建的音效播放器以此为初值；`CBGM` 自己设 0.3，不受影响。
- `gunbros.cfg` 新增 `EffectsVolume = 3`（0..10），`HostSettings` 解析，两个 main 在读配置后应用 `档位 × 0.1`。嫌小就调大，10 等于改动前的行为。

`--combat-feedback-check` 新增 `[audio-health-check] effects-dial=3 gain=0.30 music-gain=0.30`，把配置到混音的这条线端到端验证。
