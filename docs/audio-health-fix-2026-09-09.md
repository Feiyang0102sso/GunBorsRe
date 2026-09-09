# 血条复核与群体死亡音效修复（2026-09-09）

> 后续位置纠错：本轮遗漏了最终屏幕矩形的水平居中检查，像素半宽被误当世界偏移。已修复并补充 9 组对齐回归，见[血条对齐记录](healthbar-alignment-2026-09-09.md)。本页旧截图保留当时状态。

## 范围与验收标准

按用户新反馈重新核对 BIG、BT 和 iOS 消费函数，先建立失败检查，再修复实际战斗路径。目标是：血条服从原敌人脚本、尺寸不随错误的轮次/镜头倍率放大；同帧同一声音只播一次，不同声音各播一次；减少音频设备与播放流的反复创建。没有添加手写敌人血条白名单、血条资源表或音效冷却秒数。

## 血条：上一轮错误与原证据

上一轮把 `CLevel::DrawEnemyHealthBars` 的 `mem+311032` 错认成轮次，实际是 **LEVEL 脚本变量 4**；还混淆了 `CCamera mem+0` 的屏幕倍率与 `SnapScale` 的镜头倍率。原生尺寸被错误放大。上一轮只手动改显隐变量的检查不能证明原敌人显隐已经验收，本轮以真实 BIG 脚本替代该检查。

| 来源 | 已确认行为 |
|---|---|
| `CEnemy::Bind`，iOS 反编译 73381 起；`VariableResolver`，68982 | `mem+944` 初始化为 1，映射 `CEnemy.variable[15]`。初始化属于 Bind，Spawn 不应覆盖脚本留下的状态。 |
| pack1 ENEMY 3；`pack1_xga_0025_0x36f5.bin` | Spawn 函数内 `@0x152` 将变量 15 设为 0，默认不画血条。 |
| pack1 ENEMY 20；`pack1_xga_0042_0x4d31.bin` | Spawn `@0x15A` 关闭；state 4 的 `@0xF1` 开启；state 6 的 `@0x135` 关闭。 |
| pack7 LEVEL；`pack7_xga_0011_0x1e62.bin` | state 114 的 `@0x27AA` 设置 LEVEL 变量 4；后续 `@0x27E6` 清除。它是原脚本控制的特殊波次标志，不是波数除以每轮波数。 |
| `CLevel::DrawEnemyHealthBars`，120454 | 检查对象类型、正血量与变量 15；原生尺寸 30×4、内缩 1，根据 LEVEL 变量 4 选择 1/2 倍。 |
| `CCamera` 构造函数，64622；`ConvertToScreenSpace`，65079 | `mem+0 = min(screenWidth/480, screenHeight/320)`。血条尺寸使用屏幕倍率，锚点经过镜头投影；不再将已计算的屏幕尺寸乘一次镜头倍率。 |

ENEMY 样本位于 `big_360_out/pack1_xga/0xf4e02223/06_ENEMY/`，运行时仍读取 `big/` 原归档。上述 `@` 地址为 `out/binary-research/flow-disassembly/` 对应清单的定位。BT 为 `big_assets/enemy_template.bt`、`entries/common.bt` 和 `flow_bytecode.bt`；已补充血条消费证据。**BIG 保存显隐脚本及模型信息；30×4 是原程序绘制常量，BIG 没有独立血条尺寸表。**

实现：`CEnemy::Bind/Spawn`、`CLevel::HasLargeEnemyHealthBars`、`CombatScene::EnemyHealthBars`、`MapScene` 的 HUD 坐标转换。

## 音效：已证实与尚未证实的边界

真实群杀复现：pack1 ENEMY 0/1 的原死亡动画分别引用 **pack1 WAV 2/5**。12 个同类死亡触发 12 次，两个种类共 24 个触发 24 次。声音来自 BIG 的 ENEMY MoveSet 声音帧，不是新增死亡音效表。

核对 `entries/sound_effect.bt`、`wav_audio.bt` 与原函数：

- `SoundEffect::Init`（130191）只读取 WAV 引用，没有播放间隔字段；死亡声音还可由 `CMoveSetMeshController::Update`（134044）直接引用 WAV。
- `CSoundQueue::QueueSound` 是资源加载登记，不能把它的资源查重解释成播放帧去重。
- 已追到 `CSoundQueue::PlaySound`（102896）、`CMediaPlayer::PlayInternal`（363259）、`CMediaPlayer_Cocoa::PlayInternal`（303604）和 `CSoundEvent_Cocoa::Initialize`（303729）。Cocoa 使用可复用的 OpenAL 资源池；当前 Windows 实现却为每次声音调用 `SDL_OpenAudioDeviceStream`。
- **尚未从这条原链路定位到用户观察的精确去重窗口。** 本轮按用户明确描述实现“每次战斗更新内，相同 WAV 合并”，标为 Windows 宿主适配，不宣称已完整恢复 iOS 去重算法。不臆造毫秒冷却，也不阻止下一次更新再次播放。

实现：`WeaponEffects::BeginAudioFrame/PlayWav` 以解析后的包 hash + WAV 序号合并单次音效；两个不同 WAV 保留两次播放。循环音效保持各自 owner。缓存 SoundEffect 引用和已解码 WAV，重复触发不再重复读取原归档。`CAudioPlayer` 改为一个持久设备和可复用播放流，保留原有宿主 32 路上限、停止、暂停和循环操作。背景音乐继续走独立通道。

## 验证结果

永久入口仍为研究菜单 **72 / `--combat-feedback-check --mute`**，包含此前尖塔、结算、穿透弹检查与本轮新增检查。新增后端检查仅对自己的播放器开启 **零音量真实 SDL 设备验证**；普通 `--mute` 仍不播放声音。未使用用户正式账户作为测试存档。

| 检查 | 修复前 | 修复后 |
|---|---|---|
| 相同尺寸下进入下一轮 | 血条宽 24→48 | 未设置原脚本标志时宽度保持不变；设置标志才翻倍 |
| 1600×1200 普通波次 | 错误使用镜头/轮次换算 | 原公式得到 100×13 屏幕像素 |
| BIG 显隐 | 上一轮只手动切变量 | ENEMY 3 默认隐藏，ENEMY 20 的 state 4/6 开关均通过 |
| 12 个同类同时死亡 | 12 次声音 | 1 次 |
| 两类各 12 个同时死亡 | 24 次声音 | 2 次 |
| 下一次战斗更新再次提交原声音 | 未验证 | 两种原声音均能再次播放，无额外冷却 |
| 零音量真实后端，16 次播放 | 16 个设备、16 条流；约 713ms | 1 个设备、2 条流；约 336ms，含首次设备初始化及暂停恢复 |
| 已预热后提交 200 次声音 | 未记录 | 约 0.5ms，无新设备/播放流创建；这不是游戏帧率测试 |
| 循环声音与暂停 | 未专项覆盖 | 停止 owner 101 不影响 102；暂停状态和恢复、按 WAV 停止通过 |

失败日志：`out/audio-health-before.log`、`out/audio-health-backend-before.log`；最终专项日志：`out/audio-health-final.log`（退出 0）。正式操作回归 `--profile-play-check --mute` 退出 0，见 `out/audio-health-controls.log`。Release 构建日志：`out/audio-health-build.log`（退出 0）。

截图：[原脚本显隐与修正尺寸](../out/audio-health-authored.png)。

`--native-profile-play-check --mute` 最终退出 0：四个正式星球各两波、Horde 两波及装备/进度保存重载通过，见 `out/audio-health-native-final.log`。首次回归发现旧 `CheckLevelSounds` 将多个共享 WAV 的资源引用放在同帧并要求每个都新增播放；现将这些引用分帧独立验证，未改成放宽成功条件。原失败日志保留在 `out/audio-health-native-play.log`。

正式 EXE：`bin/x64/Release/gun_bros_re.exe`，最终构建时间 2026-09-09 15:24:44（本机）。检查前后的原存档和正式账户共 61 个文件 SHA-256 无变化，见 `out/audio-health-protected-result.json`；`git diff --check` 退出 0。后端耗时测量针对播放指令提交，不代表整场游戏帧率或所有卡顿均已解决。
