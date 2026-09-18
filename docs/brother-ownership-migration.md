# Brother 所有权归并

后续增补：`ZBrotherRenderer` 已继续拆除，当前结构与验证见 [Brother 绘制职责归并](brother-renderer-removal.md)。以下保留上一阶段的方案和验证记录。

用户已批准将 `brother/` 中的角色、装备、资源与绘制包装归回实际所有者。

## 方案与任务

1. `CBrother` 直接持有角色脚本、强化状态和枪槽；显式重置运行状态，普通换枪保持角色身份和动画。
2. `CGun`、`CArmor` 持有模板与各自资源，删除装备包装。旧枪的躯干资源保留到动画不再使用。
3. 直接使用 `CBrother::Template`，通用模型加载移至 `data/ZMeshAssets`，裸模型浏览移至 Viewer。
4. 仅保留 `ZBrotherRenderer` 作为 GPU 上传、姿态和绘制适配；删除全部 `ZPlayer*` 过渡模块。

依据：`player_template.bt`、`gun_template.bt`、`armor_template.bt`；原 `CBrother::Bind`（135608）、`SetGun`（136958、137019）、`SetAuxGun`（136986）、`CGun::Bind`（128739）、`CArmor::Bind`（176618）。本次不改磁盘结构、资源数值或碰撞算法。

验收：Game、Viewer、Tests 构建；角色反馈、武器与双持、盔甲、强化、死亡重生、本地合作和 PvP 回归。自动测试使用静音，原始输入只读，不重复全量截图基线。

## 结果

已落实的结构：

| 归属 | 实现 |
|---|---|
| 角色身份、PLAYER 脚本、强化状态、模式、枪槽 | `CBrother.h/.cpp`；完整绑定重置生命阶段，普通切枪保留脚本计时器 |
| 身体装配、装备入口、模板加载 | `CBrotherBody.cpp`、`CBrotherEquipment.cpp`、`CBrotherResources.cpp` |
| 枪械、盔甲自己的模板及资源 | `CGun`／`CArmor`，资源装配在 `CGunDrawing.cpp`／`CArmorDrawing.cpp` |
| 对战枪械缓存 | `CBrother::SelectCachedWeapon`，完成资源加载和上传后才加入缓存 |
| 姿态、骨骼挂点、GPU 缓冲和绘制 | `ZBrotherRenderer.h/.cpp`，`Part` 为嵌套类型 |
| 通用网格及图集加载 | `data/ZMeshAssets.h/.cpp`，供弹体、角色和 Viewer 共用 |
| 裸模型挂枪与动作浏览、地图预览移动 | `gun_bros_viewer/scenes/BrotherPreview.h/.cpp` |
| Bot 强化选择随机流 | 合作及 PvP Bot 实例，测试显式传入独立输入流 |

游戏、菜单、关卡、弹体和测试现直接引用 `CBrother`。没有旧类型别名或转发头，也不再复制一份 `ZPlayerTemplateData`。原有身体／武器坐标、动画时间、模板字段读取顺序不变。

首轮角色反馈与菜单双枪切换回归 2/2 通过，受保护输入变化 0；日志 `obj/brother-ownership-first-tests.log`，证据 `obj/brother-ownership-first-evidence/`。

收尾后完整相关回归 **17/17 通过，退出码 0，受保护输入变化 0**。覆盖武器、武器效果、地雷、死亡重开、本地合作、对战数据、对战与反馈、盔甲数据与绘制、角色反馈、拾取物绘制、伙伴行为、强化使用、首波生存、菜单双枪切换和地图装备预览。

```powershell
pwsh -File tests/run.ps1 -Configuration Debug -Case actor-feedback,weapons,weapon-effects,mines,armor-data,armor-render,brother,pickup-render,powerup-play,player-death,local-live,deathmatch,deathmatch-feedback,deathmatch-data,dual-weapon,horde-first,debug-map-profile -NoBuild
```

日志：`obj/brother-ownership-tests.log`；结果：`obj/brother-ownership-results.json`、`obj/brother-ownership-summary.json`；各项输出及截图：`obj/brother-ownership-evidence/`。全部自动运行由测试入口传入 `--mute`。

### 构建与核对

- `pwsh -File obj/build-runtime.ps1 -Product Game` 退出码 0，Debug Game、Viewer、Tests 均生成；日志 `obj/brother-ownership-build.log`。
- `CBrother`、`CGun`、`CArmor` 的 `Template::Init` 函数体与 HEAD 完全一致；`LoadMeshAndAtlas` 与迁移前函数体完全一致。记录：`obj/brother-ownership-audit.json`。
- 源码和测试中的旧包装类型、加载入口及装备自由函数引用全部移除。
- 角色重置断言先建立击退、护盾与等待出生状态，再检查重开清空状态；原换枪与菜单三次切换的旧躯干资源检查继续保留。
- `git diff --check` 通过。

本轮只验证 Debug；未运行 Release 或全量截图基线。
