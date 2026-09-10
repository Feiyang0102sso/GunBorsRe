# 商店切枪音效与跨场景音乐

## 范围与验收方案

按用户授权，恢复商店切枪的原动作声音及持续BGM：菜单音乐经过进场加载CG，战斗开始切战斗音乐；离场CG和结算保持战斗音乐，结算退出后返回精炼厂/菜单才切菜单音乐。资源和真实存档只读，使用隔离账户测试。

- [x] 建立实际商店模型切枪与菜单/战斗/结算交接的失败检查。
- [x] 按原动作帧引用恢复UI声音采集与播放，不手填换枪WAV。
- [x] 音乐由外层游戏流程持有，加载/卸载不销毁；只在原业务边界切曲。
- [x] 检查音乐设置、暂停恢复、重复菜单、进入战斗、结算和精炼，构建正式Release并交接。

已查 `entries/common.bt` 的MeshFrameSound/MeshMove、`wav_audio.bt`、`ui_movie.bt`；原 `CBrother::UpdateUI` :137516同时查询躯干和腿部跨越的声音帧，再调用CSoundQueue播放。`CGunBros::OnLoaded` :79274在游戏Bind后NextTrack；`LoadMenus` :79556和`EnterShell`附近 :79672播放菜单track0。原CBGM由CGunBros持有，非每页局部对象。具体切回时机同时遵从用户本轮实机说明。

## 原因与实现

1. `CBrother::UpdateUI`只直接推动动画时钟，未查询声音帧；`GameMenu::AdvancePlayerPreview`也没有WAV播放出口。新增共用的 `CMoveSetMeshController::CollectSounds`，保留UI原先的速度截断及腿部相位同步，按 `[previous,current)` 采集原WAV；菜单缓存、解码并播放。实际两位兄弟双向切枪各触发一次 `58595522:3`（core WAV3），来源是动作资源，不是点击按钮时硬填音效ID。
2. 原 `ShowGameMenu` 与 `RunSurvival`各建局部CBGM，离开菜单即销毁音轨，地图加载完成才新建战斗音轨；返回菜单又在加载后重播track0。现由 `RunGameFrontEnd`持有音乐，通过 `SurvivalGameContext`及菜单参数共享，所有正式生存/教程/Horde入口均传递同一对象。`LoadingScreen`只Update当前音轨，不选曲。
3. `BeginPostGame`保留战斗音乐，包含Overview、Casualties及熟练度弹窗。原结算BACK关闭计时完成后才解除保留，进入精炼厂（无XPlo时返回菜单）时播放track0。投降退出暂停菜单会恢复同一音轨。
4. `CBGM::Play`在旧曲仍播放时解码新曲，并复用已解码PCM；切曲不重置音乐开关。原 `SetEnabled(false)`会被下一次Play中的固定0.3音量覆盖，此次同时修正。音乐文件继续使用原 `mp3/game_0.mp3` 与 `1.mp3`–`6.mp3`；这些是原程序引用的独立音乐文件，不是从BIG手工复制的资源表。

## 红绿检查

2026-09-10后续暂停反馈补充：游戏暂停采用原 `SetVolume(0.5)` 保持战斗曲流动，恢复/投降进入结算时还原1.0。暂停停止流的旧行为已纠正，新增真实HUD+SDL检查见[暂停与结算修复](postgame-pause-fix-2026-09-10.md)。

永久研究菜单76 / `--mute --audio-transitions-check`，已加入 `test-muted.ps1 -Phase Core`。

- 修复前：`out/audio-transitions-before.log`，退出1。两位兄弟双向4次切枪声音全部为0；菜单额外启动一次音轨、战斗未交接给流程持有者、结算重播菜单曲，共7项失败。
- 首轮修复后：`out/audio-transitions-after.log`，退出0；切枪各1次原WAV，菜单/结算额外播放次数均0，战斗track1保留到结算，精炼厂track0。
- 零音量真实SDL验证：`out/audio-transitions-sdl.log`，退出0。整个菜单→进场加载→两波战斗→离场加载→结算→精炼流程复用1个音频设备、1条流，保持1个音乐声部，末端仍有16,746,432字节排队；暂停恢复、关闭音乐后切曲、重新打开音乐均通过。数值为当次快照，排队字节随实际耗时变化。

研究检查仅对自己的播放器开启真实设备零增益模式；普通 `--mute`仍跳过外放而继续解码/资源检查。测试账户位于 `out/audio-transitions/<tick>/`；截图为 `out/audio-transition-{menu,postgame,refinery}.png`。本轮未做扬声器耳听或与iOS逐采样波形比对，不把队列验证描述为原版音频波形完全一致。

## 最终回归与构建

所有运行使用 `bin/x64/Release/gun_bros_research.exe --mute`，下表命令追加相应参数；均退出0。

| 参数 | 覆盖 | 日志 |
|---|---|---|
| `--weapon-check` | 76武器模板、3视觉条目、原动作发射与停止 | `out/audio-regression-weapon-check.log` |
| `--dual-weapon-check` | 商店双枪装备、重复保护、实际存档重载 | `out/audio-regression-dual-weapon-check.log` |
| `--options-check` | 原选项及设置保存重载 | `out/audio-regression-options-check.log` |
| `--postgame-menu-check` | 原实战结果、结算开合/页签/退出/不重复发奖 | `out/audio-regression-postgame-menu-check.log` |
| `--profile-play-check` | 从第2波续玩至第4波，XP101、XPlo90 | `out/audio-regression-profile-play-check.log` |
| `--scene-transition-check` | Logo→菜单→战斗→菜单保留窗口和GL上下文 | `out/audio-regression-scene-transition-check.log` |
| `--research`，输入76 | 从永久菜单实际进入音频专项，真实SDL检查通过 | `out/audio-transitions-menu.log` |

MSBuild `gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`退出0，正式和研究EXE均更新；日志 `out/audio-transitions-build.log`。`git diff --check`通过。原BIG、解包样本、mp3、原iOS程序和`saves/`均只读，未提交游戏资源。
