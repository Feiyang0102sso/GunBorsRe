# Brother 绘制职责归并

用户已批准拆分并删除 `ZBrotherRenderer`，不保留别名、转发头或替代 Renderer 包装。

## 方案与验收

1. 将姿态上传、绘制顺序、枪口与角色缩放归入 `CBrother`，保留旧枪躯干动画的资源身份。
2. `CBrother`、`CGun`、`CArmor` 分别拥有私有绘制数据；弹体使用自己的最小网格数据，不再借用角色部件。
3. 动作槽浏览、裸模型挂枪、预览动画推进与地图预览预热归 Viewer；通用矩阵归 `CMeshCamera`。
4. 游戏逻辑直接调用 `SetInput`／`Update`；`Draw` 上传当前姿态，初次创建缓冲仍立即上传初始动作帧。
5. 删除旧 Renderer 与全部自由函数引用；构建 Game、Viewer、Tests，再运行相关回归。

依据：原 `CBrother::Draw`、`DrawUI`、`CMeshCamera::OrientForGame`／`OrientForUI`／`DrawHeirarchy`；PLAYER、GUN、ARMOR 模板。这里只调整职责和资源所有权，不修改原数据字段及读取顺序。

## 最终归属

| 职责 | 实现 |
|---|---|
| 身体网格、动画姿态、可见性、闪烁、双持与盔甲绘制顺序 | `CBrotherDrawing.cpp`，私有数据在 `CBrotherDrawing.h` |
| 当前躯干解析 | `CBrother::ResolveTorsoDrawing`，按实际动画网格身份搜索身体、双枪与缓存枪 |
| 枪械网格、枪械动画与躯干动作网格 | `CGunDrawing.h/.cpp`，只向调用方提供控制器使用的只读网格身份 |
| 盔甲贴图与骨骼附件 | `CArmorDrawing.h/.cpp` |
| 弹体模型 | `ZBulletVisual::Mesh`，仅持有网格、贴图与缓冲 |
| 世界、UI 与附件矩阵 | `engine/graphics/CMeshCamera.h/.cpp` |
| 动作槽、裸模型挂枪、预览时钟和地图预热 | `gun_bros_viewer/scenes/BrotherPreview.h/.cpp` |
| GPU 资源与提交 | 继续使用 `ZMeshBuffer`、`ZTexture` |

`brother/` 根目录不再有 `Z` 开头的文件；Bot 文件继续位于 `brother/bot/`。

## 验证记录

- `pwsh -File obj/build-runtime.ps1 -Product Game`：退出码 0，Debug Game、Viewer、Tests 三个产物均通过；日志 `obj/renderer-removal-build.log`。
- 独立 Viewer 地图、武器、盔甲预览 3/3 通过，均推进 400 ms，武器确认实际开火；日志与截图在 `obj/renderer-viewer/`，汇总 `obj/renderer-viewer-checks.log`。
- 新增裸模型预览检查：加载原版身体与枪械，选择动作并推进，确认逻辑推进不改已上传顶点，绘制后顶点与当前动画一致，附件绘制没有 GL 错误。该检查并入 `actor-feedback`。
- 保留菜单换枪时的旧／新躯干网格身份断言，以及关卡初始动作姿态断言；调用方改用只读网格与已上传姿态查询。
- 本轮前后核对：三个模板读取函数、通用网格／图集加载、角色 `SetInput`／`Update`／`Bind` 未改变；旧符号引用为零，绘制数据均为私有，`brother/` 根目录无 Z 文件。记录：`obj/renderer-removal-audit.json`。

相关流程回归 **17/17 通过，退出码 0，受保护输入变化 0**。覆盖武器、武器效果、地雷、死亡重生、本地合作、PvP 数据与主流程及反馈、盔甲数据与绘制、角色反馈、拾取物绘制、伙伴行为、强化、首波生存、菜单双枪切换与地图装备预览。

```powershell
pwsh -File tests/run.ps1 -Configuration Debug -Case actor-feedback,weapons,weapon-effects,mines,armor-data,armor-render,brother,pickup-render,powerup-play,player-death,local-live,deathmatch,deathmatch-feedback,deathmatch-data,dual-weapon,horde-first,debug-map-profile -NoBuild
```

日志：`obj/renderer-removal-tests.log`；结果：`obj/renderer-removal-results.json`、`obj/renderer-removal-summary.json`；输出与截图：`obj/renderer-removal-evidence/`。`git diff --check` 通过。

本轮验证 Debug，未运行 Release。全部自动运行使用 `--mute`，未重复全量截图基线。
