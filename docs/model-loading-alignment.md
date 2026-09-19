# 模型加载职责归位

日期：2026-09-19。用户已批准继续上一轮提出的模型加载修正，删除 ZMeshAssets，收掉多余 application／graphics 目录。

## 原版依据与方案

- `CGun::Template::Load/LoadMesh`：127965、128918；`CArmor::Template::Load/LoadMesh`：176493、176635；`CBullet::Template::LoadMesh`：60448。
- `CMoveSetMesh::Load/LoadMesh/Free`：123213、123157、123189。模型加载与图像加载是分别安排的任务，模型保留在模板／动作配置中。
- `CResourceLoader::AddFunction/AddImage/RemoveImage/LoadNext/LoadImmediate`：102097、102249、102032、102418、102496。图像由 CImagePool 复用，释放解除调用者引用。
- `CImagePool::GetImage/Remove`：85609、85427，命中增加使用计数，最后一个使用者移除时销毁图像。`CEnemy::Template::Load`：68895，委托 CMoveSetMesh。
- 同时核对 mesh.bt、entries/common.bt、gun_template.bt、armor_template.bt；不改原资源、字段与动画筛选规则。
- 模板／动作集持有可共享的 CPU 模型；实例持有动态绘制缓冲。图像池以原包、资源 handle 及宿主 GL 上下文区分纹理，最后一个使用者释放时销毁，包缓存不延长 GPU 寿命。
- CResourceLoader 位于 engine/resources，只接收已解析的模型／图像分区范围，不依赖游戏类型；CGunBros 负责从真实 keyset 配置这些范围。恢复模型回调和图像请求队列，当前同步消费，错误明确返回。

## 实施与验收

1. 实现资源加载队列与图像池，归还模板／动作模型加载职责。
2. 正式游戏和 Viewer 改用同一资源入口，删除组合助手；CGunBros 头文件归 data/objects。
3. 验证模板模型复用、图像复用与释放、取消待加载图像、加载失败清理、上下文隔离，以及装备／弹体／Viewer 实际流程。
4. Debug／Release 构建三产物，记录专项结果和剩余边界。自动检查静音，原件只读，不重复全量截图基线。

实施及验收已完成。旧组合助手和多余目录已删除，模型与模型贴图的实际加载、复用与释放链已接入正式游戏和 Viewer；其余资源加载分支的边界见末节。

## 阶段验证

| 阶段 | 检查 | 结果 |
|---|---|---|
| 模型归位 | Debug Tests；weapons、armor-data、powerups、progress、armor-render、brother、dual-weapon | 构建退出 0，7/7 通过，受保护文件变化 0 |
| 加载器／图像池及敌人接入 | Debug Tests；weapon-effects、enemies、resource-loading、armor-render、brother、store-equipped、dual-weapon | 构建退出 0，7/7 通过，受保护文件变化 0 |
| 最终构建 | Debug／Release 的 Game、Viewer、Tests | 两种配置均退出 0；Debug 自动检查和 Viewer smoke 全部通过 |
| 最终 Debug 流程 | big-version、resource-loading、native-profile-play、store-cards、store-equipped、dual-weapon | 修正窗口采样后 6/6 通过，退出 0，受保护文件变化 0 |
| 最终 Release 流程 | weapon-effects、enemies、resource-loading、armor-render、brother、native-profile、dual-weapon | 7/7 通过，退出 0，受保护文件变化 0 |

新增永久入口 `--resource-loading-check`（tests/run.ps1 的 `resource-loading`）。使用真实 BIG 和 GL 上下文核对：模板副本共享模型且不重读、重新 Init 不破坏旧副本、图像重复请求只读取一次、最后使用者释放后 glIsTexture 为 false、待加载图像可取消、失败清理后续请求、不同上下文不串用图像。

新增测试入口时编译曾发现 main 缺少 Checks.h，补齐后通过；没有借助测试宏或替代资源绕过失败。详细命令、日志和阶段二截图／输入哈希保存在本地 `obj/model-loading-20260919/`，阶段二归档为 `stage2-details.zip`。`before.zip` 保存本轮开始时的源码和文档。

最终进关检查曾遇到测试采样问题：窗口按当前桌面缩为 805×604，旧空袭边框检查固定读取 y=640..1039，三次均采到画面外。失败截图确认边框实际存在。`SurvivalWavesChecks.cpp` 现按实际 viewport 缩放原 1600×1200 参考采样矩形，保留原 300/80000 的有效像素比例；缺失边框仍会失败。仅修改测试采样，不更改游戏绘制或断言含义。失败日志与截图保留在 `viewport-failure-details.zip`；Debug／Release Tests 增量构建均退出 0。

对同一 805×604 失败截图重新分析，缩放区域 20,100 像素中有 16,358 个有效边框像素，按原比例计算的最低值为 76，记录见 `viewport-pixel-analysis.txt`。常规 1600×1200 重跑仍使用原 80,000 像素区域，边框像素为 65,010，所有星球与 horde 的进关、保存、重载通过。另人工查看了商店盔甲展开卡截图，人物身体与双枪贴图正常。

最终 Debug 日志、结果、受保护输入哈希与截图已归档为 `final-debug-details.zip`；构建自动检查与 Viewer smoke 日志归档为 `build-smoke-details.zip`。
最终 Release 对应证据归档为 `final-release-details.zip`。新加载实现无编译警告；其他模块已有的数值转换警告未在本轮扩散处理。`git diff --check` 通过，engine 无游戏包 include，旧助手调用与旧头文件路径均已清零。

构建命令（Configuration 分别取 Debug 和 Release；Game 自动产出三个 EXE）：

```powershell
& 'D:/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' GunBrosRe.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:GbProduct=Game /v:minimal /nologo
```

阶段二专项命令：

```powershell
pwsh -NoProfile -File tests/run.ps1 -Configuration Debug -Phase Core,OriginalUI -Case resource-loading,enemies,weapon-effects,armor-render,brother,dual-weapon,store-equipped -NoBuild
```

最终流程命令：

```powershell
pwsh -NoProfile -File tests/run.ps1 -Configuration Debug -Phase Core,OriginalUI -Case resource-loading,big-version,store-cards,store-equipped,dual-weapon,native-profile-play -NoBuild
pwsh -NoProfile -File tests/run.ps1 -Configuration Release -Phase Core,OriginalUI -Case resource-loading,weapon-effects,enemies,armor-render,brother,dual-weapon,native-profile -NoBuild
```

## 已完成的代码归属

| 职责 | 当前源码 |
|---|---|
| 游戏 keyset 与模型／图像分区范围 | `data/objects/CGunBros.h`、`CGunBrosResources.cpp` |
| 回调／图像请求排队、消费、取消与失败清理 | `engine/resources/CResourceLoader.*` |
| 同一包、handle、GL 上下文中的图像复用与释放 | `engine/resources/CImagePool.*` |
| 动作配置的模型持有与加载 | `engine/graphics/CMoveSetMesh.*`、`CMoveSetMeshResources.cpp` |
| 枪、盔甲的模型持有及加载调度 | `gameplay/weapon/CGunTemplateLoading.cpp`、`gameplay/armor/CArmorTemplateLoading.cpp` |
| 弹体自身模型的读取 | `gameplay/weapon/CBulletResources.cpp` 中的 Template::LoadMesh |
| 角色与敌人的装配 | 各自 Body／Drawing／Resources，引用模板模型并取得图像句柄 |
| 任意模型研究 | Viewer 自行读取所选模型，贴图使用同一图像池；无贴图的研究材质仍明确标记 |

`ZMeshAssets.h/.cpp` 已删除，没有留下别名或转发文件。`application/` 和 `graphics/` 空目录已移除。原组合助手的注释作为历史说明保留在 CResourceLoader.h；模型平铺纹理的注释保留在 CImagePool.cpp。

模型持有采用 shared_ptr，让模板快照共享不可变几何数据；角色的姿态与动态缓冲仍独立。两把同款枪共享模型时，躯干绘制先匹配控制器所属武器，避免仅按模型地址选错姿态缓冲。敌人仍使用已有的场景绘制缓冲缓存，逐实例上传姿态。

## 还原边界

- 本轮落实模型与模型贴图的加载链。Sprite、Movie、音频、媒体的加载分支保留既有实现，没有宣称恢复完整 CResourceLoader。
- 恢复 AddFunction／AddImage 队列、15ms LoadNext 时间片、同步 LoadImmediate、取消待加载图像及引用释放。当前模型回调是同步读取，false 表示明确失败并清理未执行请求；原版其他异步回调的重试语义不在本轮实现。
- CImagePool 用弱索引和 shared_ptr 表达原版使用计数；GL 上下文是宿主额外隔离条件。图像消费者须在其窗口／上下文销毁前释放，图像池不会额外延长 GPU 寿命。
- 原版小屏设备图像缩放、专用纹理压缩分支未照搬到 Windows；模型仍使用现有 RGBA8、GL_REPEAT 后端。枪的原 Load 不要求动作模型贴图，Windows 的离场姿态绘制通过 CMoveSetMesh 已有的图像分支显式请求这些贴图。
- CMoveSetMesh 的声音引用仍由现有音频调用链消费，本轮不重写音频调度。
