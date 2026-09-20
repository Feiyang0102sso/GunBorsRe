# 启动设置窗口（2026-09-20）

## 方案与边界

用户已授权实现启动小窗，按现有 `[common]`、`[audio]`、`[game]`、`[control]`、`[debug]` 分组，新增 `ScreenX`、`ScreenY`，默认 1600×1200；后续追加 `[common] StartDialog=1` 控制是否显示小窗。界面小标题硬编码英文，不显示 `[xxx]` 后缀，cfg 仍保留分组。这是 Windows 宿主界面，不修改原版 BIG、Movie、相机或玩法数据，无需伪造原版界面布局。

同一 Game EXE 在创建 SDL 窗口前显示原生 Win32 设置窗口。顶部完整展示用户指定图片，下方采用双列分组，常规设置跨两列；只提供 Launch game、Cancel 两个按钮。前者保存当前配置后启动游戏，后者及关闭按钮直接退出，不保存当前编辑。保存保留 cfg 注释、分组和未知行。窗口标题直接使用 cfg 的 Title，小窗没有标题编辑框。截图调用跳过交互窗口，Tests 和 Viewer 保持原入口。

`StartDialog=0` 时跳过小窗，默认及旧配置缺键时使用 1。已有 `--game` 命令行改为直接使用保存设置进入游戏；正式 EXE 的自动检查传入它，防止 Release 无截图模式被小窗阻塞。测试关闭窗口按进程及非空标题定位，兼容用户现有的 `Title` 设置。

视觉：底色金属灰 `#E5E9E8`，面板灰白 `#F7F8F5`，正文深灰绿 `#283634`，次要文字 `#64716B`，边框 `#ADB397`，主按钮黄铜 `#B5A16C`。正文使用 Segoe UI，分组标题使用加粗 Segoe UI，参数值沿用系统控件。以原始抬头图为唯一主要装饰，避免额外纹理和动画。

尺寸是 SDL 窗口客户区尺寸，启动时继续执行现有屏幕适配。按用户追加要求，界面全部使用英文，分辨率改为一个大下拉框，枚举 640×480、800×600、1024×768、1280×960、1600×1200、1920×1440、2048×1536。cfg 保存选中的预设，不用自动缩小后的尺寸覆盖选择。手改 cfg 中已有的非预设尺寸作为 saved 项保留，不开放自由输入；每个分辨率不另设相机参数。输入校验和小屏幕布局属于宿主实现。

界面可调常量集中在 `src/gun_bros_re/host/ZLaunchDialogConfig.h`：Text、Layout、颜色、字体、DPI 基准、按钮和分辨率预设。分辨率枚举类型保留在 `ZScreenResolution.h`。连接勾选项文案为 Enable local online connection。资源统一由工程已有的 `assets/**` 复制规则部署。

## 任务与验收

1. 下载并验证 PNG，加入运行文件复制流程。
2. 扩展配置读取和保留注释的保存，验证默认值、旧配置、重复键、未知行及失败不覆盖。
3. 实现分组窗口、DPI 布局与输入校验，接入正式启动。
4. 构建 Debug/Release，执行相关配置与现有自动检查，验证真实界面、保存取消和 SDL 尺寸。

## 图片来源

- 用户提供页面：https://gunbros.fandom.com/wiki/Gun_Bros_Wiki?file=Gun_Bros_Header_Art.png
- 页面引用原图：https://static.wikia.nocookie.net/gunbros/images/0/0a/Gun_Bros_Header_Art.png/revision/latest?cb=20260705115331&format=original
- 本地文件：`assets/startup/Gun_Bros_Header_Art.png`。服务器默认内容协商会返回 WebP；使用原始格式参数后取得 PNG，并验证八字节签名。原图仅作用户指定宿主抬头图，保持本地资源不提交的约定。

## 首版验证结果（本次修订前）

- PNG：574×190，204270 字节；SHA-256 `84E594B70986DDA106BBECFE73CFB367AA73B0E7B525B050EDFF8B023FFA7FD9`。Debug/Release 的 `asset/` 已复制相同原图。
- Debug/Release Game、Viewer、Tests 构建成功。完整 Debug 自动检查 progress、big-version、viewer-controls、viewer-smoke 均通过，日志 `obj/launcher-build-debug-full.log`；Release 日志 `obj/launcher-build-release.log`。最后的未保存提示为界面文案修正，最终产物日志 `obj/launcher-build-debug-final.log`、`obj/launcher-build-release-final.log`。
- `pwsh -NoProfile -File tests/run.ps1 -Configuration Debug -Case host-settings,audio-transitions -NoBuild`：2/2 Passed，退出码 0，保护文件变化 0。记录 `obj/launcher-validation/regression-debug.log`。
- Release `GunBrosTests.exe --mute --host-settings-check --test-output E:/coding_projects/c_projects/gun_bro_re/obj/launcher-validation/config-release`：失败数 0，退出码 0。配置回归覆盖默认值、缺少尺寸的旧配置、UTF-8/BOM、CRLF/LF、重复键、注释/未知行、非法尺寸、幂等保存、保存失败不覆盖和音量消费。测试最初的 LF 断言混用了 Windows 文本输出，已用明确的二进制 LF 样本修正并复测。
- `pwsh -NoProfile -File tests/run.ps1 -Configuration Release -Case game-menu -NoBuild -TimeoutSeconds 60`：1/1 Passed，退出码 0，保护文件变化 0。验证正式 Release 使用 `--game` 跳过小窗、初始化菜单、自动正常退出，日志 `obj/launcher-validation/release-menu.log`。
- 真实英文小窗及分辨率展开列表已目视检查。隔离程序从原配置读入，点击 Save 后补齐两个尺寸键；选择 1024×768 并点击 Launch game 后 cfg 正确写入，SDL 日志明确显示 `[window] 1024x768`，进入标题菜单并正常关闭，退出码 0。记录 `obj/launcher-validation/runtime/logs/GunBrosRe.log`。实际启动显式传入 `--mute --skip-intro --profile saves-test`，未访问用户存档。
- 本地 `bin/Debug/GunBrosRe.cfg` 与 `bin/Release/GunBrosRe.cfg` 仅补齐 `ScreenX=1600`、`ScreenY=1200` 及注释；保留用户标题、音量、难度、在线菜单和调试选项。源码原有的 `Game Config (EN).txt` 删除状态属于本轮开始前的用户改动，未恢复或覆盖。
- 小窗取消/关闭后未保存的配置保持原样。最终 Debug 与 Release 构建均完成。
- DPI 布局按显示器工作区缩放；已经在当前桌面检查，无额外显示器或不同 DPI 档位的实机覆盖。本轮不新增全屏功能，不修改原版菜单布局和相机行为。

## 本次修订验收

- 常量集中、固定英文分组、Title 只读、两个操作按钮以及新资源路径。
- StartDialog 默认和旧配置兼容；0/1 保存回读；非法值拒绝；新键写入 common；已有注释和分组不变。
- Debug/Release 构建与配置检查，实际小窗取消不保存、启动保存以及关闭提示后的直启流程。
