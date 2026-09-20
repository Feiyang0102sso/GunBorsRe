# 游戏配置

`GunBrosRe.exe` 同目录的 `GunBrosRe.cfg` 在启动时读取。以下列出全部 11 个支持的键及默认值；修改后重启生效。键名区分大小写，`control` 保持小写。StartDialog=1 时普通启动先显示英文设置小窗；Launch game 保存后进入游戏，Cancel 或关闭窗口放弃本次编辑。界面分组标题是固定英文，不显示方括号标签。

分类头只用于排版，键仍按全局名称读取；旧的无分类配置继续兼容，同名键最后一次赋值生效。支持 `#`、`;` 注释。文件使用 UTF-8；标题可包含空格，不加引号，`#`、`;` 保留为注释符。

```ini
# Section headers are labels; keys are global and case-sensitive.
[common]
# Game window title (UTF-8, no quotes).
Title=GunBroRe
# Show launch dialog at startup: 0=off, 1=on.
StartDialog=1
# Window client size; oversized windows fit the desktop automatically.
ScreenX=1600
ScreenY=1200

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

`Title` 同时控制设置小窗和游戏窗口的标题，只能手动修改 cfg，小窗不提供标题编辑框，不改变 EXE 和配置文件名。`StartDialog` 位于 `[common]`，默认 1；设为 0 后下次启动直接进入游戏。要恢复小窗，手动改回 1。`SoundVolume` 只调节 BGM，`EffectsVolume` 调节音效；游戏内音乐开关及自动测试的 `--mute` 仍然有效。

`ScreenX`、`ScreenY` 保存分辨率下拉框选中的窗口客户区宽高。预设为 640×480、800×600、1024×768、1280×960、1600×1200、1920×1440、2048×1536，全部保持 4:3。屏幕放不下时沿用原有自动等比缩小，但不回写缩小后的尺寸覆盖选择。旧 cfg 缺少两项时使用 1600×1200；手改的合法非预设尺寸会作为 saved 项显示。宿主尺寸范围为每轴 1～16384。

小窗可修改尺寸、启动提示、音量、连接、难度、操作与调试选项；保存时保留 cfg 注释、分组、标题及未知行，不新增全屏或显示器选项。重复键仍按全局处理，保存时同步更新所有同名键。截图调用跳过小窗；已有命令行 `--game` 直接使用保存的设置进入游戏，自动检查使用该入口避免被小窗阻塞。Viewer、Tests 保持原入口。抬头图使用 `assets/startup/Gun_Bros_Header_Art.png`；界面文字、颜色、字体、间距、控件布局与分辨率预设集中在 `src/gun_bros_re/host/ZLaunchDialogConfig.h`。实现及验证记录见 [启动设置窗口](launch-dialog.md)。

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
