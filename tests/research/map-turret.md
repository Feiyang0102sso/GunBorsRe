# pack9 炮塔状态检查

## 结论

Map Viewer 原来没有绑定两个指示灯的 `CProp` 脚本。另一个问题在共用 `CEnemy`：预览关闭战斗模拟时忽略脚本变量 14，错误地让部件 0 播放炮身序列。真实失败记录为 `sequence-move=2 selected-part=1 actual=1`，退出码 1。

修复后始终按脚本选择动画部件。viewer 专用状态选择位于 `src/gun_bros_viewer/scenes/MapTurretPreview.*`；主包没有新增展示场景。指示灯帧、持续时间、炮身动作和展开/收起序列仍从 BIG 读取。

| 展示状态 | ENEMY 状态 | PROP 状态 | 效果 |
|---|---|---|---|
| Idle（默认） | 2 | 2 | 炮身收起，绿灯闪烁 |
| Active | 3 → 4 | 3 | 展开后保持打开，红灯闪烁 |
| Off | 7 → 8 | 0 | 收起，指示灯熄灭 |
| Charging | 8 | 1 | 炮身收起，黄灯闪烁 |

`T` 循环切换，`G` 控制地形显隐；切换地图重新初始化。窗口标题显示当前炮塔状态。展示不会启用战斗 AI；玩家可持续观察一个状态。

## 原版依据

- `enemy_template.bt`：ENEMY 读取结构。pack9 ENEMY ordinal 0 对应物理资源 `0006_0x1a5e`；Spawn 写变量 14 为 1，其 states 2/3/7/8 分别选择原动作。
- 原反编译 `CEnemy::VariableResolver :68982`：变量 14 对应 mem+1064；`OnMoveChanged :68811`、`GetMoveLooped :68817` 都选择该部件，与是否运行战斗无关。
- `entries/prop_template.bt`、`CProp::Bind :124863`、`CProp::HandleMessage :123941`：灯效由脚本选择背景动画，消息 0 对应脚本事件 2。
- pack9 PROP ordinal 47 对应物理资源 `0078_0x432a`：原状态顺序是熄灭、黄色、绿色、红色。此处仅选择原状态，没有复制或重写动画帧表。
- pack9 LEVEL `0008_0x1bf7` 的字节码 `@0x288E..0x2930` 把敌人 120/121 与指示灯 37/38 的状态消息关联起来。

## 验证

从项目根目录运行：

```powershell
pwsh -File tests/run.ps1 -Case map-turret -NoBuild
```

若要保留其他检查的输出，直接指定独立目录：

```powershell
bin/Debug/GunBrosTests.exe --map-turret-check --mute --test-output E:/coding_projects/c_projects/gun_bro_re/tests/out/map-turret
```

检查默认状态、四态循环、真实灯效帧推进、关机静止、暂停、展开/收起自动完成、底座与炮身的动画部件选择，以及切换到空地图后清除旧对象引用。另经真实 `MapPropWorld::SendMessage` 验证两灯的四态循环，并开启战斗模拟验证 `CEnemy::HandleMessage` 的激活与展开，退出码 0。

`--viewer-controls-check` 验证实际 SDL 输入中 T/G 不冲突，退出码 0。Viewer Debug/Release 均构建成功，真实 Map Viewer 截图已核对。此检查覆盖游戏共用组件和消息入口，未完整重跑 pack9 的整局关卡事件调度。

## 2026-09-19：重构后炮塔迟迟不开火

### 方案与验收

只修正敌人视线检查使用的碰撞集合；射速、连发次数和状态转换继续执行 BIG 的原脚本。验证两座炮塔在原地图位置对射程内目标持续开火，同时确保真实子弹墙仍会遮挡视线，再检查 pack9 最后两波。

### 根因与原版依据

- `6a59939` 新增 native 23 的地图视线检查，错误读取 `weaponCollision.terrain`。该集合包含行走边界；炮塔即使转向目标，也会停在 state 5 等待视线放行。此前 native 23 无条件返回 1，因此重构前没有这个症状。
- 原 `CEnemy::FunctionResolver` 的 native `0x17` 在反编译行 72162 读取 `CMap mem+9932`；`CMap::SetBulletCollisionLayer :91991` 写入这个字段。行走层是 `mem+9928`，两者不能混用。
- `CLayerCollision::TestCollisionSegment :125270` 检查所选层及活动道具的 `GetBulletCollision`，对应本项目 `weaponCollision.walls`。修复位于 `src/gun_bros_re/gameplay/enemy/CEnemyPerception.cpp`。
- 已查 `enemy_template.bt`、`flow_bytecode.bt`、`entries/collision_data.bt`。pack9 ENEMY 0（`0006_0x1a5e`）state 5 在 `@0xC8` 查询视线；state 6 在 `@0x106` 发射，`@0x118` 判断三连发。LEVEL 0（`0008_0x1bf7`）在 `@0x2CAD` 选择行走层 4、`@0x2CB3` 选择子弹层 9；测试直接执行原 LEVEL 取得这些选择。

### 复现与验证

- 修复前：空场 5 秒发出 27 发；接入原地图、LEVEL 和炮塔位置后，5 秒 0 发，state 5。`tests/turret-regression-red.log`、`tests/turret-regression-red-detail.log`，检查退出 1。
- 修复后：两座原地图炮塔在约 5 秒模拟时间内各发出 27 发，最大间隔 336 ms。持续射击断言为至少 15 发且间隔不超过 1 秒，避免仅凭偶发射击通过；这些是测试容差，不是运行时射速。
- `path-cache` 同时覆盖子弹墙阻挡、禁用子弹墙但保留行走边界后放行、近处目标和共线线段。
- Debug：`pwsh -File tests/run.ps1 -Configuration Debug -Case map-turret,path-cache,final-pack9`，3/3 通过，退出 0，受保护文件变化 0。日志 `tests/turret-regression-green.log`。
- Release：`pwsh -File tests/run.ps1 -Configuration Release -Case map-turret,path-cache,final-pack9 -NoBuild`，3/3 通过，退出 0，受保护文件变化 0。两座炮塔仍各为 27 发、最长间隔 336 ms。日志 `tests/turret-regression-Release.log`。
- Debug／Release 均以 `GbProduct=Game`、`SkipAutoTests=true` 构建游戏及 Viewer／Tests，退出 0；上述定向检查单独执行并由 runner 显式传入 `--mute`。构建日志为 `tests/turret-build-Debug.log` 和 `tests/turret-build-Release.log`。
- 炮塔检查计数真实 `CEnemy` 脚本产生的 Bullet action；波次检查使用实际战斗流程。没有把定点模拟或自动驾驶验收称为人工手感验收，也没有重复全量截图基线。
