# 角色职责拆分（2026-09-17）

此页记录第一阶段过渡结构及其验证。后续用户批准继续收回所有权，本文中的 `ZPlayer*` 模块现已全部删除，最终结构见 [Brother 所有权归并](brother-ownership-migration.md)。

## 范围与依据

用户已批准继续优化角色链路。本阶段先修复验证场景，再拆开角色宿主、装备资源和模型绘制；不扩展菜单或地图实现。

- `PLAYER` 模板：`entries/player_template.bt`，原 `CBrother::Template::Init :134571`、`CBrother::Bind :135608`。
- 武器和盔甲：`entries/gun_template.bt`、`entries/armor_template.bt`，原 `CBrother::SetGun :136958/:137019`、`CGun::Bind :128739`。角色持有枪，换枪不创建另一位角色。
- 绘制：原 `CBrother::Draw :134619`，保持躯干动画控制挂点、旧枪躯干动画完成前保留资源。
- 碰撞：原 `CPlayer::Move :100623`，需要关卡地图；先边界、再敌人、再墙体。不改原 CircleCircle 的特殊数值分支。

## 调研结果

`actor-feedback` 原基线失败 5 项。测试只调用 `BindCombat`，到最后一项才绑定地图，前面的敌人阻挡因此没有执行。把实际 BIG 地图提前绑定后，近战免疫结束恢复阻挡通过；原敌人位置还与地图边缘重叠，移至测试已使用的空旷区域后所有检查通过（0 失败）。生产碰撞逻辑无需修改。

## 实施任务与验收

1. 修复 `actor-feedback` 场景装配，并明确断言使用的站位不受墙体修正。
2. `ZPlayerActor` 持有角色输入、模式、生命引用、强化状态，以及独立的 PLAYER 脚本宿主；完整重新装备仍显式重建宿主，普通换枪保持宿主与计时器。
3. `ZPlayerEquipment` 持有武器、盔甲和换枪资源库；武器不再包含 `CBrother` 或 PLAYER 脚本。
4. 将资源装配、装备操作、姿态与绘制分成有明确职责的实现文件，保留原注释与 BIG 读取路径。
5. 更新游戏、查看器和测试的调用点。验证三种 Debug 产物，以及角色反馈、盔甲、武器、死亡、合作与对战流程；不重跑全量截图基线。

## 已实现的结构

| 文件 | 职责 |
|---|---|
| `ZPlayerActor.h/.cpp` | 玩家／伙伴模式、输入选择、生命引用、强化状态；稳定持有 PLAYER 脚本与 `CBrother` |
| `ZPlayerEquipment.h/.cpp` | 枪械与盔甲、熟练度、普通／UI／对战换枪资源库、装备装配 |
| `ZPlayerPart.h` | 网格、纹理、缓冲区、动画控制器和骨骼挂点 |
| `ZPlayerResources.cpp` | BIG 玩家模板查找、网格／图集读取、身体装配 |
| `ZPlayerRendering.cpp` | 动画推进、姿态上传、挂点、枪口、菜单与战斗矩阵和绘制 |
| `ZPlayerModel.h` | 身体资源、角色和装备的组合入口，不再平铺所有状态 |

`ZPlayerModel.cpp` 已删除。原工程按目录通配纳入源码，不需要增加工程或手工源文件清单。重建身体时先释放脚本宿主，再清除旧装备网格，保留熟练度等配置数据。普通换枪不重新初始化角色；完整装备／重开仍重建宿主，拷贝完旧脚本后才释放旧对象，支持重生入口传入自身脚本。

## 验证记录

- 原检查：`pwsh -File tests/run.ps1 -Configuration Debug -Case actor-feedback -NoBuild`，退出码 1，5 项失败；日志 `obj/player-baseline-tests.log`。
- 修复场景后，同一命令退出码 0；完整输出保存在 `obj/player-collision-baseline.log`。
- 三产物构建：`pwsh -File obj/build-runtime.ps1 -Product Game`，退出码 0；输出 Game、Viewer、Tests，日志 `obj/player-debug-build.log`。
- 拆分后 `actor-feedback` 退出码 0；新增换枪保持角色／脚本、重开重新初始化宿主的断言通过。完整日志 `obj/player-actor-final.log` 与上述碰撞修复基线逐行一致。
- 受影响回归 14/14 通过，退出码 0：武器、武器效果、地雷、死亡／重开、合作、对战、对战反馈、盔甲数据、盔甲绘制、拾取物绘制、伙伴行为、强化使用、首波生存、菜单双武器切换。加上独立运行的角色反馈，共 15 项通过；受保护文件变化 0。
- 批次日志：`obj/player-regression-tests.log`；结果：`obj/player-regression-results.json`、`obj/player-regression-summary.json`；各项 stdout/stderr 副本：`obj/player-regression-evidence/`。
- `git diff --check` 通过；源码与测试中不再存在 `weapon->brother`、`weapon->playerScript` 或旧 `ZPlayerModel.cpp` 引用。

回归命令：

```powershell
pwsh -File tests/run.ps1 -Configuration Debug -Case weapons,weapon-effects,mines,armor-data,armor-render,brother,pickup-render,powerup-play,player-death,local-live,deathmatch,deathmatch-feedback,dual-weapon,horde-first -NoBuild
```

所有自动运行由测试入口传入 `--mute`。本阶段不修改原资源格式、碰撞算法、装备数值或 Flow 行为。未声明整个角色系统已完整对应原版；桌面组合入口和现有宿主适配仍保留。本轮验证 Debug，未运行 Release 或全量截图基线。
