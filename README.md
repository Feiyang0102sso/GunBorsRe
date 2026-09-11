# Gun Bros Windows 重建

打开 `gun_bro_re.slnx`，使用 Visual Studio 的 x64 配置构建。仅保留 Re、Viewer、Tests 三个 EXE 工程。编译选项、源码清单和自动测试规则直接写在各自的 `.vcxproj` 中。

| 目录 | 用途 |
| --- | --- |
| `src/engine` | 通用资源、渲染、平台与 Glu Script/Movie/Sprite |
| `src/gun_bros_re` | 游戏启动、玩法、菜单、条目与存档 |
| `src/gun_bros_viewer` | 查看器、里程碑和开发入口 |
| `big`、`assets` | 运行所需的原归档、媒体和宿主着色器 |
| `tests` | 检查实现、运行脚本与 `fixtures/saves` 原存档样本副本 |
| `bin/Debug`、`bin/Release` | 全部 EXE 及运行依赖，按配置统一输出 |
| `obj` | 中间文件、调试符号及构建日志 |
| `_prep` | 已整体忽略的历史参考资料，仅在必须核对原版证据时查阅 |

直接运行 `bin/Release/GunBrosRe.exe`；查看器是同目录的 `GunBrosViewer.exe`。Debug 的三个程序全部位于 `bin/Debug`，Tests 不参与 Release 构建。不生成内部静态库，不使用其他 EXE 输出目录。

Debug 和 Release 均启用已有作弊码。VS 的 F5／Ctrl+F5 默认有声音；自动测试通过 `--mute` 显式静音。

在 Developer PowerShell 中：

```powershell
msbuild gun_bro_re.slnx /p:Configuration=Debug /p:Platform=x64 /m
msbuild gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /m
```

Debug 构建自动生成 `GunBrosTests.exe` 并运行对应检查：Re 为 `progress`，Viewer 为 `movies`。其他检查按需运行，脚本自动增量构建测试程序：

```powershell
pwsh -File tests/run.ps1 -Case resources,progress,movies
pwsh -File tests/run.ps1 -List
pwsh -File tests/verify-runtime.ps1
```

集中修改时可传 `/p:SkipAutoTests=true`，完成后再选择相关检查。测试日志、截图和临时账户统一在 `tests/out`；测试不复制 EXE。代码、脚本和工程配置的注释统一使用英文。

构建和测试只依赖当前工程内的 三个 `.vcxproj`、`src`、`big`、`assets` 与 `tests`，不读取 `_prep`。原始 BIG、运行媒体和存档样本不提交版本库；新环境需要自行提供这些输入。

程序以 EXE 所在目录解析资源和相对路径，默认账户在该目录的 `saves`。已有 `userdata` 账户可通过绝对 `--profile` 路径继续使用；测试样本不会自动导入正式账户。
