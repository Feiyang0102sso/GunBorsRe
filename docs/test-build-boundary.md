# 测试编译边界调整

## 方案与验收

用户要求移除测试编译宏，tests 直接调用 src；构建主程序时同时生成 Viewer、Tests。本轮只调整宿主代码与构建边界，不修改原资源读取和玩法算法。

- 一个工程继续按 `GbProduct` 生成三个 EXE；Debug、Release 均构建伴随程序。
- `tests/*.cpp` 只进入 Tests；Game、Viewer 不链接测试实现。
- 移除 `GB_ENABLE_TESTS` 的定义和分支；保留现有可选场景、输入与诊断接口，默认不启用。
- Arena 初始化完成后通过可选回调交给测试端，消除对 `CheckArena` 的直接链接依赖。
- 截图独立控制：Debug 和 Tests 支持截图，Release Game/Viewer 保持关闭。
- 两种配置完成构建；执行现有构建检查及 Arena、菜单输入、战斗场景相关检查，全程静音。

## 任务

1. 已完成：清理条件编译，修正 Arena 调用边界。
2. 已完成：按产物选择测试源码，补齐 Release 测试构建与运行入口。
3. 已完成：同步开发约定，验证构建与相关流程，记录结果。

## 结果

MSBuild 求值确认两种配置均为：Game 171 个编译单元、Viewer 183 个，测试源码均为 0；Tests 254 个编译单元，其中测试源码 74 个。测试宏不再进入预处理定义。

### 构建

以下命令均退出 0，两种配置分别生成 Game、Viewer、Tests：

```powershell
msbuild GunBrosRe.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
msbuild GunBrosRe.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

Debug 自动执行 `progress`、`big-version`、`viewer-controls`，3/3 通过；Viewer 六入口、原始动画、返回菜单及参数/配置校验全部通过。

### 专项回归

以下命令均退出 0，脚本向所有游戏进程传入 `--mute`：

```powershell
pwsh -File tests/run.ps1 -Configuration Debug -Case enemies,brother,play-interaction,scene-transition,path-cache,flock,game-menu -NoBuild
pwsh -File tests/run.ps1 -Configuration Release -Case progress,enemies,brother,play-interaction,scene-transition,path-cache,flock
pwsh -File tests/verify-runtime.ps1
```

- Debug 专项 7/7、Release 专项 7/7 通过；两轮受保护文件变化均为 0。
- Arena 核对 78 个条目，其中 76 个有脚本、2 个无脚本，未知调用和检查失败均为 0。
- Release 正式程序通过异工作目录启动、菜单、作弊码、存档和 Viewer 命令边界检查。
- `verify-runtime.ps1` 的旧 `Viewer --m1` 调用改由 Tests 执行，Viewer 另行走正常启动流程。
- 没有修改原资源解析和玩法算法，没有重跑全量截图基线。

### 日志与验证边界

构建日志在 `obj/test-boundary/debug-build.log`、`release-build.log`；自动检查、专项回归和截图分别保存在该目录的 `debug-auto`、`debug-checks`、`release-checks`、`runtime` 下。`tests/out` 保留最近一轮结果。

首轮沙箱构建因 Visual Studio FileTracker 权限失败，后续在沙箱外通过；首次自动检查因构建日志占用了待清理的 `tests/out` 而停止，日志改存 `obj` 后通过。Release Viewer 仍有旧 `GunBrosViewer.vcxproj` 中间文件引起的 MSB8028 警告；完整编译还有既有数值转换警告，没有编译错误。

保留历史注释中的宏名称；源码及工程中已无该宏的定义和预处理分支。截图能力仍由独立截图开关控制，`game-menu` 截图用例使用 Debug 主程序，Release 主程序通过 `verify-runtime.ps1` 验证。
