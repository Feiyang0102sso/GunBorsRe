# Viewer BIG 格式自动识别

## 范围与实现

按用户要求，多版本仅用于 Viewer 资源查看；不迁移存档，不复刻旧版熟练度、等级限制或游戏规则。`BigVersion` 只记录格式大类 1/2/3，最新为 1，不建立小版本配置。

三条编译期配置集中于 [BigVersions.h](../src/gun_bros_re/data/BigVersions.h)：1 对应 28 类/33 段，2 对应 27 类/32 段，3 对应 26 类/31 段。运行时参数来自自动识别，无需手工填写 CFG。配置仅描述已验证的格式特征，资源值仍从 BIG 读取。

`CGameObjectPack` 读取原 keyset 前缀，并核对具名 `OBJECT_SCRIPT__COUNTS_` 及其实际负载长度。图片、声音、模型按对象类型数量定位，字符串按原聚合句柄读取；不存在的新对象类型不会别名到旧包图片。媒体段跨度只计算逻辑 ID，排除 PNG/WAV/BIN 类型标签的高位差。

Viewer 的 `InitAuto` 优先 XGA TOC，无 XGA 时读取普通 TOC；已存在但损坏的 XGA 不静默回退。启动检查每个包，不接受未知或混合格式。`--big-version` 和永久菜单 81 显示识别结果。游戏菜单和生存入口仍要求 BigVersion 1，在接触账户前拒绝旧格式。

M1 原来的固定字符串引用移至 Tests `--asset-sample-check`；通用 M1 改为读取当前包实际首条字符串，并保留物理解压检查。原样本常量及注释保留在独立测试。M1 中的包名称表遍历仍会报告不能作为句柄解析的值，未把这些未知值重写成资源。

## 依据

- 原 BIG 的 `___GAME_TOC_KEYSET`、`OBJECT_SCRIPT__COUNTS_`；本地 263 个非空 BIG 的逐包偏移、类型数量记录位于此前的 `tests/out/version-compatibility/resource-structure.json`。
- 结构模板：`_prep/_Big_tool/binary template/big_assets/big_archive.bt`、`big_keyset.bt`、`big_TOC.bt`。
- 原反编译 `CBigFileReader::Open` :356787、`GetResourceDataStream` :356239、`CGameObjectPack::InitizlizeIndices` :129893、`InitializeCounts` :129948。类型计数是原数据；图片等尾部分类通过原包句柄和物理组交叉核对。
- 项目构建及常规回归不依赖 `_prep`；旧版原件仅用于本轮人工指定路径的兼容核查。

## 验证结果（2026-09-11）

| 检查 | 结果 |
| --- | --- |
| Debug 游戏、Viewer、Tests 构建 | 退出 0 |
| Release 游戏、Viewer 构建 | 退出 0；仍有未修改代码的数值转换警告 |
| 自包含 `--big-version-check` | 退出 0；三档、普通/XGA、媒体、字符串、未知/错配/截断及混合包 |
| 22 套实际样本自动识别 | 21 套退出 0；2.2.1 的原 TOC 为空，退出 1 |
| 三档代表及普通/XGA：地图枚举、全部模型解析 | 均退出 0；代表样本模型无读取失败或尾部剩余 |
| 三档代表：地图、模型实际截图 | 6 项退出 0，均目视核对；模型单部件显示不是缺失整个角色 |
| 4 套代表样本通用 M1 | 均退出 0，字符串来自各自原 keyset |
| 最新资源进度/商店与 Movie 回归 | 退出 0，`failures=0` |
| 独立原样本字符串检查 | 退出 0，`Colony Test` 匹配 |
| Debug 正式菜单 | 最新资源退出 0；旧资源退出 1 且未创建测试账户 |
| Release 三档自动识别 | 均退出 0；截图参数仍拒绝，退出 1 |
| Debug 当前手换旧 BIG | Viewer 自动识别为 BigVersion 3；构建未覆盖该资源目录 |
| `git diff --check` | 退出 0 |

日志和截图：`tests/out/big-version-implementation/`。本轮未运行全量截图基线。格式自动识别成功不代表同档所有实体、Movie、Flow 或游戏行为已经兼容。

## 复现

在项目根目录运行，资源目录使用绝对路径：

```powershell
$bigPath = 'E:\coding_projects\c_projects\gun_bro_re\_prep\versions\1.0.0\big'
& .\bin\Debug\GunBrosViewer.exe --mute --big $bigPath --big-version
& .\bin\Debug\GunBrosViewer.exe --mute --big $bigPath --maps
& .\bin\Debug\GunBrosViewer.exe --mute --big $bigPath --meshes
& .\bin\Debug\GunBrosViewer.exe --mute --big $bigPath --m1

$output = Join-Path (Get-Location) 'tests/out/big-version-manual'
& .\bin\Debug\GunBrosTests.exe --mute --big-version-check --test-output $output
```

普通回归入口为 `pwsh -File tests/run.ps1 -Case big-version,asset-sample`，其中原样本检查使用项目默认参考资源。需要保留 EXE 目录内替换的 BIG 时，构建使用 `/p:SkipRuntimeStaging=true /p:SkipAutoTests=true`，随后用独立 `--test-output` 和显式 `--big` 执行相应检查；正常 VS 构建行为不变。
