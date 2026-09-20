# 游戏配置

`GunBrosRe.exe` 同目录的 `GunBrosRe.cfg` 在启动时读取。以下列出全部 8 个支持的键及默认值；修改后重启生效。键名区分大小写，`control` 保持小写。

分类头只用于排版，键仍按全局名称读取；旧的无分类配置继续兼容，同名键最后一次赋值生效。支持 `#`、`;` 注释。文件使用 UTF-8；标题可包含空格，不加引号，`#`、`;` 保留为注释符。

```ini
# Section headers are labels; keys are global and case-sensitive.
[common]
# Game window title (UTF-8, no quotes).
Title=GunBroRe

[audio]
# BGM volume: 0..10; 0=mute, 3=original volume.
SoundVolume=3
# Sound effects volume: 0..10; 0=mute.
EffectsVolume=3

[game]
# Local online-menu adapter: 0=off, 1=on; no remote connection.
IsConnected=0
# Deathmatch bot: 1=Easy, 2=Normal, 3=Hard
DMBotLevel=1

[control]
# Mouse fire: 1=screen aim, 2=right stick drag
control=1

[debug]
# Debug information and debug hotkeys: 0=off, 1=on.
DebugMode=0
# FPS counter: 0=hidden, 1=visible.
DrawFPS=1
```

`Title` 控制启动、加载、菜单及战斗的游戏窗口标题，不改变 EXE 和配置文件名。`SoundVolume` 只调节 BGM，`EffectsVolume` 调节音效；游戏内音乐开关及自动测试的 `--mute` 仍然有效。

## 本次方案与验收

范围：按用户要求整理原有六项，只增加 `Title`、`SoundVolume`；查看器配置保持现状。

任务：补全默认配置与注释 → 接入所有游戏窗口和 BGM → 保留本地 Debug／Release 原配置值并补齐新项 → 构建并验证。

依据：`CBGM::SetVolume`（`_prep/_IDA_OUT/gunbros_3.6.0_IOS.c:59971`）的基础增益为 0.3；新宿主音量档位乘以 0.1，默认 3 保持旧行为，场景暂停倍率继续独立相乘。本次不修改音频格式或 BIG 数据，已核对 `wav_audio.bt` 的资源职责边界。

验收：旧配置与分组配置兼容，带空格及中文的标题完整读取，非法音量被拒绝；BGM 0／3／10、暂停减半、关闭音乐和独立音效增益均正确。

## 验证结果（2026-09-20）

- Debug／Release 执行 `MSBuild GunBrosRe.vcxproj /p:Configuration=<配置> /p:Platform=x64 /m /v:minimal /nologo`，包含 Game、Viewer、Tests，退出码均为 0。Debug 自动执行的 progress、big-version、viewer-controls 及 viewer-smoke 全部通过。
- `pwsh -NoProfile -File tests/run.ps1 -Configuration Debug -Case host-settings,audio-transitions -NoBuild`：2/2 通过，退出码 0，受保护资源、存档和配置无变化。日志：`config-regression-debug.log`。
- Release 使用 `GunBrosTests.exe --mute --host-settings-check --test-output <独立输出目录>`，失败数 0、退出码 0。日志：`config-validation-release.log`。
- 两版本使用 `--mute --profile <测试目录>` 实际启动并经过片头进入菜单，按进程 ID 枚举窗口读取标题：Debug 自定义 `Gun Bros 窗口测试`、Release 默认 `GunBroRe` 均匹配，正常关闭退出码 0；测试后逐字节恢复配置。记录：`obj/config-validation/runtime/`。最初使用 `MainWindowTitle` 无法识别隐藏启动窗口，改为枚举窗口后验证通过，未因此修改游戏实现。
- 本地 Debug／Release 配置已经分组，保留原 `DebugMode`、`IsConnected`、`DMBotLevel` 等设置，补齐 `DrawFPS=1`、`Title=GunBroRe`、`SoundVolume=3`。配置文件位于忽略的 `bin/`；新安装首次运行会自动生成带分组和全部注释的配置。
